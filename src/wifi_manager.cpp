#include "wifi_manager.h"

#include <WiFi.h>
#include <string.h>
#include "esp_netif.h"

extern String wifiSSID;
extern String wifiPassword;
extern bool wifiConfigReceived;

static String s_apSsid;
static String s_apPassword;
static String s_macStored;
static int s_softApChannel = 1;
static bool s_apPausedForInternet = false;

// === WiFi AP handoff timing — perfil B (equilibrado) ===
// Revertir a perfil A (conservador): GRACE=1200, FORCE=3000, OFF=30000, QUIET=1500
// Gracia minima para que el cliente HTTP reciba la respuesta antes de tocar el AP.
static const uint32_t AP_RELEASE_GRACE_MS = 800;
/** Si el movil sigue en el AP, forzar apagado tras este tiempo (desde requestApRelease). */
static const uint32_t AP_RELEASE_FORCE_MAX_MS = 2000;
/** SoftAP off antes de restaurar AP (perfil B ~12 s; perfil A 30 s). */
static const uint32_t AP_OFF_DURATION_MS = 12000;

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
static const uint32_t STA_BEGIN_MIN_INTERVAL_MS = 12000;
static const uint32_t FIREBASE_QUIET_AFTER_AP_MS = 1000;

extern void kiln_onApRadioTransitionBegin();

static bool staHasValidIp() {
    return WiFi.status() == WL_CONNECTED && WiFi.localIP()[0] != 0;
}

/** WiFi moderno: sin modem-sleep agresivo (antes se usaba por coexistencia BLE). */
void wifi_manager_applyRadioDefaults() {
    WiFi.setSleep(false);
}

// Compat: llamadas antiguas al nombre BLE.
void wifi_manager_applyBleCoexistence() {
    wifi_manager_applyRadioDefaults();
}

static void setStaDnsEspNetif(uint8_t a, uint8_t b, uint8_t c, uint8_t d,
                             esp_netif_dns_type_t which) {
    esp_netif_t* sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta == nullptr) {
        return;
    }
    esp_netif_dns_info_t info;
    memset(&info, 0, sizeof(info));
    info.ip.type = ESP_IPADDR_TYPE_V4;
    info.ip.u_addr.ip4.addr = ESP_IP4TOADDR(a, b, c, d);
    esp_netif_set_dns_info(sta, which, &info);
}

void wifi_manager_ensurePublicDns() {
    if (WiFi.status() != WL_CONNECTED || WiFi.localIP()[0] == 0) {
        return;
    }
    const IPAddress before = WiFi.dnsIP(0);
    const IPAddress dns1(8, 8, 8, 8);
    const IPAddress dns2(1, 1, 1, 1);

    // 1) Interfaz STA de lwIP (importante con SoftAP paralelo).
    setStaDnsEspNetif(8, 8, 8, 8, ESP_NETIF_DNS_MAIN);
    setStaDnsEspNetif(1, 1, 1, 1, ESP_NETIF_DNS_BACKUP);

    // 2) API Arduino (puede no tocar la if STA correcta en AP+STA).
    if (!WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), dns1, dns2)) {
        Serial.println("[WiFi] WiFi.config DNS falló (esp_netif ya aplicado)");
    }
    Serial.printf("[WiFi] DNS STA: %s → %s / %s (mode=%d)\n",
                  before.toString().c_str(),
                  WiFi.dnsIP(0).toString().c_str(),
                  WiFi.dnsIP(1).toString().c_str(),
                  (int)WiFi.getMode());
}

static void startSoftAp();  // fwd

bool wifi_manager_pauseApForInternet() {
    if (wifi_manager_apStationCount() > 0) {
        Serial.println("[WiFi] SoftAP con móvil — no se pausa (prioridad HTTP local)");
        wifi_manager_ensurePublicDns();
        return false;
    }
    if (WiFi.getMode() == WIFI_STA && staHasValidIp()) {
        s_apPausedForInternet = true;
        wifi_manager_ensurePublicDns();
        return true;
    }

    Serial.println("[WiFi] Pausando SoftAP → solo STA (DNS/Firebase)");
    WiFi.softAPdisconnect(true);
    delay(150);
    WiFi.mode(WIFI_STA);
    delay(200);
    s_apPausedForInternet = true;
    wifi_manager_applyRadioDefaults();
    wifi_manager_ensurePublicDns();

    // Tras softAPdisconnect a veces hace falta un empujón al STA.
    if (!staHasValidIp() && wifiConfigReceived && wifiSSID.length() > 0) {
        Serial.printf("[WiFi] STA re-begin tras pausar AP: %s\n", wifiSSID.c_str());
        WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
        const unsigned long t0 = millis();
        while (!staHasValidIp() && millis() - t0 < 8000) {
            delay(200);
        }
    }
    Serial.printf("[WiFi] Tras pausa AP: status=%d IP=%s\n",
                  (int)WiFi.status(), WiFi.localIP().toString().c_str());
    return staHasValidIp();
}

void wifi_manager_resumeApAfterInternet() {
    if (!s_apPausedForInternet) {
        return;
    }
    s_apPausedForInternet = false;
    // Una sola restauracion: si estamos en ventana de liberacion AP→STA,
    // wifi_manager_tick() reabre el SoftAP al final. Evita el doble "softAP OK"
    // (Firebase + ciclo AP off) que hace que el movil se reenganche.
    if (s_cycleState == AP_CYCLE_OFF || s_cycleState == AP_CYCLE_PENDING_OFF) {
        Serial.println("[AP] SoftAP diferido (ventana liberacion activa)");
        return;
    }
    if (s_apSsid.length() == 0) {
        return;
    }
    if (staHasValidIp()) {
        const int ch = WiFi.channel();
        if (ch >= 1 && ch <= 13) {
            s_softApChannel = ch;
        }
    }
    wifi_manager_ensureApStaMode();
    startSoftAp();
    Serial.printf("[AP] SoftAP restaurado canal=%d (tras Internet/Firebase)\n",
                  s_softApChannel);
}

bool wifi_manager_isApPausedForInternet() {
    return s_apPausedForInternet;
}

static String suffixFromMac(const String& mac, size_t n) {
    if (mac.length() >= (int)n) {
        return mac.substring(mac.length() - n);
    }
    return mac;
}

void wifi_manager_ensureApStaMode() {
    WiFi.persistent(false);
    wifi_manager_applyRadioDefaults();
    if (WiFi.getMode() != WIFI_AP_STA) {
        WiFi.mode(WIFI_AP_STA);
        delay(100);
    }
}

/**
 * En ESP32, SoftAP y STA comparten un solo radio: el STA solo puede
 * asociarse a redes en el MISMO canal que el SoftAP. Si el SoftAP queda
 * fijo en canal 1 y la WiFi de casa está en otro, WiFi.status() queda en 0/6
 * para siempre. Por eso alineamos el SoftAP al canal de la red guardada.
 */
static int scanHomeWifiChannel(const char* ssid) {
    if (ssid == nullptr || strlen(ssid) == 0) {
        return -1;
    }
    Serial.printf("[WiFi] Escaneando canal de '%s'...\n", ssid);
    const int n = WiFi.scanNetworks(/*async=*/false, /*hidden=*/true);
    int channel = -1;
    for (int i = 0; i < n; i++) {
        if (WiFi.SSID(i) == ssid) {
            channel = WiFi.channel(i);
            Serial.printf("[WiFi] Encontrada '%s' RSSI=%d canal=%d\n",
                          ssid, WiFi.RSSI(i), channel);
            break;
        }
    }
    if (channel < 1) {
        Serial.printf("[WiFi] '%s' no visible (%d redes). ¿SSID/pass o fuera de alcance?\n",
                      ssid, n);
    }
    WiFi.scanDelete();
    return channel;
}

static void startSoftAp() {
    IPAddress apIp(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(apIp, gateway, subnet);
    if (s_softApChannel < 1 || s_softApChannel > 13) {
        s_softApChannel = 1;
    }
    if (!WiFi.softAP(s_apSsid.c_str(), s_apPassword.c_str(), s_softApChannel, 0, 4)) {
        Serial.println("[AP] ERROR: softAP no pudo iniciar");
    } else {
        Serial.printf("[AP] softAP OK canal=%d IP=%s\n",
                      s_softApChannel, WiFi.softAPIP().toString().c_str());
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
    wifi_manager_applyRadioDefaults();
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
    Serial.printf("[AP] STA hacia '%s' (Firebase / telemetria)\n", wifiSSID.c_str());
}

static void restoreApAfterRelease() {
    if (s_apSsid.length() == 0) {
        return;
    }
    // Si ya hay STA conectado, SoftAP en el mismo canal que la casa.
    if (staHasValidIp()) {
        const int ch = WiFi.channel();
        if (ch >= 1 && ch <= 13) {
            s_softApChannel = ch;
        }
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
    Serial.printf("[AP] Canal: %d\n", s_softApChannel);
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
    s_softApChannel = 1;
    s_apPausedForInternet = false;
    startApOnly();

    Serial.println("=================================");
    Serial.println("[AP] Punto de acceso SmartKiln activo");
    Serial.printf("[AP] SSID: %s\n", s_apSsid.c_str());
    Serial.printf("[AP] Password: %s\n", s_apPassword.c_str());
    Serial.printf("[AP] Canal: %d\n", s_softApChannel);
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
    if (s_apPausedForInternet) {
        return;
    }
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
    if (s_apPausedForInternet || s_cycleState == AP_CYCLE_OFF) {
        return false;
    }
    return WiFi.softAPIP()[0] != 0;
}

int wifi_manager_apStationCount() {
    if (s_apPausedForInternet || s_cycleState == AP_CYCLE_OFF) {
        return 0;
    }
    const wifi_mode_t mode = WiFi.getMode();
    if (mode == WIFI_AP || mode == WIFI_AP_STA) {
        return WiFi.softAPgetStationNum();
    }
    return 0;
}

void wifi_manager_prioritizeApClients() {
    if (s_apPausedForInternet) {
        return;
    }
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
    if (s_apPausedForInternet) {
        const unsigned long now = millis();
        if (s_lastStaBeginMs != 0 && now - s_lastStaBeginMs < STA_BEGIN_MIN_INTERVAL_MS) {
            return;
        }
        s_lastStaBeginMs = now;
        wifi_manager_applyRadioDefaults();
        WiFi.begin(ssid, password);
        Serial.printf("[WiFi] STA begin (AP pausado) '%s'\n", ssid);
        return;
    }
    if (wifi_manager_apStationCount() > 0) {
        Serial.println("[WiFi] STA omitido: hay movil en el AP local");
        return;
    }
    if (s_cycleState == AP_CYCLE_PENDING_OFF || s_cycleState == AP_CYCLE_OFF) {
        if (staHasValidIp() || !wifi_manager_isFirebaseSafe()) {
            return;
        }
    }
    const unsigned long now = millis();
    // Primer intento (s_lastStaBeginMs==0) siempre permitido — el intervalo
    // de 12 s bloqueaba el begin del setup (~7 s) y dejaba status 255 ~30 s.
    if (s_lastStaBeginMs != 0 && now - s_lastStaBeginMs < STA_BEGIN_MIN_INTERVAL_MS) {
        return;
    }
    s_lastStaBeginMs = now;

    wifi_manager_applyRadioDefaults();

    if (s_cycleState == AP_CYCLE_NORMAL) {
        wifi_manager_ensureApStaMode();

        const int homeCh = scanHomeWifiChannel(ssid);
        if (homeCh >= 1 && homeCh <= 13 && homeCh != s_softApChannel) {
            Serial.printf("[WiFi] SoftAP canal %d → %d (requerido por STA)\n",
                          s_softApChannel, homeCh);
            WiFi.softAPdisconnect(true);
            delay(150);
            s_softApChannel = homeCh;
            startSoftAp();
        } else {
            wifi_manager_ensureApRunning();
        }
    }

    WiFi.disconnect(false);
    delay(50);
    WiFi.begin(ssid, password);
    Serial.printf("[WiFi] STA begin '%s' (status=%d mode=%d apCh=%d)\n",
                  ssid, (int)WiFi.status(), (int)WiFi.getMode(), s_softApChannel);
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
    if (s_apPausedForInternet) {
        Serial.println("[AP] Liberacion omitida (SoftAP ya pausado para Internet)");
        return;
    }
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
