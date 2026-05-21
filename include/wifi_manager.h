#pragma once

#include <Arduino.h>

/** Inicia AP+STA: red SmartKiln-XXXX con contraseña derivada del MAC (8 chars). */
void wifi_manager_begin(const String& macNoColons);

String wifi_manager_apSsid();
String wifi_manager_apPassword();
IPAddress wifi_manager_apIp();

/** Asegura modo AP_STA antes de WiFi.begin (STA). */
void wifi_manager_ensureApStaMode();

/** Tras WiFi.mode(WIFI_OFF), vuelve a levantar AP+STA. */
void wifi_manager_restoreAp();
