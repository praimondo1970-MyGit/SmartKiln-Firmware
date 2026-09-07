#pragma once

#include <Arduino.h>

/** Inicia AP: red SmartKiln-XXXX con contraseña derivada del MAC (8 chars). */
void wifi_manager_begin(const String& macNoColons);

String wifi_manager_apSsid();
String wifi_manager_apPassword();
IPAddress wifi_manager_apIp();

/** Modem sleep WiFi desactivado (mejor STA). Alias legacy: applyBleCoexistence. */
void wifi_manager_applyRadioDefaults();
void wifi_manager_applyBleCoexistence();

/**
 * Con SoftAP+STA el DNS del router a menudo falla (p.ej. identitytoolkit.googleapis.com).
 * Fija DNS públicos 8.8.8.8 / 1.1.1.1 en la interfaz STA (lwIP + WiFi.config).
 */
void wifi_manager_ensurePublicDns();

/**
 * Sin clientes en el AP: apaga SoftAP y deja solo STA.
 * En ESP32 el SoftAP activo rompe con frecuencia las consultas DNS UDP hacia Internet.
 * Devuelve true si quedó en modo apto para Internet (STA).
 */
bool wifi_manager_pauseApForInternet();

/** Restaura SoftAP (AP+STA) tras wifi_manager_pauseApForInternet(). */
void wifi_manager_resumeApAfterInternet();

/** true si SoftAP está pausado para DNS/Firebase. */
bool wifi_manager_isApPausedForInternet();

/** Asegura modo AP_STA antes de WiFi.begin (STA). */
void wifi_manager_ensureApStaMode();

/** Tras WiFi.mode(WIFI_OFF), vuelve a levantar AP. */
void wifi_manager_restoreAp();

/** Re-levanta el AP si dejó de responder (no durante ciclo de liberación). */
void wifi_manager_ensureApRunning();

/** Clientes conectados al AP del horno (0 si el AP está apagado temporalmente). */
int wifi_manager_apStationCount();

/** true si el AP está anunciándose (no en ventana de liberación). */
bool wifi_manager_isApActive();

void wifi_manager_prioritizeApClients();

bool wifi_manager_isStaPausedForAp();

/** false durante un breve margen tras apagar el AP (evita SSL/Firebase en transición). */
bool wifi_manager_isFirebaseSafe();

/**
 * Intenta STA en segundo plano (AP+STA).
 * Alinea el canal del SoftAP al de la WiFi de casa (requisito del radio ESP32).
 */
void wifi_manager_tryStaConnect(const char* ssid, const char* password);

/** Reintento STA con antirebote. */
void wifi_manager_requestStaReconnect();

/**
 * Tras sesión HTTP local (comando, perfil, kiln-info): apaga el AP ~30s para que el
 * móvil vuelva a su WiFi de casa y luego reabre el AP automáticamente.
 */
void wifi_manager_requestApRelease();

/** Llamar periódicamente desde TaskComunicaciones (~100ms). */
void wifi_manager_tick();
