#ifndef DISPLAY_LVGL_H
#define DISPLAY_LVGL_H

#include <Arduino.h>

#include "lv_conf.h"

// Montserrat 14 solo para glifos LV_SYMBOL (WiFi, check). Texto UI → Inter (inter_fonts.h).
#ifndef LV_FONT_MONTSERRAT_14
#define LV_FONT_MONTSERRAT_14 1
#endif

#include <lvgl.h>

struct DisplayData {
    float currentTemp;
    float targetTemp;
    float stageRampCpm;
    char kilnName[64];
    char deviceId[16];
    char programName[64];
    char status[24];
    struct {
        bool wifiConnected;
        bool bleConnected;
        bool firebaseConnected;
    } connection;
    int elapsedMinutes;
    int totalMinutes;
    int remainingMinutes;
    int currentStage;
    int totalStages;
    char currentPhase[16];
};

bool initDisplayLVGL();
void display_requestMainUi();
bool display_isMainUiReady();
void updateDisplayData(const DisplayData& data);
void display_postData(const DisplayData& data);
void displayTaskHandler(void *pvParameters);

#endif
