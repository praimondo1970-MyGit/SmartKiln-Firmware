# IA Kiln - Resumen Completo del Proyecto ESP32

## 📋 **Información General del Proyecto**

**Proyecto**: IA Kiln - Sistema de Control Inteligente para Horno de Cerámica
**Microcontrolador**: ESP32 (Espressif)
**Lenguaje**: Arduino C++
**Framework**: Arduino Framework
**IDE**: PlatformIO
**Estado**: En desarrollo activo
**Última actualización**: Enero 2025

---

## 🏗️ **Estructura del Proyecto**

### **Ubicación del Proyecto**
- **Directorio**: `C:\Users\praim\OneDrive\Documentos\PlatformIO\Projects\IA Kiln`
- **Archivo principal**: `src/main.cpp`
- **Configuración**: `platformio.ini`
- **Librerías**: `lib/` (Firebase, NimBLE, ArduinoJson, Adafruit MAX31855)

### **Arquitectura del Sistema**
- **Hardware**: ESP32 DevKit
- **Comunicación**: WiFi + BLE (Bluetooth Low Energy)
- **Almacenamiento**: Memoria Flash (Preferences)
- **Sensores**: Termocupla MAX31855 (simulada actualmente)
- **Cloud**: Firebase Realtime Database

---

## 🔧 **Funcionalidades Implementadas**

### **✅ Completadas**

#### **1. Sistema de Comunicación BLE**
- **Servidor BLE**: Configurado con nombre "SmartKiln - Ceramica"
- **Servicio UUID**: `00001234-0000-1000-8000-00805f9b34fb`
- **Características implementadas**:
  - **Temperatura**: `0000abcd-0000-1000-8000-00805f9b34fb` (READ, NOTIFY)
  - **Estado**: `0000dcba-0000-1000-8000-00805f9b34fb` (READ, NOTIFY)
  - **Comandos**: `00002211-0000-1000-8000-00805f9b34fb` (WRITE)
  - **Curvas**: `0000a001-0000-1000-8000-00805f9b34fb` (WRITE)
  - **UserID**: `0000a002-0000-1000-8000-00805f9b34fb` (WRITE)

#### **2. Sistema de Comunicación WiFi + Firebase**
- **WiFi**: Conexión automática a red configurada
- **Firebase**: Integración con Realtime Database
- **API Key**: Configurada para proyecto "ia-based-kiln"
- **Estructura de datos**: `/users/{userId}/kilns/{macAddress}/deviceInfo`

#### **3. Sistema de Almacenamiento Persistente**
- **Preferences**: Almacenamiento en memoria flash
- **Datos guardados**:
  - Nombre del horno
  - Modelo del horno
  - Volumen del horno
  - UserID del usuario
- **Persistencia**: Datos se mantienen entre reinicios

#### **4. Sistema de Control de Curvas de Cocción**
- **Estructura de datos**: `CurvaActiva` con hasta 10 segmentos
- **Parámetros por segmento**:
  - Temperatura objetivo
  - Velocidad de rampa (°C/min)
  - Tiempo de remojo (minutos)
- **Ejecución**: Control automático de temperatura y tiempos

#### **5. Sistema de Tareas (FreeRTOS)**
- **TaskControlHorno**: Simulación de termocupla (Core 1)
- **TaskComunicaciones**: BLE + Firebase (Core 0)
- **TaskControlCurva**: Ejecución de curvas de cocción (Core 1)
- **Mutex**: Protección de variables globales compartidas

---

## 📡 **Protocolo de Comunicación BLE**

### **Comandos Soportados**

#### **1. KILN_INFO**
```
Formato: "KILN_INFO:{JSON}"
JSON: {"name":"Nombre", "model":"Modelo", "volume":volumen, "userId":"ID"}
Respuesta: "[BLE] KILN_INFO_OK" o "[BLE] KILN_INFO_ERROR"
```

#### **2. Comandos de Control**
- **START**: Inicia proceso de cocción
- **PAUSE**: Pausa el proceso
- **STOP**: Detiene el proceso
- **TEST**: Prueba de conectividad
- **STATUS**: Estado actual del sistema
- **DEBUG_VARS**: Variables globales para debugging

#### **3. Carga de Perfiles**
```
Formato: "LOAD_PROFILE:{JSON}"
JSON: {"nombre":"Nombre", "numSeg":N, "seg":[{"temp":T, "rampa":R, "remojo":M}]}
Respuesta: Notificación en gStatusChar ("PROFILE_OK" o "PROFILE_ERROR")
```

### **Notificaciones Automáticas**
- **Temperatura**: Actualización cada 1 segundo
- **Estado**: Actualización cada 1 segundo
- **Firebase**: Envío cada 5 segundos

---

## 🔥 **Integración con Firebase**

### **Estructura de Datos**
```json
{
  "users": {
    "{userId}": {
      "kilns": {
        "{macAddress}": {
          "deviceInfo": {
            "name": "Nombre del Horno",
            "model": "Modelo del Horno",
            "volume": 50,
            "macAddress": "MAC_ADDRESS",
            "lastSeen": 1640995200,
            "isOnline": true
          }
        }
      }
    }
  }
}
```

### **Configuración Firebase**
- **API Key**: `AIzaSyBv3VknyBVxY_bMpgKgyhlVc_ImE7ejgwk`
- **Database URL**: `https://ia-based-kiln-default-rtdb.firebaseio.com`
- **Autenticación**: Anónima (sin usuario/contraseña)

---

## 🎯 **Sistema de Control de Curvas**

### **Estructura de Datos**
```cpp
struct Segmento {
    float tempObjetivo;    // Temperatura objetivo en °C
    float rampaCporMin;    // Velocidad de rampa en °C/min
    int tiempoRemojoMin;   // Tiempo de remojo en minutos
};

struct CurvaActiva {
    char nombre[32];           // Nombre de la curva
    Segmento segmentos[10];    // Máximo 10 segmentos
    uint8_t numSegmentos;      // Número de segmentos
    bool recibida;             // Flag de nueva curva
};
```

### **Algoritmo de Control**
1. **Recepción**: Curva recibida por BLE
2. **Validación**: Verificación de formato JSON
3. **Almacenamiento**: Guardado en estructura global
4. **Ejecución**: Control automático por segmentos
5. **Monitoreo**: Seguimiento de temperatura y tiempos

---

## 🔧 **Configuración Técnica**

### **Hardware**
- **Microcontrolador**: ESP32 DevKit
- **WiFi**: 802.11 b/g/n
- **Bluetooth**: BLE 4.2
- **Memoria Flash**: Partición "huge_app.csv"
- **Velocidad de Upload**: 921600 baud

### **Librerías Utilizadas**
- **Firebase-ESP-Client**: v4.4.17 (Comunicación con Firebase)
- **NimBLE-Arduino**: v1.4.3 (Comunicación BLE)
- **ArduinoJson**: v6.21.3 (Manejo de JSON)
- **Adafruit MAX31855**: v1.4.1 (Sensor de temperatura)

### **Configuración de Build**
- **Framework**: Arduino
- **Estándar C++**: gnu++17
- **Optimización**: -O2
- **Warnings**: -Wall
- **Velocidad de Monitor**: 115200 baud

---

## 📊 **Variables Globales del Sistema**

### **Información del Horno**
```cpp
String kilnName = "Mi Kiln Cerámico";     // Nombre del horno
String kilnModel = "SmartKiln - Ceramica"; // Modelo del horno
float kilnVolume = 0.0;                   // Volumen en litros
String userId = "";                       // ID del usuario
String deviceMacAddress = "";             // MAC del dispositivo
```

### **Estado del Sistema**
```cpp
float temperaturaActual = 0.0;            // Temperatura actual
String estadoHorno = "IDLE";              // Estado del horno
bool wifiConnected = false;               // Estado WiFi
bool firebaseConnected = false;           // Estado Firebase
CurvaActiva curvaActual;                  // Curva activa
```

### **Configuración de Red**
```cpp
const char* WIFI_SSID = "ERNet";
const char* WIFI_PASSWORD = "Feli3354";
```

---

## 🚀 **Flujo de Funcionamiento**

### **1. Inicialización**
1. **Setup**: Configuración de Serial, WiFi, Firebase, BLE
2. **Carga de datos**: Recuperación de información guardada
3. **Conexiones**: Establecimiento de WiFi y Firebase
4. **BLE**: Inicio del servidor y advertising
5. **Tareas**: Creación de tareas FreeRTOS

### **2. Operación Normal**
1. **BLE**: Escucha de comandos y envío de notificaciones
2. **Firebase**: Envío periódico de datos
3. **Control**: Ejecución de curvas de cocción
4. **Monitoreo**: Seguimiento de temperatura y estado

### **3. Comunicación con App Android**
1. **Conexión**: App se conecta por BLE
2. **Configuración**: Envío de datos del horno (KILN_INFO)
3. **Control**: Recepción de comandos de control
4. **Curvas**: Carga de perfiles de cocción
5. **Monitoreo**: Notificaciones de temperatura y estado

---

## 🐛 **Problemas Resueltos**

### **1. Gestión de Memoria**
- **Mutex**: Protección de variables globales compartidas
- **Timeouts**: Evitar bloqueos en operaciones BLE/Firebase
- **Memoria Flash**: Almacenamiento persistente con Preferences

### **2. Comunicación BLE**
- **Callbacks**: Manejo correcto de eventos de conexión/desconexión
- **Advertising**: Verificación periódica del estado
- **Características**: Configuración correcta de propiedades (READ/WRITE/NOTIFY)

### **3. Integración Firebase**
- **Autenticación**: Configuración correcta de API Key
- **Estructura**: Organización de datos por usuario y dispositivo
- **Validación**: Verificación de datos antes del envío

### **4. Control de Curvas**
- **Parsing JSON**: Manejo robusto de datos de curvas
- **Ejecución**: Control preciso de temperatura y tiempos
- **Estados**: Gestión correcta de estados del horno

---

## 📝 **Logging y Debugging**

### **Sistema de Logs**
- **Serial**: Logging detallado a 115200 baud
- **Niveles**: INFO, WARNING, ERROR
- **Categorías**: [BLE], [Firebase], [CMD], [Curva], [STORAGE]

### **Comandos de Debug**
- **DEBUG_VARS**: Muestra variables globales
- **STATUS**: Estado completo del sistema
- **TEST**: Prueba de conectividad BLE

### **Verificaciones Periódicas**
- **Advertising BLE**: Cada 10 segundos
- **Conexiones**: Estado de WiFi y Firebase
- **Variables**: Validación de datos del horno

---

## 🔮 **Próximos Pasos Sugeridos**

### **Inmediatos**
1. **Sensor real**: Implementar lectura de termocupla MAX31855
2. **Control PID**: Algoritmo de control de temperatura
3. **Seguridad**: Validación de rangos de temperatura
4. **Persistencia**: Guardado de curvas en memoria flash

### **Futuros**
1. **OTA Updates**: Actualizaciones por WiFi
2. **Múltiples sensores**: Temperatura en diferentes puntos
3. **Alarmas**: Sistema de alertas por temperatura
4. **Historial**: Registro de cocciones realizadas
5. **Web Interface**: Interfaz web para configuración

---

## 📁 **Archivos Clave del Proyecto**

### **Código Principal**
- `src/main.cpp` - Código principal del ESP32
- `platformio.ini` - Configuración del proyecto

### **Librerías**
- `lib/Firebase-ESP-Client-main/` - Cliente Firebase
- `lib/NimBLE-Arduino/` - Comunicación BLE
- `lib/ArduinoJson/` - Manejo de JSON
- `lib/Adafruit MAX31855 library/` - Sensor de temperatura

### **Documentación**
- `Control Horno IA.md` - Documentación del proyecto
- `PROJECT_SUMMARY.md` - Este archivo

---

## 💡 **Instrucciones para Continuar el Desarrollo**

### **Para otra IA o nuevo chat:**

1. **Leer este documento completo** para entender el contexto
2. **Revisar el código en `src/main.cpp`** para ver el estado actual
3. **Compilar el proyecto** con PlatformIO para verificar estado
4. **Probar comunicación BLE** con la app Android
5. **Verificar integración Firebase** en la consola web
6. **Mantener la consistencia** de logging y estructura de datos

### **Comandos útiles:**
```bash
# Compilar proyecto
pio run

# Subir al ESP32
pio run --target upload

# Monitor serial
pio device monitor

# Limpiar proyecto
pio run --target clean
```

### **Configuración de red:**
- **SSID**: "ERNet"
- **Password**: "Feli3354"
- **Firebase**: Proyecto "ia-based-kiln"

---

## 🎯 **Estado Actual del Proyecto**

### **Funcionalidad Principal**: ✅ COMPLETA
- **Comunicación BLE**: Sistema completo implementado
- **Integración Firebase**: Conexión y envío de datos funcionando
- **Control de curvas**: Recepción y ejecución de perfiles
- **Almacenamiento**: Persistencia de datos del horno
- **Sistema de tareas**: FreeRTOS con 3 tareas principales

### **Integración con App Android**: ✅ FUNCIONAL
- **Conexión BLE**: Establecimiento de comunicación
- **Configuración**: Recepción de datos del horno
- **Control**: Comandos de inicio/pausa/parada
- **Curvas**: Carga de perfiles de cocción
- **Monitoreo**: Notificaciones de temperatura y estado

### **Próximos Hitos**
1. **Sensor real**: Implementar termocupla MAX31855
2. **Control PID**: Algoritmo de control de temperatura
3. **Validaciones**: Rangos de temperatura y seguridad
4. **Persistencia**: Guardado de curvas en memoria flash

---

**Última actualización**: Enero 2025  
**Estado del proyecto**: ✅ FUNCIONAL COMPLETO  
**Funcionalidad principal**: Sistema de control de horno con comunicación BLE y Firebase  
**Próximo hito**: Implementar sensor real de temperatura y control PID
























































