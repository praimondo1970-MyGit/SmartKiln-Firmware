#ifndef DISPLAY_LVGL_H
#define DISPLAY_LVGL_H

#include <Arduino.h>

// Incluir lv_conf.h primero para configurar LVGL
#include "lv_conf.h"

// Definir fuentes Montserrat antes de incluir lvgl.h (solo las usadas)
#ifndef LV_FONT_MONTSERRAT_14
#define LV_FONT_MONTSERRAT_14 1
#endif
#ifndef LV_FONT_MONTSERRAT_16
#define LV_FONT_MONTSERRAT_16 1
#endif
#ifndef LV_FONT_MONTSERRAT_18
#define LV_FONT_MONTSERRAT_18 1
#endif
#ifndef LV_FONT_MONTSERRAT_44
#define LV_FONT_MONTSERRAT_44 1
#endif
// LV_FONT_MONTSERRAT_48 deshabilitada - no se usa en el código

#include <lvgl.h>

// Estructura para datos de la pantalla
// Optimización: Usar char[] en lugar de String para evitar fragmentación de memoria
struct DisplayData {
    float currentTemp;
    float targetTemp;
    char kilnName[64];
    char programName[64];
    char status[16];          // "CALENTANDO", "PAUSADO", "DETENIDO", "FINALIZADO", "IDLE"
    struct {
        bool wifiConnected;
        bool bleConnected;
        bool firebaseConnected;
    } connection;
    int elapsedMinutes;
    int totalMinutes;
    int remainingMinutes;   // Tiempo restante para finalizar (en minutos)
    int currentStage;       // Etapa actual (1-indexed, 0 = sin etapa)
    int totalStages;        // Total de etapas
    char currentPhase[16];    // "Ramp", "Soak", etc.
};

// Funciones principales
bool initDisplayLVGL();
void updateDisplayData(const DisplayData& data);
void displayTaskHandler(void *pvParameters);

#endif // DISPLAY_LVGL_H


