# 🎯 Plan Detallado de Implementación: Control PID con QuickPID

## 📋 Índice
1. [Arquitectura General](#arquitectura-general)
2. [Flujo de Control Completo](#flujo-de-control-completo)
3. [Integración con la Curva de Temperatura](#integración-con-la-curva-de-temperatura)
4. [Estados del Sistema](#estados-del-sistema)
5. [Funcionamiento del PID](#funcionamiento-del-pid)
6. [Sistema de Autotune](#sistema-de-autotune)
7. [Control de Salida (Relay/Heater)](#control-de-salida-relayheater)
8. [Sincronización con la Curva](#sincronización-con-la-curva)
9. [Manejo de Rampas](#manejo-de-rampas)
10. [Manejo de Remojos](#manejo-de-remojos)
11. [Pausa y Reanudación](#pausa-y-reanudación)
12. [Seguridad y Protecciones](#seguridad-y-protecciones)
13. [Estructura de Código](#estructura-de-código)
14. [Variables y Estructuras](#variables-y-estructuras)
15. [Tareas FreeRTOS](#tareas-freertos)
16. [Comunicación con el Sistema](#comunicación-con-el-sistema)

---

## 🏗️ Arquitectura General

### Visión de Alto Nivel

```
┌─────────────────────────────────────────────────────────────┐
│                    APLICACIÓN MÓVIL                         │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐  │
│  │  START   │  │  PAUSE   │  │   STOP   │  │  AUTOTUNE │  │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘  │
└───────┼──────────────┼──────────────┼──────────────┼────────┘
        │              │              │              │
        └──────────────┴──────────────┴──────────────┘
                        │ BLE
                        ▼
┌─────────────────────────────────────────────────────────────┐
│                    ESP32-S3 FIRMWARE                         │
│                                                               │
│  ┌─────────────────────────────────────────────────────┐    │
│  │         TaskComunicaciones (BLE/WiFi)                │    │
│  │  - Recibe comandos: START, PAUSE, STOP, AUTOTUNE    │    │
│  │  - Actualiza estadoHorno: "CALENTANDO", "PAUSADO"   │    │
│  └─────────────────────────────────────────────────────┘    │
│                        │                                      │
│                        ▼                                      │
│  ┌─────────────────────────────────────────────────────┐    │
│  │         TaskControlCurva (Coordinador)                │    │
│  │  - Lee curvaActual (segmentos con tempObjetivo)      │    │
│  │  - Calcula setpoint dinámico según rampa actual      │    │
│  │  - Actualiza setpointPID (temperatura objetivo)      │    │
│  │  - Maneja transiciones entre segmentos               │    │
│  └─────────────────────────────────────────────────────┘    │
│                        │                                      │
│                        ▼                                      │
│  ┌─────────────────────────────────────────────────────┐    │
│  │         TaskControlPID (Control Real)                │    │
│  │  - Lee temperaturaActual (entrada)                  │    │
│  │  - Lee setpointPID (objetivo dinámico)               │    │
│  │  - QuickPID calcula outputPID (0-100% o PWM)         │    │
│  │  - Controla relay/heater según outputPID             │    │
│  │  - Ejecuta autotune si está activo                   │    │
│  └─────────────────────────────────────────────────────┘    │
│                        │                                      │
│                        ▼                                      │
│  ┌─────────────────────────────────────────────────────┐    │
│  │         TaskControlHorno (Sensores)                   │    │
│  │  - Lee termocupla MAX31855                           │    │
│  │  - Actualiza temperaturaActual                       │    │
│  └─────────────────────────────────────────────────────┘    │
│                                                               │
│  ┌─────────────────────────────────────────────────────┐    │
│  │              Hardware: Relay/SSR                      │    │
│  │  - Controla elemento calefactor                      │    │
│  │  - Modo: ON/OFF o PWM según outputPID                │    │
│  └─────────────────────────────────────────────────────┘    │
└───────────────────────────────────────────────────────────────┘
```

---

## 🔄 Flujo de Control Completo

### Secuencia Normal de Operación

```
1. APP → BLE: Envía curva (segmentos con tempObjetivo, rampa, remojo)
   └─> TaskComunicaciones recibe y guarda en curvaActual
   └─> curvaActual.recibida = true

2. APP → BLE: Comando "START"
   └─> TaskComunicaciones actualiza: estadoHorno = "CALENTANDO"
   └─> TaskControlCurva detecta: estadoHorno == "CALENTANDO" && curvaActual.recibida
   └─> TaskControlCurva inicia ejecución de curva

3. TaskControlCurva (Cada segundo):
   a) Determina segmento actual según tiempo transcurrido
   b) Calcula setpoint dinámico según rampa:
      - Si está en fase de rampa: setpoint = tempActual + (rampaPorSegundo * dt)
      - Si está en fase de remojo: setpoint = tempObjetivo (constante)
   c) Actualiza setpointPID (variable compartida)
   d) Verifica si debe pasar al siguiente segmento

4. TaskControlPID (Cada 100-500ms):
   a) Lee temperaturaActual (desde TaskControlHorno)
   b) Lee setpointPID (desde TaskControlCurva)
   c) QuickPID.Compute() calcula outputPID
   d) Convierte outputPID a señal de control:
      - Modo PWM: outputPID → duty cycle (0-100%)
      - Modo ON/OFF: outputPID > threshold → ON, else → OFF
   e) Controla relay/heater

5. TaskControlHorno (Cada 500ms):
   a) Lee termocupla MAX31855
   b) Actualiza temperaturaActual
   c) Aplica filtro de suavizado si es necesario

6. Loop continúa hasta:
   - Todos los segmentos completados → estadoHorno = "FINALIZADO"
   - Comando "STOP" → estadoHorno = "DETENIDO"
   - Comando "PAUSE" → estadoHorno = "PAUSADO"
```

---

## 📊 Integración con la Curva de Temperatura

### Estructura Actual de la Curva

```cpp
struct Segmento {
    float tempObjetivo;      // Temperatura objetivo del segmento (ej: 980°C)
    float rampaCporMin;       // Velocidad de rampa (°C/min, ej: 150°C/min)
    int tiempoRemojoMin;      // Tiempo de remojo en minutos (ej: 30 min)
};

struct CurvaActiva {
    char nombre[32];
    Segmento segmentos[10];  // Máximo 10 segmentos
    uint8_t numSegmentos;
    bool recibida;
};
```

### Cómo el PID se Integra con la Curva

#### Fase 1: Rampa de Calentamiento
```
Segmento actual: tempObjetivo = 980°C, rampaCporMin = 150°C/min

Tiempo 0s:   tempActual = 25°C
             setpointPID = 25°C + (150/60 * 0) = 25°C
             PID intenta calentar para alcanzar 25°C

Tiempo 1s:   tempActual = 27°C (el PID está calentando)
             setpointPID = 25°C + (150/60 * 1) = 27.5°C
             PID ajusta para alcanzar 27.5°C

Tiempo 60s:  tempActual = 150°C
             setpointPID = 25°C + (150/60 * 60) = 175°C
             PID sigue la rampa dinámicamente

...continúa hasta que tempActual ≈ tempObjetivo (980°C)
```

#### Fase 2: Remojo (Sostenimiento)
```
Una vez que tempActual alcanza tempObjetivo (980°C):

setpointPID = 980°C (constante)
PID mantiene la temperatura en 980°C durante tiempoRemojoMin minutos
El PID compensa pérdidas de calor y mantiene estabilidad
```

#### Fase 3: Transición al Siguiente Segmento
```
Cuando termina el remojo del segmento actual:
- TaskControlCurva pasa al siguiente segmento
- Nuevo tempObjetivo y rampaCporMin
- setpointPID se recalcula desde la temperatura actual
- El PID continúa siguiendo la nueva rampa
```

---

## 🔀 Estados del Sistema

### Estados Actuales
```cpp
estadoHorno puede ser:
- "IDLE"      → Sistema en reposo, sin programa cargado
- "CALENTANDO" → Ejecutando curva, PID activo
- "PAUSADO"    → Programa pausado, PID desactivado
- "DETENIDO"   → Programa detenido, PID desactivado
- "FINALIZADO" → Programa completado, PID desactivado
- "AUTOTUNE"   → Ejecutando autotune (nuevo estado)
```

### Máquina de Estados

```
                    ┌─────────┐
                    │  IDLE   │
                    └────┬────┘
                         │ curvaActual.recibida = true
                         ▼
                    ┌─────────┐
                    │  READY  │ (implícito, no es estado)
                    └────┬────┘
                         │ Comando "START"
                         ▼
              ┌──────────────────────┐
              │    CALENTANDO        │◄──────────┐
              │  - PID activo        │           │
              │  - Siguiendo curva   │           │
              └────┬───────────┬─────┘           │
                   │           │                 │
        Comando    │           │ Todos los       │
        "PAUSE"    │           │ segmentos       │
                   │           │ completados     │
                   ▼           ▼                 │
              ┌─────────┐  ┌──────────┐         │
              │ PAUSADO │  │FINALIZADO│         │
              │ PID OFF │  │  PID OFF │         │
              └────┬────┘  └──────────┘         │
                   │                            │
        Comando    │                            │
        "START"    │                            │
        (resume)   │                            │
                   └────────────────────────────┘
                         │
        Comando "STOP"   │
                         ▼
                    ┌─────────┐
                    │ DETENIDO│
                    │ PID OFF │
                    └────┬────┘
                         │
                         │ Reset
                         ▼
                    ┌─────────┐
                    │  IDLE   │
                    └─────────┘

Estado especial:
    ┌─────────┐
    │AUTOTUNE │ (puede activarse desde cualquier estado)
    │ PID OFF │
    │ Autotune│
    │ activo  │
    └────┬────┘
         │
         │ Autotune completo
         │ → Guarda Kp, Ki, Kd
         │ → Vuelve al estado anterior
         ▼
```

---

## 🎛️ Funcionamiento del PID

### Conceptos Básicos

El PID (Proporcional-Integral-Derivativo) calcula una salida de control basada en el error:

```
error = setpoint - temperaturaActual
outputPID = Kp * error + Ki * integral(error) + Kd * derivada(error)
```

### Componentes del PID

#### 1. **Proporcional (Kp)**
- **Función**: Respuesta proporcional al error actual
- **Efecto**: 
  - Kp alto → Respuesta rápida, pero puede oscilar
  - Kp bajo → Respuesta lenta, pero más estable
- **Para hornos**: Kp moderado (ej: 2.0-5.0) para respuesta rápida sin oscilaciones

#### 2. **Integral (Ki)**
- **Función**: Elimina error de estado estacionario
- **Efecto**:
  - Ki alto → Elimina error rápido, pero puede causar overshoot
  - Ki bajo → Elimina error lento
- **Para hornos**: Ki bajo (ej: 0.01-0.1) para evitar windup en rampas

#### 3. **Derivativo (Kd)**
- **Función**: Predice comportamiento futuro basado en la tasa de cambio
- **Efecto**:
  - Kd alto → Reduce overshoot, pero sensible a ruido
  - Kd bajo → Menos amortiguamiento
- **Para hornos**: Kd moderado (ej: 10-50) para suavizar transiciones

### Cálculo del Output

```cpp
// QuickPID calcula internamente:
double error = setpointPID - temperaturaActual;
double output = Kp * error + Ki * integral + Kd * (error - lastError);
output = constrain(output, outputMin, outputMax);  // Normalmente 0-255 o 0-100%
```

### Conversión a Control de Hardware

#### Opción A: Control PWM (Recomendado)
```cpp
// outputPID está en rango 0-100%
// Convertir a duty cycle PWM (0-255 para 8-bit)
int pwmValue = (int)(outputPID * 2.55);  // 0-100% → 0-255
analogWrite(RELAY_PIN, pwmValue);
```

#### Opción B: Control ON/OFF con Histeresis
```cpp
// outputPID está en rango 0-100%
if (outputPID > 50.0) {  // Threshold
    digitalWrite(RELAY_PIN, HIGH);  // ON
} else {
    digitalWrite(RELAY_PIN, LOW);   // OFF
}
```

#### Opción C: Control ON/OFF con Ventana de Tiempo (Time Proportional)
```cpp
// outputPID = 75% significa: ON 75% del tiempo, OFF 25%
unsigned long windowSize = 10000;  // 10 segundos
unsigned long onTime = (unsigned long)(outputPID * windowSize / 100.0);
// En cada ventana de 10s: ON por onTime, OFF por (windowSize - onTime)
```

---

## 🔧 Sistema de Autotune

### Cuándo Usar Autotune

1. **Primera vez**: Cuando se instala el horno
2. **Cambio de carga**: Diferente cantidad de cerámica
3. **Mantenimiento**: Después de reparaciones
4. **Optimización**: Para mejorar el control

### Flujo de Autotune

```
1. APP → BLE: Comando "AUTOTUNE"
   └─> TaskComunicaciones detecta comando
   └─> estadoHorno = "AUTOTUNE"
   └─> autotuneActive = true

2. TaskControlPID detecta autotuneActive:
   a) Verifica que temperaturaActual esté en rango seguro (ej: 25-200°C)
   b) Inicia QuickPID autotune:
      - Establece setpoint de prueba (ej: temperaturaActual + 50°C)
      - Alterna relay ON/OFF para crear oscilaciones
      - Mide amplitud y período de oscilaciones
      - Calcula Kp, Ki, Kd usando método Relé

3. Durante autotune (5-15 minutos):
   - El horno oscilará alrededor del setpoint de prueba
   - QuickPID registra datos de oscilación
   - El usuario debe esperar (no puede usar el horno normalmente)

4. Autotune completo:
   a) QuickPID calcula parámetros óptimos
   b) Guarda Kp, Ki, Kd en Preferences (NVS)
   c) Carga parámetros en QuickPID
   d) estadoHorno vuelve al estado anterior (ej: "IDLE")
   e) autotuneActive = false

5. Parámetros guardados:
   - Se pueden usar inmediatamente
   - Se cargan automáticamente en el próximo arranque
   - Se pueden ajustar manualmente si es necesario
```

### Parámetros de Autotune

```cpp
// Configuración de autotune
struct AutotuneConfig {
    float setpointTest;        // Temperatura de prueba (ej: tempActual + 50°C)
    float noiseBand;            // Banda de ruido (ej: 1.0°C)
    unsigned long lookBackTime; // Tiempo de análisis (ej: 60000ms = 1 min)
    bool isPositive;           // true si calienta, false si enfría
};

// QuickPID autotune usa método Relé:
// - Alterna entre outputMax y outputMin
// - Mide período y amplitud de oscilaciones
// - Calcula parámetros usando fórmulas de Ziegler-Nichols
```

---

## ⚡ Control de Salida (Relay/Heater)

### Hardware Asumido

```
ESP32-S3 GPIO → Relay/SSR → Elemento Calefactor
```

### Opciones de Control

#### Opción 1: Relay Mecánico (ON/OFF)
```cpp
#define RELAY_PIN 4

// Control simple ON/OFF
void controlRelay(float outputPID) {
    if (outputPID > 50.0) {  // Threshold 50%
        digitalWrite(RELAY_PIN, HIGH);
    } else {
        digitalWrite(RELAY_PIN, LOW);
    }
}
```

#### Opción 2: SSR (Solid State Relay) con PWM
```cpp
#define RELAY_PIN 4
#define PWM_FREQUENCY 1000  // 1 kHz (SSR puede manejar esto)

void controlSSR(float outputPID) {
    // Convertir 0-100% a 0-255 (8-bit PWM)
    int pwmValue = (int)(outputPID * 2.55);
    analogWrite(RELAY_PIN, pwmValue);
}
```

#### Opción 3: SSR con Time Proportional Control
```cpp
#define RELAY_PIN 4
#define WINDOW_SIZE_MS 10000  // Ventana de 10 segundos

unsigned long windowStartTime = 0;
bool relayState = false;

void controlSSR_TimeProportional(float outputPID) {
    unsigned long now = millis();
    
    // Reiniciar ventana cada WINDOW_SIZE_MS
    if (now - windowStartTime >= WINDOW_SIZE_MS) {
        windowStartTime = now;
        unsigned long onTime = (unsigned long)(outputPID * WINDOW_SIZE_MS / 100.0);
        relayOnTime = onTime;
        relayState = true;  // Empezar ON
    }
    
    // Control dentro de la ventana
    unsigned long elapsed = now - windowStartTime;
    if (elapsed < relayOnTime) {
        digitalWrite(RELAY_PIN, HIGH);
    } else {
        digitalWrite(RELAY_PIN, LOW);
    }
}
```

### Recomendación

**SSR con Time Proportional Control** es la mejor opción para hornos:
- ✅ Prolonga vida útil del relay (menos ciclos)
- ✅ Control más suave que ON/OFF puro
- ✅ Compatible con SSR (no requiere PWM real)
- ✅ Eficiente energéticamente

---

## 🎯 Sincronización con la Curva

### Cálculo del Setpoint Dinámico

La clave está en calcular el setpoint en tiempo real según la rampa:

```cpp
// En TaskControlCurva, cada segundo:

// 1. Determinar segmento actual según tiempo transcurrido
int currentSegment = determineCurrentSegment(programStartTime, curvaActual);

// 2. Calcular tiempo dentro del segmento actual
unsigned long segmentStartTime = calculateSegmentStartTime(currentSegment);
unsigned long timeInSegment = (millis() - segmentStartTime) / 1000;  // segundos

// 3. Calcular temperatura objetivo según rampa
Segmento seg = curvaActual.segmentos[currentSegment];
float rampaPorSegundo = seg.rampaCporMin / 60.0;

// 4. Determinar si estamos en rampa o remojo
float tempInicioSegmento = (currentSegment == 0) ? 
    temperaturaActual : curvaActual.segmentos[currentSegment - 1].tempObjetivo;

float tiempoRampaSegundos = (seg.tempObjetivo - tempInicioSegmento) / rampaPorSegundo;

if (timeInSegment < tiempoRampaSegundos) {
    // FASE DE RAMPA
    setpointPID = tempInicioSegmento + (rampaPorSegundo * timeInSegment);
} else {
    // FASE DE REMOJO
    setpointPID = seg.tempObjetivo;  // Constante
}
```

### Ejemplo Práctico

```
Segmento: tempObjetivo = 980°C, rampaCporMin = 150°C/min, remojo = 30 min
Temp inicial: 25°C

Tiempo 0s:    setpointPID = 25°C + (150/60 * 0) = 25°C
Tiempo 60s:   setpointPID = 25°C + (150/60 * 60) = 175°C
Tiempo 120s:  setpointPID = 25°C + (150/60 * 120) = 325°C
...
Tiempo 2280s: setpointPID = 980°C (alcanzado tempObjetivo)
Tiempo 2281s-4080s: setpointPID = 980°C (remojo, constante)
```

---

## 📈 Manejo de Rampas

### Desafío: Seguir Rampas Precisas

El PID debe seguir una rampa, no solo alcanzar un setpoint fijo. Esto requiere:

#### 1. **Setpoint Dinámico**
- Actualizar setpointPID cada segundo según la rampa
- El PID "persigue" un setpoint que se mueve

#### 2. **Anticipación (Feedforward)**
- Opcional: Pre-calcular la potencia necesaria para la rampa
- Ayuda al PID a seguir la rampa más precisamente

```cpp
// Feedforward simple
float feedforward = calculateFeedforward(rampaCporMin, tempActual);
float outputPID_withFF = outputPID + feedforward;
```

#### 3. **Ajuste de Parámetros en Rampa vs Remojo**
- En rampa: Ki más bajo para evitar windup
- En remojo: Ki normal para eliminar error

```cpp
if (isInRampPhase) {
    myPID.SetTunings(Kp, Ki_ramp, Kd);  // Ki_ramp < Ki_normal
} else {
    myPID.SetTunings(Kp, Ki_normal, Kd);
}
```

---

## ⏱️ Manejo de Remojos

### Objetivo: Mantener Temperatura Constante

Durante el remojo, el setpoint es constante, pero el PID debe:
- Compensar pérdidas de calor
- Mantener temperatura estable
- Responder a perturbaciones (puerta abierta, etc.)

### Estrategia

```cpp
// Durante remojo:
setpointPID = seg.tempObjetivo;  // Constante

// El PID naturalmente mantendrá la temperatura
// Pero podemos optimizar:

// 1. Aumentar Ki ligeramente para mejor seguimiento
myPID.SetTunings(Kp, Ki_soak, Kd);  // Ki_soak > Ki_ramp

// 2. Verificar que la temperatura esté dentro de tolerancia
float tolerance = 5.0;  // ±5°C
if (abs(temperaturaActual - setpointPID) > tolerance) {
    // Alerta: temperatura fuera de rango
    Serial.printf("[PID] ⚠️ Temperatura fuera de rango: %.1f°C (objetivo: %.1f°C)\n",
                  temperaturaActual, setpointPID);
}
```

---

## ⏸️ Pausa y Reanudación

### Comportamiento Actual

```cpp
// Comando "PAUSE":
estadoHorno = "PAUSADO";
programPauseTime = millis();
// TaskControlCurva detecta y detiene actualización de setpoint
// TaskControlPID detecta y desactiva PID (output = 0)

// Comando "START" (resume):
estadoHorno = "CALENTANDO";
// Ajustar programStartTime para compensar tiempo pausado
unsigned long pauseDuration = millis() - programPauseTime;
programStartTime += pauseDuration;
programPauseTime = 0;
// TaskControlCurva continúa desde donde se pausó
// TaskControlPID reactiva PID
```

### Consideraciones para PID

#### 1. **Durante Pausa**
```cpp
// PID debe estar en modo manual (desactivado)
myPID.SetMode(QuickPID::Control::manual);
outputPID = 0;  // Apagar relay
```

#### 2. **Al Reanudar**
```cpp
// Reactivar PID
myPID.SetMode(QuickPID::Control::automatic);

// Opcional: Resetear integral para evitar salto
// (QuickPID puede tener función para esto)
myPID.Reset();  // Si está disponible
```

#### 3. **Cálculo de Setpoint al Reanudar**
```cpp
// El setpoint debe calcularse desde la temperatura actual
// No desde donde se pausó (la temperatura puede haber bajado)
float currentTemp = temperaturaActual;
int currentSegment = determineCurrentSegment(programStartTime, curvaActual);
// Recalcular setpoint desde currentTemp y currentSegment
```

---

## 🛡️ Seguridad y Protecciones

### Protecciones Críticas

#### 1. **Límite de Temperatura Máxima**
```cpp
#define MAX_TEMP_SAFE 1200.0  // °C (ajustar según horno)

if (temperaturaActual > MAX_TEMP_SAFE) {
    // EMERGENCIA: Apagar inmediatamente
    digitalWrite(RELAY_PIN, LOW);
    myPID.SetMode(QuickPID::Control::manual);
    estadoHorno = "DETENIDO";
    Serial.println("[SAFETY] ⛔ TEMPERATURA MÁXIMA EXCEDIDA - SISTEMA DETENIDO");
}
```

#### 2. **Detección de Termocupla Desconectada**
```cpp
// MAX31855 puede detectar termocupla desconectada
if (thermocouple.readError() != 0) {
    // Error en lectura
    digitalWrite(RELAY_PIN, LOW);
    myPID.SetMode(QuickPID::Control::manual);
    Serial.println("[SAFETY] ⛔ ERROR EN TERMOCUPLA - SISTEMA DETENIDO");
}
```

#### 3. **Timeout de Comunicación**
```cpp
// Si no hay actualización de temperatura en X segundos
unsigned long lastTempUpdate = 0;
#define TEMP_UPDATE_TIMEOUT_MS 10000  // 10 segundos

if (millis() - lastTempUpdate > TEMP_UPDATE_TIMEOUT_MS) {
    // No hay lectura de temperatura reciente
    digitalWrite(RELAY_PIN, LOW);
    Serial.println("[SAFETY] ⛔ TIMEOUT EN LECTURA DE TEMPERATURA");
}
```

#### 4. **Protección contra Windup Integral**
```cpp
// QuickPID tiene anti-windup integrado, pero verificar:
myPID.SetOutputLimits(0, 100);  // Limitar salida
// Esto previene que la integral se acumule indefinidamente
```

#### 5. **Verificación de Rampa Excesiva**
```cpp
// Verificar que la rampa no sea demasiado rápida
if (rampaCporMin > MAX_RAMP_RATE) {
    Serial.printf("[SAFETY] ⚠️ Rampa muy rápida: %.1f°C/min (máx: %.1f°C/min)\n",
                  rampaCporMin, MAX_RAMP_RATE);
    // Opcional: Limitar rampa
    rampaCporMin = MAX_RAMP_RATE;
}
```

---

## 💻 Estructura de Código

### Archivos a Crear/Modificar

```
IA Kiln/
├── src/
│   ├── main.cpp                    (modificar)
│   └── pid_control.cpp             (nuevo)
├── include/
│   ├── pid_control.h               (nuevo)
│   └── display_lvgl.h              (sin cambios)
└── platformio.ini                 (modificar: agregar QuickPID)
```

### Estructura de `pid_control.h`

```cpp
#ifndef PID_CONTROL_H
#define PID_CONTROL_H

#include <QuickPID.h>

// Configuración
#define RELAY_PIN 4
#define PID_UPDATE_INTERVAL_MS 100
#define PID_OUTPUT_MIN 0.0
#define PID_OUTPUT_MAX 100.0

// Estados de autotune
enum AutotuneState {
    AUTOTUNE_IDLE,
    AUTOTUNE_RUNNING,
    AUTOTUNE_COMPLETE,
    AUTOTUNE_FAILED
};

// Estructura de parámetros PID
struct PIDParams {
    float Kp;
    float Ki;
    float Kd;
    bool loaded;
};

// Variables globales (extern)
extern QuickPID myPID;
extern float setpointPID;
extern float outputPID;
extern bool pidEnabled;
extern AutotuneState autotuneState;

// Funciones
void initPID();
void updatePID();
void startAutotune(float testSetpoint);
void stopAutotune();
void loadPIDParams();
void savePIDParams();
void controlRelay(float output);

#endif
```

### Estructura de `pid_control.cpp`

```cpp
#include "pid_control.h"
#include <Preferences.h>

// Variables globales
QuickPID myPID(&temperaturaActual, &outputPID, &setpointPID, 
               Kp_default, Ki_default, Kd_default,
               QuickPID::Action::direct);

float setpointPID = 25.0;
float outputPID = 0.0;
bool pidEnabled = false;
AutotuneState autotuneState = AUTOTUNE_IDLE;

// Funciones de inicialización
void initPID() {
    // Cargar parámetros guardados
    loadPIDParams();
    
    // Configurar PID
    myPID.SetOutputLimits(PID_OUTPUT_MIN, PID_OUTPUT_MAX);
    myPID.SetMode(QuickPID::Control::manual);  // Iniciar desactivado
    
    // Configurar pin de relay
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);
}

// Función principal de actualización (llamar desde tarea)
void updatePID() {
    if (!pidEnabled) {
        outputPID = 0.0;
        controlRelay(0.0);
        return;
    }
    
    // Calcular output del PID
    myPID.Compute();
    
    // Controlar relay
    controlRelay(outputPID);
}

// ... más funciones
```

---

## 📦 Variables y Estructuras

### Variables Globales Nuevas

```cpp
// En main.cpp o pid_control.h

// PID Control
QuickPID myPID;
float setpointPID = 25.0;        // Setpoint dinámico (actualizado por TaskControlCurva)
float outputPID = 0.0;           // Salida del PID (0-100%)
bool pidEnabled = false;         // Flag para habilitar/deshabilitar PID
AutotuneState autotuneState;     // Estado del autotune

// Parámetros PID (cargados desde NVS o defaults)
struct PIDParams {
    float Kp = 2.0;    // Default, se ajusta con autotune
    float Ki = 0.05;
    float Kd = 20.0;
} pidParams;

// Control de relay
int relayPin = 4;
bool relayState = false;
unsigned long relayWindowStart = 0;
unsigned long relayOnTime = 0;
```

### Modificaciones a Variables Existentes

```cpp
// estadoHorno: Agregar nuevo estado
// "AUTOTUNE" → Durante ejecución de autotune

// curvaActual: Sin cambios (ya tiene toda la info necesaria)
```

---

## 🔄 Tareas FreeRTOS

### Tarea Nueva: TaskControlPID

```cpp
void TaskControlPID(void* pvParameters) {
    const TickType_t xFrequency = pdMS_TO_TICKS(PID_UPDATE_INTERVAL_MS);
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    initPID();
    
    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        // Verificar estado del horno
        bool shouldRun = false;
        if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            shouldRun = (strcmp(estadoHorno, "CALENTANDO") == 0) || 
                       (strcmp(estadoHorno, "AUTOTUNE") == 0);
            xSemaphoreGive(xMutex);
        }
        
        if (shouldRun) {
            pidEnabled = true;
            myPID.SetMode(QuickPID::Control::automatic);
        } else {
            pidEnabled = false;
            myPID.SetMode(QuickPID::Control::manual);
            outputPID = 0.0;
        }
        
        // Actualizar PID
        updatePID();
        
        // Manejar autotune si está activo
        if (autotuneState == AUTOTUNE_RUNNING) {
            handleAutotune();
        }
    }
}
```

### Modificaciones a TaskControlCurva

```cpp
void TaskControlCurva(void* pvParameters) {
    for (;;) {
        // ... código existente ...
        
        // NUEVO: Calcular y actualizar setpointPID
        if (strcmp(estadoHorno, "CALENTANDO") == 0 && curvaActual.recibida) {
            // Determinar segmento actual
            int currentSegment = determineCurrentSegment();
            
            // Calcular setpoint dinámico
            float newSetpoint = calculateDynamicSetpoint(currentSegment);
            
            // Actualizar setpointPID (protegido con mutex)
            if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                setpointPID = newSetpoint;
                xSemaphoreGive(xMutex);
            }
        }
        
        // ... resto del código ...
    }
}
```

### TaskControlHorno (Sin Cambios)

```cpp
// Ya lee la termocupla y actualiza temperaturaActual
// No requiere cambios
```

---

## 📡 Comunicación con el Sistema

### Comandos BLE Nuevos

```cpp
// En el callback de BLE_COMMAND_CHAR:

if (cmd == "AUTOTUNE") {
    // Iniciar autotune
    if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        strncpy(estadoHorno, "AUTOTUNE", sizeof(estadoHorno) - 1);
        estadoHorno[sizeof(estadoHorno) - 1] = '\0';
        xSemaphoreGive(xMutex);
    }
    
    // Calcular setpoint de prueba
    float testSetpoint = temperaturaActual + 50.0;  // +50°C desde actual
    startAutotune(testSetpoint);
    
    Serial.println("[CMD] Autotune iniciado");
}
```

### Reporte a Firebase

```cpp
// En reportToFirebase(), agregar información del PID:
void reportToFirebase(float temp, const char* estado) {
    // ... código existente ...
    
    // Agregar datos del PID
    char pidPath[200];
    snprintf(pidPath, sizeof(pidPath), 
             "/users/%s/kilns/%s/realtimeData/pid",
             userId, deviceMacAddressNoColon);
    
    FirebaseJson pidData;
    pidData.set("setpoint", setpointPID);
    pidData.set("output", outputPID);
    pidData.set("kp", pidParams.Kp);
    pidData.set("ki", pidParams.Ki);
    pidData.set("kd", pidParams.Kd);
    
    Firebase.RTDB.setJSON(fbdo, pidPath, &pidData);
}
```

### Display LVGL

```cpp
// En TaskUpdateDisplayLVGL, mostrar información del PID:
displayData.pidOutput = outputPID;  // 0-100%
displayData.pidSetpoint = setpointPID;
// Actualizar display con estos valores
```

---

## 🎯 Resumen del Flujo Completo

### Secuencia de Inicio de Programa

```
1. APP envía curva → curvaActual.recibida = true
2. APP envía "START" → estadoHorno = "CALENTANDO"
3. TaskControlCurva:
   - Calcula setpointPID inicial (tempActual + rampa)
   - Actualiza setpointPID cada segundo
4. TaskControlPID:
   - Lee temperaturaActual
   - Lee setpointPID
   - QuickPID.Compute() → outputPID
   - controlRelay(outputPID)
5. TaskControlHorno:
   - Lee termocupla → temperaturaActual
6. Loop continúa hasta finalizar programa
```

### Secuencia de Autotune

```
1. APP envía "AUTOTUNE" → estadoHorno = "AUTOTUNE"
2. TaskControlPID:
   - Verifica condiciones (temp en rango seguro)
   - Inicia QuickPID autotune
   - Alterna relay ON/OFF
   - Mide oscilaciones
3. Autotune completo:
   - Calcula Kp, Ki, Kd
   - Guarda en NVS
   - Carga en QuickPID
   - estadoHorno = "IDLE"
```

---

## ✅ Checklist de Implementación

### Fase 1: Preparación
- [ ] Agregar QuickPID a platformio.ini
- [ ] Crear pid_control.h y pid_control.cpp
- [ ] Definir pin de relay/SSR
- [ ] Configurar hardware de control

### Fase 2: PID Básico
- [ ] Implementar initPID()
- [ ] Implementar updatePID()
- [ ] Crear TaskControlPID
- [ ] Implementar controlRelay()
- [ ] Probar con setpoint fijo

### Fase 3: Integración con Curva
- [ ] Modificar TaskControlCurva para calcular setpointPID
- [ ] Implementar calculateDynamicSetpoint()
- [ ] Sincronizar con segmentos de curva
- [ ] Probar seguimiento de rampas

### Fase 4: Autotune
- [ ] Implementar startAutotune()
- [ ] Implementar handleAutotune()
- [ ] Guardar/cargar parámetros en NVS
- [ ] Agregar comando "AUTOTUNE" en BLE
- [ ] Probar autotune completo

### Fase 5: Seguridad
- [ ] Implementar protecciones de temperatura
- [ ] Detección de errores de termocupla
- [ ] Timeouts y validaciones
- [ ] Logs de seguridad

### Fase 6: Integración Completa
- [ ] Integrar con Firebase (reportar datos PID)
- [ ] Actualizar display LVGL con info PID
- [ ] Probar pausa/reanudación
- [ ] Probar con curva completa
- [ ] Optimización y ajustes finales

---

## 📝 Notas Finales

### Consideraciones Importantes

1. **Frecuencia de Actualización PID**:
   - Recomendado: 100-500ms
   - Más rápido = mejor control, pero más carga de CPU
   - Para hornos: 200-500ms es suficiente

2. **Filtrado de Temperatura**:
   - La termocupla puede tener ruido
   - Considerar filtro de media móvil
   - Ejemplo: `tempFiltrada = 0.8 * tempFiltrada + 0.2 * tempActual`

3. **Ajuste Manual de Parámetros**:
   - Después del autotune, puede requerir ajustes finos
   - Considerar comandos BLE para ajustar Kp, Ki, Kd manualmente

4. **Múltiples Zonas** (Futuro):
   - Si el horno tiene múltiples elementos calefactores
   - Cada zona necesita su propio PID
   - Coordinación entre zonas

5. **Eficiencia Energética**:
   - Time Proportional Control reduce ciclos del relay
   - Prolonga vida útil del hardware
   - Reduce consumo eléctrico

---

*Este plan proporciona una base sólida para implementar control PID completo con QuickPID en el sistema SmartKiln.*




