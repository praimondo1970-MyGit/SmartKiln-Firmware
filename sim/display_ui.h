#ifndef DISPLAY_UI_H
#define DISPLAY_UI_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
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
} DisplayData;

void display_ui_show_splash(void);
void display_ui_show_main(void);
void display_ui_update(const DisplayData *data);
void display_ui_fill_demo(DisplayData *data, uint32_t tick_ms);

#endif
