#include <Arduino.h>
#include <ArduinoJson.h>
#include "Wire.h"
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <Adafruit_MAX31855.h>
// Deshabilitar logs de debug de NimBLE antes de incluir
#define NIMBLE_CPP_LOG_LEVEL 0  // 0=NONE, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG
#include <esp_log.h>
#include <NimBLEDevice.h>
#include "NimBLE2904.h"
#include <Preferences.h>
#include "esp_wifi.h"
#include "silent_serial.h"
#include "kiln_shared.h"
#include "wifi_manager.h"
#include "kiln_api.h"
#if defined(ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S3)
#include "display_lvgl.h"
#endif

// -------------------- MAX31855 (termocupla tipo K) --------------------
// Conector P2 ESP32-4827S043: SCK=IO13, CS=IO11, SO=IO19. No insertar tarjeta SD.
#define MAX31855_SCK  13
#define MAX31855_CS   11
#define MAX31855_SO   19
Adafruit_MAX31855 thermocouple(MAX31855_SCK, MAX31855_CS, MAX31855_SO);

// -------------------- HELPER PARA SERIAL SEGURO --------------------
// Función helper para escribir en Serial de forma segura en ESP32-S3 con USB CDC
void safeSerialPrint(const char* str) {
    if (Serial) {
        Serial.print(str);
        // Usar delay pequeño en lugar de flush para evitar bloqueos
        delay(1);
    }
}

void safeSerialPrintln(const char* str) {
    if (Serial) {
        Serial.println(str);
        delay(1);
    }
}

// -------------------- HANDLER DE CRASHES --------------------
// Función para capturar información de crashes
void printCrashInfo() {
    // Intentar escribir en Serial incluso si está crasheando
    Serial.println("\n\n!!! CRASH DETECTED !!!");
    Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
    Serial.printf("Largest free block: %d\n", ESP.getMaxAllocHeap());
    Serial.flush();
}

// -------------------- CONFIGURACIÓN TEMPRANA DE LOGS --------------------
// MOVIDA A setup() para evitar crashes en inicialización global
// Se ejecutará al inicio de setup() después de que Serial esté listo

// -------------------- DECLARACIONES DE FUNCIONES --------------------
void reportToFirebase(float temp, const String& estado);
void updateDeviceInfoFirebase();
void saveKilnInfo();
void loadKilnInfo();
void saveWifiConfig();
void loadWifiConfig();
void saveProfile();
void loadProfile();
void logStoredData();
void reconnectToWifi();
String getMacAddress();
bool initFirebase();
int calculateProgramTotalDuration(float initialTemp);
float leerTermocupla();
#if defined(ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S3)
void TaskUpdateDisplayLVGL(void *pvParameters);
#endif

// -------------------- CONFIGURACIONES --------------------
// WiFi credentials now come from the app via BLE
// No hardcoded values

const char* FIREBASE_API_KEY = "AIzaSyBv3VknyBVxY_bMpgKgyhlVc_ImE7ejgwk";
const char* FIREBASE_DATABASE_URL = "https://ia-based-kiln-default-rtdb.firebaseio.com";

const char* BLE_DEVICE_NAME = "SmartKiln - Ceramica";
const char* BLE_SERVICE_UUID = "00001234-0000-1000-8000-00805f9b34fb";
const char* BLE_TEMP_CHAR_UUID   = "0000abcd-0000-1000-8000-00805f9b34fb";
const char* BLE_STATUS_CHAR_UUID = "0000dcba-0000-1000-8000-00805f9b34fb";
const char* BLE_COMMAND_CHAR_UUID= "00002211-0000-1000-8000-00805f9b34fb";
const char* BLE_CURVA_CHAR_UUID= "0000a001-0000-1000-8000-00805f9b34fb";
const char* BLE_USER_ID_CHAR_UUID = "0000a002-0000-1000-8000-00805f9b34fb";
const char* BLE_WIFI_CONFIG_CHAR_UUID = "0000a003-0000-1000-8000-00805f9b34fb";
const char* BLE_PROGRAM_NAME_CHAR_UUID = "0000a004-0000-1000-8000-00805f9b34fb";


// -------------------- VARIABLES GLOBALES --------------------
// Usar punteros e inicializar en setup() para evitar crash en inicialización global

FirebaseData* fbdo = nullptr;
FirebaseAuth* auth = nullptr;
FirebaseConfig* config = nullptr;

SemaphoreHandle_t xMutex = NULL;

NimBLEServer* pServer = nullptr;
NimBLECharacteristic* gTempChar;
NimBLECharacteristic* gStatusChar;
NimBLECharacteristic* gUserIdChar;
NimBLECharacteristic* gProgramNameChar;

float temperaturaActual = 0.0;
String estadoHorno = "IDLE";
bool wifiConnected = false;
bool firebaseConnected = false;
bool needsDeviceInfoUpdate = false;  // Flag para actualizar deviceInfo desde tarea
unsigned int firebaseErrorCount = 0;  // Contador de errores consecutivos

String deviceMacAddress = "";
String userId = ""; // Se recibirá por BLE desde la app

// Variables para información del horno
String kilnName = "Mi Horno Cerámico";  // Valor por defecto
String kilnModel = "SmartKiln - Ceramica";
float kilnVolume = 0.0;

// Variables para WiFi dinámico
String wifiSSID = "";
String wifiPassword = "";
bool wifiConfigReceived = false;
bool needsWifiReconnect = false;
volatile bool pendingWifiConfigSave = false;  // BLE: diferir save/load WiFi a TaskComunicaciones (evitar stack overflow en callback NimBLE)
volatile bool pendingKilnInfoSave = false;   // BLE: diferir saveKilnInfo() a TaskComunicaciones
bool statusLogsEnabled = false;

// NUEVO: Variables para control de suspensión WiFi/BLE
bool bleClientConnected = false;  // Indica si hay un cliente BLE conectado
bool wifiSuspended = false;       // Indica si WiFi está suspendido por BLE
unsigned long lastBleDisconnectTime = 0;  // Timestamp de última desconexión BLE
const unsigned long BLE_DISCONNECT_DELAY = 5000;  // 5 segundos antes de reactivar WiFi

// NUEVO: Objeto para almacenamiento persistente
Preferences preferences;

// Variables para seguimiento de tiempo del programa
unsigned long programStartTime = 0;  // Timestamp de inicio del programa (en millis)
unsigned long programPauseTime = 0;  // Timestamp de última pausa (en millis)
int programTotalDurationMinutes = 0;  // Duración total del programa en minutos


// CurvaActiva / Segmento en kiln_shared.h
CurvaActiva curvaActual = {"", {}, 0, false};


// -------------------- FUNCIONES --------------------

String getMacAddress() {
    uint8_t baseMac[6];
    // Usar MAC del Bluetooth para coincidir con el MAC que ve la app al escanear BLE
    esp_read_mac(baseMac, ESP_MAC_BT);
    
    char macStr[18] = {0};
    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", 
            baseMac[0], baseMac[1], baseMac[2], 
            baseMac[3], baseMac[4], baseMac[5]);
    
    return String(macStr);
}

void saveKilnInfo() {
    Serial.printf("[STORAGE] Guardando datos del horno:\n");
    Serial.printf("  - Nombre: '%s'\n", kilnName.c_str());
    Serial.printf("  - Modelo: '%s'\n", kilnModel.c_str());
    Serial.printf("  - Volumen: %.1f\n", kilnVolume);
    Serial.printf("  - UserID: '%s'\n", userId.c_str());
    
    preferences.begin("kiln_config", false);
    bool nameSaved = preferences.putString("name", kilnName);
    bool modelSaved = preferences.putString("model", kilnModel);
    bool volumeSaved = preferences.putFloat("volume", kilnVolume);
    bool userIdSaved = preferences.putString("userId", userId);
    preferences.end();
    
    Serial.printf("[STORAGE] Resultado del guardado:\n");
    Serial.printf("  - name: %s\n", nameSaved ? "OK" : "FAIL");
    Serial.printf("  - model: %s\n", modelSaved ? "OK" : "FAIL");
    Serial.printf("  - volume: %s\n", volumeSaved ? "OK" : "FAIL");
    Serial.printf("  - userId: %s\n", userIdSaved ? "OK" : "FAIL");
    Serial.printf("[STORAGE] Datos del horno guardados: %s - %s (%.1fL) - User: %s\n", 
                  kilnName.c_str(), kilnModel.c_str(), kilnVolume, userId.c_str());
}

void loadKilnInfo() {
    Serial.println("[STORAGE] Iniciando carga de datos del horno...");
    
    preferences.begin("kiln_config", false);
    
    // Cargar datos guardados, usar valores por defecto si no existen
    String savedName = preferences.getString("name", "Mi Kiln Cerámico");
    String savedModel = preferences.getString("model", "SmartKiln - Ceramica");
    float savedVolume = preferences.getFloat("volume", 0.0);
    String savedUserId = preferences.getString("userId", "");
    
    preferences.end();
    
    Serial.printf("[STORAGE] Datos leídos de memoria:\n");
    Serial.printf("  - Nombre: '%s'\n", savedName.c_str());
    Serial.printf("  - Modelo: '%s'\n", savedModel.c_str());
    Serial.printf("  - Volumen: %.1f\n", savedVolume);
    Serial.printf("  - UserID: '%s'\n", savedUserId.c_str());
    
    // Actualizar variables globales
    kilnName = savedName;
    kilnModel = savedModel;
    kilnVolume = savedVolume;
    userId = savedUserId;
    
    Serial.printf("[STORAGE] Variables globales actualizadas:\n");
    Serial.printf("  - kilnName: '%s'\n", kilnName.c_str());
    Serial.printf("  - kilnModel: '%s'\n", kilnModel.c_str());
    Serial.printf("  - kilnVolume: %.1fL\n", kilnVolume);
    Serial.printf("  - userId: '%s'\n", userId.c_str());
}

void debugPrintWifiCredentials(const char* context, const String& ssid, const String& password) {
    Serial.println("=================================");
    Serial.printf("[WiFi][DEBUG] %s\n", context);
    Serial.printf("[WiFi][DEBUG] SSID  : '%s' (longitud: %d)\n", ssid.c_str(), ssid.length());
    Serial.printf("[WiFi][DEBUG] PASS  : '%s' (longitud: %d)\n", password.c_str(), password.length());
    Serial.println("=================================");
}

void saveWifiConfig() {
    Serial.println("[STORAGE] =========================================");
    Serial.println("[STORAGE] 💾 GUARDANDO CREDENCIALES WiFi...");
    Serial.printf("[STORAGE] SSID: '%s' (longitud: %d)\n", wifiSSID.c_str(), wifiSSID.length());
    Serial.printf("[STORAGE] Password: [OCULTO] (longitud: %d)\n", wifiPassword.length());
    debugPrintWifiCredentials("Credenciales recibidas (RAM antes de guardar)", wifiSSID, wifiPassword);
    
    if (wifiSSID.length() == 0) {
        Serial.println("[STORAGE] ❌ ERROR: SSID vacío, no se puede guardar");
        return;
    }
    
    preferences.begin("wifi_config", false);
    bool ssidSaved = preferences.putString("ssid", wifiSSID);
    bool passwordSaved = preferences.putString("password", wifiPassword);
    
    // Verificar que se guardó correctamente
    String verifySSID = preferences.getString("ssid", "");
    String verifyPassword = preferences.getString("password", "");
    
    preferences.end();
    
    Serial.printf("[STORAGE] Resultado del guardado WiFi:\n");
    Serial.printf("  - ssid: %s\n", ssidSaved ? "OK" : "FAIL");
    Serial.printf("  - password: %s\n", passwordSaved ? "OK" : "FAIL");
    
    // Verificación
    if (verifySSID == wifiSSID && verifyPassword.length() == wifiPassword.length()) {
        Serial.printf("[STORAGE] ✅ WiFi guardado y verificado: SSID='%s'\n", wifiSSID.c_str());
        Serial.println("[STORAGE] =========================================");
        debugPrintWifiCredentials("Credenciales verificadas desde NVS", verifySSID, verifyPassword);
    } else {
        Serial.println("[STORAGE] ❌ ERROR: Verificación falló después de guardar");
        Serial.printf("[STORAGE] Esperado SSID: '%s'\n", wifiSSID.c_str());
        Serial.printf("[STORAGE] Guardado SSID: '%s'\n", verifySSID.c_str());
        Serial.println("[STORAGE] =========================================");
    }
}

void loadWifiConfig() {
    Serial.println("[STORAGE] =========================================");
    Serial.println("[STORAGE] 📥 CARGANDO CREDENCIALES WiFi...");
    preferences.begin("wifi_config", false);
    
    // Verificar si existen las claves
    size_t ssidLen = preferences.getBytesLength("ssid");
    size_t passwordLen = preferences.getBytesLength("password");
    
    Serial.printf("[STORAGE] Longitud SSID en NVS: %d bytes\n", ssidLen);
    Serial.printf("[STORAGE] Longitud Password en NVS: %d bytes\n", passwordLen);
    
    wifiSSID = preferences.getString("ssid", "");
    wifiPassword = preferences.getString("password", "");
    preferences.end();
    
    Serial.printf("[STORAGE] Datos cargados:\n");
    Serial.printf("  - SSID: '%s' (longitud: %d)\n", wifiSSID.c_str(), wifiSSID.length());
    Serial.printf("  - Password: [OCULTO] (longitud: %d)\n", wifiPassword.length());
    
    if (wifiSSID != "" && wifiSSID.length() > 0) {
        Serial.printf("[STORAGE] ✅ WiFi cargado correctamente: SSID='%s'\n", wifiSSID.c_str());
        wifiConfigReceived = true;
        Serial.printf("[STORAGE] wifiConfigReceived = true\n");
        debugPrintWifiCredentials("Credenciales en RAM tras cargar NVS", wifiSSID, wifiPassword);
    } else {
        Serial.println("[STORAGE] ⚠️ No hay WiFi guardado");
        Serial.println("[STORAGE] Esperando configuración desde la app...");
        wifiConfigReceived = false;
    }
    Serial.println("[STORAGE] =========================================");
}

void logStoredData() {
    Serial.println("=================================");
    Serial.println("[MEMORIA] DATOS ACTUALES AL INICIAR");
    Serial.printf("  - kilnName: '%s'\n", kilnName.c_str());
    Serial.printf("  - kilnModel: '%s'\n", kilnModel.c_str());
    Serial.printf("  - kilnVolume: %.1f L\n", kilnVolume);
    Serial.printf("  - userId: '%s'\n", userId.c_str());

    if (wifiSSID.length() > 0) {
        Serial.printf("  - WiFi SSID: '%s'\n", wifiSSID.c_str());
        Serial.printf("  - WiFi password length: %d\n", wifiPassword.length());
    } else {
        Serial.println("  - WiFi: sin credenciales guardadas");
    }

    if (strlen(curvaActual.nombre) > 0) {
        Serial.printf("  - Programa activo: '%s'\n", curvaActual.nombre);
        Serial.printf("  - Segmentos: %d\n", curvaActual.numSegmentos);
    } else {
        Serial.println("  - Programa activo: no establecido");
    }
    Serial.println("=================================");
}

void attemptImmediateWifiReconnect(const char* originContext) {
    Serial.println("=================================");
    Serial.printf("[WiFi] 🔁 Intentando reconexión inmediata (%s)\n", originContext);
    Serial.printf("[WiFi] Estado wifiSuspended: %s\n", wifiSuspended ? "SÍ" : "NO");
    Serial.printf("[WiFi] BLE conectado: %s\n", bleClientConnected ? "SÍ" : "NO");
    Serial.println("=================================");

    if (wifiSuspended || bleClientConnected) {
        Serial.println("[WiFi] ⚠️ Reconexión inmediata omitida porque BLE está activo o WiFi suspendido.");
        Serial.println("[WiFi]    Se programará reconexión mediante needsWifiReconnect.");
        needsWifiReconnect = true;
        return;
    }

    if (wifiSSID == "" || wifiPassword == "") {
        Serial.println("[WiFi] ❌ Reconexión inmediata cancelada: credenciales vacías.");
        return;
    }

    reconnectToWifi();
}

// Función auxiliar para calcular la duración total del programa en minutos
int calculateProgramTotalDuration(float initialTemp) {
    int totalMinutes = 0;
    float previousTemp = initialTemp;
    
    for (int i = 0; i < curvaActual.numSegmentos; i++) {
        Segmento seg = curvaActual.segmentos[i];
        float tempDiff = seg.tempObjetivo - previousTemp;
        
        // Calcular tiempo de rampa (en minutos)
        float rampMinutes = 0;
        if (seg.rampaCporMin > 0 && tempDiff > 0) {
            rampMinutes = tempDiff / seg.rampaCporMin;
        }
        
        // Sumar tiempo de remojo
        totalMinutes += (int)rampMinutes + seg.tiempoRemojoMin;
        previousTemp = seg.tempObjetivo;
    }
    
    return totalMinutes;
}

void saveProfile() {
    Serial.println("[STORAGE] Guardando programa activo...");
    
    preferences.begin("active_profile", false);
    
    // Guardar nombre del programa
    bool nameSaved = preferences.putString("programName", String(curvaActual.nombre));
    
    // Guardar número de segmentos
    bool numSegSaved = preferences.putUChar("numSegmentos", curvaActual.numSegmentos);
    
    // Guardar cada segmento
    bool segmentsSaved = true;
    for (int i = 0; i < curvaActual.numSegmentos && i < 10; i++) {
        String segmentKey = "seg" + String(i) + "_temp";
        preferences.putFloat(segmentKey.c_str(), curvaActual.segmentos[i].tempObjetivo);
        
        segmentKey = "seg" + String(i) + "_rampa";
        preferences.putFloat(segmentKey.c_str(), curvaActual.segmentos[i].rampaCporMin);
        
        segmentKey = "seg" + String(i) + "_remojo";
        preferences.putInt(segmentKey.c_str(), curvaActual.segmentos[i].tiempoRemojoMin);
    }
    
    // Guardar flag de recibida
    bool receivedSaved = preferences.putBool("recibida", curvaActual.recibida);
    
    preferences.end();
    
    Serial.printf("[STORAGE] Programa guardado: %s (%d segmentos)\n", 
                  curvaActual.nombre, curvaActual.numSegmentos);
    Serial.printf("[STORAGE] Resultado: name=%s, numSeg=%s, recibida=%s\n",
                  nameSaved ? "OK" : "FAIL",
                  numSegSaved ? "OK" : "FAIL",
                  receivedSaved ? "OK" : "FAIL");
}

void loadProfile() {
    Serial.println("[STORAGE] =========================================");
    Serial.println("[STORAGE] Iniciando carga de programa activo desde memoria...");
    
    preferences.begin("active_profile", false);
    
    // Cargar nombre del programa
    String savedName = preferences.getString("programName", "");
    Serial.printf("[STORAGE] 📋 Nombre del programa leído de Preferences:\n");
    Serial.printf("  - Valor leído: '%s'\n", savedName.c_str());
    Serial.printf("  - Longitud: %d caracteres\n", savedName.length());
    Serial.printf("  - ¿Está vacío? %s\n", savedName == "" ? "SÍ" : "NO");
    
    if (savedName == "") {
        Serial.println("[STORAGE] ⚠️ No hay programa guardado en memoria");
        Serial.printf("[STORAGE] Estado inicial de curvaActual.nombre: '%s' (longitud: %d)\n", 
                     curvaActual.nombre, strlen(curvaActual.nombre));
        preferences.end();
        return;
    }
    
    // Cargar número de segmentos
    uint8_t savedNumSeg = preferences.getUChar("numSegmentos", 0);
    Serial.printf("[STORAGE] 📊 Número de segmentos leído: %d\n", savedNumSeg);
    
    if (savedNumSeg == 0 || savedNumSeg > 10) {
        Serial.println("[STORAGE] ⚠️ Número de segmentos inválido, limpiando perfil guardado");
        preferences.clear();
        preferences.end();
        return;
    }
    
    // Limpiar estructura actual
    Serial.println("[STORAGE] 🧹 Limpiando estructura curvaActual...");
    Serial.printf("  - Antes de limpiar: curvaActual.nombre='%s' (longitud: %d)\n", 
                 curvaActual.nombre, strlen(curvaActual.nombre));
    memset(&curvaActual, 0, sizeof(CurvaActiva));
    Serial.printf("  - Después de limpiar: curvaActual.nombre='%s' (longitud: %d)\n", 
                 curvaActual.nombre, strlen(curvaActual.nombre));
    
    // Copiar nombre
    Serial.println("[STORAGE] 📝 Copiando nombre a curvaActual.nombre...");
    Serial.printf("  - Tamaño del buffer: %d caracteres\n", sizeof(curvaActual.nombre));
    strncpy(curvaActual.nombre, savedName.c_str(), sizeof(curvaActual.nombre) - 1);
    curvaActual.nombre[sizeof(curvaActual.nombre) - 1] = '\0';
    Serial.printf("  - Después de copiar: curvaActual.nombre='%s'\n", curvaActual.nombre);
    Serial.printf("  - Longitud: %d caracteres\n", strlen(curvaActual.nombre));
    Serial.printf("  - Verificación byte a byte: ");
    for (int i = 0; i < min(10, (int)strlen(curvaActual.nombre) + 1); i++) {
        if (curvaActual.nombre[i] == '\0') {
            Serial.printf("\\0");
        } else {
            Serial.printf("'%c'", curvaActual.nombre[i]);
        }
    }
    Serial.println();
    
    // Cargar número de segmentos
    curvaActual.numSegmentos = savedNumSeg;
    Serial.printf("[STORAGE] ✅ curvaActual.numSegmentos = %d\n", curvaActual.numSegmentos);
    
    // Cargar cada segmento
    bool allSegmentsValid = true;
    for (int i = 0; i < savedNumSeg && i < 10; i++) {
        String segmentKey = "seg" + String(i) + "_temp";
        curvaActual.segmentos[i].tempObjetivo = preferences.getFloat(segmentKey.c_str(), 0.0);
        
        segmentKey = "seg" + String(i) + "_rampa";
        curvaActual.segmentos[i].rampaCporMin = preferences.getFloat(segmentKey.c_str(), 0.0);
        
        segmentKey = "seg" + String(i) + "_remojo";
        curvaActual.segmentos[i].tiempoRemojoMin = preferences.getInt(segmentKey.c_str(), 0);
        
        // Validar que el segmento tenga valores válidos
        if (curvaActual.segmentos[i].tempObjetivo <= 0 && 
            curvaActual.segmentos[i].rampaCporMin <= 0 && 
            curvaActual.segmentos[i].tiempoRemojoMin <= 0) {
            allSegmentsValid = false;
        }
    }
    
    // Cargar flag de recibida
    curvaActual.recibida = preferences.getBool("recibida", false);
    
    preferences.end();
    
    if (allSegmentsValid) {
        Serial.printf("[STORAGE] ✅ PROGRAMA CARGADO CORRECTAMENTE:\n");
        Serial.printf("  - Nombre: '%s'\n", curvaActual.nombre);
        Serial.printf("  - Longitud del nombre: %d caracteres\n", strlen(curvaActual.nombre));
        Serial.printf("  - Segmentos: %d\n", curvaActual.numSegmentos);
        Serial.printf("  - Verificación final: curvaActual.nombre[0]='%c', nombre completo='%s'\n", 
                     curvaActual.nombre[0], curvaActual.nombre);
        for (int i = 0; i < curvaActual.numSegmentos && i < 10; i++) {
            Serial.printf("  Segmento %d -> T=%.1f, R=%.1f, M=%d\n",
                          i+1,
                          curvaActual.segmentos[i].tempObjetivo,
                          curvaActual.segmentos[i].rampaCporMin,
                          curvaActual.segmentos[i].tiempoRemojoMin);
        }
        
        // Calcular duración total del programa (usando temperatura ambiente como inicial)
        // Nota: No usar mutex aquí porque loadProfile() se llama antes de crear el mutex en setup()
        float initialTemp = 25.0; // Temperatura ambiente por defecto
        // Usar temperaturaActual directamente si el mutex existe, sino usar valor por defecto
        if (xMutex != NULL) {
            if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                if (temperaturaActual > 0) {
                    initialTemp = temperaturaActual;
                }
                xSemaphoreGive(xMutex);
            }
        }
        programTotalDurationMinutes = calculateProgramTotalDuration(initialTemp);
        Serial.printf("[STORAGE] ⏱️ Duración total del programa calculada: %d minutos\n", programTotalDurationMinutes);
    } else {
        Serial.println("[STORAGE] ❌ Segmentos inválidos, limpiando perfil");
        memset(&curvaActual, 0, sizeof(CurvaActiva));
    }
}

void reconnectToWifi() {
    if (wifiSSID == "" || wifiPassword == "") {
        Serial.println("[WiFi] No hay credenciales WiFi disponibles");
        return;
    }
    
    Serial.println("=================================");
    Serial.printf("[WiFi] 🔄 Iniciando reconexión a red WiFi\n");
    Serial.printf("[WiFi] SSID: '%s'\n", wifiSSID.c_str());
    Serial.printf("[WiFi] Password: [%d caracteres]\n", wifiPassword.length());
    
    // Verificar si es la misma red actual
    if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == wifiSSID) {
        Serial.println("[WiFi] ⚠️ Misma red detectada - puede ser cambio de contraseña");
    }
    
    Serial.println("[WiFi] 📡 Desconectando WiFi actual...");
    Serial.println("=================================");
    
    // Desconectar completamente y apagar WiFi
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    vTaskDelay(pdMS_TO_TICKS(2000)); // Esperar a que se apague completamente
    
    Serial.println("=================================");
    Serial.printf("[WiFi] 🔌 Encendiendo WiFi y conectando a: %s\n", wifiSSID.c_str());
    Serial.println("=================================");
    
    // Encender WiFi y conectar (restaura también el AP del horno)
    WiFi.persistent(false);
    wifi_manager_restoreAp();
    vTaskDelay(pdMS_TO_TICKS(500));
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
    
    unsigned long startTime = millis();
    int dots = 0;
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 30000) {
        vTaskDelay(pdMS_TO_TICKS(500));
        Serial.print(".");
        dots++;
        if (dots % 20 == 0) Serial.println();
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.println("=================================");
        Serial.printf("[WiFi] ✅ CONECTADO EXITOSAMENTE a %s\n", wifiSSID.c_str());
        Serial.printf("[WiFi] Nueva IP: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("[WiFi] Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
        Serial.printf("[WiFi] DNS: %s\n", WiFi.dnsIP().toString().c_str());
        Serial.printf("[WiFi] RSSI: %d dBm\n", WiFi.RSSI());
        Serial.println("=================================");
        
        // Intentar reconectar Firebase
        if (initFirebase()) {
            firebaseConnected = true;
            firebaseErrorCount = 0;
            Serial.println("[Firebase] ✅ Reconectado tras cambio de WiFi");
        }
    } else {
        wifiConnected = false;
        firebaseConnected = false;
        Serial.println("=================================");
        Serial.printf("[WiFi] ❌ ERROR conectando a: %s\n", wifiSSID.c_str());
        Serial.printf("[WiFi] Estado WiFi: %d\n", WiFi.status());
        Serial.println("[WiFi] Posibles causas:");
        Serial.println("  - SSID incorrecto");
        Serial.println("  - Contraseña incorrecta");
        Serial.println("  - Red fuera de alcance");
        Serial.println("  - Red en 5GHz (ESP32 solo soporta 2.4GHz)");
        Serial.println("=================================");
    }
}

void connectWiFi() {
    // Intentar conectar solo si hay credenciales guardadas
    
    if (wifiConfigReceived && wifiSSID != "") {
        Serial.printf("[WiFi] Conectando a: %s\n", wifiSSID.c_str());
        
        // Configurar modo WiFi (AP del horno sigue activo)
        WiFi.persistent(false);
        wifi_manager_ensureApStaMode();
        WiFi.disconnect(true);
        delay(500);
        
        // Iniciar conexión
        WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
        
        // Esperar conexión con timeout más largo (60 segundos)
        unsigned long startTime = millis();
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && millis() - startTime < 60000) {
            delay(500);
            Serial.print(".");
            attempts++;
            if (attempts % 20 == 0) {
                Serial.printf("\n[WiFi] Esperando... Estado: %d\n", WiFi.status());
            }
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            wifiConnected = true;
            Serial.printf("[WiFi] ✅ CONECTADO EXITOSAMENTE\n");
        } else {
            wifiConnected = false;
            Serial.println("[WiFi] ❌ FALLÓ LA CONEXIÓN");
            int wifiStatus = WiFi.status();
            
            // Traducir código de estado a mensaje legible
            const char* statusMsg = "";
            switch(wifiStatus) {
                case WL_IDLE_STATUS: statusMsg = "WL_IDLE_STATUS (0) - WiFi en modo idle"; break;
                case WL_NO_SSID_AVAIL: statusMsg = "WL_NO_SSID_AVAIL (1) - SSID no encontrado"; break;
                case WL_SCAN_COMPLETED: statusMsg = "WL_SCAN_COMPLETED (2) - Escaneo completado"; break;
                case WL_CONNECTED: statusMsg = "WL_CONNECTED (3) - Conectado"; break;
                case WL_CONNECT_FAILED: statusMsg = "WL_CONNECT_FAILED (4) - Falló la conexión"; break;
                case WL_CONNECTION_LOST: statusMsg = "WL_CONNECTION_LOST (5) - Conexión perdida"; break;
                case WL_DISCONNECTED: statusMsg = "WL_DISCONNECTED (6) - Desconectado"; break;
                default: statusMsg = "Estado desconocido"; break;
            }
            Serial.printf("[WiFi] Estado: %s\n", statusMsg);
            Serial.printf("[WiFi] SSID intentado: '%s'\n", wifiSSID.c_str());
            Serial.printf("[WiFi] Password length: %d\n", wifiPassword.length());
            Serial.println("[WiFi] Posibles causas:");
            Serial.println("  - SSID incorrecto o red fuera de alcance");
            Serial.println("  - Contraseña incorrecta");
            Serial.println("  - Red en 5GHz (ESP32 solo soporta 2.4GHz)");
            Serial.println("  - Router con problemas");
            Serial.println("  - MAC filtering activado en router");
            Serial.println("[WiFi] =========================================");
        }
    } else {
        Serial.println("[WiFi] =========================================");
        Serial.println("[WiFi] ⚠️ No hay credenciales WiFi configuradas");
        Serial.println("[WiFi] Esperando configuración desde la app via BLE...");
        Serial.println("[WiFi] =========================================");
        wifiConnected = false;
    }
}

bool initFirebase() {
    Serial.println("[Firebase] =========================================");
    Serial.println("[Firebase] Iniciando conexión a Firebase...");
    Serial.printf("[Firebase] Database URL: %s\n", FIREBASE_DATABASE_URL);
    Serial.printf("[Firebase] WiFi status: %d\n", WiFi.status());
    
    if (config == nullptr || auth == nullptr || fbdo == nullptr) {
        Serial.println("[Firebase] ❌ ERROR: Objetos Firebase no inicializados");
        return false;
    }
    
    config->api_key = FIREBASE_API_KEY;
    config->database_url = FIREBASE_DATABASE_URL;
    config->timeout.serverResponse = 3000;  // Timeout de 3 segundos
    config->timeout.socketConnection = 3000;
    config->timeout.rtdbKeepAlive = 30000;  // Keep alive cada 30s
    config->timeout.rtdbStreamReconnect = 1000;  // Reconectar rápido
    
    fbdo->setResponseSize(512);
    fbdo->setBSSLBufferSize(512, 512);

    Serial.println("[Firebase] Intentando signUp...");
    if (Firebase.signUp(config, auth, "", "")) {
        Serial.println("[Firebase] signUp exitoso, iniciando Firebase.begin...");
        Firebase.begin(config, auth);
        Firebase.reconnectWiFi(true);
        firebaseConnected = true;
        Serial.println("[Firebase] =========================================");
        Serial.println("[Firebase] ✅ CONECTADO CORRECTAMENTE");
        Serial.println("[Firebase] =========================================");
        return true;
    } else {
        firebaseConnected = false;
        Serial.println("[Firebase] =========================================");
        Serial.println("[Firebase] ❌ ERROR EN CONEXIÓN");
        Serial.printf("[Firebase] Error: %s\n", fbdo->errorReason().c_str());
        Serial.printf("[Firebase] Error code: %d\n", fbdo->errorCode());
        Serial.println("[Firebase] =========================================");
        return false;
    }
}

void reportToFirebase(float temp, const String& estado) {
    if (!wifiConnected || !firebaseConnected) {
        static unsigned long lastSkipLog = 0;
        if (millis() - lastSkipLog >= 30000) {
            Serial.printf("[Firebase] ⏭️ Saltando actualización: wifiConnected=%s, firebaseConnected=%s\n",
                         wifiConnected ? "SÍ" : "NO", firebaseConnected ? "SÍ" : "NO");
            lastSkipLog = millis();
        }
        return;
    }
    
    // Si hay muchos errores consecutivos, saltarse Firebase
    if (firebaseErrorCount > 3) {
        firebaseConnected = false;
        firebaseErrorCount = 0;
        Serial.println("[Firebase] ⚠️ Demasiados errores, desactivando temporalmente");
        return;
    }
    
    String localUserId, localDeviceMacAddress;
    String localProgramName = "";
    unsigned long localProgramStartTime = 0;
    unsigned long localProgramPauseTime = 0;
    int localProgramTotalDuration = 0;
    
    if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        localUserId = userId;
        localDeviceMacAddress = deviceMacAddress;
        // Obtener nombre del programa si existe
        if (strlen(curvaActual.nombre) > 0) {
            localProgramName = String(curvaActual.nombre);
        }
        localProgramStartTime = programStartTime;
        localProgramPauseTime = programPauseTime;
        localProgramTotalDuration = programTotalDurationMinutes;
        xSemaphoreGive(xMutex);
    } else {
        vTaskDelay(pdMS_TO_TICKS(10));
        return;
    }
    
    if (localUserId == "" || localDeviceMacAddress == "") {
        return;
    }

    if (fbdo == nullptr) return;
    fbdo->setResponseSize(256);
    Firebase.RTDB.setwriteSizeLimit(fbdo, "tiny");
    fbdo->setBSSLBufferSize(512, 512);
    
    // ========== REALTIME DATA ==========
    String realtimePath = "/users/" + localUserId + "/kilns/" + localDeviceMacAddress + "/realtimeData";

    // Usar método directo para cada campo (menos stack que FirebaseJson)
    Firebase.RTDB.setFloat(fbdo, (realtimePath + "/temperature").c_str(), temp);
    Firebase.RTDB.setString(fbdo, (realtimePath + "/status").c_str(), estado);
    
    // Actualizar nombre del programa si existe
    if (localProgramName != "") {
        Firebase.RTDB.setString(fbdo, (realtimePath + "/programName").c_str(), localProgramName);
        
        // Enviar información de tiempo del programa si está corriendo o pausado
        if (estado == "CALENTANDO" || estado == "PAUSADO") {
            if (localProgramStartTime > 0) {
                // Convertir millis a segundos (timestamp Unix)
                unsigned long startTimeSeconds = localProgramStartTime / 1000;
                Firebase.RTDB.setInt(fbdo, (realtimePath + "/programStartTime").c_str(), startTimeSeconds);
                
                // Enviar duración total
                if (localProgramTotalDuration > 0) {
                    Firebase.RTDB.setInt(fbdo, (realtimePath + "/programTotalDuration").c_str(), localProgramTotalDuration);
                }
                
                // Enviar tiempo de pausa si está pausado
                if (estado == "PAUSADO" && localProgramPauseTime > 0) {
                    unsigned long pauseTimeSeconds = localProgramPauseTime / 1000;
                    Firebase.RTDB.setInt(fbdo, (realtimePath + "/programPauseTime").c_str(), pauseTimeSeconds);
                } else {
                    // Si no está pausado, eliminar programPauseTime
                    Firebase.RTDB.deleteNode(fbdo, (realtimePath + "/programPauseTime").c_str());
                }
            }
        } else {
            // Si el programa no está corriendo, limpiar campos de tiempo
            Firebase.RTDB.deleteNode(fbdo, (realtimePath + "/programStartTime").c_str());
            Firebase.RTDB.deleteNode(fbdo, (realtimePath + "/programTotalDuration").c_str());
            Firebase.RTDB.deleteNode(fbdo, (realtimePath + "/programPauseTime").c_str());
        }
    } else {
        // Si no hay programa, limpiar todos los campos relacionados
        Firebase.RTDB.deleteNode(fbdo, (realtimePath + "/programName").c_str());
        Firebase.RTDB.deleteNode(fbdo, (realtimePath + "/programStartTime").c_str());
        Firebase.RTDB.deleteNode(fbdo, (realtimePath + "/programTotalDuration").c_str());
        Firebase.RTDB.deleteNode(fbdo, (realtimePath + "/programPauseTime").c_str());
    }

    // Usar Firebase Server Timestamp
    bool success = Firebase.RTDB.setTimestamp(fbdo, (realtimePath + "/timestamp").c_str());
    
    if (success) {
        if (localProgramName != "") {
            Serial.printf("[Firebase] ✅ %.1f°C - Programa: %s\n", temp, localProgramName.c_str());
        } else {
            Serial.printf("[Firebase] ✅ %.1f°C\n", temp);
        }
        firebaseErrorCount = 0;  // Resetear contador de errores
    } else {
        firebaseErrorCount++;
        if (fbdo == nullptr) return;
        Serial.printf("[Firebase] ❌ %s (errores: %d)\n", fbdo->errorReason().c_str(), firebaseErrorCount);
        
        // Si es error de conexión o timeout, desactivar
        if (fbdo->errorReason().indexOf("timed out") >= 0 || 
            fbdo->errorReason().indexOf("connection") >= 0) {
            firebaseConnected = false;
            Serial.println("[Firebase] Desactivado - reconectará en 30s");
        }
    }
}

// Crea deviceInfo Y realtimeData cuando se recibe KILN_INFO por BLE (primera vez)
void updateDeviceInfoFirebase() {
    if (!wifiConnected || !firebaseConnected) {
        return;
    }
    
    String localKilnName, localKilnModel, localUserId, localDeviceMacAddress, localEstado;
    String localProgramName = "";
    float localKilnVolume, localTemp;
    
    if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        localKilnName = kilnName;
        localKilnModel = kilnModel;
        localKilnVolume = kilnVolume;
        localUserId = userId;
        localDeviceMacAddress = deviceMacAddress;
        localTemp = temperaturaActual;
        localEstado = estadoHorno;
        // Obtener nombre del programa si existe
        if (strlen(curvaActual.nombre) > 0) {
            localProgramName = String(curvaActual.nombre);
        }
        xSemaphoreGive(xMutex);
    } else {
        return;
    }
    
    if (localUserId == "" || localDeviceMacAddress == "") {
        return;
    }
    
    if (fbdo == nullptr) return;
    fbdo->setResponseSize(256);
    Firebase.RTDB.setwriteSizeLimit(fbdo, "tiny");
    fbdo->setBSSLBufferSize(512, 512);
    
    // ========== DEVICE INFO ==========
    String deviceInfoPath = "/users/" + localUserId + "/kilns/" + localDeviceMacAddress + "/deviceInfo";
    
    // Usar método directo para cada campo (menos stack)
    Firebase.RTDB.setString(fbdo, (deviceInfoPath + "/name").c_str(), localKilnName);
    Firebase.RTDB.setString(fbdo, (deviceInfoPath + "/model").c_str(), localKilnModel);
    Firebase.RTDB.setInt(fbdo, (deviceInfoPath + "/volume").c_str(), (int)localKilnVolume);
    bool success1 = Firebase.RTDB.setString(fbdo, (deviceInfoPath + "/macAddress").c_str(), localDeviceMacAddress);
    
    // ========== REALTIME DATA (inicial) ==========
    String realtimePath = "/users/" + localUserId + "/kilns/" + localDeviceMacAddress + "/realtimeData";
    
    // Usar método directo para cada campo (menos stack)
    Firebase.RTDB.setFloat(fbdo, (realtimePath + "/temperature").c_str(), localTemp);
    Firebase.RTDB.setString(fbdo, (realtimePath + "/status").c_str(), localEstado);
    
    // Actualizar nombre del programa si existe
    if (localProgramName != "") {
        Firebase.RTDB.setString(fbdo, (realtimePath + "/programName").c_str(), localProgramName);
    }
    
    // Usar Firebase Server Timestamp
    bool success2 = Firebase.RTDB.setTimestamp(fbdo, (realtimePath + "/timestamp").c_str());
    
    if (success1 && success2) {
        Serial.println("[Firebase] ✅ deviceInfo + realtimeData creados");
    } else if (!success1 || !success2) {
        Serial.println("[Firebase] ❌ Error en creación inicial");
        if (fbdo == nullptr || fbdo->errorReason().indexOf("connection") >= 0) {
            firebaseConnected = false;
        }
    }
}

// Lee temperatura real del MAX31855. Si hay fallo (NaN), mantiene última válida.
float leerTermocupla() {
    double c = thermocouple.readCelsius();
    static float lastValid = 0.0f;
    if (!isnan(c)) {
        lastValid = (float)c;
        return lastValid;
    }
    // Fallo (termocupla abierta/cortocircuito): devolver última válida
    (void)thermocouple.readError();  // limpiar estado de error para próxima lectura
    return lastValid;
}

// -------------------- BLE CALLBACKS --------------------
class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) override {
        bleClientConnected = true;
        Serial.println("=================================");
        Serial.println("[BLE] Dispositivo conectado");
        Serial.printf("[BLE] Clientes activos: %d\n", pServer->getConnectedCount());
        Serial.println("=================================");
    }

    void onDisconnect(NimBLEServer* pServer) override {
        bleClientConnected = false;
        lastBleDisconnectTime = millis();
        Serial.println("=================================");
        Serial.println("[BLE] Dispositivo desconectado");
        Serial.printf("[BLE] Clientes activos: %d\n", pServer->getConnectedCount());
        Serial.println("=================================");
    }
};

class CommandCallback : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) override {
        String cmd = pCharacteristic->getValue();
        cmd.trim();

        if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (cmd.startsWith("KILN_INFO:")) {
                String jsonStr = cmd.substring(10); // Remover "KILN_INFO:"
                StaticJsonDocument<512> doc;
                DeserializationError error = deserializeJson(doc, jsonStr);

                if (!error) {
                    // Extraer y almacenar datos
                    String nombre = doc["name"] | "Sin Nombre";
                    String modelo = doc["model"] | "Sin Modelo";
                    float volumen = doc["volume"] | 0.0;
                    String receivedUserId = doc["userId"] | "";

                    kilnName = nombre;
                    kilnModel = modelo;
                    kilnVolume = volumen;
                    userId = receivedUserId;
                    pendingKilnInfoSave = true;  // Guardar en TaskComunicaciones (menos pila en callback BLE)
                    needsDeviceInfoUpdate = true;

                    Serial.println("=================================");
                    Serial.println("[BLE] Configuración del horno recibida");
                    Serial.printf("  - Nombre: '%s'\n", nombre.c_str());
                    Serial.printf("  - Modelo: '%s'\n", modelo.c_str());
                    Serial.printf("  - Volumen: %.1f L\n", volumen);
                    Serial.printf("  - Usuario: '%s'\n", receivedUserId.c_str());
                    Serial.println("=================================");

                    JsonVariantConst wifiCfg = doc["wifi"];
                    if (!wifiCfg.isNull()) {
                        String newSsid = wifiCfg["ssid"] | "";
                        String newPassword = wifiCfg["password"] | "";
                        SerialLogScope logScope(SilentSerial);
                        Serial.println("=================================");
                        Serial.println("[BLE] WiFi recibido dentro de KILN_INFO");
                        Serial.printf("  - SSID: '%s'\n", newSsid.c_str());
                        Serial.printf("  - Password length: %d\n", newPassword.length());

                        if (newSsid.length() > 0) {
                            wifiSSID = newSsid;
                            wifiPassword = newPassword;
                            wifiConfigReceived = true;
                            needsWifiReconnect = true;
                            pendingWifiConfigSave = true;  // Guardar en TaskComunicaciones
                            Serial.println("[BLE] Credenciales WiFi programadas para guardar");
                        } else {
                            Serial.println("[BLE] ⚠️ SSID vacío; se mantiene la configuración anterior");
                        }
                        Serial.println("=================================");
                    }

                    String respuesta = "[BLE] KILN_INFO_OK";
                    pCharacteristic->setValue(respuesta);
                    xSemaphoreGive(xMutex);
                    return;
                } else {
                    Serial.println("=================================");
                    Serial.println("[BLE] Error al parsear configuración del horno");
                    Serial.printf("  - Motivo: %s\n", error.c_str());
                    Serial.println("=================================");

                    String respuesta = "[BLE] KILN_INFO_ERROR";
                    pCharacteristic->setValue(respuesta);
                    xSemaphoreGive(xMutex);
                    return;
                }
            }
            
            // Comandos existentes...
            if (cmd == "START") {
                estadoHorno = "CALENTANDO";
                
                // Si había una pausa y ahora se reanuda, resetear programPauseTime
                if (programPauseTime > 0) {
                    // Ajustar programStartTime para compensar el tiempo pausado
                    unsigned long pauseDuration = millis() - programPauseTime;
                    programStartTime += pauseDuration;
                    programPauseTime = 0;
                    Serial.printf("[CMD] Programa reanudado (pausa: %lu ms)\n", pauseDuration);
                } else if (programStartTime == 0 && curvaActual.recibida && curvaActual.numSegmentos > 0) {
                    // Si no hay timestamp de inicio pero hay un programa cargado, calcular duración e iniciar
                    float currentTemp = temperaturaActual;
                    int totalDuration = calculateProgramTotalDuration(currentTemp);
                    programStartTime = millis();
                    programTotalDurationMinutes = totalDuration;
                    Serial.printf("[CMD] Programa iniciado - Duración: %d minutos\n", totalDuration);
                }
                
                Serial.println("[CMD] Inicio de horneado");
            }
            else if (cmd == "PAUSE") {
                estadoHorno = "PAUSADO";
                programPauseTime = millis();
                Serial.println("[CMD] Programa pausado");
            }
            else if (cmd == "STOP") {
                estadoHorno = "DETENIDO";
                programStartTime = 0;
                programPauseTime = 0;
                Serial.println("[CMD] Programa detenido");
            }
            else if (cmd == "TEST") {
                Serial.println("[CMD] ✅ Comando TEST recibido correctamente");
                String testResponse = "[BLE] TEST_OK";
                pCharacteristic->setValue(testResponse);
                xSemaphoreGive(xMutex);
                return;
            }
            else if (cmd == "CLEAR_WIFI") {
                Serial.println("=================================");
                Serial.println("[CMD] 🗑️ LIMPIANDO CREDENCIALES WIFI");
                preferences.begin("wifi_config", false);
                preferences.clear();
                preferences.end();
                
                wifiSSID = "";
                wifiPassword = "";
                wifiConfigReceived = false;
                
                Serial.println("[CMD] ✅ Credenciales WiFi eliminadas");
                Serial.println("=================================");
                
                String response = "[BLE] WIFI_CLEARED";
                pCharacteristic->setValue(response);
                xSemaphoreGive(xMutex);
                return;
            }
            else if (cmd == "DEBUG_VARS") {
                Serial.println("=================================");
                Serial.println("[CMD] DEBUG - Variables globales actuales:");
                Serial.printf("  - kilnName: '%s'\n", kilnName.c_str());
                Serial.printf("  - kilnModel: '%s'\n", kilnModel.c_str());
                Serial.printf("  - kilnVolume: %.1f\n", kilnVolume);
                Serial.printf("  - userId: '%s'\n", userId.c_str());
                Serial.printf("  - deviceMacAddress: '%s'\n", deviceMacAddress.c_str());
                Serial.println("=================================");
                
                String debugResponse = "DEBUG|" + kilnName + "|" + kilnModel + "|" + String(kilnVolume) + "|" + userId;
                pCharacteristic->setValue(debugResponse);
                xSemaphoreGive(xMutex);
                return;
            }
            else if (cmd == "STATUS") {
                Serial.println("=================================");
                Serial.println("[CMD] ESTADO ACTUAL DEL SISTEMA:");
                Serial.printf("  - kilnName: '%s'\n", kilnName.c_str());
                Serial.printf("  - kilnModel: '%s'\n", kilnModel.c_str());
                Serial.printf("  - kilnVolume: %.1fL\n", kilnVolume);
                Serial.printf("  - userId: '%s'\n", userId.c_str());
                Serial.printf("  - deviceMacAddress: '%s'\n", deviceMacAddress.c_str());
                Serial.printf("  - estadoHorno: '%s'\n", estadoHorno.c_str());
                Serial.printf("  - temperaturaActual: %.1f°C\n", temperaturaActual);
                Serial.printf("  - wifiConnected: %s\n", wifiConnected ? "SI" : "NO");
                Serial.printf("  - firebaseConnected: %s\n", firebaseConnected ? "SI" : "NO");
                Serial.println("=================================");
                
                String statusResponse = "STATUS|" + kilnName + "|" + kilnModel + "|" + String(kilnVolume) + "|" + userId + "|" + deviceMacAddress;
                pCharacteristic->setValue(statusResponse);
                xSemaphoreGive(xMutex);
                return;
            }

            String respuesta = "[BLE] " + cmd + "|OK";
            pCharacteristic->setValue(respuesta);
            Serial.printf("[CMD] Respuesta enviada: '%s'\n", respuesta.c_str());
            xSemaphoreGive(xMutex);
            Serial.println("[CMD] Mutex liberado");
        } else {
            Serial.println("[CMD] ❌ No se pudo adquirir mutex");
        }
    }
};

class CurvaCallback : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) override {
        String payload = pCharacteristic->getValue();
        Serial.printf("[BLE] Programa recibido (%d bytes)\n", payload.length());

        // Verificar si empieza con "LOAD_PROFILE:"
        if (!payload.startsWith("LOAD_PROFILE:")) {
            Serial.println("[BLE] ❌ Formato inválido");
            return;
        }

        String jsonStr = payload.substring(strlen("LOAD_PROFILE:")); // recorta el prefijo

        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, jsonStr);
        if (error) {
            Serial.printf("[BLE] ❌ Error parseando JSON: %s\n", error.c_str());

            if (pServer->getConnectedCount() > 0 && gStatusChar != nullptr) {
                gStatusChar->setValue("PROFILE_ERROR");
                gStatusChar->notify();
            }

            return;
        }

        // Bloquear para modificar curvaActual (con timeout para evitar bloqueos)
        if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(5000))) {
            memset(&curvaActual, 0, sizeof(CurvaActiva));

            // Copiar nombre
            const char* nombre = doc["nombre"] | "SIN_NOMBRE";
            strncpy(curvaActual.nombre, nombre, sizeof(curvaActual.nombre) - 1);

            // Copiar segmentos
            int numSeg = doc["numSeg"] | 0;
            curvaActual.numSegmentos = min(numSeg, 10);

            for (int i = 0; i < curvaActual.numSegmentos; i++) {
                curvaActual.segmentos[i].tempObjetivo   = doc["seg"][i]["temp"]   | 0;
                curvaActual.segmentos[i].rampaCporMin   = doc["seg"][i]["rampa"]  | 0;
                curvaActual.segmentos[i].tiempoRemojoMin= doc["seg"][i]["remojo"] | 0;
            }

            curvaActual.recibida = true;
            
            // Guardar datos locales para uso después de liberar el mutex
            int numSegLocal = curvaActual.numSegmentos;
            char nombreLocal[64];
            strncpy(nombreLocal, curvaActual.nombre, sizeof(nombreLocal) - 1);
            nombreLocal[sizeof(nombreLocal) - 1] = '\0';
            
            // Copiar segmentos localmente para no depender del mutex
            float temps[10], rampas[10];
            int remojos[10];
            for (int i = 0; i < numSegLocal && i < 10; i++) {
                temps[i] = curvaActual.segmentos[i].tempObjetivo;
                rampas[i] = curvaActual.segmentos[i].rampaCporMin;
                remojos[i] = curvaActual.segmentos[i].tiempoRemojoMin;
            }
            
            // Calcular duración total del programa (usando temperatura actual como inicial)
            float currentTempForDuration = 25.0; // Temperatura ambiente por defecto
            if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                if (temperaturaActual > 0) {
                    currentTempForDuration = temperaturaActual;
                }
                xSemaphoreGive(xMutex);
            }
            // Calcular duración total (fuera del mutex ya que curvaActual ya está copiada)
            programTotalDurationMinutes = calculateProgramTotalDuration(currentTempForDuration);
            
            // CRÍTICO: Confirmar hacia la App PRIMERO (antes de Serial.printf)
            // Los Serial.printf pueden ser lentos y causar timeout de la conexión BLE
            // Si enviamos PROFILE_OK primero, la app recibirá la confirmación antes de que se pierda la conexión
            if (pServer->getConnectedCount() > 0 && gStatusChar != nullptr) {
                gStatusChar->setValue("PROFILE_OK");
                gStatusChar->notify();
                Serial.println("[BLE] ✅ PROFILE_OK enviado a la app");
            } else {
                Serial.println("[BLE] ⚠️ No se pudo enviar PROFILE_OK: sin conexión o gStatusChar es null");
            }

            // Guardar programa en memoria persistente
            saveProfile();
            
            Serial.printf("[BLE] ⏱️ Duración total del programa calculada: %d minutos\n", programTotalDurationMinutes);
            
            // DIAGNÓSTICO: Verificar que el nombre se guardó correctamente (reducido para evitar saturación Serial)
            Serial.printf("[BLE] Programa recibido: '%s' (%d seg, %d min total)\n", 
                         nombreLocal, numSegLocal, programTotalDurationMinutes);
            
            // Notificar nombre del programa por BLE
            // IMPORTANTE: Enviar inmediatamente después de guardar
            // El loop periódico también lo enviará, pero esto asegura recepción inmediata
            if (pServer->getConnectedCount() > 0 && gProgramNameChar != nullptr) {
                gProgramNameChar->setValue(String(nombreLocal));
                gProgramNameChar->notify();
                Serial.printf("[BLE] ✅ Nombre del programa enviado por BLE: %s\n", nombreLocal);
            } else {
                Serial.printf("[BLE] ⚠️ No se pudo enviar nombre del programa: conectados=%d, gProgramNameChar=%p\n", 
                             pServer->getConnectedCount(), gProgramNameChar);
            }

            // Confirmar por Serial (resumido para evitar saturación del buffer)
            // Los detalles de segmentos se omiten para evitar bloquear Serial
            
            // CRÍTICO: Liberar el mutex para que otras tareas puedan leer curvaActual
            xSemaphoreGive(xMutex);
            // Delay pequeño para permitir que Serial procese antes de continuar
            delay(10);
            Serial.println("[BLE] ✅ Programa procesado y guardado");
        } else {
            Serial.println("[BLE] ❌ ERROR: No se pudo tomar el mutex para recibir programa");
        }
    }
};

class UserIdCallback : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) override {
        String receivedUserId = pCharacteristic->getValue();
        receivedUserId.trim();
        
        Serial.printf("[BLE] UserID recibido: '%s'\n", receivedUserId.c_str());
        
        if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            userId = receivedUserId;
            xSemaphoreGive(xMutex);
        }
        
        // Confirmar recepción
        String respuesta = "[BLE] USER_ID_OK";
        pCharacteristic->setValue(respuesta);
    }
};

class WifiConfigCallback : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) override {
        Serial.println("=================================");
        Serial.println("[BLE] 🔔 WIFI_CONFIG CALLBACK EJECUTADO");
        Serial.printf("[BLE] Conexiones activas: %d\n", pServer->getConnectedCount());
        
        String payload = pCharacteristic->getValue();
        Serial.println("=================================");
        Serial.println("[BLE] 📶 WIFI_CONFIG recibido:");
        Serial.printf("[BLE] Longitud del payload: %d bytes\n", payload.length());
        Serial.printf("[BLE] Payload: '%s'\n", payload.c_str());
        Serial.println("=================================");

        if (payload.length() == 0) {
            Serial.println("[BLE] ❌ Payload vacío!");
            String respuesta = "[BLE] WIFI_CONFIG_ERROR:EMPTY";
            pCharacteristic->setValue(respuesta);
            return;
        }

        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, payload);
        
        if (error) {
            Serial.println("=================================");
            Serial.println("[BLE] ❌ ERROR PARSEANDO JSON");
            Serial.printf("[BLE] Error: %s\n", error.c_str());
            Serial.printf("[BLE] Payload recibido: '%s'\n", payload.c_str());
            Serial.println("=================================");
            String respuesta = "[BLE] WIFI_CONFIG_ERROR:PARSE";
            pCharacteristic->setValue(respuesta);
            return;
        }

        Serial.println("[BLE] ✅ JSON parseado correctamente");
        Serial.printf("[BLE] Extrayendo datos...\n");

        String incomingSSID = "";
        String incomingPassword = "";

        if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            wifiSSID = doc["ssid"] | "";
            wifiPassword = doc["password"] | "";
            incomingSSID = wifiSSID;
            incomingPassword = wifiPassword;
            wifiConfigReceived = (wifiSSID.length() > 0);
            needsWifiReconnect = true;
            xSemaphoreGive(xMutex);
        } else {
            Serial.println("[BLE] ❌ No se pudo adquirir mutex para WiFi config");
            String respuesta = "[BLE] WIFI_CONFIG_ERROR:MUTEX";
            pCharacteristic->setValue(respuesta);
            return;
        }

        Serial.println("=================================");
        Serial.println("[BLE] ✅ WiFi config procesado:");
        debugPrintWifiCredentials("Credenciales recibidas por BLE", incomingSSID, incomingPassword);
        Serial.printf("[BLE] wifiConfigReceived (RAM): %s\n", wifiConfigReceived ? "SÍ" : "NO");
        Serial.println("=================================");

        // Diferir save/load/reconnect a TaskComunicaciones para no usar pila NimBLE (evita reinicio al conectar BLE)
        if (wifiConfigReceived && wifiSSID.length() > 0) {
            pendingWifiConfigSave = true;
            Serial.println("[BLE] ✅ Credenciales aceptadas; guardado programado en tarea principal");
            pCharacteristic->setValue("[BLE] WIFI_CONFIG_OK");
        } else {
            Serial.println("[BLE] ❌ Credenciales inválidas (SSID vacío)");
            pCharacteristic->setValue("[BLE] WIFI_CONFIG_ERROR:EMPTY_SSID");
        }
    }
};

void setupBLE() {
    // Reforzar deshabilitación de logs de NimBLE ANTES de inicializar (nivel C de ESP-IDF)
    esp_log_level_set("NimBLEDevice", ESP_LOG_NONE);
    esp_log_level_set("NimBLEServer", ESP_LOG_NONE);
    esp_log_level_set("NimBLEService", ESP_LOG_NONE);
    esp_log_level_set("NimBLEAdvertising", ESP_LOG_NONE);
    esp_log_level_set("NimBLECharacteristic", ESP_LOG_NONE);
    esp_log_level_set("NimBLEClient", ESP_LOG_NONE);
    esp_log_level_set("NimBLEScan", ESP_LOG_NONE);
    esp_log_level_set("NimBLEUtils", ESP_LOG_NONE);
    
    Serial.println("=================================");
    Serial.println("[BLE] 🚀 INICIANDO CONFIGURACIÓN BLE");
    Serial.printf("[BLE] Nombre del dispositivo: '%s'\n", BLE_DEVICE_NAME);
    Serial.printf("[BLE] UUID del servicio: '%s'\n", BLE_SERVICE_UUID);
    Serial.println("=================================");
    
    NimBLEDevice::init(BLE_DEVICE_NAME);
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());
    Serial.println("[BLE] ✅ Servidor BLE creado con callbacks de conexión");

    NimBLEService* pService = pServer->createService(BLE_SERVICE_UUID);
    Serial.println("[BLE] ✅ Servicio BLE creado");

    // Características existentes...
    gTempChar = pService->createCharacteristic(
        BLE_TEMP_CHAR_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    gTempChar->addDescriptor(new NimBLE2904());

    gStatusChar = pService->createCharacteristic(
        BLE_STATUS_CHAR_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    gStatusChar->addDescriptor(new NimBLE2904());

    NimBLECharacteristic* commandChar = pService->createCharacteristic(
        BLE_COMMAND_CHAR_UUID,
        NIMBLE_PROPERTY::WRITE
    );
    Serial.printf("[BLE] ✅ Característica de comandos creada: '%s'\n", BLE_COMMAND_CHAR_UUID);
    commandChar->setCallbacks(new CommandCallback());
    Serial.println("[BLE] ✅ Callback de comandos configurado");
    
    NimBLECharacteristic* pCurvaChar = pService->createCharacteristic(
        BLE_CURVA_CHAR_UUID,
        NIMBLE_PROPERTY::WRITE
    );
    pCurvaChar->setCallbacks(new CurvaCallback());

    // NUEVA: Característica para recibir UserID
    gUserIdChar = pService->createCharacteristic(
        BLE_USER_ID_CHAR_UUID,
        NIMBLE_PROPERTY::WRITE
    );
    gUserIdChar->setCallbacks(new UserIdCallback());
    
    // NUEVA: Característica para recibir credenciales WiFi
    NimBLECharacteristic* pWifiConfigChar = pService->createCharacteristic(
        BLE_WIFI_CONFIG_CHAR_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::READ
    );
    pWifiConfigChar->setCallbacks(new WifiConfigCallback());
    Serial.printf("[BLE] ✅ Característica WiFi config creada: '%s'\n", BLE_WIFI_CONFIG_CHAR_UUID);

    // NUEVA: Característica para enviar nombre del programa activo
    gProgramNameChar = pService->createCharacteristic(
        BLE_PROGRAM_NAME_CHAR_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    gProgramNameChar->addDescriptor(new NimBLE2904());
    Serial.printf("[BLE] ✅ Característica de nombre de programa creada: '%s'\n", BLE_PROGRAM_NAME_CHAR_UUID);

    pService->start();
    Serial.println("[BLE] ✅ Servicio BLE iniciado");
    
    pServer->getAdvertising()->start();
    Serial.println("[BLE] ✅ Advertising BLE iniciado");
    
    // Verificar estado del advertising
    Serial.println("=================================");
    Serial.println("[BLE] 🔍 VERIFICACIÓN DE ADVERTISING:");
    Serial.printf("[BLE] Advertising activo: %s\n", pServer->getAdvertising()->isAdvertising() ? "SÍ" : "NO");
    Serial.printf("[BLE] Dispositivos conectados: %d\n", pServer->getConnectedCount());
    Serial.printf("[BLE] Nombre del dispositivo: '%s'\n", BLE_DEVICE_NAME);
    Serial.printf("[BLE] UUID del servicio: '%s'\n", BLE_SERVICE_UUID);
    Serial.println("=================================");
    Serial.println("[BLE] 🎯 BLE CONFIGURADO COMPLETAMENTE");
    Serial.println("[BLE] Esperando conexiones de dispositivos...");
    Serial.println("=================================");
}

// -------------------- TAREAS --------------------
void TaskControlHorno(void* pvParameters) {
    for (;;) {
        if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            temperaturaActual = leerTermocupla();
            xSemaphoreGive(xMutex);
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void TaskComunicaciones(void* pvParameters) {
    Serial.println("[TaskCom] ✅ TAREA INICIADA - TaskComunicaciones comenzando ejecución");
    
    unsigned long lastBLEUpdate = 0;
    unsigned long lastFirebaseUpdate = millis();  // Inicializar con millis() actual
    unsigned long lastAdvertisingCheck = 0;
    unsigned long lastFirebaseReconnect = 0;
    unsigned long lastWiFiCheck = 0;
    static unsigned int loopCounter = 0;

    for (;;) {
        kiln_api_loop();

        unsigned long now = millis();
        loopCounter++;
        
        // Log cada 50 iteraciones (~5 segundos con delay de 100ms)
        if (loopCounter % 50 == 0) {
            Serial.printf("[TaskCom] 🔄 Loop activo (iteración %u, tiempo: %lu ms)\n", loopCounter, now);
        }
        
        // --- NUEVA LÓGICA: Control de WiFi basado en estado BLE ---
        if (now - lastWiFiCheck >= 5000) {
            // Verificar si hay cliente BLE conectado
            bool currentBleConnected = (pServer->getConnectedCount() > 0);
            
            if (currentBleConnected && !bleClientConnected) {
                // Cliente BLE se conectó - actualizar estado
                bleClientConnected = true;
                Serial.println("[BLE] 🔵 Cliente BLE detectado - suspendiendo WiFi");
            } else if (!currentBleConnected && bleClientConnected) {
                // Cliente BLE se desconectó - actualizar estado
                bleClientConnected = false;
                lastBleDisconnectTime = now;
                Serial.println("[BLE] 🔴 Cliente BLE desconectado - programando reactivación WiFi");
            }
            
            if (bleClientConnected) {
                // BLE activo - suspender WiFi si está conectado
                if (wifiConnected && !wifiSuspended) {
                    Serial.println("[WiFi] 🔄 SUSPENDIENDO WiFi para priorizar BLE");
                    WiFi.disconnect();
                    wifiConnected = false;
                    firebaseConnected = false;
                    wifiSuspended = true;
                    Serial.println("[WiFi] ✅ WiFi suspendido - solo BLE activo");
                }
            } else {
                // BLE inactivo - reactivar WiFi después del delay
                if (wifiSuspended && (now - lastBleDisconnectTime >= BLE_DISCONNECT_DELAY)) {
                    Serial.println("[WiFi] 🔄 REACTIVANDO WiFi tras desconexión BLE");
                    
                    if (wifiConfigReceived && wifiSSID != "") {
                        Serial.printf("[WiFi] Conectando a: %s\n", wifiSSID.c_str());
                        wifi_manager_ensureApStaMode();
                        WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
                        
                        // Timeout más largo para reconexión (30 segundos)
                        unsigned long startTime = millis();
                        int attempts = 0;
                        while (WiFi.status() != WL_CONNECTED && millis() - startTime < 30000) {
                            vTaskDelay(pdMS_TO_TICKS(500));
                            attempts++;
                            if (attempts % 10 == 0) {
                                Serial.printf("[WiFi] Esperando reconexión... Estado: %d\n", WiFi.status());
                            }
                        }
                        
                        if (WiFi.status() == WL_CONNECTED) {
                            wifiConnected = true;
                            wifiSuspended = false;
                            Serial.printf("[WiFi] ✅ Reactivado. IP: %s\n", WiFi.localIP().toString().c_str());
                            
                            // Reconectar Firebase
                            if (initFirebase()) {
                                firebaseConnected = true;
                                firebaseErrorCount = 0;
                                Serial.println("[Firebase] ✅ Reconectado tras reactivación WiFi");
                            }
                        } else {
                            Serial.printf("[WiFi] ❌ Error reactivando WiFi. Estado: %d\n", WiFi.status());
                            wifiSuspended = false; // Permitir nuevos intentos
                        }
                    } else {
                        Serial.println("[WiFi] ⚠️ No hay credenciales para reactivar WiFi");
                        wifiSuspended = false;
                    }
                }
            }
            
            // Verificar conexión WiFi normal (solo si no está suspendido)
            if (!wifiSuspended && WiFi.status() != WL_CONNECTED && wifiConfigReceived) {
                if (wifiConnected) {
                    Serial.println("[WiFi] ⚠️ Conexión perdida, intentando reconectar...");
                    wifiConnected = false;
                    firebaseConnected = false;
                }
                
                Serial.printf("[WiFi] Reconectando a: %s\n", wifiSSID.c_str());
                wifi_manager_ensureApStaMode();
                WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
                
                // Timeout más largo para reconexión (30 segundos)
                unsigned long startTime = millis();
                int attempts = 0;
                while (WiFi.status() != WL_CONNECTED && millis() - startTime < 30000) {
                    vTaskDelay(pdMS_TO_TICKS(500));
                    attempts++;
                    if (attempts % 10 == 0) {
                        Serial.printf("[WiFi] Esperando reconexión... Estado: %d\n", WiFi.status());
                    }
                }
                
                if (WiFi.status() == WL_CONNECTED) {
                    wifiConnected = true;
                    Serial.printf("[WiFi] ✅ Reconectado. IP: %s, RSSI: %d dBm\n", 
                                 WiFi.localIP().toString().c_str(), WiFi.RSSI());
                    
                    if (initFirebase()) {
                        firebaseConnected = true;
                        firebaseErrorCount = 0;
                        Serial.println("[Firebase] ✅ Reconectado");
                    }
                } else {
                    int wifiStatus = WiFi.status();
                    Serial.printf("[WiFi] ❌ No se pudo reconectar. Estado: %d\n", wifiStatus);
                    const char* statusMsg = "";
                    switch(wifiStatus) {
                        case WL_IDLE_STATUS: statusMsg = "WL_IDLE_STATUS (0)"; break;
                        case WL_NO_SSID_AVAIL: statusMsg = "WL_NO_SSID_AVAIL (1) - SSID no encontrado"; break;
                        case WL_CONNECT_FAILED: statusMsg = "WL_CONNECT_FAILED (4) - Falló la conexión"; break;
                        case WL_CONNECTION_LOST: statusMsg = "WL_CONNECTION_LOST (5) - Conexión perdida"; break;
                        case WL_DISCONNECTED: statusMsg = "WL_DISCONNECTED (6) - Desconectado"; break;
                        default: statusMsg = "Estado desconocido"; break;
                    }
                    Serial.printf("[WiFi] Estado: %s\n", statusMsg);
                }
            } else if (!wifiSuspended && WiFi.status() == WL_CONNECTED && !wifiConnected) {
                wifiConnected = true;
                Serial.printf("[WiFi] ✅ Conexión restaurada. IP: %s\n", WiFi.localIP().toString().c_str());
            }
            
            lastWiFiCheck = now;
        }
        
        // --- Reconectar Firebase si es necesario (cada 30 seg) ---
        if (!firebaseConnected && wifiConnected && (now - lastFirebaseReconnect >= 30000)) {
            Serial.println("[Firebase] Intentando reconectar...");
            if (initFirebase()) {
                firebaseErrorCount = 0;  // Resetear contador de errores
                Serial.println("[Firebase] ✅ Reconectado");
            }
            lastFirebaseReconnect = now;
        }

        // --- BLE: Actualización cada 1 seg (no bloqueante) ---
        if (now - lastBLEUpdate >= 1000) {
            if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(10)) == pdTRUE) {  // Timeout reducido
                if (pServer->getConnectedCount() > 0) {
                    gTempChar->setValue(temperaturaActual);
                    gTempChar->notify();
                    gStatusChar->setValue(estadoHorno);
                    gStatusChar->notify();

                    // Log cada ~10 s para confirmar envío por serial (no cada 1 s para no saturar)
                    static unsigned long lastBleLog = 0;
                    if (now - lastBleLog >= 10000) {
                        Serial.printf("[BLE] 📤 Enviando temp=%.1f°C, estado=%s (cada 1 s)\n",
                                      temperaturaActual, estadoHorno.c_str());
                        lastBleLog = now;
                    }

                    // Notificar nombre del programa si existe
                    if (gProgramNameChar != nullptr && strlen(curvaActual.nombre) > 0) {
                        gProgramNameChar->setValue(String(curvaActual.nombre));
                        gProgramNameChar->notify();
                    }
                }
                xSemaphoreGive(xMutex);
            }
            lastBLEUpdate = now;
        }

        // --- Verificación de Advertising cada 10 seg ---
        if (now - lastAdvertisingCheck >= 10000) {
            if (statusLogsEnabled) {
                Serial.println("=================================");
                Serial.println("[BLE] 🔍 VERIFICACIÓN PERIÓDICA DE ADVERTISING:");
                Serial.printf("[BLE] Advertising activo: %s\n", pServer->getAdvertising()->isAdvertising() ? "SÍ" : "NO");
                Serial.printf("[BLE] Dispositivos conectados: %d\n", pServer->getConnectedCount());
                Serial.printf("[BLE] Estado BLE: %s\n", bleClientConnected ? "CONECTADO" : "DESCONECTADO");
                Serial.println("=================================");
                Serial.println("[WiFi] 📶 ESTADO DE CONEXIÓN WiFi:");
                Serial.printf("[WiFi] wifiConfigReceived: %s\n", wifiConfigReceived ? "SÍ" : "NO");
                if (wifiConfigReceived) {
                    Serial.printf("[WiFi] SSID guardado: '%s'\n", wifiSSID.c_str());
                    Serial.printf("[WiFi] Password length: %d\n", wifiPassword.length());
                }
                Serial.printf("[WiFi] WiFi.status(): %d\n", WiFi.status());
                Serial.printf("[WiFi] wifiConnected: %s\n", wifiConnected ? "SÍ" : "NO");
                if (wifiConnected) {
                    Serial.printf("[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());
                    Serial.printf("[WiFi] RSSI: %d dBm\n", WiFi.RSSI());
                }
                Serial.printf("[WiFi] WiFi suspendido: %s\n", wifiSuspended ? "SÍ" : "NO");
                Serial.println("=================================");
                Serial.println("[Firebase] 🔥 ESTADO DE FIREBASE:");
                Serial.printf("[Firebase] firebaseConnected: %s\n", firebaseConnected ? "SÍ" : "NO");
                Serial.println("=================================");
            }
            lastAdvertisingCheck = now;
        }

        // --- Verificar si necesita actualizar deviceInfo (cuando se recibe KILN_INFO) ---
        if (needsDeviceInfoUpdate && wifiConnected && firebaseConnected) {
            Serial.println("[Firebase] Flag detectada - actualizando deviceInfo...");
            
            if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                needsDeviceInfoUpdate = false;
                xSemaphoreGive(xMutex);
                
                vTaskDelay(pdMS_TO_TICKS(10));
                updateDeviceInfoFirebase();
                Serial.println("[Firebase] deviceInfo actualizado desde KILN_INFO");
            }
        }

        // --- Guardado diferido desde callbacks BLE (evita stack overflow en tarea NimBLE) ---
        if (pendingKilnInfoSave) {
            pendingKilnInfoSave = false;
            Serial.println("[BLE] 💾 Guardando datos del horno (diferido desde KILN_INFO)...");
            saveKilnInfo();
        }
        if (pendingWifiConfigSave) {
            pendingWifiConfigSave = false;
            Serial.println("[BLE/WiFi] 💾 Guardando y recargando credenciales WiFi (diferido desde BLE)...");
            saveWifiConfig();
            loadWifiConfig();
            attemptImmediateWifiReconnect("TaskCom pendingWifiConfigSave");
        }
        
        // --- NUEVO: Verificar reconexión WiFi ---
        if (needsWifiReconnect) {
            Serial.println("=================================");
            Serial.println("[WiFi] 🔄 NUEVAS CREDENCIALES WiFi DETECTADAS");
            Serial.println("[WiFi] Iniciando reconexión automática...");
            Serial.println("=================================");
            
            if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                // Copiar credenciales para reconexión
                String ssidToConnect = wifiSSID;
                needsWifiReconnect = false;
                xSemaphoreGive(xMutex);
                
                Serial.printf("[WiFi] Credenciales recibidas: SSID='%s'\n", ssidToConnect.c_str());
                
                // Reconectar
                reconnectToWifi();
            } else {
                Serial.println("[WiFi] ⚠️ No se pudo tomar mutex para reconexión WiFi");
            }
        }
        
        // --- Firebase: Envío cada 5 seg (solo si WiFi no está suspendido) ---
        if (now - lastFirebaseUpdate >= 5000) {
            Serial.printf("[TaskCom] 🔍 Verificando Firebase update: wifiConnected=%s, firebaseConnected=%s, wifiSuspended=%s\n",
                         wifiConnected ? "SÍ" : "NO",
                         firebaseConnected ? "SÍ" : "NO",
                         wifiSuspended ? "SÍ" : "NO");
            
            if (wifiConnected && firebaseConnected && !wifiSuspended) {
                float tempCopy = 0;
                String estadoCopy = "";
                String userIdCopy = "";
                
                Serial.println("[TaskCom] ✅ Condiciones cumplidas, intentando tomar mutex...");
                
                if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    tempCopy = temperaturaActual;
                    estadoCopy = estadoHorno;
                    userIdCopy = userId;
                    xSemaphoreGive(xMutex);
                    
                    Serial.printf("[TaskCom] 📊 Datos copiados: temp=%.1f°C, estado=%s, userId=%s\n",
                                 tempCopy, estadoCopy.c_str(), userIdCopy.length() > 0 ? userIdCopy.c_str() : "VACÍO");
                    
                    // Yields múltiples para resetear WDT antes y después de Firebase
                    vTaskDelay(pdMS_TO_TICKS(50));
                    
                    // Verificar Firebase.ready() antes de enviar
                    bool firebaseReady = Firebase.ready();
                    Serial.printf("[TaskCom] 🔥 Firebase.ready() = %s\n", firebaseReady ? "TRUE" : "FALSE");
                    
                    // Enviar solo si tenemos userId
                    if (userIdCopy != "") {
                        if (firebaseReady) {
                            Serial.printf("[Firebase] 📤 Enviando datos: temp=%.1f°C, estado=%s, userId=%s\n", 
                                         tempCopy, estadoCopy.c_str(), userIdCopy.c_str());
                            reportToFirebase(tempCopy, estadoCopy);
                            vTaskDelay(pdMS_TO_TICKS(50));  // Yield adicional después de Firebase
                        } else {
                            Serial.println("[Firebase] ⚠️ Firebase no está listo (token/cliente no válido)");
                        }
                    } else {
                        Serial.println("[Firebase] ⚠️ No se puede actualizar: userId vacío (esperando desde app BLE)");
                    }
                } else {
                    Serial.println("[TaskCom] ❌ No se pudo tomar mutex para Firebase update");
                    vTaskDelay(pdMS_TO_TICKS(10));
                    continue;
                }
            } else {
                Serial.printf("[Firebase] ⚠️ Condiciones no cumplidas: wifiConnected=%s, firebaseConnected=%s, wifiSuspended=%s\n",
                             wifiConnected ? "SÍ" : "NO",
                             firebaseConnected ? "SÍ" : "NO",
                             wifiSuspended ? "SÍ" : "NO");
            }
            lastFirebaseUpdate = now;
        } else if (wifiSuspended) {
            // WiFi suspendido - solo actualizar timestamp para evitar envíos
            lastFirebaseUpdate = now;
        }

        // --- Libera CPU frecuentemente ---
        vTaskDelay(pdMS_TO_TICKS(100));  // Más delay para el WDT
    }
}

void TaskControlCurva(void* pvParameters) {
    for (;;) {
        // Espera curva recibida y horno en modo calentando
        if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            bool run = (estadoHorno == "CALENTANDO" && curvaActual.recibida);
            xSemaphoreGive(xMutex);

            if (!run) {
                vTaskDelay(pdMS_TO_TICKS(500));
                continue;
            }
        }

        Serial.println("[Curva] Iniciando ejecución...");
        
        // Obtener temperatura inicial y calcular duración total
        float initialTemp = 0.0;
        if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            initialTemp = temperaturaActual;
            xSemaphoreGive(xMutex);
        }
        
        // Calcular duración total usando función auxiliar
        int totalDurationMinutes = calculateProgramTotalDuration(initialTemp);
        
        // Guardar timestamp de inicio y duración total
        if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            programStartTime = millis();
            programTotalDurationMinutes = totalDurationMinutes;
            programPauseTime = 0;  // Resetear tiempo de pausa
            xSemaphoreGive(xMutex);
        }
        
        Serial.printf("[Curva] Duración total calculada: %d minutos\n", totalDurationMinutes);
        
        for (int i = 0; i < curvaActual.numSegmentos; i++) {
            Segmento seg = curvaActual.segmentos[i];
            Serial.printf("[Curva] Segmento %d / %d: T=%.1f, R=%.1f, M=%d\n",
                          i + 1, curvaActual.numSegmentos,
                          seg.tempObjetivo, seg.rampaCporMin, seg.tiempoRemojoMin);

            // --- Rampa ---
            float tempLocal;
            if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                tempLocal = temperaturaActual;
                xSemaphoreGive(xMutex);
            }

            float rampaPorSegundo = seg.rampaCporMin / 60.0;
            while (tempLocal < seg.tempObjetivo && estadoHorno == "CALENTANDO") {
                tempLocal += rampaPorSegundo;  // Simulación
                if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                    temperaturaActual = tempLocal;
                    xSemaphoreGive(xMutex);
                }
                vTaskDelay(pdMS_TO_TICKS(1000));
            }

            // --- Remojo ---
            unsigned long inicioRemojo = millis();
            while ((millis() - inicioRemojo) < (unsigned long)seg.tiempoRemojoMin * 60000UL
                   && estadoHorno == "CALENTANDO") {
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        }

        // --- Finalización ---
        if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            estadoHorno = "FINALIZADO";
            curvaActual.recibida = false;
            programStartTime = 0;  // Resetear timestamp de inicio
            programPauseTime = 0;  // Resetear timestamp de pausa
            xSemaphoreGive(xMutex);
        }
        Serial.println("[Curva] Finalizada.");
    }
}

#if defined(ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S3)
// Tarea para actualizar la pantalla LVGL con datos del horno
void TaskUpdateDisplayLVGL(void* pvParameters) {
    for (;;) {
        DisplayData displayData;
        
        // Leer datos protegidos con mutex
        if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            // Temperatura
            displayData.currentTemp = temperaturaActual;
            
            // Estado del horno
            strncpy(displayData.status, estadoHorno.c_str(), sizeof(displayData.status) - 1);
            displayData.status[sizeof(displayData.status) - 1] = '\0';
            
            // Información del horno
            strncpy(displayData.kilnName, kilnName.c_str(), sizeof(displayData.kilnName) - 1);
            displayData.kilnName[sizeof(displayData.kilnName) - 1] = '\0';
            
            // Cargar nombre del programa con verificación
            static unsigned long lastProgramNameLog = 0;
            unsigned long currentTimeProg = millis();
            
            // Verificar que curvaActual.nombre tenga contenido válido
            String programNameStr = "";
            if (strlen(curvaActual.nombre) > 0) {
                programNameStr = String(curvaActual.nombre);
                programNameStr.trim();  // Eliminar espacios al inicio y final
            }
            
            // Logs de display comentados para enfocarse en BT y WiFi
            // if (currentTimeProg - lastProgramNameLog >= 3000) {
            //     Serial.printf("[DISPLAY] 📋 DEBUG PROGRAMA:\n");
            //     Serial.printf("  - curvaActual.nombre='%s' (strlen: %d)\n", 
            //                  curvaActual.nombre, strlen(curvaActual.nombre));
            //     Serial.printf("  - Bytes del array: ");
            //     for (int i = 0; i < min(10, (int)sizeof(curvaActual.nombre)); i++) {
            //         Serial.printf("%02X ", (unsigned char)curvaActual.nombre[i]);
            //     }
            //     Serial.println();
            //     Serial.printf("  - String(curvaActual.nombre)='%s' (longitud: %d)\n", 
            //                  programNameStr.c_str(), programNameStr.length());
            //     Serial.printf("  - ¿Está vacío? %s\n", programNameStr.isEmpty() ? "SÍ" : "NO");
            //     Serial.printf("  - ¿Es igual a ''? %s\n", (programNameStr == "") ? "SÍ" : "NO");
            //     lastProgramNameLog = currentTimeProg;
            // }
            
            strncpy(displayData.programName, programNameStr.c_str(), sizeof(displayData.programName) - 1);
            displayData.programName[sizeof(displayData.programName) - 1] = '\0';
            
            // Si el programa está vacío pero hay segmentos, podría ser un programa sin nombre
            // if (programNameStr.isEmpty() && curvaActual.numSegmentos > 0) {
            //     Serial.println("[DISPLAY] ⚠️ Hay programa con segmentos pero sin nombre!");
            // }
            
            // Conexión
                   displayData.connection.wifiConnected = wifiConnected;
                   displayData.connection.bleConnected = bleClientConnected;
                   displayData.connection.firebaseConnected = firebaseConnected;
            
            // Calcular temperatura objetivo (siempre de la etapa 1 del programa)
            displayData.targetTemp = 0.0;
            if (curvaActual.numSegmentos > 0) {
                // Siempre usar la temperatura objetivo del primer segmento (etapa 1)
                displayData.targetTemp = curvaActual.segmentos[0].tempObjetivo;
            }
            
            // Calcular tiempo transcurrido y total
            // El tiempo total siempre se muestra si hay un programa cargado
            displayData.totalMinutes = programTotalDurationMinutes;
            
            if (programStartTime > 0) {
                unsigned long elapsedMs = millis() - programStartTime;
                if (programPauseTime > 0 && estadoHorno == "PAUSADO") {
                    // Si está pausado, usar el tiempo hasta la pausa
                    elapsedMs = programPauseTime - programStartTime;
                }
                displayData.elapsedMinutes = elapsedMs / 60000;
                
                // Calcular tiempo restante
                if (displayData.totalMinutes > 0 && displayData.elapsedMinutes >= 0) {
                    displayData.remainingMinutes = displayData.totalMinutes - displayData.elapsedMinutes;
                    if (displayData.remainingMinutes < 0) {
                        displayData.remainingMinutes = 0;  // No mostrar negativo
                    }
                } else {
                    displayData.remainingMinutes = -1;
                }
            } else {
                displayData.elapsedMinutes = -1;
                displayData.remainingMinutes = -1;
            }
            
            // Calcular etapa (siempre mostrar etapa 1 del total)
            if (curvaActual.numSegmentos > 0) {
                displayData.totalStages = curvaActual.numSegmentos;
                // Siempre mostrar etapa 1
                displayData.currentStage = 1;
            } else {
                displayData.currentStage = 0;
                displayData.totalStages = 0;
            }
            
            // Determinar fase actual
            if (estadoHorno == "CALENTANDO" && curvaActual.numSegmentos > 0) {
                strncpy(displayData.currentPhase, "Ramp", sizeof(displayData.currentPhase) - 1);
                displayData.currentPhase[sizeof(displayData.currentPhase) - 1] = '\0';
            } else if (estadoHorno == "PAUSADO") {
                strncpy(displayData.currentPhase, "Paused", sizeof(displayData.currentPhase) - 1);
                displayData.currentPhase[sizeof(displayData.currentPhase) - 1] = '\0';
            } else {
                displayData.currentPhase[0] = '\0';
            }
            
            xSemaphoreGive(xMutex);
            
            // Actualizar pantalla (logs comentados para enfocarse en BT y WiFi)
            // static unsigned long lastMainLog = 0;
            // unsigned long currentTime = millis();
            // if (currentTime - lastMainLog >= 2000) {
            //     Serial.printf("[MAIN] 📤 Enviando datos al display:\n");
            //     Serial.printf("  - temp=%.2f°C\n", displayData.currentTemp);
            //     Serial.printf("  - status='%s'\n", displayData.status.c_str());
            //     Serial.printf("  - targetTemp=%.2f°C\n", displayData.targetTemp);
            //     Serial.printf("  - kilnName='%s'\n", displayData.kilnName.c_str());
            //     Serial.printf("  - programName='%s'\n", displayData.programName.c_str());
            //     lastMainLog = currentTime;
            // }
            updateDisplayData(displayData);
        } else {
            // Serial.println("[DISPLAY] ⚠️ No se pudo adquirir mutex para actualizar display");
        }
        
        // Actualizar cada 500ms (2 FPS es suficiente para la pantalla)
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
#endif

// -------------------- SETUP Y LOOP --------------------
void setup() {
    // Delay inicial para USB CDC - ESP32-S3 necesita tiempo para enumerarse
    delay(3000);
    
    // Inicializar Serial - intentar múltiples veces si es necesario
    Serial.begin(115200);
    delay(2000);
    Serial.setLogsEnabled(true);
    statusLogsEnabled = true;
    
    // Intentar escribir inmediatamente para verificar que funciona
    Serial.write(0x0A); Serial.write(0x0A); Serial.write(0x0A); Serial.write(0x0A);
    delay(100);
    
    Serial.println("========================================");
    Serial.println("= ESP32-S3 INICIANDO.....              =");
    Serial.println("========================================");
    Serial.flush();
    delay(500);
    
    // Configurar niveles de log DESPUÉS de que Serial esté funcionando
    esp_log_level_set("*", ESP_LOG_NONE);
    esp_log_level_set("Preferences", ESP_LOG_NONE);
    esp_log_level_set("NimBLEDevice", ESP_LOG_NONE);
    esp_log_level_set("NimBLEServer", ESP_LOG_NONE);
    esp_log_level_set("NimBLEService", ESP_LOG_NONE);
    esp_log_level_set("NimBLEAdvertising", ESP_LOG_NONE);
    esp_log_level_set("NimBLECharacteristic", ESP_LOG_NONE);
    esp_log_level_set("NimBLEClient", ESP_LOG_NONE);
    esp_log_level_set("NimBLEScan", ESP_LOG_NONE);
    esp_log_level_set("NimBLEUtils", ESP_LOG_NONE);
    esp_log_level_set("wifi", ESP_LOG_ERROR);
    esp_log_level_set("phy_init", ESP_LOG_ERROR);
    
    Serial.println("[SETUP] ✅ Serial iniciado correctamente");
    delay(50);  // Pequeño delay para asegurar que se envíe
    
    // Inicializar objetos Firebase DESPUÉS de que Serial esté funcionando
    Serial.println("[SETUP] Inicializando objetos Firebase...");
    if (fbdo == nullptr) {
        fbdo = new FirebaseData();
        auth = new FirebaseAuth();
        config = new FirebaseConfig();
        Serial.println("[SETUP] ✅ Objetos Firebase inicializados");
    }
    delay(50);
    
    // Obtener MAC address al inicio
    Serial.println("[SETUP] Obteniendo MAC address...");
    deviceMacAddress = getMacAddress();
    deviceMacAddress.replace(":","");
    Serial.printf("[SETUP] MAC Address: %s\n", deviceMacAddress.c_str());
    delay(50);

    wifi_manager_begin(deviceMacAddress);
    kiln_api_begin();
    delay(50);

    // NUEVO: Cargar datos del horno guardados
    loadKilnInfo();
    Serial.println("[SETUP] ✅ Datos del horno cargados");
    delay(50);
    
    // NUEVO: Cargar credenciales WiFi guardadas
    loadWifiConfig();
    
    // Verificar que se cargaron correctamente
    if (wifiConfigReceived && wifiSSID.length() > 0) {
        Serial.println("[SETUP] ✅ Credenciales WiFi cargadas correctamente");
    } else {
        Serial.println("[SETUP] ⚠️ No hay credenciales WiFi guardadas");
        Serial.println("[SETUP] El ESP32 esperará configuración desde la app vía BLE");
    }

    delay(50);
    
    // NUEVO: Cargar programa activo guardado
    loadProfile();
    Serial.println("[SETUP] ✅ Programa activo cargado");
    delay(50);

    // MAX31855 (termocupla): mismo pinout que proyecto MAX31855, conector P2
    Serial.println("[SETUP] Inicializando sensor MAX31855 (termocupla)...");
    if (thermocouple.begin()) {
        double tInt = thermocouple.readInternal();
        if (!isnan(tInt) && tInt >= -40.0 && tInt <= 85.0) {
            Serial.printf("[SETUP] ✅ MAX31855 OK (temp. interna chip: %.1f C)\n", tInt);
        } else {
            Serial.println("[SETUP] ✅ MAX31855 iniciado (revisar conexión si temp. incorrecta)");
        }
    } else {
        Serial.println("[SETUP] ⚠️ MAX31855 no respondió; revisar pines SCK=13, CS=11, SO=19");
    }
    delay(50);
      
    logStoredData();
    
    xMutex = xSemaphoreCreateMutex();
    
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    // Intentar conectar WiFi si hay credenciales guardadas
    connectWiFi();
    
    if (wifiConnected) {
        Serial.println("\n[SETUP] =========================================");
        Serial.println("[SETUP] INTENTANDO CONECTAR Firebase...");
        Serial.println("[SETUP] =========================================");
        initFirebase();
    } else {
        Serial.println("\n[SETUP] ⚠️ WiFi no conectado - Firebase no se inicializará");
    }
    
    Serial.println("\n[SETUP] =========================================");
    Serial.println("[SETUP] INICIANDO BLE...");
    Serial.println("[SETUP] =========================================");
    setupBLE();
    
    Serial.println("\n[SETUP] =========================================");
    Serial.println("[SETUP] ✅ SETUP COMPLETADO");
    Serial.println("[SETUP] =========================================");
    Serial.println("\n");
    
    // NUEVO: Enviar nombre del programa cargado por BLE si existe
    if (strlen(curvaActual.nombre) > 0 && gProgramNameChar != nullptr) {
        gProgramNameChar->setValue(String(curvaActual.nombre));
        Serial.printf("[BLE] ✅ Nombre del programa inicial enviado: %s\n", curvaActual.nombre);
    }

    // Inicializar pantalla LVGL (solo para ESP32-S3)
    // Retrasar inicialización para asegurar que todo lo demás esté listo
    delay(500);
    // Logs de display comentados para enfocarse en BT y WiFi
    // Serial.println("[SETUP] Preparándose para inicializar pantalla...");
    // Reducir flush() - puede causar bloqueos en USB CDC
    #if defined(ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S3)
        // Serial.println("[SETUP] Intentando inicializar pantalla...");
        if (initDisplayLVGL()) {
            // Serial.println("[SETUP] ✅ Pantalla LVGL inicializada");
            // Crear tarea para el handler de LVGL
            if (xTaskCreatePinnedToCore(displayTaskHandler, "LVGL_Handler", 4096, NULL, 1, NULL, 1) != pdPASS) {
                // Serial.println("[SETUP] ⚠️ Error al crear tarea LVGL_Handler");
            }
            // Crear tarea para actualizar datos de la pantalla
            if (xTaskCreatePinnedToCore(TaskUpdateDisplayLVGL, "UpdateDisplay", 4096, NULL, 1, NULL, 0) != pdPASS) {
                // Serial.println("[SETUP] ⚠️ Error al crear tarea UpdateDisplay");
            }
        } else {
            // Serial.println("[SETUP] ⚠️ Pantalla no inicializada (continuando sin pantalla)");
        }
    #else
        // Serial.println("[SETUP] ⚠️ No es ESP32-S3, pantalla no se inicializará");
    #endif

    xTaskCreatePinnedToCore(TaskControlHorno, "ControlHorno", 4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(TaskComunicaciones, "Comunicaciones", 10240, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(TaskControlCurva, "ControlCurva", 6144, NULL, 1, NULL, 1);
}

void loop() {
    // Heartbeat periódico para mantener Serial activo (cada 30 segundos)
    static unsigned long lastHeartbeat = 0;
    unsigned long now = millis();
    
    if (now - lastHeartbeat >= 30000) {
        lastHeartbeat = now;
        
        // Verificar y revivir Serial si es necesario
        if (!Serial) {
            Serial.end();
            delay(100);
            Serial.begin(115200);
            delay(100);
            Serial.println("[SERIAL] 🔄 Serial reiniciado");
        } else {
            // Heartbeat simple para mantener el puerto activo
            Serial.println("[HEARTBEAT] Sistema activo");
        }
    }
    
    delay(1000);
}