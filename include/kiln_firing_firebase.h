#pragma once

#include <Arduino.h>
#include "kiln_shared.h"

class FirebaseData;

/**
 * Historial de cocción en Firebase (mismo patrón que DataLogger):
 *   users/{uid}/kilns/{mac}/recordings/{id}/meta|samples|stages
 *   users/{uid}/activity/{id}
 *
 * startedAtMs / endedAtMs: epoch Unix en ms. Nunca 0 ni millis() (eso es dic 1969).
 */
void kilnFiringBegin(
    const String& mac,
    const String& programName,
    const String& deviceName,
    float startTempC,
    const Segmento* segmentos,
    int numSegmentos
);

/** Llamar desde reportToFirebase tras telemetría OK. */
void kilnFiringOnReport(
    FirebaseData* fbdo,
    const String& userId,
    const String& mac,
    const String& estado,
    float tempC,
    unsigned long programStartMillis
);

/** STOP / abort / FINALIZADO: cierra la sesión si había una activa. */
void kilnFiringRequestFinalize(bool completedNormally);
