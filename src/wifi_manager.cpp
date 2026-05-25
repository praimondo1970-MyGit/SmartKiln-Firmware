#include "wifi_manager.h"

#include <WiFi.h>

extern String wifiSSID;
extern String wifiPassword;
extern bool wifiConfigReceived;

static String s_apSsid;
static String s_apPassword;
static String s_macStored;

// Gracia minima para que el cliente HTTP reciba la respuesta antes de tocar el AP.
// Antes era 2,5 s pero el POST se completa en <300 ms; 1,2 s alcanza con margen
// y acelera notablemente la transicion a Firebase tras START/PAUSE/STOP.
static const uint32_t AP_RELEASE_GRACE_MS = 1200;
/** Si el movil sigue en el AP, forzar apagado tras este tiempo (desde requestApRelease).
 *  Android casi nunca se desasocia solo de un AP sin internet, asi que esperar mas
 *  es perdida de tiempo: forzamos el cierre rapido. */
static const uint32_t AP_RELEASE_FORCE_MAX_MS = 3000;
static const uint32_t AP_OFF_DURATION_MS = 10000;

enum ApCycleState {
    AP_CYCLE_NORMAL = 0,
    AP_CYCLE_PENDING_OFF,
    AP_CYCLE_OFF,
};

static ApCycleState s_cycleState = AP_CYCLE_NORMAL;
static unsigned long s_releaseAt = 0;
static unsigned long s_forceReleaseAt = 0;
static unsigned long s_apRestoreAt = 0;
static unsigned long s_lastStaBeginMs = 0;
static unsigned long s_firebaseQuietUntil = 0;
static const uint32_t STA_BEGIN_MIN_INTERVAL_MS = 8000;
// Tiempo minimo de silencio Firebase tras una transicion AP/STA, para que lwip
// y el chip WiFi terminen de re-inicializarse antes de abrir un socket nuevo.
// Antes era 8 s; con el cierre explicito del cliente TCP en
// kiln_onApRadioTransitionBegin() ya no hace falta tanto margen.
static const uint32_t FIREBASE_QUIET_AFTER_AP_MS = 1500;

extern void kiln_onApRadioTransitionBegin();

static bool staHasValidIp() {
    return WiFi.status() == WL_CONNECTED && WiFi.localIP()[0] != 0;
}

void wifi_manager_applyBleCoexistence() {
    WiFi.setSleep(true);
}

static String suffixFromMac(const String& mac, size_t n) {
    if (mac.length() >= (int)n) {
        return mac.substring(mac.length() - n);
    }
    return mac;
}

void wifi_manager_ensureApStaMode() {
    WiFi.persistent(false);
    wifi_manager_applyBleCoexistence();
    if (WiFi.getMode() != WIFI_AP_STA) {
        WiFi.mode(WIFI_AP_STA);
        delay(100);
    }
}

static void startSoftAp() {
    IPAddress apIp(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(apIp, gateway, subnet);
    if (!WiFi.softAP(s_apSsid.c_str(), s_apPassword.c_str(), 1, 0, 4)) {
        Serial.println("[AP] ERROR: softAP no pudo iniciar");
    }
    delay(250);
}

static void startApOnly() {
    WiFi.persistent(false);
    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_AP);
    delay(100);
    startSoftAp();
}

static void markFirebaseQuietPeriod() {
    s_firebaseQuietUntil = millis() + FIREBASE_QUIET_AFTER_AP_MS;
}

static void beginStaToHome() {
    if (!wifiConfigReceived || wifiSSID.length() == 0) {
        WiFi.mode(WIFI_OFF);
        Serial.println("[AP] AP apagado (sin credenciales STA guardadas)");
        markFirebaseQuietPeriod();
        return;
    }

    markFirebaseQuietPeriod();
    kiln_onApRadioTransitionBegin();

    const bool hadStaIp = staHasValidIp();
    WiFi.softAPdisconnect(true);
    delay(150);

    if (hadStaIp || staHasValidIp()) {
        Serial.printf("[AP] AP apagado; STA mantiene IP %s (sin reiniciar WiFi)\n",
                      WiFi.localIP().toString().c_str());
        return;
    }

    WiFi.mode(WIFI_STA);
    delay(100);
    s_lastStaBeginMs = millis();
    wifi_manager_applyBleCoexistence();
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
    Serial.printf("[AP] STA hacia '%s' (Firebase / telemetria)\n", wifiSSID.c_str());
}

static void restoreApAfterRelease() {
    if (s_apSsid.length() == 0) {
        return;
    }
    wifi_manager_ensureApStaMode();
    startSoftAp();
    if (wifiConfigReceived && wifiSSID.length() > 0 && wifi_manager_apStationCount() == 0) {
        if (!staHasValidIp()) {
            wifi_manager_requestStaReconnect();
            Serial.printf("[AP] STA reanudado hacia '%s' (sin clientes en AP)\n", wifiSSID.c_str());
        }
    } else if (wifi_manager_apStationCount() > 0) {
        Serial.println("[AP] STA diferido: movil conectado al AP (prioridad HTTP local)");
    }
    Serial.println("=================================");
    Serial.println("[AP] Punto de acceso disponible de nuevo (modo AP+STA)");
    Serial.printf("[AP] SSID: %s\n", s_apSsid.c_str());
    Serial.printf("[AP] Password: %s\n", s_apPassword.c_str());
    Serial.printf("[AP] IP: %s\n", WiFi.softAPIP().toString().c_str());
    Serial.println("=================================");
}

void wifi_manager_begin(const String& macNoColons) {
    s_macStored = macNoColons;
    String mac = macNoColons;
    mac.toUpperCase();

    s_apSsid = "SmartKiln-" + suffixFromMac(mac, 4);
    s_apPassword = suffixFromMac(mac, 8);
    if (s_apPassword.length() < 8) {
        s_apPassword = mac;
        while (s_apPassword.length() < 8) {
            s_apPassword = "0" + s_apPassword;
        }
        if (s_apPassword.length() > 8) {
            s_apPassword = s_apPassword.substring(s_apPassword.length() - 8);
        }
    }

    s_cycleState = AP_CYCLE_NORMAL;
    s_releaseAt = 0;
    s_forceReleaseAt = 0;
    startApOnly();

    Serial.println("=================================");
    Serial.println("[AP] Punto de acceso SmartKiln activo");
    Serial.printf("[AP] SSID: %s\n", s_apSsid.c_str());
    Serial.printf("[AP] Password: %s\n", s_apPassword.c_str());
    Serial.printf("[AP] IP: %s\n", WiFi.softAPIP().toString().c_str());
    Serial.println("[AP] API: http://192.168.4.1/api/status");
    Serial.println("=================================");
}

void wifi_manager_restoreAp() {
    if (s_macStored.length() > 0) {
        wifi_manager_begin(s_macStored);
    }
}

void wifi_manager_ensureApRunning() {
    if (s_apSsid.length() == 0) {
        return;
    }
    if (s_cycleState == AP_CYCLE_OFF) {
        return;
    }
    if (s_cycleState == AP_CYCLE_PENDING_OFF) {
        return;
    }
    if (WiFi.getMode() != WIFI_AP && WiFi.getMode() != WIFI_AP_STA) {
        WiFi.mode(WIFI_AP);
        delay(50);
    }
    IPAddress ip = WiFi.softAPIP();
    if (ip[0] == 0) {
        Serial.println("[AP] softAP caido — reiniciando...");
        startSoftAp();
        Serial.printf("[AP] Reiniciado. IP: %s\n", WiFi.softAPIP().toString().c_str());
    }
}

bool wifi_manager_isApActive() {
    if (s_cycleState == AP_CYCLE_OFF) {
        return false;
    }
    return WiFi.softAPIP()[0] != 0;
}

int wifi_manager_apStationCount() {
    if (s_cycleState == AP_CYCLE_OFF) {
        return 0;
    }
    const wifi_mode_t mode = WiFi.getMode();
    if (mode == WIFI_AP || mode == WIFI_AP_STA) {
        return WiFi.softAPgetStationNum();
    }
    return 0;
}

void wifi_manager_prioritizeApClients() {
    const int stations = wifi_manager_apStationCount();

    if (s_cycleState == AP_CYCLE_NORMAL && stations > 0) {
        wifi_manager_ensureApRunning();
    }

    static int lastStations = 0;
    if (s_cycleState == AP_CYCLE_NORMAL && lastStations > 0 && stations == 0 &&
        wifiConfigReceived && wifiSSID.length() > 0) {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[AP] Movil desconectado — iniciando STA para Firebase");
            wifi_manager_requestStaReconnect();
        }
    }
    lastStations = stations;
}

bool wifi_manager_isStaPausedForAp() {
    return s_cycleState == AP_CYCLE_PENDING_OFF || s_cycleState == AP_CYCLE_OFF;
}

bool wifi_manager_isFirebaseSafe() {
    return millis() >= s_firebaseQuietUntil;
}

void wifi_manager_tryStaConnect(const char* ssid, const char* password) {
    if (ssid == nullptr || strlen(ssid) == 0) {
        return;
    }
    if (staHasValidIp()) {
        return;
    }
    if (wifi_manager_apStationCount() > 0) {
        return;
    }
    if (s_cycleState == AP_CYCLE_PENDING_OFF || s_cycleState == AP_CYCLE_OFF) {
        if (staHasValidIp() || !wifi_manager_isFirebaseSafe()) {
            return;
        }
    }
    const unsigned long now = millis();
    if (now - s_lastStaBeginMs < STA_BEGIN_MIN_INTERVAL_MS) {
        return;
    }
    s_lastStaBeginMs = now;

    if (s_cycleState == AP_CYCLE_NORMAL) {
        wifi_manager_ensureApStaMode();
        wifi_manager_ensureApRunning();
    }

    wifi_manager_applyBleCoexistence();
    WiFi.begin(ssid, password);
    Serial.printf("[WiFi] STA begin '%s' (estado previo: %d)\n", ssid, WiFi.status());
}

void wifi_manager_requestStaReconnect() {
    if (!wifiConfigReceived || wifiSSID.length() == 0) {
        return;
    }
    wifi_manager_tryStaConnect(wifiSSID.c_str(), wifiPassword.c_str());
}

static void scheduleApRelease() {
    const unsigned long now = millis();
    s_releaseAt = now + AP_RELEASE_GRACE_MS;
    s_forceReleaseAt = now + AP_RELEASE_FORCE_MAX_MS;
    s_cycleState = AP_CYCLE_PENDING_OFF;
    Serial.printf("[AP] Liberacion programada: gracia %lu s, forzar AP off max %lu s\n",
                  (unsigned long)(AP_RELEASE_GRACE_MS / 1000),
                  (unsigned long)(AP_RELEASE_FORCE_MAX_MS / 1000));
}

void wifi_manager_requestApRelease() {
    if (s_cycleState == AP_CYCLE_PENDING_OFF) {
        Serial.println("[AP] Liberacion ya en curso");
        return;
    }
    if (s_cycleState == AP_CYCLE_OFF) {
        Serial.println("[AP] Liberacion omitida (ventana STA activa)");
        return;
    }
    const int stations = wifi_manager_apStationCount();
    if (stations > 0) {
        Serial.printf("[AP] Movil en AP (%d) — se apagara el AP en <=%lu s para liberar WiFi de casa\n",
                      stations, (unsigned long)(AP_RELEASE_FORCE_MAX_MS / 1000));
    }
    scheduleApRelease();
}

void wifi_manager_tick() {
    const unsigned long now = millis();

    switch (s_cycleState) {
        case AP_CYCLE_PENDING_OFF: {
            const int stations = wifi_manager_apStationCount();
            const bool graceElapsed = (now >= s_releaseAt);
            const bool forceElapsed = (now >= s_forceReleaseAt);

            if (!graceElapsed) {
                break;
            }
            if (stations > 0 && !forceElapsed) {
                break;
            }

            if (stations > 0 && forceElapsed) {
                Serial.printf("[AP] Liberacion FORZADA (%d cliente(s) aun asociados) — AP off para Firebase\n",
                              stations);
            } else {
                Serial.println("[AP] Liberacion de AP (sin clientes en gracia)");
            }

            beginStaToHome();
            s_apRestoreAt = now + AP_OFF_DURATION_MS;
            s_cycleState = AP_CYCLE_OFF;
            Serial.printf("[AP] El movil puede volver a su WiFi de casa; AP vuelve en %lu s\n",
                          (unsigned long)(AP_OFF_DURATION_MS / 1000));
            break;
        }

        case AP_CYCLE_OFF:
            if (now >= s_apRestoreAt) {
                restoreApAfterRelease();
                s_cycleState = AP_CYCLE_NORMAL;
            }
            break;

        default:
            break;
    }
}

String wifi_manager_apSsid() { return s_apSsid; }
String wifi_manager_apPassword() { return s_apPassword; }
IPAddress wifi_manager_apIp() { return WiFi.softAPIP(); }
