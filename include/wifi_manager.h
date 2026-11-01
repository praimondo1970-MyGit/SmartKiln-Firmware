#pragma once

#include <Arduino.h>

/** Inicia SoftAP horno: SmartKiln-<8 hex MAC> con la misma clave (WPA2).
 *  Prefijo SmartLink- reservado para DataLogger. */
void wifi_manager_begin(const String& macNoColons);

String wifi_manager_apSsid();
String wifi_manager_apPassword();
IPAddress wifi_manager_apIp();

/** Modem sleep WiFi desactivado (mejor STA). */
void wifi_manager_applyRadioDefaults();

/**
 * Con SoftAP+STA el DNS del router a menudo falla (p.ej. identitytoolkit.googleapis.com).
 * Fija DNS públicos 8.8.8.8 / 1.1.1.1 en la interfaz STA (lwIP + WiFi.config).
 */
void wifi_manager_ensurePublicDns();

/**
 * SoftAP siempre vivo (Online): NO apaga el AP por DNS/Firebase.
 * Solo asegura DNS públicos en STA y reporta si hay IP.
 * Devuelve true si STA tiene IP (apto para intentar nube).
 */
bool wifi_manager_pauseApForInternet();

/** Compat: no-op si SoftAP no se pausó (Paso 1 Online). */
void wifi_manager_resumeApAfterInternet();

/** true solo si en el pasado se pausó SoftAP (legado; debería ser raro). */
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

/** Desconecta solo STA (WiFi de casa); SoftAP sigue activo. */
void wifi_manager_disconnectStaKeepAp();

/**
 * Tras sesión HTTP local (comando, perfil, kiln-info): apaga el AP unos segundos
 * para que el móvil vuelva a su WiFi de casa y luego reabre el AP automáticamente.
 *
 * Solo se programa si el canal nube está OK (Firebase RTDB write reciente),
 * salvo [wifi_manager_armApReleaseWithoutFirebaseOnce] (salida de Solo local).
 */
void wifi_manager_requestApRelease();

/**
 * La próxima [wifi_manager_requestApRelease] puede liberar SoftAP aunque
 * Firebase aún no esté OK (transición Solo local → Online).
 */
void wifi_manager_armApReleaseWithoutFirebaseOnce();

/**
 * Marca un write RTDB exitoso. Habilita liberar SoftAP tras la próxima sesión HTTP
 * (el móvil puede volver a WiFi de casa y seguir por Firebase).
 */
void wifi_manager_noteFirebaseOk();

/** true si hubo write Firebase reciente (criterio para soltar SoftAP). */
bool wifi_manager_isFirebaseOk();

/**
 * Marca tráfico HTTP real de la app (status/command/profile/…).
 * Mientras esté “fresco”, se pausan reintentos agresivos de STA/Firebase
 * para no ensuciar SoftAP.
 */
void wifi_manager_noteLocalHttp();

/**
 * true si la app está activamente en Local: HTTP reciente o cliente SoftAP
 * en gracia (sin HTTP eterno si el móvil se asocia y se va).
 */
bool wifi_manager_isLocalAppActive();

/** Llamar periódicamente desde TaskComunicaciones (~100ms). */
void wifi_manager_tick();
