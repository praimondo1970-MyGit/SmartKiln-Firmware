#include "wifi_manager.h"
#include <WiFi.h>

static String s_apSsid;
static String s_apPassword;
static String s_macStored;

static String suffixFromMac(const String& mac, size_t n) {
    if (mac.length() >= (int)n) {
        return mac.substring(mac.length() - n);
    }
    return mac;
}

void wifi_manager_ensureApStaMode() {
    WiFi.persistent(false);
    if (WiFi.getMode() != WIFI_AP_STA) {
        WiFi.mode(WIFI_AP_STA);
        delay(100);
    }
}

void wifi_manager_restoreAp() {
    if (s_macStored.length() > 0) {
        wifi_manager_begin(s_macStored);
    }
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

    wifi_manager_ensureApStaMode();
    WiFi.softAP(s_apSsid.c_str(), s_apPassword.c_str());
    delay(200);

    Serial.println("=================================");
    Serial.println("[AP] Punto de acceso SmartKiln activo");
    Serial.printf("[AP] SSID: %s\n", s_apSsid.c_str());
    Serial.printf("[AP] Password: %s\n", s_apPassword.c_str());
    Serial.printf("[AP] IP: %s\n", WiFi.softAPIP().toString().c_str());
    Serial.println("[AP] API: http://192.168.4.1/api/status");
    Serial.println("=================================");
}

String wifi_manager_apSsid() { return s_apSsid; }
String wifi_manager_apPassword() { return s_apPassword; }
IPAddress wifi_manager_apIp() { return WiFi.softAPIP(); }
