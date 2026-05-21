#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <Adafruit_MAX31855.h>
#include <NimBLEDevice.h>
#include "NimBLE2904.h"  // Actualizado a la versión más reciente
#include <ArduinoJson.h>

// -------------------- CONFIGURACIONES --------------------
// Configuración WiFi
const char* WIFI_SSID = "ERNet";
const char* WIFI_PASSWORD = "Feli3354";

// Configuración Firebase
const char* FIREBASE_API_KEY = "AIzaSyBv3VknyBVxY_bMpgKgyhlVc_ImE7ejgwk";
const char* FIREBASE_DATABASE_URL = "https://ia-based-kiln-default-rtdb.firebaseio.com";

// Configuración Termopar
const int MAXCLK = 18;
const int MAXCS = 5;
const int MAXDO = 19;

// Configuración BLE
const char* BLE_DEVICE_NAME = "HornoCeramica";
const char* BLE_SERVICE_UUID = "1234";
const char* BLE_TEMP_CHAR_UUID = "ABCD";
const char* BLE_STATUS_CHAR_UUID = "DCBA";
const char* BLE_PROGRAM_CHAR_UUID = "1122";
const char* BLE_COMMAND_CHAR_UUID = "2211";

// -------------------- OBJETOS GLOBALES --------------------
Adafruit_MAX31855 thermocouple(MAXCLK, MAXCS, MAXDO);
//Inicio
float temperaturaSimulada = 25.0; // Valor inicial para pruebas
#ifdef TEST_MODE
  #define thermocouple.readCelsius() temperaturaSimulada
#endif
//fin

// Firebase
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Mutex para comunicación segura entre tareas
SemaphoreHandle_t xMutex;

// BLE
NimBLEServer* pServer = nullptr;
NimBLECharacteristic* gTempChar;
NimBLECharacteristic* gStatusChar;

// Variables de estado
float temperaturaActual = 0.0;
String estadoHorno = "IDLE";
bool wifiConnected = false;
bool firebaseConnected = false;

// -------------------- PROTOTIPOS DE FUNCIONES --------------------
void connectWiFi();
bool initFirebase();
void setupBLE();
void reportToFirebase(float temp, const String& estado);
void TaskControlHorno(void* pvParameters);
void TaskComunicaciones(void* pvParameters);
void checkThermocoupleError();

// -------------------- CLASES DE CALLBACK BLE --------------------
class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
        Serial.println("[BLE] Cliente conectado");
    };

    void onDisconnect(NimBLEServer* pServer) {
        Serial.println("[BLE] Cliente desconectado");
        NimBLEDevice::startAdvertising();
    };
};

class ProgramCallback : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) override {
        String value = pCharacteristic->getValue();
        if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            Serial.println("[BLE] Programa recibido: " + value);
            estadoHorno = "PROGRAMANDO";
            xSemaphoreGive(xMutex);
        }
    }
};

class CommandCallback : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) override {
        String cmd = pCharacteristic->getValue();
        if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            Serial.println("[BLE] Comando recibido: " + cmd);
            if (cmd == "START") estadoHorno = "CALENTANDO";
            else if (cmd == "STOP") estadoHorno = "DETENIDO";
            else if (cmd == "PAUSE") estadoHorno = "PAUSADO";
            xSemaphoreGive(xMutex);
        }
    }
};

// -------------------- IMPLEMENTACIÓN DE FUNCIONES --------------------
void checkThermocoupleError() {
    uint8_t fault = thermocouple.readError();
    if (fault) {
        Serial.print("Error termopar: ");
        if (fault & MAX31855_FAULT_OPEN) Serial.println("Conexión abierta");
        if (fault & MAX31855_FAULT_SHORT_GND) Serial.println("Cortocircuito a GND");
        if (fault & MAX31855_FAULT_SHORT_VCC) Serial.println("Cortocircuito a VCC");
        estadoHorno = "ERROR_TERMOPAR";
    }
}

void connectWiFi() {
    Serial.print("Conectando a WiFi: ");
    Serial.println(WIFI_SSID);
    
    WiFi.disconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 30000) {
        delay(500);
        Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.println("\n[WiFi] Conectado. IP: " + WiFi.localIP().toString());
    } else {
        wifiConnected = false;
        Serial.println("\n[WiFi] Falló la conexión!");
    }
}

bool initFirebase() {
    config.api_key = FIREBASE_API_KEY;
    config.database_url = FIREBASE_DATABASE_URL;

    if (Firebase.signUp(&config, &auth, "", "")) {
        Firebase.begin(&config, &auth);
        Firebase.reconnectWiFi(true);
        Firebase.RTDB.setReadTimeout(&fbdo, 1000 * 5);
        Firebase.RTDB.setwriteSizeLimit(&fbdo, "tiny");
        
        firebaseConnected = true;
        Serial.println("[Firebase] Conectado correctamente.");
        return true;
    } else {
        firebaseConnected = false;
        Serial.printf("[Firebase] Error: %s\n", config.signer.signupError.message.c_str());
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
        Serial.println("[Firebase] Error: " + fbdo.errorReason());
        if (fbdo.errorReason() == "connection lost") {
            firebaseConnected = false;
        }
    }
}

void setupBLE() {
    if (!NimBLEDevice::getInitialized()) {
        NimBLEDevice::init(BLE_DEVICE_NAME);
        NimBLEDevice::setPower(ESP_PWR_LVL_P9); // Máxima potencia
    }

    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    NimBLEService* pService = pServer->createService(BLE_SERVICE_UUID);
    if (!pService) {
        Serial.println("[BLE] Error creando servicio!");
        return;
    }

    // Característica de temperatura
    gTempChar = pService->createCharacteristic(
        BLE_TEMP_CHAR_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    gTempChar->addDescriptor(new NimBLE2904()); // Actualizado a 2904

    // Característica de estado
    gStatusChar = pService->createCharacteristic(
        BLE_STATUS_CHAR_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    gStatusChar->addDescriptor(new NimBLE2904()); // Actualizado a 2904

    // Característica de programación
    NimBLECharacteristic* programChar = pService->createCharacteristic(
        BLE_PROGRAM_CHAR_UUID,
        NIMBLE_PROPERTY::WRITE
    );
    programChar->setCallbacks(new ProgramCallback());

    // Característica de comandos
    NimBLECharacteristic* commandChar = pService->createCharacteristic(
        BLE_COMMAND_CHAR_UUID,
        NIMBLE_PROPERTY::WRITE
    );
    commandChar->setCallbacks(new CommandCallback());

    pService->start();
    pServer->getAdvertising()->start();
    Serial.println("[BLE] Servicio iniciado. Esperando conexiones...");
}

// funcion para simular la lectura de termocupla
float leerTemperatura() {
  #ifdef TEST_MODE
    // Modo simulación (sin sensor)
    static unsigned long lastUpdate = 0;
    static float temp = 25.0;  // Temp inicial de prueba
    
    if (millis() - lastUpdate > 2000) {  // Actualiza cada 2 segundos
      lastUpdate = millis();
      if (estadoHorno == "CALENTANDO") temp += 10.0;  // Rampa de calentamiento
      if (estadoHorno == "DETENIDO") temp = 25.0;     // Reset al detenerse
      if (temp > 1200.0) temp = 1200.0;              // Límite máximo
    }
    Serial.println("[TEST] Temp simulada: " + String(temp) + "°C");
    return temp;

  #else
    // Modo real (con sensor MAX31855)
    float temp = thermocouple.readCelsius();
    if (isnan(temp)) {
      Serial.println("ERROR: Fallo en termopar");
      return -999.0;  // Valor de error
    }
    return temp;
  #endif
}


// -------------------- TAREAS FreeRTOS --------------------
void TaskControlHorno(void* pvParameters) {
    Serial.println("Tarea ControlHorno iniciada en core " + String(xPortGetCoreID()));
    
    for (;;) {
        if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            //float temp = thermocouple.readCelsius();
            //if (!isnan(temp)) {
            //    temperaturaActual = temp;
            //    checkThermocoupleError();
            //}
            temperaturaActual = leerTemperatura();  // Usa la nueva función
            if (temperaturaActual != -999.0) {     // Si no hay error
                checkThermocoupleError();
            }



            // Lógica de control básica
            if (estadoHorno == "CALENTANDO" && temperaturaActual >= 1000.0) {
                estadoHorno = "MANTENIENDO";
            }
            
            xSemaphoreGive(xMutex);
        }
        
        vTaskDelay(pdMS_TO_TICKS(500)); // Espera 500ms
    }
}

void TaskComunicaciones(void* pvParameters) {
  for (;;) {
    unsigned long taskStartTime = millis(); // Medir tiempo de ejecución
    
    if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) {
      // --- Código existente ---
      xSemaphoreGive(xMutex);
    }

    // Resetear el watchdog manualmente
    esp_task_wdt_reset();

    // Asegurar tiempo mínimo entre iteraciones
    vTaskDelay(pdMS_TO_TICKS(500)); 
    Serial.println("Tiempo tarea: " + String(millis() - taskStartTime) + "ms");
  }
}

// -------------------- SETUP Y LOOP --------------------
void setup() {
    Serial.begin(115200);
    delay(1000); // Espera para estabilizar la consola serial

    Serial.println("\nIniciando sistema de control de horno...");

    // Inicializar mutex
    xMutex = xSemaphoreCreateMutex();
    if (xMutex == NULL) {
        Serial.println("Error creando mutex!");
        while(1) delay(1000);
    }

    // Inicializar termopar
    if (!thermocouple.begin()) {
        Serial.println("Error inicializando termopar!");
        while(1) delay(1000);
    }

    // Conectar WiFi
    connectWiFi();

    // Inicializar Firebase si WiFi está conectado
    if (wifiConnected) {
        initFirebase();
    }

    // Configurar BLE
    setupBLE();

    // Crear tareas
    if (xTaskCreatePinnedToCore(
        TaskControlHorno, 
        "ControlHorno", 
        3072, 
        NULL, 
        1, 
        NULL, 
        1) != pdPASS) {
        Serial.println("Error creando tarea ControlHorno!");
    }

    if (xTaskCreatePinnedToCore(
        TaskComunicaciones, 
        "Comunicaciones", 
        8192, 
        NULL, 
        2, 
        NULL, 
        0) != pdPASS) {
        Serial.println("Error creando tarea Comunicaciones!");
    }

    Serial.println("Sistema iniciado correctamente");

    esp_task_wdt_init(10, false); // 10 segundos de timeout
}

void loop() {
    // Nada aquí, todo se maneja en las tareas
    delay(10000); // Pequeño delay para evitar watchdog
}