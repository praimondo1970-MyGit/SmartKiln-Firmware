#pragma once



#include <Arduino.h>



/** Inicia AP: red SmartKiln-XXXX con contraseña derivada del MAC (8 chars). */

void wifi_manager_begin(const String& macNoColons);



String wifi_manager_apSsid();

String wifi_manager_apPassword();

IPAddress wifi_manager_apIp();



/** Obligatorio con NimBLE activo: modem sleep WiFi (evita abort ESP-IDF). */

void wifi_manager_applyBleCoexistence();



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

/** false durante ~5 s tras apagar el AP (evita SSL/Firebase en transición de radio). */
bool wifi_manager_isFirebaseSafe();

/** Intenta STA en segundo plano (AP+STA). No llamar desde setup(). */

void wifi_manager_tryStaConnect(const char* ssid, const char* password);

/** Reintento STA con antirebote (max ~1 begin cada 8 s). */

void wifi_manager_requestStaReconnect();



/**

 * Tras sesión HTTP local (comando, perfil, kiln-info): apaga el AP ~10s para que el

 * móvil vuelva a su WiFi de casa y luego reabre el AP automáticamente.

 */

void wifi_manager_requestApRelease();



/** Llamar periódicamente desde TaskComunicaciones (~100ms). */

void wifi_manager_tick();

