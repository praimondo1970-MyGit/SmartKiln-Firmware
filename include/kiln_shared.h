#pragma once

#include <Arduino.h>
#include <freertos/semphr.h>

struct Segmento {
    float tempObjetivo;
    float rampaCporMin;
    int tiempoRemojoMin;
};

struct CurvaActiva {
    char nombre[32];
    Segmento segmentos[10];
    uint8_t numSegmentos;
    bool recibida;
};

// Estado compartido (definido en main.cpp)
extern SemaphoreHandle_t xMutex;
extern float temperaturaActual;
extern String estadoHorno;
extern String kilnName;
extern String kilnModel;
extern float kilnVolume;
extern String userId;
extern String deviceMacAddress;
extern CurvaActiva curvaActual;
extern unsigned long programStartTime;
extern unsigned long programPauseTime;
extern int programTotalDurationMinutes;
extern volatile bool pendingKilnInfoSave;
extern volatile bool pendingWifiConfigSave;
extern bool needsWifiReconnect;
extern bool needsDeviceInfoUpdate;
extern String wifiSSID;
extern String wifiPassword;
extern bool wifiConfigReceived;

int calculateProgramTotalDuration(float initialTemp);
void saveProfile();
void saveKilnInfo();

/** Timestamp (ms) del programa en la app; se publica en RTDB profileState. */
extern unsigned long profileUpdatedAtMs;
extern volatile bool needsProfileStatePublish;

void kiln_scheduleProfileStatePublish();
