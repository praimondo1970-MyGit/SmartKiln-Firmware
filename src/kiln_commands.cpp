#include "kiln_commands.h"
#include "kiln_shared.h"
#include "wifi_manager.h"
#include <ArduinoJson.h>

extern void kiln_notifyLocalSessionComplete();

String kiln_buildStatusJson() {
    StaticJsonDocument<512> doc;
    if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        doc["temperature"] = temperaturaActual;
        doc["status"] = estadoHorno;
        doc["programName"] = curvaActual.recibida ? curvaActual.nombre : "";
        doc["numStages"] = curvaActual.recibida ? curvaActual.numSegmentos : 0;
        doc["profileLoaded"] = curvaActual.recibida;
        doc["programDurationMin"] = programTotalDurationMinutes;
        int elapsedMin = 0;
        int remainingMin = 0;
        if (programStartTime > 0) {
            unsigned long elapsedMs = millis() - programStartTime;
            if (programPauseTime > 0 && estadoHorno == "PAUSADO") {
                elapsedMs = programPauseTime - programStartTime;
            }
            elapsedMin = (int)(elapsedMs / 60000UL);
            remainingMin = programTotalDurationMinutes - elapsedMin;
            if (remainingMin < 0) {
                remainingMin = 0;
            }
        }
        doc["elapsedMinutes"] = elapsedMin;
        doc["remainingMinutes"] = remainingMin;
        doc["currentStage"] = curvaActual.recibida && curvaActual.numSegmentos > 0 ? 1 : 0;
        xSemaphoreGive(xMutex);
    } else {
        doc["error"] = "mutex_timeout";
    }
    String out;
    serializeJson(doc, out);
    return out;
}

String kiln_buildInfoJson() {
    StaticJsonDocument<384> doc;
    if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        doc["name"] = kilnName;
        doc["model"] = kilnModel;
        doc["volume"] = kilnVolume;
        doc["userId"] = userId;
        doc["macAddress"] = deviceMacAddress;
        xSemaphoreGive(xMutex);
    }
    String out;
    serializeJson(doc, out);
    return out;
}

String kiln_buildApInfoJson() {
    StaticJsonDocument<256> doc;
    doc["ssid"] = wifi_manager_apSsid();
    doc["password"] = wifi_manager_apPassword();
    doc["url"] = "http://192.168.4.1";
    doc["apiStatus"] = "http://192.168.4.1/api/status";
    String out;
    serializeJson(doc, out);
    return out;
}

String kiln_processCommand(const String& cmdRaw) {
    String cmd = cmdRaw;
    cmd.trim();
    StaticJsonDocument<128> resp;

    if (cmd.length() == 0) {
        resp["ok"] = false;
        resp["error"] = "empty_command";
        String out;
        serializeJson(resp, out);
        return out;
    }

    if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(500)) != pdTRUE) {
        resp["ok"] = false;
        resp["error"] = "mutex_timeout";
        String out;
        serializeJson(resp, out);
        return out;
    }

    if (cmd == "START") {
        estadoHorno = "CALENTANDO";
        if (programPauseTime > 0) {
            unsigned long pauseDuration = millis() - programPauseTime;
            programStartTime += pauseDuration;
            programPauseTime = 0;
        } else if (programStartTime == 0 && curvaActual.recibida && curvaActual.numSegmentos > 0) {
            float currentTemp = temperaturaActual;
            int totalDuration = calculateProgramTotalDuration(currentTemp);
            programStartTime = millis();
            programTotalDurationMinutes = totalDuration;
        }
        Serial.println("[API] START");
    } else if (cmd == "PAUSE") {
        estadoHorno = "PAUSADO";
        programPauseTime = millis();
        Serial.println("[API] PAUSE");
    } else if (cmd == "STOP") {
        estadoHorno = "DETENIDO";
        programStartTime = 0;
        programPauseTime = 0;
        Serial.println("[API] STOP");
    } else if (cmd == "TEST") {
        resp["ok"] = true;
        resp["message"] = "TEST_OK";
        xSemaphoreGive(xMutex);
        String out;
        serializeJson(resp, out);
        return out;
    } else if (cmd == "STATUS") {
        xSemaphoreGive(xMutex);
        return kiln_buildStatusJson();
    } else {
        resp["ok"] = false;
        resp["error"] = "unknown_command";
        resp["cmd"] = cmd;
        xSemaphoreGive(xMutex);
        String out;
        serializeJson(resp, out);
        return out;
    }

    xSemaphoreGive(xMutex);
    resp["ok"] = true;
    resp["cmd"] = cmd;
    resp["status"] = estadoHorno;
    if (cmd == "START" || cmd == "PAUSE" || cmd == "STOP") {
        kiln_onLocalSessionEnd(cmd.c_str());
    }
    String out;
    serializeJson(resp, out);
    return out;
}

bool kiln_loadProfileJson(const String& jsonStr, String& errorOut) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, jsonStr);
    if (error) {
        errorOut = error.c_str();
        return false;
    }

    if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        errorOut = "mutex_timeout";
        return false;
    }

    memset(&curvaActual, 0, sizeof(CurvaActiva));
    const char* nombre = doc["nombre"] | "SIN_NOMBRE";
    strncpy(curvaActual.nombre, nombre, sizeof(curvaActual.nombre) - 1);

    int numSeg = doc["numSeg"] | 0;
    curvaActual.numSegmentos = min(numSeg, 10);
    for (int i = 0; i < curvaActual.numSegmentos; i++) {
        curvaActual.segmentos[i].tempObjetivo = doc["seg"][i]["temp"] | 0;
        curvaActual.segmentos[i].rampaCporMin = doc["seg"][i]["rampa"] | 0;
        curvaActual.segmentos[i].tiempoRemojoMin = doc["seg"][i]["remojo"] | 0;
    }
    curvaActual.recibida = true;

    float currentTempForDuration = 25.0f;
    if (temperaturaActual > 0) {
        currentTempForDuration = temperaturaActual;
    }
    programTotalDurationMinutes = calculateProgramTotalDuration(currentTempForDuration);
    xSemaphoreGive(xMutex);

    saveProfile();
    Serial.printf("[API] Perfil cargado: %s (%d seg)\n", curvaActual.nombre, curvaActual.numSegmentos);
    kiln_onLocalSessionEnd("profile");
    return true;
}

bool kiln_applyKilnInfoJson(const String& jsonStr, String& errorOut) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, jsonStr);
    if (error) {
        errorOut = error.c_str();
        return false;
    }

    if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(500)) != pdTRUE) {
        errorOut = "mutex_timeout";
        return false;
    }

    kilnName = doc["name"] | "Sin Nombre";
    kilnModel = doc["model"] | "Sin Modelo";
    kilnVolume = doc["volume"] | 0.0;
    userId = doc["userId"] | "";
    pendingKilnInfoSave = true;
    needsDeviceInfoUpdate = true;

    JsonVariantConst wifiCfg = doc["wifi"];
    if (!wifiCfg.isNull()) {
        String newSsid = wifiCfg["ssid"] | "";
        String newPassword = wifiCfg["password"] | "";
        if (newSsid.length() > 0) {
            wifiSSID = newSsid;
            wifiPassword = newPassword;
            wifiConfigReceived = true;
            needsWifiReconnect = true;
            pendingWifiConfigSave = true;
        }
    }
    xSemaphoreGive(xMutex);
    Serial.println("[API] kiln-info aplicado");
    kiln_onLocalSessionEnd("kiln-info");
    return true;
}

void kiln_onLocalSessionEnd(const char* reason) {
    Serial.printf("[API] Fin sesion local (%s)\n", reason ? reason : "?");
    wifi_manager_requestApRelease();
    kiln_notifyLocalSessionComplete();
}
