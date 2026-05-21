#include <Arduino.h>
#include "Wire.h"
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <Adafruit_MAX31855.h>
#include <NimBLEDevice.h>
#include "NimBLE2904.h"



// -------------------- CONFIGURACIONES --------------------
const char* WIFI_SSID = "ERNet";
const char* WIFI_PASSWORD = "Feli3354";

const char* FIREBASE_API_KEY = "AIzaSyBv3VknyBVxY_bMpgKgyhlVc_ImE7ejgwk";
const char* FIREBASE_DATABASE_URL = "https://ia-based-kiln-default-rtdb.firebaseio.com";

const char* BLE_DEVICE_NAME = "HornoCeramica";
const char* BLE_SERVICE_UUID = "1234";
const char* BLE_TEMP_CHAR_UUID = "ABCD";
const char* BLE_STATUS_CHAR_UUID = "DCBA";
const char* BLE_COMMAND_CHAR_UUID = "2211";

// -------------------- VARIABLES GLOBALES --------------------
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

SemaphoreHandle_t xMutex;

NimBLEServer* pServer = nullptr;
NimBLECharacteristic* gTempChar;
NimBLECharacteristic* gStatusChar;

float temperaturaActual = 0.0;
String estadoHorno = "IDLE";
bool wifiConnected = false;
bool firebaseConnected = false;

// Cada segmento define un objetivo de temperatura, una rampa y un tiempo de remojo
struct Segmento {
    float tempObjetivo;     // Temperatura objetivo en °C
    float rampaCporMin;     // Velocidad de rampa en °C/min
    int tiempoRemojoMin;    // Tiempo de remojo en minutos
};

// Definición de la curva completa
#define MAX_SEGMENTOS 10    // Límite de seguridad
struct Curva {
    char nombre[32];               // Nombre de la curva (ej. "Biscuit_980C")
    Segmento segmentos[MAX_SEGMENTOS]; // Lista de segmentos
    int numSegmentos;               // Cantidad de segmentos válidos
};

// Variable global para almacenar la curva activa
Curva curvaActiva;



// -------------------- FUNCIONES --------------------
void connectWiFi() {
    Serial.printf("Conectando a WiFi: %s\n", WIFI_SSID);
    WiFi.disconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 30000) {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.printf("\n[WiFi] Conectado. IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        wifiConnected = false;
        Serial.println("\n[WiFi] Falló la conexión!");
    }
}

bool initFirebase() {
    config.api_key = FIREBASE_API_KEY;
    config.database_url = FIREBASE_DATABASE_URL;
    fbdo.setResponseSize(1024);

    if (Firebase.signUp(&config, &auth, "", "")) {
        Firebase.begin(&config, &auth);
        Firebase.reconnectWiFi(true);
        firebaseConnected = true;
        Serial.println("[Firebase] Conectado correctamente.");
        return true;
    } else {
        firebaseConnected = false;
        Serial.printf("[Firebase] Error: %s\n", fbdo.errorReason().c_str());
        return false;
    }
}

void reportToFirebase(float temp, const String& estado) {
    if (!wifiConnected || !firebaseConnected) return;

    FirebaseJson json;
    json.set("temperatura", temp);
    json.set("estado", estado);
    json.set("timestamp", millis() / 1000);

    if (Firebase.RTDB.setJSON(&fbdo, "/horno/status", &json)) {
        Serial.println("[Firebase] Datos enviados.");
    } else {
        Serial.printf("[Firebase] Error: %s\n", fbdo.errorReason().c_str());
    }
}

float simularTermocupla() {
    static unsigned long lastUpdate = 0;
    static float temp = 25.0;
    
    if (millis() - lastUpdate > 2000) {
        lastUpdate = millis();
        temp += 5.0;
        if (temp > 1200.0) temp = 1200.0;
        Serial.println("[SIM] Temperatura: " + String(temp) + "°C");
    }
    return temp;
}

// -------------------- BLE CALLBACKS --------------------
class CommandCallback : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) override {
        String cmd = pCharacteristic->getValue();
        cmd.trim();

        if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (cmd == "START") estadoHorno = "CALENTANDO";
            else if (cmd == "PAUSE") estadoHorno = "PAUSADO";
            else if (cmd == "STOP") estadoHorno = "DETENIDO";

            String respuesta = "[BLE] " + cmd + "|OK";
            pCharacteristic->setValue(respuesta);
            xSemaphoreGive(xMutex);
        }
    }
};

void setupBLE() {
    NimBLEDevice::init(BLE_DEVICE_NAME);
    pServer = NimBLEDevice::createServer();

    NimBLEService* pService = pServer->createService(BLE_SERVICE_UUID);

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
    commandChar->setCallbacks(new CommandCallback());

    pService->start();
    pServer->getAdvertising()->start();
    Serial.println("[BLE] Servicio iniciado. Esperando conexiones...");
}

// -------------------- TAREAS --------------------
void TaskControlHorno(void* pvParameters) {
    for (;;) {
        if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            temperaturaActual = simularTermocupla();
            xSemaphoreGive(xMutex);
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void TaskComunicaciones(void* pvParameters) {
    unsigned long lastBLEUpdate = 0;
    unsigned long lastFirebaseUpdate = 0;

    for (;;) {
        unsigned long now = millis();

        // --- BLE: Actualización cada 1 seg (no bloqueante) ---
        if (now - lastBLEUpdate >= 1000) {
            if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(10)) == pdTRUE) {  // Timeout reducido
                if (pServer->getConnectedCount() > 0) {
                    gTempChar->setValue(temperaturaActual);
                    gTempChar->notify();
                    gStatusChar->setValue(estadoHorno);
                    gStatusChar->notify();
                }
                xSemaphoreGive(xMutex);
            }
            lastBLEUpdate = now;
        }

        // --- Firebase: Envío cada 5 seg (con timeout explícito) ---
        if (now - lastFirebaseUpdate >= 5000 && wifiConnected && firebaseConnected) {
            float tempCopy;
            String estadoCopy;
            
            // Copia rápida de datos (sin bloquear mutex demasiado tiempo)
            if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                tempCopy = temperaturaActual;
                estadoCopy = estadoHorno;
                xSemaphoreGive(xMutex);
            }

            // Envía fuera del mutex
            if (tempCopy != 0.0 || estadoCopy != "") {  // Evita enviar datos vacíos
                FirebaseJson json;
                json.set("temperatura", tempCopy);
                json.set("estado", estadoCopy);
                json.set("timestamp", millis() / 1000);

                fbdo.setResponseSize(512);
                Firebase.RTDB.setwriteSizeLimit(&fbdo, "tiny");
                
                if (!Firebase.RTDB.setJSON(&fbdo, "/horno/status", &json)) {
                    Serial.printf("[Firebase] Error: %s\n", fbdo.errorReason().c_str());
                }
            }
            lastFirebaseUpdate = now;
        }

        // --- Libera CPU frecuentemente ---
        vTaskDelay(pdMS_TO_TICKS(50));  // Asegura que el WDT se resetee
    }
}
// -------------------- SETUP Y LOOP --------------------
void setup() {
    Serial.begin(115200);
    delay(1000);

    xMutex = xSemaphoreCreateMutex();
    connectWiFi();
    if (wifiConnected) initFirebase();
    setupBLE();

    xTaskCreatePinnedToCore(TaskControlHorno, "ControlHorno", 4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(TaskComunicaciones, "Comunicaciones", 6144, NULL, 1, NULL, 0);
}

void loop() {
    delay(10000);
}