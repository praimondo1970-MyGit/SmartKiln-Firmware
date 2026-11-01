#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>
#include <esp_system.h>
#include <time.h>
#include "Wire.h"
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <Adafruit_MAX31855.h>
#include <esp_log.h>
#include <Preferences.h>
#include "esp_wifi.h"
#include "silent_serial.h"
#include "kiln_shared.h"
#include "wifi_manager.h"
#include "kiln_api.h"
#include "kiln_firing_firebase.h"
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

// -------------------- HANDLER DE CRASHES / RESET --------------------
// Función para capturar información de crashes
void printCrashInfo() {
    // Intentar escribir en Serial incluso si está crasheando
    Serial.println("\n\n!!! CRASH DETECTED !!!");
    Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
    Serial.printf("Largest free block: %d\n", ESP.getMaxAllocHeap());
    Serial.flush();
}

/** Causa del último arranque (esp_reset_reason). Visible en Serial + /api/status. */
String g_lastResetReason = "UNKNOWN";
int g_lastResetCode = -1;
uint32_t g_bootCount = 0;
String g_prevResetReason = "";

static const char* resetReasonToCStr(esp_reset_reason_t r) {
    switch (r) {
        case ESP_RST_UNKNOWN: return "UNKNOWN";
        case ESP_RST_POWERON: return "POWERON";
        case ESP_RST_EXT: return "EXT";
        case ESP_RST_SW: return "SW";
        case ESP_RST_PANIC: return "PANIC";
        case ESP_RST_INT_WDT: return "INT_WDT";
        case ESP_RST_TASK_WDT: return "TASK_WDT";
        case ESP_RST_WDT: return "WDT";
        case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
        case ESP_RST_BROWNOUT: return "BROWNOUT";
        case ESP_RST_SDIO: return "SDIO";
        default: return "OTHER";
    }
}

static bool isAbnormalReset(esp_reset_reason_t r) {
    return r == ESP_RST_PANIC ||
           r == ESP_RST_INT_WDT ||
           r == ESP_RST_TASK_WDT ||
           r == ESP_RST_WDT ||
           r == ESP_RST_BROWNOUT ||
           r == ESP_RST_SW;
}

/** Imprime y persiste en NVS (diag) la causa del reset de este boot. */
static void logAndPersistResetReason() {
    const esp_reset_reason_t reason = esp_reset_reason();
    g_lastResetCode = static_cast<int>(reason);
    g_lastResetReason = resetReasonToCStr(reason);

    Preferences diag;
    String prevName;
    int prevCode = -1;
    uint32_t bootN = 0;
    if (diag.begin("diag", true)) {
        prevCode = diag.getInt("rstCode", -1);
        prevName = diag.getString("rstName", "");
        bootN = diag.getUInt("bootN", 0);
        diag.end();
    }

    g_bootCount = bootN + 1;
    g_prevResetReason = prevName;

    if (diag.begin("diag", false)) {
        diag.putInt("rstCode", g_lastResetCode);
        diag.putString("rstName", g_lastResetReason);
        diag.putUInt("bootN", g_bootCount);
        if (prevCode >= 0) {
            diag.putInt("prevRstCode", prevCode);
            diag.putString("prevRstName", prevName);
        }
        diag.end();
    }

    Serial.println("----------------------------------------");
    Serial.printf("[RESET] Este boot: %s (code=%d)  boot#%u\n",
                  g_lastResetReason.c_str(), g_lastResetCode,
                  static_cast<unsigned>(g_bootCount));
    if (prevCode >= 0 && prevName.length() > 0) {
        Serial.printf("[RESET] Boot anterior: %s (code=%d)\n",
                      prevName.c_str(), prevCode);
    }
    if (isAbnormalReset(reason)) {
        Serial.println("[RESET] *** Reinicio anormal (panic/WDT/brownout/SW) ***");
        Serial.println("[RESET] Guardá este log Serial y compartilo para diagnóstico.");
    }
    Serial.println("----------------------------------------");
    Serial.flush();
}

// -------------------- CONFIGURACIÓN TEMPRANA DE LOGS --------------------
// MOVIDA A setup() para evitar crashes en inicialización global
// Se ejecutará al inicio de setup() después de que Serial esté listo

// -------------------- DECLARACIONES DE FUNCIONES --------------------
void reportToFirebase(float temp, const String& estado);
bool updateDeviceInfoFirebase();
void onStaGainedConnection();
void saveKilnInfo();
void loadKilnInfo();
void saveWifiConfig();
void loadWifiConfig();
void saveProfile();
void loadProfile();
void logStoredData();
void reconnectToWifi();
void kiln_onProfileLoaded(unsigned long programUpdatedAtFromApp);
bool publishProfileStateToFirebase();
String getMacAddress();
bool initFirebase();
void scheduleFirebaseInit(const char* reason);
void kiln_notifyLocalSessionComplete();
/** Intenta escribir status (+ timestamp) en RTDB ya (p. ej. tras RESUME). */
void kiln_tryPublishStatusNow(const char* status);
void kiln_onApRadioTransitionBegin();
void processImmediateFirebaseSync();
int calculateProgramTotalDuration(float initialTemp);
float leerTermocupla();
#if defined(ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S3)
void TaskUpdateDisplayLVGL(void *pvParameters);
#endif

// -------------------- CONFIGURACIONES --------------------
// Credenciales WiFi llegan por HTTP (kiln-info / wifi-sta)

const char* FIREBASE_API_KEY = "AIzaSyBv3VknyBVxY_bMpgKgyhlVc_ImE7ejgwk";
const char* FIREBASE_DATABASE_URL = "https://ia-based-kiln-default-rtdb.firebaseio.com";



// -------------------- VARIABLES GLOBALES --------------------
// Usar punteros e inicializar en setup() para evitar crash en inicialización global

FirebaseData* fbdo = nullptr;
FirebaseAuth* auth = nullptr;
FirebaseConfig* config = nullptr;

SemaphoreHandle_t xMutex = NULL;


float temperaturaActual = 0.0;
String estadoHorno = "IDLE";
bool wifiConnected = false;
bool firebaseConnected = false;
bool needsDeviceInfoUpdate = false;  // Flag para actualizar deviceInfo desde tarea
unsigned long profileUpdatedAtMs = 0;
volatile bool needsProfileStatePublish = false;
bool needsBootRealtimePublish = false;  // Publicar estado real al conectar Firebase tras boot
unsigned int firebaseErrorCount = 0;  // Contador de errores consecutivos

String deviceMacAddress = "";
String userId = ""; // Llega por HTTP kiln-info

// Variables para información del horno
String kilnName = "Mi Horno Cerámico";  // Valor por defecto
String kilnModel = "SmartKiln - Ceramica";
float kilnVolume = 0.0;

/** true = Solo local: SoftAP OK, sin STA/Firebase (taller sin internet). */
bool offlineOnly = false;

// Variables para WiFi dinámico
String wifiSSID = "";
String wifiPassword = "";
bool wifiConfigReceived = false;
bool needsWifiReconnect = false;
volatile bool pendingWifiConfigSave = false;  // Diferir save/load WiFi a TaskComunicaciones
volatile bool pendingKilnInfoSave = false;   // Diferir saveKilnInfo() a TaskComunicaciones
bool statusLogsEnabled = false;
volatile bool needsImmediateFirebaseSync = false;
/** Status de proceso (START/PAUSE/RESUME/STOP) pendiente de RTDB. */
static char s_pendingRealtimeStatus[24] = {0};

static void stashPendingRealtimeStatus(const char* status);
static bool flushPendingRealtimeStatusWrite(const char* status);


// NUEVO: Objeto para almacenamiento persistente
Preferences preferences;

// Variables para seguimiento de tiempo del programa
unsigned long programStartTime = 0;  // Timestamp de inicio del programa (en millis)
unsigned long programPauseTime = 0;  // Timestamp de última pausa (en millis)
int programTotalDurationMinutes = 0;  // Duración total del programa en minutos

// --------------------------------------------------------------------
// Estado de ejecución persistido (para sobrevivir a un reinicio del ESP)
// --------------------------------------------------------------------
// Estos valores se guardan en NVS (namespace "run_state") cada vez que el
// programa avanza de fase/segmento. Al boot, si encontramos CALENTANDO,
// PAUSADO o INTERRUMPIDO previo, marcamos estadoHorno = "INTERRUMPIDO" y la
// app le ofrece al usuario reanudar (RESUME) o cancelar (STOP).
//
// runCurrentSegment: índice del segmento en el que iba el programa cuando
//                    se cortó la luz; -1 = no había programa en marcha.
// runCurrentPhase  : "RAMPA" o "REMOJO" dentro del segmento actual.
// runSoakAccumMs   : ms acumulados en la fase de remojo del segmento
//                    actual (0 cuando estamos en rampa).
volatile int runCurrentSegment = -1;
String runCurrentPhase = "";
volatile unsigned long runSoakAccumMs = 0;
// resumeFromSavedState arranca en true si en setup() detectamos que había
// una cocción interrumpida; TaskControlCurva lo lee al disparar la rampa
// para empezar desde runCurrentSegment + runCurrentPhase + runSoakAccumMs
// en vez de hacerlo desde cero.
volatile bool resumeFromSavedState = false;


// CurvaActiva / Segmento en kiln_shared.h
CurvaActiva curvaActual = {"", {}, 0, false};


// -------------------- FUNCIONES --------------------

String getMacAddress() {
    uint8_t baseMac[6];
    esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
    
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
    bool offlineSaved = preferences.putBool("offlineOnly", offlineOnly);
    preferences.end();
    
    Serial.printf("[STORAGE] Resultado del guardado:\n");
    Serial.printf("  - name: %s\n", nameSaved ? "OK" : "FAIL");
    Serial.printf("  - model: %s\n", modelSaved ? "OK" : "FAIL");
    Serial.printf("  - volume: %s\n", volumeSaved ? "OK" : "FAIL");
    Serial.printf("  - userId: %s\n", userIdSaved ? "OK" : "FAIL");
    Serial.printf("  - offlineOnly: %s (%s)\n", offlineSaved ? "OK" : "FAIL",
                  offlineOnly ? "SI" : "NO");
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
    bool savedOffline = preferences.getBool("offlineOnly", false);
    
    preferences.end();
    
    Serial.printf("[STORAGE] Datos leídos de memoria:\n");
    Serial.printf("  - Nombre: '%s'\n", savedName.c_str());
    Serial.printf("  - Modelo: '%s'\n", savedModel.c_str());
    Serial.printf("  - Volumen: %.1f\n", savedVolume);
    Serial.printf("  - UserID: '%s'\n", savedUserId.c_str());
    Serial.printf("  - offlineOnly: %s\n", savedOffline ? "SI" : "NO");
    
    // Actualizar variables globales
    kilnName = savedName;
    kilnModel = savedModel;
    kilnVolume = savedVolume;
    userId = savedUserId;
    offlineOnly = savedOffline;
    
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
    Serial.printf("[WiFi][DEBUG] PASS  : [OCULTO] (longitud: %d)\n", password.length());
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
    Serial.println("=================================");

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

    preferences.putULong("programUpdatedAt", profileUpdatedAtMs);
    
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
    profileUpdatedAtMs = preferences.getULong("programUpdatedAt", 0UL);
    
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
        kiln_scheduleProfileStatePublish();
    } else {
        Serial.println("[STORAGE] ❌ Segmentos inválidos, limpiando perfil");
        memset(&curvaActual, 0, sizeof(CurvaActiva));
    }
}

// --------------------------------------------------------------------
// Persistencia del estado de ejecución (run_state)
// --------------------------------------------------------------------
// Guardamos: estado actual del horno, segmento en curso, fase (RAMPA|REMOJO),
// ms acumulados en remojo y la temperatura simulada al momento de persistir.
// Se llama desde TaskControlCurva al cambiar de segmento/fase y desde los
// handlers de START/PAUSE para grabar el estado nuevo. Se borra al
// terminar (FINALIZADO), detener (DETENIDO/STOP) o cuando el usuario
// cancela un programa interrumpido.
void saveRunState(const String& estado,
                  int segmentIdx,
                  const String& phase,
                  unsigned long soakAccumMs,
                  float tempSnapshot) {
    preferences.begin("run_state", false);
    preferences.putString("estado", estado);
    preferences.putInt("segIdx", segmentIdx);
    preferences.putString("phase", phase);
    preferences.putULong("soakMs", soakAccumMs);
    preferences.putFloat("temp", tempSnapshot);
    preferences.end();
    Serial.printf("[RunState] saved: estado=%s seg=%d phase=%s soakMs=%lu temp=%.1f\n",
                  estado.c_str(), segmentIdx, phase.c_str(), soakAccumMs, tempSnapshot);
}

void clearRunState() {
    preferences.begin("run_state", false);
    preferences.clear();
    preferences.end();
    runCurrentSegment = -1;
    runCurrentPhase = "";
    runSoakAccumMs = 0;
    resumeFromSavedState = false;
    Serial.println("[RunState] cleared");
}

// Lee el run_state guardado y, si encontramos un estado CALENTANDO,
// PAUSADO o INTERRUMPIDO previo, deja estadoHorno = "INTERRUMPIDO" para
// que la app le pregunte al usuario qué hacer. Si no había nada (o era un
// estado terminal) NO toca estadoHorno y lo deja en su default ("IDLE").
void loadRunState() {
    preferences.begin("run_state", true);  // read-only
    String estadoPrev = preferences.getString("estado", "");
    int segIdx = preferences.getInt("segIdx", -1);
    String phase = preferences.getString("phase", "");
    unsigned long soakMs = preferences.getULong("soakMs", 0UL);
    float tempPrev = preferences.getFloat("temp", 0.0f);
    preferences.end();

    Serial.printf("[RunState] loaded: estado=%s seg=%d phase=%s soakMs=%lu temp=%.1f\n",
                  estadoPrev.c_str(), segIdx, phase.c_str(), soakMs, tempPrev);

    // CALENTANDO/PAUSADO = corte con cocción activa.
    // INTERRUMPIDO = ya estábamos esperando RESUME/STOP y el ESP volvió a
    // reiniciar: hay que restaurar el mismo estado (antes se ignoraba y
    // el horno caía a IDLE perdiendo la cocción pendiente).
    if (estadoPrev != "CALENTANDO" && estadoPrev != "PAUSADO" &&
        estadoPrev != "INTERRUMPIDO") {
        return;
    }
    if (!curvaActual.recibida || curvaActual.numSegmentos <= 0) {
        // No hay programa cargado para reanudar; descartamos el run_state.
        Serial.println("[RunState] sin programa cargado: ignorando run_state");
        clearRunState();
        return;
    }
    if (segIdx < 0 || segIdx >= curvaActual.numSegmentos) {
        Serial.println("[RunState] segIdx fuera de rango: ignorando");
        clearRunState();
        return;
    }
    if (phase != "RAMPA" && phase != "REMOJO") {
        Serial.println("[RunState] phase inválida: ignorando");
        clearRunState();
        return;
    }

    runCurrentSegment = segIdx;
    runCurrentPhase = phase;
    runSoakAccumMs = soakMs;
    // No restaurar temperaturaActual desde NVS: la termocupla es la fuente
    // de verdad (evita mostrar un valor simulado obsoleto tras un corte).
    resumeFromSavedState = true;
    estadoHorno = "INTERRUMPIDO";
    // Solo reescribir NVS si venimos de CALENTANDO/PAUSADO (evitar wear y
    // trabajo extra cuando ya estábamos en INTERRUMPIDO).
    if (estadoPrev != "INTERRUMPIDO") {
        saveRunState("INTERRUMPIDO", segIdx, phase, soakMs, tempPrev);
    }
    // Forzar publicación a Firebase apenas STA conecte (sobrescribe CALENTANDO viejo).
    needsBootRealtimePublish = true;
    Serial.printf("[RunState] *** Cocción interrumpida detectada (seg %d, %s) → estadoHorno=INTERRUMPIDO\n",
                  segIdx + 1, phase.c_str());
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
    
    // Primer intento “duro”: reset radio + espera acotada.
    // Sin router siempre llega al tope; 20 s basta (antes 30).
    // Reintentos en background: wifi_manager_requestStaReconnect() → WiFi.begin
    // sin WIFI_OFF (SoftAP se mantiene).
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
    wifi_manager_applyRadioDefaults();
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
    
    unsigned long startTime = millis();
    int dots = 0;
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 20000) {
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
        
        scheduleFirebaseInit("reconnectToWifi");
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
        
        wifi_manager_applyRadioDefaults();
        WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
        
        // Esperar conexión con timeout más largo (60 segundos)
        unsigned long startTime = millis();
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && millis() - startTime < 15000) {
            delay(500);
            Serial.print(".");
            attempts++;
            if (attempts % 20 == 0) {
                Serial.printf("\n[WiFi] Esperando... Estado: %d\n", WiFi.status());
            }
        }
        Serial.println();

        wifi_manager_ensureApRunning();

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
        Serial.println("[WiFi] Esperando configuración WiFi desde la app (HTTP)...");
        Serial.println("[WiFi] =========================================");
        wifiConnected = false;
    }
}

static volatile bool s_firebaseInitPending = false;
static volatile bool s_firebaseInitBusy = false;
static const char* s_firebaseInitReason = nullptr;
/** Tras fallo de init (sin DNS/internet): no volver a tumbar SoftAP un rato. */
static unsigned long s_lastFirebaseInitFailMs = 0;
static const uint32_t FIREBASE_INIT_FAIL_BACKOFF_MS = 120000;
static volatile bool pendingFirebaseReport = false;
static bool s_firebaseBootstrapBusy = false;
static unsigned long s_lastFirebaseBootstrapMs = 0;
static unsigned long s_firebaseConnectMs = 0;
static const uint32_t FIREBASE_BOOTSTRAP_MIN_INTERVAL_MS = 3000;
static const uint32_t FIREBASE_POST_CONNECT_DELAY_MS = 5000;

static bool kiln_staLinkUp() {
    return WiFi.status() == WL_CONNECTED && WiFi.localIP()[0] != 0;
}

static bool firebaseRtdbOkAfterWrite();

static void stashPendingRealtimeStatus(const char* status) {
    if (status == nullptr || status[0] == '\0') {
        return;
    }
    strncpy(s_pendingRealtimeStatus, status, sizeof(s_pendingRealtimeStatus) - 1);
    s_pendingRealtimeStatus[sizeof(s_pendingRealtimeStatus) - 1] = '\0';
}

/**
 * Solo status+timestamp. Nunca con SoftAP “Local activo”: TLS concurrente con
 * HTTP SoftAP → BearSSL LoadProhibited (log PAUSE tras Local→Online).
 */
static bool flushPendingRealtimeStatusWrite(const char* status) {
    if (status == nullptr || status[0] == '\0') {
        return false;
    }
    if (offlineOnly) {
        return false;
    }
    if (wifi_manager_isLocalAppActive()) {
        return false;
    }
    if (!wifi_manager_isFirebaseSafe() || !kiln_staLinkUp()) {
        return false;
    }
    if (!wifiConnected || !firebaseConnected || !Firebase.ready() || fbdo == nullptr) {
        return false;
    }

    String localUserId;
    String localMac;
    if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        localUserId = userId;
        localMac = deviceMacAddress;
        xSemaphoreGive(xMutex);
    } else {
        return false;
    }
    if (localUserId.length() == 0 || localMac.length() == 0) {
        return false;
    }

    String path = "/users/" + localUserId + "/kilns/" + localMac + "/realtimeData";
    fbdo->setResponseSize(256);
    Firebase.RTDB.setwriteSizeLimit(fbdo, "tiny");
    const bool okStatus = Firebase.RTDB.setString(fbdo, (path + "/status").c_str(), status);
    const bool okTs = Firebase.RTDB.setTimestamp(fbdo, (path + "/timestamp").c_str());
    if (okStatus && okTs && firebaseRtdbOkAfterWrite()) {
        wifi_manager_noteFirebaseOk();
        return true;
    }
    return false;
}

static void firebaseBootstrapTelemetry() {
    if (s_firebaseBootstrapBusy) {
        return;
    }
    if (millis() - s_lastFirebaseBootstrapMs < FIREBASE_BOOTSTRAP_MIN_INTERVAL_MS) {
        return;
    }
    if (s_firebaseConnectMs > 0 &&
        millis() - s_firebaseConnectMs < FIREBASE_POST_CONNECT_DELAY_MS) {
        return;
    }
    if (!wifi_manager_isFirebaseSafe() || !kiln_staLinkUp() || wifi_manager_isLocalAppActive()) {
        return;
    }
    if (!firebaseConnected || !Firebase.ready()) {
        return;
    }

    s_firebaseBootstrapBusy = true;
    s_lastFirebaseBootstrapMs = millis();
    wifiConnected = true;

    float tempCopy = 0.0f;
    String estadoCopy = "DETENIDO";
    String uidCopy;
    if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        tempCopy = temperaturaActual;
        estadoCopy = estadoHorno;
        uidCopy = userId;
        xSemaphoreGive(xMutex);
    }
    if (uidCopy == "" || deviceMacAddress == "") {
        Serial.println("[Firebase] Sin userId/MAC — esperando kiln-info");
        s_firebaseBootstrapBusy = false;
        return;
    }
    Serial.println("[Firebase] Bootstrap tras conexion STA...");
    reportToFirebase(tempCopy, estadoCopy);
    s_firebaseBootstrapBusy = false;
}

static void requestFirebaseBootstrap() {
    pendingFirebaseReport = true;
}

static void runPendingFirebaseReport() {
    if (!pendingFirebaseReport) {
        return;
    }
    if (s_firebaseInitPending || s_firebaseInitBusy) {
        return;
    }
    if (!wifi_manager_isFirebaseSafe() || !kiln_staLinkUp() || wifi_manager_isLocalAppActive()) {
        return;
    }
    if (!firebaseConnected || !Firebase.ready()) {
        return;
    }
    if (s_firebaseConnectMs > 0 &&
        millis() - s_firebaseConnectMs < FIREBASE_POST_CONNECT_DELAY_MS) {
        return;
    }

    float tempCopy = 0.0f;
    String estadoCopy = "DETENIDO";
    if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        tempCopy = temperaturaActual;
        estadoCopy = estadoHorno;
        xSemaphoreGive(xMutex);
    }
    pendingFirebaseReport = false;
    Serial.println("[Firebase] Sync pendiente...");
    reportToFirebase(tempCopy, estadoCopy);
}

static void runPendingFirebaseInit() {
    if (offlineOnly) {
        s_firebaseInitPending = false;
        return;
    }
    if (!s_firebaseInitPending || s_firebaseInitBusy || firebaseConnected) {
        return;
    }
    if (!wifi_manager_isFirebaseSafe() || !kiln_staLinkUp() || wifi_manager_isLocalAppActive()) {
        return;
    }
    // Sin nube usable: no tumbar SoftAP cada pocos segundos (taller offline).
    if (s_lastFirebaseInitFailMs != 0 &&
        millis() - s_lastFirebaseInitFailMs < FIREBASE_INIT_FAIL_BACKOFF_MS) {
        s_firebaseInitPending = false;
        return;
    }

    s_firebaseInitPending = false;
    s_firebaseInitBusy = true;
    Serial.printf("[Firebase] Init en TaskCom (%s), heap: %u\n",
                  s_firebaseInitReason ? s_firebaseInitReason : "?",
                  (unsigned)ESP.getFreeHeap());

    wifi_manager_ensurePublicDns();

    // Probe DNS SIN pausar SoftAP. Si no hay internet, SoftAP debe seguir
    // disponible para la app (cargar programa / comandos locales).
    IPAddress resolved;
    const bool dnsOk = WiFi.hostByName("identitytoolkit.googleapis.com", resolved);
    if (!dnsOk) {
        Serial.println("[Firebase] Sin DNS — SoftAP prioritario, init aplazado 120s");
        s_lastFirebaseInitFailMs = millis();
        s_firebaseInitBusy = false;
        return;
    }

    // SoftAP siempre vivo: intentar Firebase en AP+STA (DNS públicos ya fijados).
    wifi_manager_ensurePublicDns();
    const bool ok = initFirebase();

    s_firebaseInitBusy = false;
    if (ok) {
        s_lastFirebaseInitFailMs = 0;
        wifiConnected = true;
        requestFirebaseBootstrap();
    } else {
        s_lastFirebaseInitFailMs = millis();
        Serial.println("[Firebase] Init falló — backoff 120s (SoftAP intacto)");
    }
}

void kiln_notifyLocalSessionComplete() {
    needsImmediateFirebaseSync = true;
    Serial.println("[Firebase] Sync urgente tras sesion HTTP local");
}

void kiln_tryPublishStatusNow(const char* status) {
    if (status == nullptr || strlen(status) == 0) {
        return;
    }
    stashPendingRealtimeStatus(status);

    if (offlineOnly) {
        Serial.println("[Firebase] status omitido (offlineOnly)");
        return;
    }

    // SoftAP HTTP + TLS Firebase a la vez = panic LoadProhibited. Solo stash;
    // processImmediateFirebaseSync flushea cuando Local se aquiete.
    if (wifi_manager_isLocalAppActive()) {
        needsImmediateFirebaseSync = true;
        Serial.printf(
            "[Firebase] status '%s' pendiente (Local SoftAP activo — sin TLS)\n",
            status
        );
        return;
    }

    if (flushPendingRealtimeStatusWrite(status)) {
        s_pendingRealtimeStatus[0] = '\0';
        Serial.printf("[Firebase] status publicado ya: %s\n", status);
        return;
    }

    needsImmediateFirebaseSync = true;
    Serial.printf(
        "[Firebase] status '%s' pendiente (sin STA/Firebase)\n",
        status
    );
}

void kiln_onApRadioTransitionBegin() {
    pendingFirebaseReport = false;
    s_firebaseBootstrapBusy = false;
    firebaseConnected = false;
    // No basta con cerrar el socket: el contexto BearSSL queda sucio y la
    // siguiente operacion se cuelga en br_ssl_engine_flush() (visto en abort
    // backtraces). Reciclamos el FirebaseData entero para garantizar una
    // sesion TLS fresca.
    if (fbdo != nullptr) {
        fbdo->stopWiFiClient();
        delete fbdo;
        fbdo = new FirebaseData();
    }
    Serial.println("[Firebase] FirebaseData reciclado por transicion AP/STA");
}

/** Tras error SSL/write: no reutilizar el mismo BearSSL (LoadProhibited). */
static void kiln_recycleFirebaseAfterTlsFault(const char* why) {
    Serial.printf("[Firebase] TLS fault — reciclaje (%s)\n", why ? why : "?");
    kiln_onApRadioTransitionBegin();
}

void processImmediateFirebaseSync() {
    // Status + bootstrap: solo sin SoftAP Local (evita TLS concurrente).
    if (wifi_manager_isLocalAppActive()) {
        return;
    }
    if (s_pendingRealtimeStatus[0] != '\0') {
        if (flushPendingRealtimeStatusWrite(s_pendingRealtimeStatus)) {
            Serial.printf("[Firebase] status pendiente flushed: %s\n",
                          s_pendingRealtimeStatus);
            s_pendingRealtimeStatus[0] = '\0';
        }
    }

    if (!needsImmediateFirebaseSync) {
        return;
    }
    if (!wifi_manager_isFirebaseSafe()) {
        return;
    }
    if (!kiln_staLinkUp()) {
        wifiConnected = false;
        wifi_manager_requestStaReconnect();
        return;
    }
    needsImmediateFirebaseSync = false;
    wifiConnected = true;
    if (!firebaseConnected || !Firebase.ready()) {
        scheduleFirebaseInit("post-local-session");
        return;
    }
    requestFirebaseBootstrap();
}

void scheduleFirebaseInit(const char* reason) {
    if (offlineOnly) {
        return;
    }
    if (firebaseConnected || s_firebaseInitPending || s_firebaseInitBusy) {
        return;
    }
    if (!wifi_manager_isFirebaseSafe() || !kiln_staLinkUp() || wifi_manager_isLocalAppActive()) {
        return;
    }
    s_firebaseInitReason = reason;
    s_firebaseInitPending = true;
    Serial.printf("[Firebase] Programando init (%s), heap libre: %u\n",
                  reason ? reason : "?", (unsigned)ESP.getFreeHeap());
}

bool initFirebase() {
    Serial.println("[Firebase] =========================================");
    Serial.println("[Firebase] Iniciando conexión a Firebase...");
    Serial.printf("[Firebase] Database URL: %s\n", FIREBASE_DATABASE_URL);
    Serial.printf("[Firebase] WiFi status: %d\n", WiFi.status());
    Serial.printf("[Firebase] IP: %s DNS: %s\n",
                  WiFi.localIP().toString().c_str(),
                  WiFi.dnsIP().toString().c_str());
    Serial.printf("[Firebase] Heap libre: %u (min: %u maxAlloc: %u)\n",
                  (unsigned)ESP.getFreeHeap(),
                  (unsigned)ESP.getMinFreeHeap(),
                  (unsigned)ESP.getMaxAllocHeap());

    if (config == nullptr || auth == nullptr || fbdo == nullptr) {
        Serial.println("[Firebase] ❌ ERROR: Objetos Firebase no inicializados");
        return false;
    }

    wifi_manager_ensurePublicDns();

    // Probar DNS con SoftAP vivo (Paso 1 Online: no pauseAp / no STA-only).
    {
        IPAddress resolved;
        bool dnsOk = WiFi.hostByName("identitytoolkit.googleapis.com", resolved);
        if (!dnsOk) {
            wifi_manager_ensurePublicDns();
            delay(200);
            dnsOk = WiFi.hostByName("identitytoolkit.googleapis.com", resolved);
        }
        if (!dnsOk) {
            IPAddress googleDns;
            const bool gOk = WiFi.hostByName("dns.google", googleDns);
            Serial.printf("[Firebase] ❌ DNS identitytoolkit falló (dns.google=%s) — SoftAP intacto\n",
                          gOk ? googleDns.toString().c_str() : "FAIL");
            firebaseConnected = false;
            return false;
        }
        Serial.printf("[Firebase] DNS OK identitytoolkit → %s\n", resolved.toString().c_str());
    }

    config->api_key = FIREBASE_API_KEY;
    config->database_url = FIREBASE_DATABASE_URL;
    // 3 s era demasiado corto para DNS+TLS en AP+STA.
    config->timeout.serverResponse = 15000;
    config->timeout.socketConnection = 15000;
    config->timeout.rtdbKeepAlive = 30000;
    config->timeout.rtdbStreamReconnect = 1000;

    fbdo->setResponseSize(1024);
    // 512/512 provoca "Failed to initialize the SSL layer" con poco heap.
    const uint32_t heapNow = ESP.getFreeHeap();
    if (heapNow >= 70000 && ESP.getMaxAllocHeap() >= 12000) {
        fbdo->setBSSLBufferSize(2048, 1024);
    } else if (heapNow >= 50000) {
        fbdo->setBSSLBufferSize(1024, 512);
    } else {
        fbdo->setBSSLBufferSize(768, 512);
    }

    Serial.println("[Firebase] Intentando signUp anónimo...");
    if (Firebase.signUp(config, auth, "", "")) {
        Serial.println("[Firebase] signUp exitoso, iniciando Firebase.begin...");
        Firebase.begin(config, auth);
        Firebase.reconnectWiFi(true);
        firebaseConnected = true;
        s_firebaseConnectMs = millis();
        Serial.println("[Firebase] =========================================");
        Serial.println("[Firebase] ✅ CONECTADO CORRECTAMENTE");
        Serial.println("[Firebase] =========================================");
        return true;
    }

    firebaseConnected = false;
    Serial.println("[Firebase] =========================================");
    Serial.println("[Firebase] ❌ ERROR EN CONEXIÓN");
    Serial.printf("[Firebase] Error: %s\n", fbdo->errorReason().c_str());
    Serial.printf("[Firebase] Error code: %d\n", fbdo->errorCode());
    if (config->signer.signupError.message.length() > 0) {
        Serial.printf("[Firebase] signupError: %s\n",
                      config->signer.signupError.message.c_str());
    }
    Serial.println("[Firebase] =========================================");
    return false;
}

static bool firebaseRtdbOkAfterWrite() {
    if (fbdo == nullptr || !Firebase.ready()) {
        firebaseConnected = false;
        return false;
    }
    const String err = fbdo->errorReason();
    if (err.length() == 0) {
        return true;
    }
    if (err.indexOf("SSL") >= 0 || err.indexOf("write") >= 0 || err.indexOf("Write") >= 0 ||
        err.indexOf("connection") >= 0 || err.indexOf("connected") >= 0) {
        firebaseErrorCount++;
        Serial.printf("[Firebase] ❌ RTDB: %s\n", err.c_str());
        kiln_recycleFirebaseAfterTlsFault(err.c_str());
        return false;
    }
    return true;
}

void kiln_scheduleProfileStatePublish() {
    needsProfileStatePublish = true;
}

void kiln_onProfileLoaded(unsigned long programUpdatedAtFromApp) {
    profileUpdatedAtMs = programUpdatedAtFromApp;
    if (profileUpdatedAtMs == 0UL) {
        profileUpdatedAtMs = millis();
    }
    kiln_scheduleProfileStatePublish();
}

bool publishProfileStateToFirebase() {
    if (wifi_manager_isLocalAppActive()) {
        return false;
    }
    if (!wifi_manager_isFirebaseSafe() || !kiln_staLinkUp()) {
        return false;
    }
    if (s_firebaseConnectMs > 0 &&
        millis() - s_firebaseConnectMs < FIREBASE_POST_CONNECT_DELAY_MS) {
        return false;
    }
    if (!wifiConnected || !firebaseConnected || !Firebase.ready()) {
        return false;
    }
    if (fbdo == nullptr) {
        return false;
    }

    String localUserId;
    String localDeviceMacAddress;
    String localProgramName;
    bool profileLoaded = false;
    unsigned long localUpdatedAt = 0;

    if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return false;
    }
    localUserId = userId;
    localDeviceMacAddress = deviceMacAddress;
    profileLoaded = curvaActual.recibida;
    if (profileLoaded && strlen(curvaActual.nombre) > 0) {
        localProgramName = String(curvaActual.nombre);
    }
    localUpdatedAt = profileUpdatedAtMs;
    xSemaphoreGive(xMutex);

    if (localUserId == "" || localDeviceMacAddress == "") {
        return false;
    }

    fbdo->setResponseSize(256);
    Firebase.RTDB.setwriteSizeLimit(fbdo, "tiny");
    fbdo->setBSSLBufferSize(512, 512);

    const String basePath = "/users/" + localUserId + "/kilns/" + localDeviceMacAddress + "/profileState";
    if (!profileLoaded) {
        Firebase.RTDB.setString(fbdo, (basePath + "/programName").c_str(), "");
        if (!firebaseRtdbOkAfterWrite()) {
            return false;
        }
        Firebase.RTDB.setDouble(fbdo, (basePath + "/programUpdatedAt").c_str(), 0.0);
        if (!firebaseRtdbOkAfterWrite()) {
            return false;
        }
    } else {
        Firebase.RTDB.setString(fbdo, (basePath + "/programName").c_str(), localProgramName);
        if (!firebaseRtdbOkAfterWrite()) {
            return false;
        }
        Firebase.RTDB.setDouble(fbdo, (basePath + "/programUpdatedAt").c_str(), (double)localUpdatedAt);
        if (!firebaseRtdbOkAfterWrite()) {
            return false;
        }
    }

    if (!Firebase.RTDB.setTimestamp(fbdo, (basePath + "/loadedAt").c_str())) {
        return false;
    }
    if (!firebaseRtdbOkAfterWrite()) {
        return false;
    }

    Serial.printf("[Firebase] profileState publicado: '%s' updatedAt=%lu\n",
                  localProgramName.c_str(), localUpdatedAt);
    return true;
}

void reportToFirebase(float temp, const String& estado) {
    if (wifi_manager_isLocalAppActive()) {
        return;
    }
    if (!wifi_manager_isFirebaseSafe() || !kiln_staLinkUp()) {
        return;
    }
    if (s_firebaseConnectMs > 0 &&
        millis() - s_firebaseConnectMs < FIREBASE_POST_CONNECT_DELAY_MS) {
        return;
    }
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

    if (fbdo == nullptr) {
        return;
    }
    fbdo->setResponseSize(256);
    Firebase.RTDB.setwriteSizeLimit(fbdo, "tiny");
    fbdo->setBSSLBufferSize(512, 512);
    
    // ========== REALTIME DATA ==========
    String realtimePath = "/users/" + localUserId + "/kilns/" + localDeviceMacAddress + "/realtimeData";

    // deviceInfo.type una vez tras conectar (hornoss viejos sin el campo)
    static bool s_deviceTypePublished = false;
    static String s_deviceTypeMac;
    if (!s_deviceTypePublished || s_deviceTypeMac != localDeviceMacAddress) {
        String typePath = "/users/" + localUserId + "/kilns/" + localDeviceMacAddress +
                          "/deviceInfo/type";
        if (Firebase.RTDB.setString(fbdo, typePath.c_str(), "KILN")) {
            s_deviceTypePublished = true;
            s_deviceTypeMac = localDeviceMacAddress;
        }
    }

    // Usar método directo para cada campo (menos stack que FirebaseJson)
    Firebase.RTDB.setFloat(fbdo, (realtimePath + "/temperature").c_str(), temp);
    if (!firebaseRtdbOkAfterWrite()) {
        return;
    }
    Firebase.RTDB.setString(fbdo, (realtimePath + "/status").c_str(), estado);
    if (!firebaseRtdbOkAfterWrite()) {
        return;
    }
    Firebase.RTDB.setString(fbdo, (realtimePath + "/type").c_str(), "KILN");
    if (!firebaseRtdbOkAfterWrite()) {
        return;
    }
    
    const bool activeRun = (estado == "CALENTANDO" || estado == "PAUSADO");
    if (!activeRun) {
        // IDLE / INTERRUMPIDO / DETENIDO / FINALIZADO: limpiar nombre y tiempos
        // en RTDB para que la app no infiera "Horneando" por datos obsoletos.
        Firebase.RTDB.setString(fbdo, (realtimePath + "/programName").c_str(), "");
        if (!firebaseRtdbOkAfterWrite()) {
            return;
        }
        Firebase.RTDB.setInt(fbdo, (realtimePath + "/programStartTime").c_str(), 0);
        if (!firebaseRtdbOkAfterWrite()) {
            return;
        }
        Firebase.RTDB.setInt(fbdo, (realtimePath + "/programTotalDuration").c_str(), 0);
        if (!firebaseRtdbOkAfterWrite()) {
            return;
        }
        Firebase.RTDB.setInt(fbdo, (realtimePath + "/programPauseTime").c_str(), 0);
        if (!firebaseRtdbOkAfterWrite()) {
            return;
        }
        Firebase.RTDB.setInt(fbdo, (realtimePath + "/elapsedMinutes").c_str(), 0);
        if (!firebaseRtdbOkAfterWrite()) {
            return;
        }
        Firebase.RTDB.setInt(fbdo, (realtimePath + "/remainingMinutes").c_str(), 0);
        if (!firebaseRtdbOkAfterWrite()) {
            return;
        }
    } else if (localProgramName != "") {
        Firebase.RTDB.setString(fbdo, (realtimePath + "/programName").c_str(), localProgramName);
        if (!firebaseRtdbOkAfterWrite()) {
            return;
        }
        if (estado == "CALENTANDO" || estado == "PAUSADO") {
            if (localProgramStartTime > 0) {
                // Fuente de verdad para la app: minutos reales del ESP (no millis/1000
                // disfrazado de epoch — eso hacía que el % en la app saltara a 100%).
                unsigned long elapsedMs = millis() - localProgramStartTime;
                if (estado == "PAUSADO" && localProgramPauseTime > 0 &&
                    localProgramPauseTime >= localProgramStartTime) {
                    elapsedMs = localProgramPauseTime - localProgramStartTime;
                }
                const int elapsedMin = (int)(elapsedMs / 60000UL);
                int remainingMin = localProgramTotalDuration - elapsedMin;
                if (remainingMin < 0) remainingMin = 0;
                Firebase.RTDB.setInt(fbdo, (realtimePath + "/elapsedMinutes").c_str(),
                                    elapsedMin);
                if (!firebaseRtdbOkAfterWrite()) {
                    return;
                }
                Firebase.RTDB.setInt(fbdo, (realtimePath + "/remainingMinutes").c_str(),
                                    remainingMin);
                if (!firebaseRtdbOkAfterWrite()) {
                    return;
                }

                // Epoch Unix real si NTP está listo; si no, 0 (la app usa elapsedMinutes).
                time_t nowEpoch = time(nullptr);
                if (nowEpoch > 1600000000L) {
                    const unsigned long runElapsedSec = elapsedMs / 1000UL;
                    const unsigned long startEpoch =
                        (unsigned long)nowEpoch > runElapsedSec
                            ? (unsigned long)nowEpoch - runElapsedSec
                            : 0UL;
                    if (startEpoch > 1600000000UL) {
                        Firebase.RTDB.setInt(fbdo,
                            (realtimePath + "/programStartTime").c_str(),
                            (int)startEpoch);
                        if (!firebaseRtdbOkAfterWrite()) {
                            return;
                        }
                    }
                }
                if (localProgramTotalDuration > 0) {
                    Firebase.RTDB.setInt(fbdo, (realtimePath + "/programTotalDuration").c_str(),
                                        localProgramTotalDuration);
                    if (!firebaseRtdbOkAfterWrite()) {
                        return;
                    }
                }
                if (estado == "PAUSADO" && localProgramPauseTime > 0 &&
                    nowEpoch > 1600000000L) {
                    Firebase.RTDB.setInt(fbdo, (realtimePath + "/programPauseTime").c_str(),
                                        (int)nowEpoch);
                    if (!firebaseRtdbOkAfterWrite()) {
                        return;
                    }
                }
            }
        }
    }

    // Usar Firebase Server Timestamp
    const bool success = Firebase.RTDB.setTimestamp(fbdo, (realtimePath + "/timestamp").c_str());
    
    if (success && firebaseRtdbOkAfterWrite()) {
        wifi_manager_noteFirebaseOk();
        if (localProgramName != "") {
            Serial.printf("[Firebase] ✅ %.1f°C - Programa: %s\n", temp, localProgramName.c_str());
        } else {
            Serial.printf("[Firebase] ✅ %.1f°C\n", temp);
        }
        firebaseErrorCount = 0;  // Resetear contador de errores

        // Historial de cocción (recordings + activity) — patrón DataLogger
        kilnFiringOnReport(
            fbdo,
            localUserId,
            localDeviceMacAddress,
            estado,
            temp,
            localProgramStartTime
        );
    } else {
        firebaseErrorCount++;
        if (fbdo == nullptr) {
            return;
        }
        const String failReason = fbdo->errorReason();
        Serial.printf("[Firebase] ❌ %s (errores: %d)\n", failReason.c_str(), firebaseErrorCount);
        
        // Si es error de conexión o timeout, reciclar TLS (no reusar BearSSL sucio)
        if (failReason.indexOf("timed out") >= 0 || 
            failReason.indexOf("connection") >= 0 ||
            failReason.indexOf("SSL") >= 0 ||
            failReason.indexOf("write") >= 0) {
            kiln_recycleFirebaseAfterTlsFault(failReason.c_str());
            Serial.println("[Firebase] Desactivado - reconectará cuando Local aquiete");
        }
    }
}

// Crea deviceInfo Y realtimeData cuando se recibe kiln-info por HTTP
bool updateDeviceInfoFirebase() {
    if (wifi_manager_isLocalAppActive()) {
        return false;
    }
    if (!wifi_manager_isFirebaseSafe() || !wifiConnected || !firebaseConnected) {
        return false;
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
        return false;
    }
    
    if (localUserId == "" || localDeviceMacAddress == "") {
        return false;
    }
    
    if (fbdo == nullptr) {
        return false;
    }
    fbdo->setResponseSize(256);
    Firebase.RTDB.setwriteSizeLimit(fbdo, "tiny");
    fbdo->setBSSLBufferSize(512, 512);
    
    // ========== DEVICE INFO ==========
    String deviceInfoPath = "/users/" + localUserId + "/kilns/" + localDeviceMacAddress + "/deviceInfo";
    
    // Usar método directo para cada campo (menos stack)
    Firebase.RTDB.setString(fbdo, (deviceInfoPath + "/name").c_str(), localKilnName);
    Firebase.RTDB.setString(fbdo, (deviceInfoPath + "/model").c_str(), localKilnModel);
    Firebase.RTDB.setInt(fbdo, (deviceInfoPath + "/volume").c_str(), (int)localKilnVolume);
    Firebase.RTDB.setString(fbdo, (deviceInfoPath + "/type").c_str(), "KILN");
    bool success1 = Firebase.RTDB.setString(fbdo, (deviceInfoPath + "/macAddress").c_str(), localDeviceMacAddress);
    
    // ========== REALTIME DATA (inicial) ==========
    String realtimePath = "/users/" + localUserId + "/kilns/" + localDeviceMacAddress + "/realtimeData";
    
    // Usar método directo para cada campo (menos stack)
    Firebase.RTDB.setFloat(fbdo, (realtimePath + "/temperature").c_str(), localTemp);
    Firebase.RTDB.setString(fbdo, (realtimePath + "/status").c_str(), localEstado);

    // Solo publicar nombre de programa si la cocción está activa (no INTERRUMPIDO/IDLE).
    if (localEstado == "CALENTANDO" || localEstado == "PAUSADO") {
        if (localProgramName != "") {
            Firebase.RTDB.setString(fbdo, (realtimePath + "/programName").c_str(), localProgramName);
        }
    } else {
        Firebase.RTDB.setString(fbdo, (realtimePath + "/programName").c_str(), "");
        Firebase.RTDB.setInt(fbdo, (realtimePath + "/programStartTime").c_str(), 0);
        Firebase.RTDB.setInt(fbdo, (realtimePath + "/programTotalDuration").c_str(), 0);
    }

    // Usar Firebase Server Timestamp
    bool success2 = Firebase.RTDB.setTimestamp(fbdo, (realtimePath + "/timestamp").c_str());
    
    if (success1 && success2 && firebaseRtdbOkAfterWrite()) {
        wifi_manager_noteFirebaseOk();
        Serial.println("[Firebase] ✅ deviceInfo + realtimeData creados");
        return true;
    }
    Serial.println("[Firebase] ❌ Error en creación inicial");
    const String initErr = (fbdo != nullptr) ? fbdo->errorReason() : String("null-fbdo");
    if (fbdo == nullptr || initErr.indexOf("connection") >= 0 ||
        initErr.indexOf("SSL") >= 0 || initErr.indexOf("write") >= 0) {
        kiln_recycleFirebaseAfterTlsFault(initErr.c_str());
    }
    return false;
}

void onStaGainedConnection() {
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }
    wifi_manager_ensurePublicDns();
    if (wifi_manager_isLocalAppActive()) {
        Serial.println("[Firebase] STA lista pero Local activo — telemetria diferida");
        return;
    }
    wifiConnected = true;

    if (!firebaseConnected || !Firebase.ready()) {
        scheduleFirebaseInit("onStaGainedConnection");
        return;
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
    
    unsigned long lastFirebaseUpdate = millis();  // Inicializar con millis() actual
    unsigned long lastAdvertisingCheck = 0;
    unsigned long lastFirebaseReconnect = 0;
    unsigned long lastWiFiCheck = 0;
    static unsigned int loopCounter = 0;

    static unsigned long lastApCheck = 0;
    static bool staWasConnected = false;

    for (;;) {
        unsigned long now = millis();
        wifi_manager_tick();
        wifi_manager_prioritizeApClients();
        processImmediateFirebaseSync();
        runPendingFirebaseInit();
        runPendingFirebaseReport();
        // NO tocar lastFirebaseUpdate aquí: si se pone a millis() en cada
        // iteración, (now - lastFirebaseUpdate) hace underflow y duePeriodic
        // queda siempre true → reportToFirebase ~cada 100 ms → TLS/stack abort.

        if (now - lastApCheck >= 30000) {
            wifi_manager_ensureApRunning();
            lastApCheck = now;
        }
        loopCounter++;
        
        // Log cada 50 iteraciones (~5 segundos con delay de 100ms)
        if (loopCounter % 50 == 0) {
            Serial.printf("[TaskCom] 🔄 Loop activo (iteración %u, tiempo: %lu ms)\n", loopCounter, now);
        }
        
        if (now - lastWiFiCheck >= 5000) {
            // STA suave: WiFi.begin sin WIFI_OFF / sin tumbar SoftAP.
            // Intervalo base 60 s; si Firebase OK pero sin router, backoff 120 s.
            static unsigned long lastStaReconnectAttempt = 0;
            static uint8_t staMissStreak = 0;
            const bool localActive = wifi_manager_isLocalAppActive();
            const bool allowStaHunt = !offlineOnly;
            const uint32_t staHuntIntervalMs =
                (staMissStreak >= 2) ? 120000UL : 60000UL;
            if (!localActive && !kiln_staLinkUp() &&
                wifi_manager_isFirebaseSafe() && wifiConfigReceived && wifiSSID.length() > 0 &&
                allowStaHunt &&
                (now - lastStaReconnectAttempt >= staHuntIntervalMs)) {
                lastStaReconnectAttempt = now;
                if (staMissStreak < 250) {
                    staMissStreak++;
                }
                Serial.printf("[WiFi] STA suave (SoftAP vivo) → %s (racha=%u, cada %lu s)\n",
                              wifiSSID.c_str(), (unsigned)staMissStreak,
                              (unsigned long)(staHuntIntervalMs / 1000));
                wifi_manager_requestStaReconnect();
            } else if (offlineOnly && (now - lastStaReconnectAttempt >= 60000)) {
                lastStaReconnectAttempt = now;
                staMissStreak = 0;
                Serial.println("[WiFi] Solo local — SoftAP sticky (sin STA/Firebase)");
                wifi_manager_ensureApRunning();
            } else if (!allowStaHunt && !localActive && wifiConfigReceived &&
                       (now - lastStaReconnectAttempt >= 60000)) {
                lastStaReconnectAttempt = now;
                staMissStreak = 0;
                Serial.println("[WiFi] STA en espera — SoftAP prioritario (modo Solo local)");
                wifi_manager_ensureApRunning();
            } else if (localActive && (now - lastStaReconnectAttempt >= 60000)) {
                lastStaReconnectAttempt = now;
                Serial.println("[WiFi] STA en espera — app Local activa (HTTP/SoftAP)");
                wifi_manager_ensureApRunning();
            }
            const bool staNow = kiln_staLinkUp();
            if (offlineOnly) {
                // Solo local: no pintar "internet" aunque quede STA residual.
                if (staNow) {
                    wifi_manager_disconnectStaKeepAp();
                }
                wifiConnected = false;
                firebaseConnected = false;
                staMissStreak = 0;
            } else if (staNow) {
                staMissStreak = 0;
                if (!wifiConnected) {
                    wifiConnected = true;
                    Serial.printf("[WiFi] STA conectada. IP: %s\n", WiFi.localIP().toString().c_str());
                }
                if (!staWasConnected) {
                    onStaGainedConnection();
                }
            } else {
                wifiConnected = false;
            }
            staWasConnected = offlineOnly ? false : staNow;

            lastWiFiCheck = now;
        }
        
        if (!firebaseConnected && !offlineOnly && kiln_staLinkUp() &&
            (now - lastFirebaseReconnect >= 30000)) {
            wifiConnected = true;
            scheduleFirebaseInit("reconnect-periodic");
            lastFirebaseReconnect = now;
        }


        if (now - lastAdvertisingCheck >= 10000) {
            if (statusLogsEnabled) {
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
                Serial.println("=================================");
                Serial.println("[Firebase] 🔥 ESTADO DE FIREBASE:");
                Serial.printf("[Firebase] firebaseConnected: %s\n", firebaseConnected ? "SÍ" : "NO");
                Serial.println("=================================");
            }
            lastAdvertisingCheck = now;
        }

        // --- deviceInfo + realtimeData tras kiln-info (cuando STA/Firebase listos) ---
        // Nunca con SoftAP Local: el crash del log fue deviceInfo/PAUSE + TLS sucio.
        if (needsDeviceInfoUpdate && wifiConnected && firebaseConnected &&
            !wifi_manager_isLocalAppActive() && wifi_manager_isFirebaseSafe()) {
            Serial.println("[Firebase] Flag detectada - sincronizando deviceInfo + realtimeData...");
            if (updateDeviceInfoFirebase()) {
                needsDeviceInfoUpdate = false;
                Serial.println("[Firebase] Sincronizacion inicial OK");
            }
        }

        if (needsProfileStatePublish && wifiConnected && firebaseConnected &&
            !wifi_manager_isLocalAppActive() && wifi_manager_isFirebaseSafe()) {
            if (publishProfileStateToFirebase()) {
                needsProfileStatePublish = false;
            }
        }

        // --- Guardado diferido (HTTP kiln-info / wifi) ---
        if (pendingKilnInfoSave) {
            pendingKilnInfoSave = false;
            Serial.println("[STORAGE] Guardando datos del horno (diferido)...");
            saveKilnInfo();
        }
        if (pendingWifiConfigSave) {
            pendingWifiConfigSave = false;
            Serial.println("[STORAGE] Guardando y recargando credenciales WiFi (diferido)...");
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
        
        // --- Firebase por STA (no mientras la app usa SoftAP: libera radio para HTTP) ---
        const bool staReady = kiln_staLinkUp();
        const bool localActive = wifi_manager_isLocalAppActive();
        if (!staReady) {
            wifiConnected = false;
        }
        if (staReady && !offlineOnly && !localActive && wifi_manager_isFirebaseSafe() &&
            !s_firebaseInitPending && !s_firebaseInitBusy &&
            (s_firebaseConnectMs == 0 ||
             millis() - s_firebaseConnectMs >= FIREBASE_POST_CONNECT_DELAY_MS)) {
            wifiConnected = true;
            if (!firebaseConnected) {
                scheduleFirebaseInit("sta-loop");
            }
            const bool firebaseReady = firebaseConnected && Firebase.ready();
            const bool duePeriodic =
                (now - lastFirebaseUpdate >= 5000);
            const bool dueBoot = needsBootRealtimePublish && firebaseReady;

            if (firebaseReady && (dueBoot || duePeriodic)) {
                float tempCopy = 0;
                String estadoCopy = "";
                String userIdCopy = "";

                if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    tempCopy = temperaturaActual;
                    estadoCopy = estadoHorno;
                    userIdCopy = userId;
                    xSemaphoreGive(xMutex);

                    if (userIdCopy != "" && deviceMacAddress != "") {
                        reportToFirebase(tempCopy, estadoCopy);
                        if (dueBoot) {
                            needsBootRealtimePublish = false;
                            Serial.printf("[Firebase] Boot sync: status=%s\n",
                                            estadoCopy.c_str());
                        }
                    }
                }
                lastFirebaseUpdate = now;
            }
        }

        // --- Libera CPU frecuentemente ---
        vTaskDelay(pdMS_TO_TICKS(100));  // Más delay para el WDT
    }
}

void TaskControlCurva(void* pvParameters) {
    for (;;) {
        // Espera curva recibida y horno en modo calentando.
        // INTERRUMPIDO / IDLE / PAUSADO: solo esperar (nunca arrancar solo).
        bool run = false;
        if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            run = (estadoHorno == "CALENTANDO" && curvaActual.recibida);
            xSemaphoreGive(xMutex);
        }
        if (!run) {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        // ¿Estamos arrancando una cocción nueva o reanudando una interrumpida?
        bool resume = resumeFromSavedState;
        int startSegmentIdx = 0;
        String startPhase = "RAMPA";
        unsigned long startSoakMs = 0;
        if (resume) {
            startSegmentIdx = (int)runCurrentSegment;
            startPhase = runCurrentPhase;
            startSoakMs = runSoakAccumMs;
            resumeFromSavedState = false;
            Serial.printf("[Curva] *** Reanudando desde segmento %d, fase %s, remojo acumulado %lu ms\n",
                          startSegmentIdx + 1, startPhase.c_str(), startSoakMs);
        } else {
            Serial.println("[Curva] Iniciando ejecución desde cero...");
        }

        // Obtener temperatura inicial y calcular duración total
        float initialTemp = 0.0;
        if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            initialTemp = temperaturaActual;
            xSemaphoreGive(xMutex);
        }
        
        // Calcular duración total usando función auxiliar
        int totalDurationMinutes = calculateProgramTotalDuration(initialTemp);
        
        // Guardar timestamp de inicio y duración total. En el caso de un
        // resume preservamos programStartTime si ya estaba seteado en el
        // boot anterior; si no, lo arrancamos ahora (el progreso "exacto"
        // de tiempo total se pierde con el corte, pero los tramos en sí
        // (segmentos, rampa hasta tempObjetivo y remojo restante) se
        // respetan, que es la garantía importante de la cocción).
        bool startedFresh = false;
        String snapMac, snapName, snapDevice;
        uint8_t snapN = 0;
        Segmento snapSeg[10];
        if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            if (!resume || programStartTime == 0) {
                programStartTime = millis();
                startedFresh = true;
                snapMac = deviceMacAddress;
                snapName = String(curvaActual.nombre);
                snapDevice = kilnName;
                snapN = curvaActual.numSegmentos;
                if (snapN > 10) snapN = 10;
                for (uint8_t i = 0; i < snapN; i++) {
                    snapSeg[i] = curvaActual.segmentos[i];
                }
            }
            programTotalDurationMinutes = totalDurationMinutes;
            programPauseTime = 0;
            xSemaphoreGive(xMutex);
        }
        if (startedFresh && snapN > 0) {
            kilnFiringBegin(snapMac, snapName, snapDevice, initialTemp, snapSeg, snapN);
        }
        
        Serial.printf("[Curva] Duración total calculada: %d minutos\n", totalDurationMinutes);

        // Persistir estado inicial (el run_state cubre tanto el primer
        // arranque como la reanudación: si se vuelve a cortar la luz,
        // recuperamos el segmento/fase actual del NVS).
        {
            String estadoCopy;
            float tempCopy = 0.0f;
            if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                estadoCopy = estadoHorno;
                tempCopy = temperaturaActual;
                xSemaphoreGive(xMutex);
            }
            runCurrentSegment = startSegmentIdx;
            runCurrentPhase = startPhase;
            runSoakAccumMs = startSoakMs;
            saveRunState(estadoCopy, startSegmentIdx, startPhase, startSoakMs, tempCopy);
        }

        bool aborted = false;
        for (int i = startSegmentIdx; i < curvaActual.numSegmentos && !aborted; i++) {
            Segmento seg = curvaActual.segmentos[i];
            Serial.printf("[Curva] Segmento %d / %d: T=%.1f, R=%.1f, M=%d\n",
                          i + 1, curvaActual.numSegmentos,
                          seg.tempObjetivo, seg.rampaCporMin, seg.tiempoRemojoMin);
            runCurrentSegment = i;

            // Si el resume nos dejó en REMOJO de este segmento, saltamos la
            // rampa (la temperatura objetivo ya estaba alcanzada antes).
            bool skipRamp = (i == startSegmentIdx && startPhase == "REMOJO");

            // --- Rampa ---
            if (!skipRamp) {
                runCurrentPhase = "RAMPA";

                float tempAtRampStart = 0.0f;
                if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                    tempAtRampStart = temperaturaActual;
                    xSemaphoreGive(xMutex);
                }

                // Persistir entrada a la rampa de este segmento.
                {
                    String estadoCopy;
                    if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                        estadoCopy = estadoHorno;
                        xSemaphoreGive(xMutex);
                    }
                    saveRunState(estadoCopy, i, "RAMPA", 0UL, tempAtRampStart);
                }

                // Esperar a que la termocupla alcance la temperatura objetivo.
                // IMPORTANTE: no escribir en temperaturaActual aquí — la actualiza
                // TaskControlHorno desde el MAX31855. Antes se simulaba la rampa
                // en temperaturaActual y la UI/Firebase oscilaban entre real y objetivo.
                // Releemos tempObjetivo desde curvaActual en cada ciclo para que un
                // POST /api/profile en caliente (edición desde la app) aplique al tramo actual.
                const float reachTolerance = 2.0f;

                while (true) {
                    String estadoLocal;
                    float targetTemp = 0.0f;
                    if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                        estadoLocal = estadoHorno;
                        if (i < curvaActual.numSegmentos) {
                            targetTemp = curvaActual.segmentos[i].tempObjetivo;
                        }
                        xSemaphoreGive(xMutex);
                    } else {
                        vTaskDelay(pdMS_TO_TICKS(100));
                        continue;
                    }

                    if (estadoLocal == "DETENIDO" || estadoLocal == "IDLE") {
                        aborted = true;
                        break;
                    }
                    if (estadoLocal == "PAUSADO") {
                        vTaskDelay(pdMS_TO_TICKS(500));
                        continue;
                    }
                    if (estadoLocal != "CALENTANDO") {
                        vTaskDelay(pdMS_TO_TICKS(500));
                        continue;
                    }

                    float tempSensor = 0.0f;
                    if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                        tempSensor = temperaturaActual;
                        xSemaphoreGive(xMutex);
                    }

                    if (targetTemp > 0.0f && tempSensor >= targetTemp - reachTolerance) {
                        Serial.printf("[Curva] Segmento %d: rampa OK (%.1f°C >= objetivo %.1f°C)\n",
                                      i + 1, tempSensor, targetTemp);
                        break;
                    }

                    vTaskDelay(pdMS_TO_TICKS(500));
                }
                if (aborted) break;
            } else {
                Serial.printf("[Curva] Saltando rampa del segmento %d (resume en REMOJO)\n", i + 1);
            }

            // --- Remojo ---
            runCurrentPhase = "REMOJO";

            // En un resume del propio segmento, partimos del soak ya
            // acumulado antes del corte; en cualquier otro caso empezamos
            // desde cero para este segmento.
            unsigned long tiempoAcumuladoMs =
                (i == startSegmentIdx && startPhase == "REMOJO") ? startSoakMs : 0UL;

            // Persistir entrada a la fase de remojo.
            {
                String estadoCopy;
                float tempCopy = 0.0f;
                if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                    estadoCopy = estadoHorno;
                    tempCopy = temperaturaActual;
                    xSemaphoreGive(xMutex);
                }
                runSoakAccumMs = tiempoAcumuladoMs;
                saveRunState(estadoCopy, i, "REMOJO", tiempoAcumuladoMs, tempCopy);
            }

            // Cada cuánto persistimos el progreso de remojo en NVS. Cada
            // 30 s es un buen balance: pierde como mucho 30 s en un corte
            // de luz y no desgasta el flash.
            const unsigned long PERSIST_EVERY_MS = 30000UL;
            unsigned long lastPersistMs = tiempoAcumuladoMs;

            while (true) {
                String estadoLocal;
                int soakMin = 0;
                if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                    estadoLocal = estadoHorno;
                    if (i < curvaActual.numSegmentos) {
                        soakMin = curvaActual.segmentos[i].tiempoRemojoMin;
                    }
                    xSemaphoreGive(xMutex);
                } else {
                    vTaskDelay(pdMS_TO_TICKS(100));
                    continue;
                }

                // Objetivo releído en cada ciclo: edición en caliente desde la app.
                const unsigned long objetivoMs = (unsigned long)soakMin * 60000UL;
                if (tiempoAcumuladoMs >= objetivoMs) {
                    break;
                }

                if (estadoLocal == "DETENIDO" || estadoLocal == "IDLE") {
                    aborted = true;
                    break;
                }
                if (estadoLocal == "PAUSADO") {
                    vTaskDelay(pdMS_TO_TICKS(500));
                    continue;
                }
                if (estadoLocal != "CALENTANDO") {
                    vTaskDelay(pdMS_TO_TICKS(500));
                    continue;
                }

                tiempoAcumuladoMs += 1000;
                runSoakAccumMs = tiempoAcumuladoMs;
                vTaskDelay(pdMS_TO_TICKS(1000));

                if (tiempoAcumuladoMs - lastPersistMs >= PERSIST_EVERY_MS) {
                    String estadoCopy;
                    float tempCopy = 0.0f;
                    if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                        estadoCopy = estadoHorno;
                        tempCopy = temperaturaActual;
                        xSemaphoreGive(xMutex);
                    }
                    saveRunState(estadoCopy, i, "REMOJO", tiempoAcumuladoMs, tempCopy);
                    lastPersistMs = tiempoAcumuladoMs;
                }
            }

            // El primer segmento ya consumió su startPhase/startSoakMs;
            // los siguientes deben empezar limpios.
            startPhase = "RAMPA";
            startSoakMs = 0;
        }

        // --- Cierre del programa ---
        if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            if (aborted) {
                // No tocamos estadoHorno: el handler ya lo puso en DETENIDO.
                Serial.println("[Curva] Programa abortado por STOP.");
            } else {
                estadoHorno = "FINALIZADO";
                Serial.println("[Curva] Programa finalizado normalmente.");
                kilnFiringRequestFinalize(true);
            }
            curvaActual.recibida = false;
            programStartTime = 0;  // Resetear timestamp de inicio
            programPauseTime = 0;  // Resetear timestamp de pausa
            xSemaphoreGive(xMutex);
        }

        // Programa terminado / abortado: limpiamos run_state para que un
        // reinicio no dispare INTERRUMPIDO de algo que ya no está corriendo.
        clearRunState();
    }
}

#if defined(ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S3)
// Tarea para actualizar la pantalla LVGL con datos del horno
void TaskUpdateDisplayLVGL(void* pvParameters) {
    for (;;) {
        DisplayData displayData{};
        
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

            if (deviceMacAddress.length() >= 4) {
                String macTail = deviceMacAddress.substring(deviceMacAddress.length() - 4);
                snprintf(displayData.deviceId, sizeof(displayData.deviceId), "SK-%s", macTail.c_str());
            } else {
                strncpy(displayData.deviceId, "--", sizeof(displayData.deviceId) - 1);
            }
            displayData.deviceId[sizeof(displayData.deviceId) - 1] = '\0';
            
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
            
            // Conexión (iconos): en Solo local no mostrar internet aunque STA exista.
            displayData.connection.wifiConnected = wifiConnected && !offlineOnly;
            displayData.connection.firebaseConnected = firebaseConnected && !offlineOnly;
            displayData.connection.softApAvailable = wifi_manager_isApActive();
            
            // Temperatura objetivo y rampa del segmento en curso
            displayData.targetTemp = 0.0;
            displayData.stageRampCpm = -1.0f;
            if (curvaActual.numSegmentos > 0) {
                int segIdx = 0;
                const bool activeRun = (estadoHorno == "CALENTANDO" ||
                                        estadoHorno == "PAUSADO" ||
                                        estadoHorno == "INTERRUMPIDO");
                if (activeRun && runCurrentSegment >= 0 &&
                    runCurrentSegment < curvaActual.numSegmentos) {
                    segIdx = runCurrentSegment;
                    displayData.stageRampCpm = curvaActual.segmentos[segIdx].rampaCporMin;
                }
                displayData.targetTemp = curvaActual.segmentos[segIdx].tempObjetivo;
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
            
            // Etapa actual según progreso del programa
            if (curvaActual.numSegmentos > 0) {
                displayData.totalStages = curvaActual.numSegmentos;
                const bool activeRun = (estadoHorno == "CALENTANDO" ||
                                        estadoHorno == "PAUSADO" ||
                                        estadoHorno == "INTERRUMPIDO");
                if (activeRun && runCurrentSegment >= 0 &&
                    runCurrentSegment < curvaActual.numSegmentos) {
                    displayData.currentStage = runCurrentSegment + 1;
                } else {
                    displayData.currentStage = 0;
                }
            } else {
                displayData.currentStage = 0;
                displayData.totalStages = 0;
            }
            
            // Fase actual dentro del segmento (RAMPA / REMOJO)
            displayData.currentPhase[0] = '\0';
            if ((estadoHorno == "CALENTANDO" || estadoHorno == "PAUSADO" ||
                 estadoHorno == "INTERRUMPIDO") &&
                runCurrentSegment >= 0 && runCurrentPhase.length() > 0) {
                strncpy(displayData.currentPhase, runCurrentPhase.c_str(),
                        sizeof(displayData.currentPhase) - 1);
                displayData.currentPhase[sizeof(displayData.currentPhase) - 1] = '\0';
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
            display_postData(displayData);
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
#if defined(ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S3)
    // Primero la pantalla: el splash gira mientras Serial/USB CDC enumeran
    // (~5 s). No depende de WiFi/Firebase; el resto del setup no cambia.
    if (initDisplayLVGL()) {
        // createUI es pesado en stack; 4K quedaba justo y podía reiniciar en splash→main.
        xTaskCreatePinnedToCore(displayTaskHandler, "LVGL_Handler", 8192, NULL, 1, NULL, 1);
    }
#endif

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
    delay(200);

    logAndPersistResetReason();
    
    // Configurar niveles de log DESPUÉS de que Serial esté funcionando
    esp_log_level_set("*", ESP_LOG_NONE);
    esp_log_level_set("Preferences", ESP_LOG_NONE);
    esp_log_level_set("wifi", ESP_LOG_ERROR);
    esp_log_level_set("phy_init", ESP_LOG_ERROR);
    
    Serial.println("[SETUP] ✅ Serial iniciado correctamente");
#if defined(ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S3)
    Serial.println("[SETUP] Pantalla: splash de arranque (activo desde el inicio)");
#endif

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
    kiln_api_startTask();
    delay(50);

    // NUEVO: Cargar datos del horno guardados
    loadKilnInfo();
    Serial.println("[SETUP] ✅ Datos del horno cargados");
    delay(50);
    
    // NUEVO: Cargar credenciales WiFi guardadas
    loadWifiConfig();
    
    // Verificar que se cargaron correctamente
    if (wifiConfigReceived && wifiSSID.length() > 0 && !offlineOnly) {
        Serial.println("[SETUP] ✅ Credenciales WiFi cargadas correctamente");
        // No iniciar STA aquí: tryStaConnect puede softAPdisconnect + rescan
        // de canal mientras el splash/createUI corre → reinicios antes de la UI.
        Serial.println("[SETUP] STA diferido hasta UI principal lista");
    } else if (offlineOnly) {
        Serial.println("[SETUP] Solo local (offlineOnly) — SoftAP sin STA/Firebase");
    } else {
        Serial.println("[SETUP] ⚠️ No hay credenciales WiFi guardadas");
        Serial.println("[SETUP] Configuralas desde la app (POST /api/kiln-info)");
    }

    delay(50);
    
    // NUEVO: Cargar programa activo guardado
    loadProfile();
    Serial.println("[SETUP] ✅ Programa activo cargado");
    delay(50);

    // Si la cocción anterior estaba en CALENTANDO o PAUSADO al cortarse la
    // luz, loadRunState() pondrá estadoHorno = "INTERRUMPIDO" y dejará las
    // variables runCurrentSegment / runCurrentPhase / runSoakAccumMs listas
    // para que TaskControlCurva pueda reanudar cuando llegue RESUME.
    loadRunState();
    // Sin cocción interrumpida pendiente: arranque explícito en inactivo para
    // que HTTP/Firebase no hereden un estado viejo de la sesión anterior.
    if (estadoHorno != "INTERRUMPIDO") {
        estadoHorno = "IDLE";
        programStartTime = 0;
        programPauseTime = 0;
        Serial.println("[SETUP] Horno en IDLE (inactivo)");
    }
    // Al reconectar Firebase, publicar el estado real de inmediato para no dejar
    // CALENTANDO/INTERRUMPIDO viejos en RTDB mientras el horno ya está IDLE.
    needsBootRealtimePublish = true;
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
    // STA/ERNet en segundo plano (TaskCom) — no tocar el AP aquí
    wifiConnected = false;
    Serial.println("[SETUP] AP listo; STA/Firebase diferidos (sin bloquear HTTP)");

    Serial.println("\n[SETUP] Firebase se iniciara cuando STA conecte (sin bloquear AP)");
    
    Serial.println("[SETUP] Solo WiFi SoftAP + HTTP / STA (sin Bluetooth)");

    Serial.println("\n[SETUP] =========================================");
    Serial.println("[SETUP] ✅ SETUP COMPLETADO");
    Serial.println("[SETUP] =========================================");
    Serial.println("\n");

#if defined(ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S3)
    // Pasar de splash → UI principal (en la tarea LVGL) y empezar a pintar datos.
    display_requestMainUi();
    {
        const unsigned long t0 = millis();
        while (!display_isMainUiReady() && millis() - t0 < 3000) {
            delay(20);
        }
    }
    if (xTaskCreatePinnedToCore(TaskUpdateDisplayLVGL, "UpdateDisplay", 6144, NULL, 1, NULL, 0) != pdPASS) {
        Serial.println("[SETUP] ⚠️ No se pudo crear UpdateDisplay");
    }
#endif

    // STA solo cuando la UI principal ya está lista (evita pelear con SoftAP/LVGL).
    if (!offlineOnly && wifiConfigReceived && wifiSSID.length() > 0) {
        Serial.println("[SETUP] Programando conexion STA a la WiFi de casa...");
        wifi_manager_requestStaReconnect();
    } else if (offlineOnly) {
        Serial.println("[SETUP] Solo local — no se programa STA");
    }

    xTaskCreatePinnedToCore(TaskControlHorno, "ControlHorno", 4096, NULL, 1, NULL, 1);
    // TaskCom (Firebase + WiFi reconexion) corre en core 1 para no bloquear al
    // wifi_event_task ni al IdleTask de core 0. Un setFloat colgado por SSL
    // muerta dispararia task_wdt_isr en core 0 (visto en abort backtraces).
    xTaskCreatePinnedToCore(TaskComunicaciones, "Comunicaciones", 24576, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(TaskControlCurva, "ControlCurva", 6144, NULL, 1, NULL, 1);

    // Subimos el Task Watchdog a 30 s y desactivamos el panic. Las operaciones
    // Firebase pueden colgarse temporalmente si el socket TLS queda corrupto
    // tras una transicion AP/STA; preferimos un timeout limpio en la lib en
    // lugar de un reboot. esp_task_wdt_init es idempotente.
    esp_task_wdt_init(30, false);
}

void loop() {
    // HTTP solo en TaskHttpApi (core 0). NO llamar kiln_api_loop() acá:
    // WebServer/WiFiClient no son thread-safe; dos handleClient() en paralelo
    // → LoadProhibited en WiFiClient::read (visto con flood GET /api/status).

    // Heartbeat Serial cada 30 s. En ESP32-S3 USB-CDC NUNCA hacer
    // Serial.end()/begin(): reenumera el COM, Windows hace “plim”, y VS Code
    // / PlatformIO pierden el puerto aunque el ESP y la pantalla sigan vivos.
    static unsigned long lastHeartbeat = 0;
    unsigned long now = millis();

    if (now - lastHeartbeat >= 30000) {
        lastHeartbeat = now;
        if (Serial) {
            Serial.println("[HEARTBEAT] Sistema activo");
        }
    }

    delay(1000);
}
