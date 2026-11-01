#include "kiln_firing_firebase.h"

#include <Firebase_ESP_Client.h>
#include <time.h>

static const unsigned long KILN_FIRING_SAMPLE_EVERY_MS = 180000UL;
static const int KILN_FIRING_MAX_STAGES = 10;

static bool s_active = false;
static bool s_finalized = false;
static bool s_finalizeRequested = false;
static bool s_completedNormally = true;
static bool s_metaPushed = false;
static bool s_stagesPushed = false;

static String s_firingId;
static String s_programName;
static String s_deviceName;
static unsigned long s_startMillis = 0;
static uint32_t s_startEpochSec = 0;
static bool s_publishedWallStart = false;
static bool s_wroteMetaStart = false;
static bool s_wroteActivityStart = false;
static int s_sampleCount = 0;
static unsigned long s_lastSampleMs = 0;
static float s_lastSampleTemp = -999.0f;

static int s_stageCount = 0;
static float s_stageTemp[KILN_FIRING_MAX_STAGES];
static float s_stageRamp[KILN_FIRING_MAX_STAGES];
static int s_stageSoak[KILN_FIRING_MAX_STAGES];
static int s_stageDuration[KILN_FIRING_MAX_STAGES];

static String recordingBase(const String& userId, const String& mac) {
    return "/users/" + userId + "/kilns/" + mac + "/recordings/" + s_firingId;
}

// Epoch válido (post-2020). time() antes de NTP suele ser 0 → Date(0) = dic 1969 (UTC-3).
static const time_t kMinValidEpochSec = 1600000000L;

static bool wallClockSec(uint32_t* out) {
    time_t t = time(nullptr);
    if (t <= kMinValidEpochSec) {
        return false;
    }
    if (out) {
        *out = (uint32_t)t;
    }
    return true;
}

static uint32_t nowEpochSec() {
    uint32_t t = 0;
    return wallClockSec(&t) ? t : 0;
}

/** Si NTP llega después del BEGIN, reconstruye el instante de arranque. */
static void resolveStartEpoch() {
    if (s_startEpochSec > (uint32_t)kMinValidEpochSec) {
        return;
    }
    uint32_t now = 0;
    if (!wallClockSec(&now)) {
        return;
    }
    unsigned long elapsedSec = 0;
    if (s_startMillis > 0 && millis() >= s_startMillis) {
        elapsedSec = (millis() - s_startMillis) / 1000UL;
    }
    if (now > elapsedSec) {
        s_startEpochSec = now - (uint32_t)elapsedSec;
    } else {
        s_startEpochSec = now;
    }
    Serial.printf("[Firing] start epoch NTP=%lu (elapsed %lus)\n",
                  (unsigned long)s_startEpochSec, elapsedSec);
}

static bool hasWallStart() {
    return s_startEpochSec > (uint32_t)kMinValidEpochSec;
}

/** Nunca publica 0 ni millis-como-epoch (eso sale dic 1969 / ene 1970 en la app). */
static void writeEpochMsOrServerTs(FirebaseData* fbdo, const String& path, uint32_t epochSec) {
    if (epochSec > (uint32_t)kMinValidEpochSec) {
        Firebase.RTDB.setDouble(fbdo, path.c_str(), (double)epochSec * 1000.0);
    } else {
        Firebase.RTDB.setTimestamp(fbdo, path.c_str());
    }
}

/** startedAtMs estable: epoch NTP, o un único timestamp de servidor (no reescribirlo). */
static void writeStartedAndEndedMs(FirebaseData* fbdo, const String& path, bool ended,
                                   bool* wroteFlag) {
    if (hasWallStart()) {
        writeEpochMsOrServerTs(fbdo, path + "/startedAtMs", s_startEpochSec);
        writeEpochMsOrServerTs(fbdo, path + "/endedAtMs",
                               ended ? nowEpochSec() : s_startEpochSec);
        s_publishedWallStart = true;
        if (wroteFlag) {
            *wroteFlag = true;
        }
        return;
    }
    if (wroteFlag && !*wroteFlag) {
        Firebase.RTDB.setTimestamp(fbdo, (path + "/startedAtMs").c_str());
        Firebase.RTDB.setTimestamp(fbdo, (path + "/endedAtMs").c_str());
        *wroteFlag = true;
        return;
    }
    if (ended) {
        Firebase.RTDB.setTimestamp(fbdo, (path + "/endedAtMs").c_str());
    }
}

void kilnFiringBegin(
    const String& mac,
    const String& programName,
    const String& deviceName,
    float startTempC,
    const Segmento* segmentos,
    int numSegmentos
) {
    if (mac.length() == 0 || segmentos == nullptr || numSegmentos <= 0) {
        return;
    }
    // Si ya hay una activa (RESUME), no reiniciar
    if (s_active && !s_finalized) {
        Serial.println("[Firing] BEGIN ignorado — sesión ya activa");
        return;
    }

    s_programName = programName;
    s_deviceName = deviceName;
    s_stageCount = numSegmentos;
    if (s_stageCount > KILN_FIRING_MAX_STAGES) {
        s_stageCount = KILN_FIRING_MAX_STAGES;
    }

    float prevTemp = startTempC;
    for (int i = 0; i < s_stageCount; i++) {
        s_stageTemp[i] = segmentos[i].tempObjetivo;
        s_stageRamp[i] = segmentos[i].rampaCporMin;
        s_stageSoak[i] = segmentos[i].tiempoRemojoMin;
        int rampMin = 0;
        if (segmentos[i].rampaCporMin > 0.01f) {
            rampMin = (int)(fabsf(segmentos[i].tempObjetivo - prevTemp) /
                            segmentos[i].rampaCporMin + 0.5f);
        }
        if (rampMin < 0) rampMin = 0;
        s_stageDuration[i] = rampMin + segmentos[i].tiempoRemojoMin;
        prevTemp = segmentos[i].tempObjetivo;
    }

    s_startMillis = millis();
    s_startEpochSec = 0;
    s_publishedWallStart = false;
    s_wroteMetaStart = false;
    s_wroteActivityStart = false;
    resolveStartEpoch();
    if (hasWallStart()) {
        s_firingId = mac + "_" + String((uint64_t)s_startEpochSec * 1000ULL);
        s_publishedWallStart = true;
    } else {
        // Sin NTP: id único con millis, no "mac_0" ni epoch falso.
        s_firingId = mac + "_" + String((uint64_t)s_startMillis);
    }
    s_active = true;
    s_finalized = false;
    s_finalizeRequested = false;
    s_completedNormally = true;
    s_metaPushed = false;
    s_stagesPushed = false;
    s_sampleCount = 0;
    s_lastSampleMs = 0;
    s_lastSampleTemp = -999.0f;

    Serial.printf("[Firing] BEGIN id=%s name=%s stages=%d\n",
                  s_firingId.c_str(), s_programName.c_str(), s_stageCount);
}

void kilnFiringRequestFinalize(bool completedNormally) {
    if (!s_active || s_finalized) {
        return;
    }
    s_finalizeRequested = true;
    s_completedNormally = completedNormally;
    Serial.printf("[Firing] finalize requested complete=%d\n", completedNormally ? 1 : 0);
}

static void pushMeta(FirebaseData* fbdo, const String& userId, const String& mac,
                     bool active, bool ended) {
    String path = recordingBase(userId, mac) + "/meta";
    Firebase.RTDB.setString(fbdo, (path + "/type").c_str(), "KILN");
    Firebase.RTDB.setString(fbdo, (path + "/name").c_str(), s_programName);
    Firebase.RTDB.setString(fbdo, (path + "/deviceMac").c_str(), mac);
    writeStartedAndEndedMs(fbdo, path, ended, &s_wroteMetaStart);
    Firebase.RTDB.setBool(fbdo, (path + "/active").c_str(), active);
    Firebase.RTDB.setBool(fbdo, (path + "/ended").c_str(), ended);
    Firebase.RTDB.setInt(fbdo, (path + "/sampleCount").c_str(), s_sampleCount);
}

static void pushStages(FirebaseData* fbdo, const String& userId, const String& mac) {
    String base = recordingBase(userId, mac) + "/stages/";
    for (int i = 0; i < s_stageCount; i++) {
        String p = base + String(i);
        Firebase.RTDB.setInt(fbdo, (p + "/stageNumber").c_str(), i + 1);
        Firebase.RTDB.setFloat(fbdo, (p + "/targetTemp").c_str(), s_stageTemp[i]);
        Firebase.RTDB.setFloat(fbdo, (p + "/rampRate").c_str(), s_stageRamp[i]);
        Firebase.RTDB.setInt(fbdo, (p + "/maintenanceMinutes").c_str(), s_stageSoak[i]);
        Firebase.RTDB.setInt(fbdo, (p + "/durationMinutes").c_str(), s_stageDuration[i]);
    }
}

static void pushSample(FirebaseData* fbdo, const String& userId, const String& mac,
                       float elapsedMin, float tempC) {
    String p = recordingBase(userId, mac) + "/samples/" + String(s_sampleCount);
    Firebase.RTDB.setFloat(fbdo, (p + "/elapsedMin").c_str(), elapsedMin);
    Firebase.RTDB.setFloat(fbdo, (p + "/temperatureC").c_str(), tempC);
    writeEpochMsOrServerTs(fbdo, p + "/epochMs", nowEpochSec());
    s_sampleCount++;
}

static void pushActivity(FirebaseData* fbdo, const String& userId, const String& mac,
                         bool ended) {
    String path = "/users/" + userId + "/activity/" + s_firingId;
    float maxTemp = s_lastSampleTemp > 0 ? s_lastSampleTemp : 0;
    for (int i = 0; i < s_stageCount; i++) {
        if (s_stageTemp[i] > maxTemp) maxTemp = s_stageTemp[i];
    }
    int durationMin = 0;
    if (s_startMillis > 0) {
        durationMin = (int)((millis() - s_startMillis) / 60000UL);
    }

    Firebase.RTDB.setString(fbdo, (path + "/id").c_str(), s_firingId);
    Firebase.RTDB.setString(fbdo, (path + "/type").c_str(), "KILN");
    Firebase.RTDB.setString(fbdo, (path + "/deviceMac").c_str(), mac);
    Firebase.RTDB.setString(fbdo, (path + "/deviceName").c_str(), s_deviceName);
    Firebase.RTDB.setString(fbdo, (path + "/name").c_str(), s_programName);
    writeStartedAndEndedMs(fbdo, path, ended, &s_wroteActivityStart);
    Firebase.RTDB.setInt(fbdo, (path + "/durationMin").c_str(), durationMin);
    Firebase.RTDB.setInt(fbdo, (path + "/maxTemperatureC").c_str(), (int)(maxTemp + 0.5f));
    Firebase.RTDB.setInt(fbdo, (path + "/sampleCount").c_str(), s_sampleCount);
    Firebase.RTDB.setBool(fbdo, (path + "/complete").c_str(),
                          ended && s_completedNormally && s_sampleCount > 0);
    Firebase.RTDB.setBool(fbdo, (path + "/active").c_str(), !ended);
    Firebase.RTDB.setString(fbdo, (path + "/source").c_str(), "ESP");
    Firebase.RTDB.setString(fbdo, (path + "/recordingId").c_str(), s_firingId);
}

static void doFinalize(FirebaseData* fbdo, const String& userId, const String& mac,
                       float tempC, unsigned long programStartMillis) {
    if (!s_active || s_finalized) return;
    if (userId.length() == 0 || mac.length() == 0 || s_firingId.length() == 0) return;

    resolveStartEpoch();

    unsigned long startMs = programStartMillis > 0 ? programStartMillis : s_startMillis;
    float elapsedMin = 0;
    if (startMs > 0) {
        elapsedMin = (millis() - startMs) / 60000.0f;
    }
    if (tempC > 0) {
        pushSample(fbdo, userId, mac, elapsedMin, tempC);
        s_lastSampleTemp = tempC;
    }
    if (!s_stagesPushed) {
        pushStages(fbdo, userId, mac);
        s_stagesPushed = true;
    }
    pushMeta(fbdo, userId, mac, false, true);
    pushActivity(fbdo, userId, mac, true);

    s_finalized = true;
    s_active = false;
    s_finalizeRequested = false;
    Serial.printf("[Firing] FINALIZED id=%s samples=%d\n",
                  s_firingId.c_str(), s_sampleCount);
}

void kilnFiringOnReport(
    FirebaseData* fbdo,
    const String& userId,
    const String& mac,
    const String& estado,
    float tempC,
    unsigned long programStartMillis
) {
    if (fbdo == nullptr) return;
    if (userId.length() == 0 || mac.length() == 0) return;

    if (estado == "FINALIZADO") {
        s_finalizeRequested = true;
        s_completedNormally = true;
    }

    if (s_finalizeRequested && s_active && !s_finalized) {
        doFinalize(fbdo, userId, mac, tempC, programStartMillis);
        return;
    }

    if (!s_active || s_finalized) return;
    if (estado != "CALENTANDO" && estado != "PAUSADO" && estado != "INTERRUMPIDO") {
        return;
    }

    resolveStartEpoch();

    if (!s_metaPushed) {
        pushMeta(fbdo, userId, mac, true, false);
        s_metaPushed = true;
        // Registrar en Actividad desde el inicio (se actualiza al FINALIZADO).
        pushActivity(fbdo, userId, mac, false);
        Serial.printf("[Firing] activity created (in progress) id=%s\n", s_firingId.c_str());
    } else if (hasWallStart() && !s_publishedWallStart) {
        // NTP llegó después: corregir dic 1969 / timestamp de servidor.
        pushMeta(fbdo, userId, mac, true, false);
        pushActivity(fbdo, userId, mac, false);
        Serial.printf("[Firing] activity date corrected id=%s start=%lu\n",
                      s_firingId.c_str(), (unsigned long)s_startEpochSec);
    }
    if (!s_stagesPushed) {
        pushStages(fbdo, userId, mac);
        s_stagesPushed = true;
        Serial.printf("[Firing] stages pushed n=%d\n", s_stageCount);
    }

    String liveSession = "/users/" + userId + "/kilns/" + mac + "/realtimeData/sessionId";
    Firebase.RTDB.setString(fbdo, liveSession.c_str(), s_firingId);

    unsigned long startMs = programStartMillis > 0 ? programStartMillis : s_startMillis;
    float elapsedMin = startMs > 0 ? (millis() - startMs) / 60000.0f : 0;

    const bool dueByTime = (s_lastSampleMs == 0) ||
        (millis() - s_lastSampleMs >= KILN_FIRING_SAMPLE_EVERY_MS);
    const bool dueByTemp = (s_lastSampleTemp < -100.0f) ||
        (fabsf(tempC - s_lastSampleTemp) >= 5.0f);
    if (dueByTime || (dueByTemp && millis() - s_lastSampleMs >= 30000UL)) {
        pushSample(fbdo, userId, mac, elapsedMin, tempC);
        s_lastSampleMs = millis();
        s_lastSampleTemp = tempC;
        pushMeta(fbdo, userId, mac, true, false);
        Serial.printf("[Firing] sample[%d] t=%.1f temp=%.1f\n",
                      s_sampleCount - 1, elapsedMin, tempC);
    }
}
