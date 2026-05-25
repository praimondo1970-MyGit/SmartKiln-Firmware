#pragma once

#include <Arduino.h>

/** Procesa START, PAUSE, STOP, STATUS, TEST, etc. Devuelve JSON de respuesta o vacío. */
String kiln_processCommand(const String& cmd);

/** Carga programa desde JSON (mismo formato que LOAD_PROFILE). */
bool kiln_loadProfileJson(const String& jsonStr, String& errorOut);

/** Config horno + userId (+ wifi opcional en el mismo JSON). */
bool kiln_applyKilnInfoJson(const String& jsonStr, String& errorOut);

String kiln_buildStatusJson();
String kiln_buildInfoJson();
String kiln_buildApInfoJson();

/** Tras HTTP local (perfil, START, kiln-info): libera AP y marca sync Firebase. */
void kiln_onLocalSessionEnd(const char* reason);
