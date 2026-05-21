# 🔧 AUTOTUNE DETALLADO: Funcionamiento y Autotune por Rangos

## 📋 ÍNDICE
1. [Cómo Funciona el Autotune Actual de QuickPID](#cómo-funciona-el-autotune-actual)
2. [Almacenamiento de Parámetros](#almacenamiento-de-parámetros)
3. [Autotune por Rangos de Temperatura](#autotune-por-rangos-de-temperatura)
4. [Implementación Propuesta](#implementación-propuesta)
5. [Estructura de Datos](#estructura-de-datos)

---

## 🎯 CÓMO FUNCIONA EL AUTOTUNE ACTUAL

### Método: Relé (Relay Method) - Ziegler-Nichols

El autotune de QuickPID usa el **método Relé**, también conocido como **método de Ziegler-Nichols en lazo cerrado**.

### Proceso Paso a Paso

#### **Fase 1: Preparación**
```
1. El usuario inicia autotune desde la app (comando BLE: "AUTOTUNE")
2. El sistema verifica condiciones:
   - Temperatura actual debe estar en rango seguro (ej: 25-200°C)
   - El horno debe estar en estado IDLE o PAUSADO
   - No debe haber un programa ejecutándose
3. Se establece un setpoint de prueba:
   setpointTest = temperaturaActual + 50°C
   (ej: si tempActual = 100°C, setpointTest = 150°C)
```

#### **Fase 2: Oscilación Inducida (5-15 minutos)**
```
El autotune alterna el relay entre ON y OFF completamente:

Estado 1: Relay ON (100% potencia)
  └─> Temperatura sube
  └─> Cuando temp > setpointTest + noiseBand → Cambia a OFF

Estado 2: Relay OFF (0% potencia)
  └─> Temperatura baja
  └─> Cuando temp < setpointTest - noiseBand → Cambia a ON

Este ciclo se repite creando oscilaciones alrededor del setpoint.
```

**Parámetros de control:**
- `noiseBand`: Banda de ruido (ej: 1.0-2.0°C)
  - Define cuánto debe pasar el setpoint antes de cambiar de estado
  - Evita cambios demasiado frecuentes por ruido del sensor
- `lookBackTime`: Tiempo de análisis (ej: 60000ms = 1 minuto)
  - Período durante el cual se analizan las oscilaciones

#### **Fase 3: Análisis de Oscilaciones**

Durante las oscilaciones, QuickPID mide:

1. **Período de Oscilación (Pu)**: Tiempo entre dos picos consecutivos
   ```
   Pu = tiempo entre pico máximo y siguiente pico máximo
   Ejemplo: Si oscila cada 120 segundos → Pu = 120s
   ```

2. **Amplitud de Oscilación**: Diferencia entre pico máximo y mínimo
   ```
   Amplitud = tempMax - tempMin
   Ejemplo: Si oscila entre 145°C y 155°C → Amplitud = 10°C
   ```

3. **Ganancia Crítica (Ku)**: Ganancia que causa oscilaciones sostenidas
   ```
   Ku se calcula basándose en la amplitud y el período
   ```

#### **Fase 4: Cálculo de Parámetros PID**

QuickPID aplica las **fórmulas de Ziegler-Nichols**:

```
Kp = 0.6 × Ku
Ki = 2 × Kp / Pu
Kd = Kp × Pu / 8
```

**Ejemplo Real:**
```
Si durante autotune se obtiene:
  - Pu = 120 segundos (período de oscilación)
  - Ku = 5.0 (ganancia crítica calculada)

Entonces:
  Kp = 0.6 × 5.0 = 3.0
  Ki = 2 × 3.0 / 120 = 0.05
  Kd = 3.0 × 120 / 8 = 45.0
```

#### **Fase 5: Finalización**

```
1. QuickPID calcula los parámetros finales
2. Se guardan en NVS (Preferences)
3. Se cargan inmediatamente en el controlador PID
4. El sistema vuelve al estado anterior (IDLE)
5. Los parámetros quedan activos para uso inmediato
```

---

## 💾 ALMACENAMIENTO DE PARÁMETROS

### Ubicación: NVS (Non-Volatile Storage)

El ESP32-S3 tiene memoria flash no volátil donde se almacenan los parámetros usando la librería `Preferences`.

### Estructura Actual (Autotune Simple)

```cpp
// Namespace en NVS
preferences.begin("pid_config", false);

// Parámetros guardados:
preferences.putFloat("Kp", 3.0);
preferences.putFloat("Ki", 0.05);
preferences.putFloat("Kd", 45.0);
preferences.putBool("autotune_done", true);
preferences.putULong("autotune_timestamp", millis());

preferences.end();
```

### Carga al Iniciar

```cpp
void loadPIDParams() {
    preferences.begin("pid_config", false);
    
    float Kp = preferences.getFloat("Kp", 2.0);  // Valor por defecto si no existe
    float Ki = preferences.getFloat("Ki", 0.05);
    float Kd = preferences.getFloat("Kd", 20.0);
    bool autotuneDone = preferences.getBool("autotune_done", false);
    
    preferences.end();
    
    // Aplicar a QuickPID
    myPID.SetTunings(Kp, Ki, Kd);
}
```

### Persistencia

- ✅ **Los parámetros persisten** después de reiniciar el ESP32
- ✅ **Se mantienen** incluso si se corta la alimentación
- ✅ **Se pueden actualizar** en cualquier momento
- ✅ **Ocupan ~20 bytes** en flash (muy poco)

---

## 🌡️ AUTOTUNE POR RANGOS DE TEMPERATURA

### ¿Por Qué Autotune por Rangos?

Los hornos de cerámica tienen **dinámicas diferentes** según la temperatura:

1. **Bajas temperaturas (25-300°C)**:
   - Respuesta más lenta
   - Menos pérdidas de calor
   - Kp más bajo, Ki más bajo

2. **Temperaturas medias (300-800°C)**:
   - Respuesta moderada
   - Pérdidas de calor moderadas
   - Kp medio, Ki medio

3. **Altas temperaturas (800-1300°C)**:
   - Respuesta más rápida
   - Más pérdidas de calor
   - Kp más alto, Ki más alto

### Estrategia: Múltiples Autotunes

En lugar de un solo autotune, se ejecutan **múltiples autotunes** en diferentes rangos:

```
Rango 1: 100-200°C   → Autotune → Kp1, Ki1, Kd1
Rango 2: 400-500°C   → Autotune → Kp2, Ki2, Kd2
Rango 3: 700-800°C   → Autotune → Kp3, Ki3, Kd3
Rango 4: 1000-1100°C → Autotune → Kp4, Ki4, Kd4
```

### Selección Dinámica de Parámetros

Durante la operación normal, el sistema **selecciona automáticamente** los parámetros según la temperatura actual:

```cpp
// Pseudocódigo
float getPIDParams(float temperaturaActual) {
    if (temperaturaActual < 250) {
        return paramsRango1;  // Kp1, Ki1, Kd1
    } else if (temperaturaActual < 550) {
        return paramsRango2;  // Kp2, Ki2, Kd2
    } else if (temperaturaActual < 900) {
        return paramsRango3;  // Kp3, Ki3, Kd3
    } else {
        return paramsRango4;  // Kp4, Ki4, Kd4
    }
}
```

### Interpolación Lineal (Opcional)

Para transiciones más suaves entre rangos, se puede usar **interpolación**:

```cpp
// Ejemplo: temperaturaActual = 275°C (entre Rango1 y Rango2)
// Rango1: 100-200°C, Rango2: 400-500°C
// Interpolación entre 200°C y 400°C

float factor = (temperaturaActual - 200) / (400 - 200);  // 0.375
Kp = Kp1 + (Kp2 - Kp1) * factor;
Ki = Ki1 + (Ki2 - Ki1) * factor;
Kd = Kd1 + (Kd2 - Kd1) * factor;
```

---

## 🏗️ IMPLEMENTACIÓN PROPUESTA

### Estructura de Datos

```cpp
// Definición de un rango de temperatura
struct PIDRange {
    float tempMin;      // Temperatura mínima del rango
    float tempMax;      // Temperatura máxima del rango
    float Kp;          // Ganancia proporcional para este rango
    float Ki;          // Ganancia integral para este rango
    float Kd;          // Ganancia derivativa para este rango
    bool autotuned;    // true si ya se hizo autotune en este rango
    unsigned long timestamp;  // Cuándo se hizo el autotune
};

// Configuración de rangos
#define MAX_PID_RANGES 5
PIDRange pidRanges[MAX_PID_RANGES] = {
    {0, 250, 2.0, 0.02, 15.0, false, 0},      // Rango 1: 0-250°C
    {250, 550, 3.0, 0.05, 25.0, false, 0},    // Rango 2: 250-550°C
    {550, 900, 4.0, 0.08, 35.0, false, 0},    // Rango 3: 550-900°C
    {900, 1150, 5.0, 0.1, 45.0, false, 0},    // Rango 4: 900-1150°C
    {1150, 1300, 6.0, 0.12, 50.0, false, 0}   // Rango 5: 1150-1300°C
};
```

### Almacenamiento en NVS

```cpp
void savePIDRanges() {
    preferences.begin("pid_ranges", false);
    
    // Guardar número de rangos
    preferences.putUChar("num_ranges", MAX_PID_RANGES);
    
    // Guardar cada rango
    for (int i = 0; i < MAX_PID_RANGES; i++) {
        String prefix = "range_" + String(i) + "_";
        
        preferences.putFloat((prefix + "tempMin").c_str(), pidRanges[i].tempMin);
        preferences.putFloat((prefix + "tempMax").c_str(), pidRanges[i].tempMax);
        preferences.putFloat((prefix + "Kp").c_str(), pidRanges[i].Kp);
        preferences.putFloat((prefix + "Ki").c_str(), pidRanges[i].Ki);
        preferences.putFloat((prefix + "Kd").c_str(), pidRanges[i].Kd);
        preferences.putBool((prefix + "autotuned").c_str(), pidRanges[i].autotuned);
        preferences.putULong((prefix + "timestamp").c_str(), pidRanges[i].timestamp);
    }
    
    preferences.end();
    Serial.println("[PID] Parámetros por rangos guardados en NVS");
}

void loadPIDRanges() {
    preferences.begin("pid_ranges", false);
    
    uint8_t numRanges = preferences.getUChar("num_ranges", MAX_PID_RANGES);
    
    for (int i = 0; i < numRanges && i < MAX_PID_RANGES; i++) {
        String prefix = "range_" + String(i) + "_";
        
        pidRanges[i].tempMin = preferences.getFloat((prefix + "tempMin").c_str(), 0);
        pidRanges[i].tempMax = preferences.getFloat((prefix + "tempMax").c_str(), 0);
        pidRanges[i].Kp = preferences.getFloat((prefix + "Kp").c_str(), 2.0);
        pidRanges[i].Ki = preferences.getFloat((prefix + "Ki").c_str(), 0.05);
        pidRanges[i].Kd = preferences.getFloat((prefix + "Kd").c_str(), 20.0);
        pidRanges[i].autotuned = preferences.getBool((prefix + "autotuned").c_str(), false);
        pidRanges[i].timestamp = preferences.getULong((prefix + "timestamp").c_str(), 0);
    }
    
    preferences.end();
    Serial.println("[PID] Parámetros por rangos cargados desde NVS");
}
```

### Selección de Parámetros por Temperatura

```cpp
// Obtener parámetros PID según temperatura actual
void getPIDParamsForTemperature(float temperatura, float* Kp, float* Ki, float* Kd) {
    // Buscar el rango que contiene esta temperatura
    for (int i = 0; i < MAX_PID_RANGES; i++) {
        if (temperatura >= pidRanges[i].tempMin && temperatura < pidRanges[i].tempMax) {
            *Kp = pidRanges[i].Kp;
            *Ki = pidRanges[i].Ki;
            *Kd = pidRanges[i].Kd;
            return;
        }
    }
    
    // Si no se encuentra, usar el último rango (temperatura muy alta)
    *Kp = pidRanges[MAX_PID_RANGES - 1].Kp;
    *Ki = pidRanges[MAX_PID_RANGES - 1].Ki;
    *Kd = pidRanges[MAX_PID_RANGES - 1].Kd;
}

// Con interpolación lineal entre rangos
void getPIDParamsInterpolated(float temperatura, float* Kp, float* Ki, float* Kd) {
    // Buscar rango actual y siguiente
    int currentRange = -1;
    for (int i = 0; i < MAX_PID_RANGES - 1; i++) {
        if (temperatura >= pidRanges[i].tempMin && temperatura < pidRanges[i].tempMax) {
            currentRange = i;
            break;
        }
    }
    
    if (currentRange == -1) {
        // Usar último rango
        *Kp = pidRanges[MAX_PID_RANGES - 1].Kp;
        *Ki = pidRanges[MAX_PID_RANGES - 1].Ki;
        *Kd = pidRanges[MAX_PID_RANGES - 1].Kd;
        return;
    }
    
    // Interpolar entre rango actual y siguiente
    int nextRange = currentRange + 1;
    float rangeSize = pidRanges[nextRange].tempMin - pidRanges[currentRange].tempMin;
    float position = temperatura - pidRanges[currentRange].tempMin;
    float factor = position / rangeSize;  // 0.0 a 1.0
    
    *Kp = pidRanges[currentRange].Kp + 
          (pidRanges[nextRange].Kp - pidRanges[currentRange].Kp) * factor;
    *Ki = pidRanges[currentRange].Ki + 
          (pidRanges[nextRange].Ki - pidRanges[currentRange].Ki) * factor;
    *Kd = pidRanges[currentRange].Kd + 
          (pidRanges[nextRange].Kd - pidRanges[currentRange].Kd) * factor;
}
```

### Autotune por Rango Específico

```cpp
// Ejecutar autotune para un rango específico
bool autotuneRange(int rangeIndex) {
    if (rangeIndex < 0 || rangeIndex >= MAX_PID_RANGES) {
        return false;
    }
    
    PIDRange* range = &pidRanges[rangeIndex];
    
    // Calcular setpoint de prueba (centro del rango)
    float setpointTest = (range->tempMin + range->tempMax) / 2.0;
    
    Serial.printf("[AUTOTUNE] Iniciando autotune para rango %d (%0.1f-%0.1f°C)\n",
                  rangeIndex, range->tempMin, range->tempMax);
    Serial.printf("[AUTOTUNE] Setpoint de prueba: %0.1f°C\n", setpointTest);
    
    // Verificar que la temperatura actual esté cerca del rango
    float tempActual = readTemperature();
    if (tempActual < range->tempMin - 50 || tempActual > range->tempMax + 50) {
        Serial.printf("[AUTOTUNE] ERROR: Temperatura actual (%0.1f°C) fuera del rango\n", tempActual);
        return false;
    }
    
    // Configurar QuickPID autotune
    QuickPID myPID;
    myPID.SetMode(QUICKPID_MODE_AUTOTUNE);
    myPID.SetOutputLimits(0, 100);
    
    // Configurar autotune
    myPID.SetAutotune(true);
    myPID.SetSetpoint(setpointTest);
    
    // Ejecutar autotune (esto toma 5-15 minutos)
    unsigned long autotuneStart = millis();
    bool autotuneComplete = false;
    
    while (!autotuneComplete && (millis() - autotuneStart < 900000)) {  // Máximo 15 minutos
        float temp = readTemperature();
        float output = myPID.Compute(temp);
        controlRelay(output);
        
        // Verificar si autotune completó
        if (myPID.GetAutotuneStatus() == QUICKPID_AUTOTUNE_COMPLETE) {
            autotuneComplete = true;
            
            // Obtener parámetros calculados
            range->Kp = myPID.GetKp();
            range->Ki = myPID.GetKi();
            range->Kd = myPID.GetKd();
            range->autotuned = true;
            range->timestamp = millis();
            
            Serial.printf("[AUTOTUNE] ✅ Completado para rango %d\n", rangeIndex);
            Serial.printf("[AUTOTUNE] Kp=%0.3f, Ki=%0.3f, Kd=%0.3f\n",
                          range->Kp, range->Ki, range->Kd);
            
            // Guardar en NVS
            savePIDRanges();
            
            return true;
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));  // 100ms loop
    }
    
    Serial.println("[AUTOTUNE] ❌ Timeout o error");
    return false;
}
```

### Autotune Secuencial de Todos los Rangos

```cpp
// Ejecutar autotune para todos los rangos (uno por uno)
void autotuneAllRanges() {
    Serial.println("[AUTOTUNE] Iniciando autotune secuencial de todos los rangos");
    Serial.println("[AUTOTUNE] ⚠️ Esto tomará aproximadamente 1-2 horas");
    
    for (int i = 0; i < MAX_PID_RANGES; i++) {
        Serial.printf("\n[AUTOTUNE] ===== RANGO %d/%d =====\n", i+1, MAX_PID_RANGES);
        
        // Calentar hasta el rango objetivo
        float targetTemp = (pidRanges[i].tempMin + pidRanges[i].tempMax) / 2.0;
        Serial.printf("[AUTOTUNE] Calentando hasta %0.1f°C...\n", targetTemp);
        
        // Esperar a que la temperatura esté en el rango
        while (readTemperature() < pidRanges[i].tempMin) {
            controlRelay(100.0);  // Calentar al máximo
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        
        // Esperar estabilización
        Serial.println("[AUTOTUNE] Esperando estabilización...");
        vTaskDelay(pdMS_TO_TICKS(60000));  // 1 minuto
        
        // Ejecutar autotune
        if (!autotuneRange(i)) {
            Serial.printf("[AUTOTUNE] ❌ Falló autotune para rango %d\n", i);
            // Continuar con el siguiente rango
        }
        
        // Esperar entre rangos
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
    
    Serial.println("[AUTOTUNE] ✅ Autotune completo para todos los rangos");
}
```

### Actualización Dinámica de Parámetros

```cpp
// En TaskControlPID, actualizar parámetros según temperatura
void TaskControlPID(void *pvParameters) {
    float lastTemp = 0;
    int currentRange = -1;
    
    while (1) {
        float tempActual = readTemperature();
        
        // Detectar cambio de rango
        int newRange = -1;
        for (int i = 0; i < MAX_PID_RANGES; i++) {
            if (tempActual >= pidRanges[i].tempMin && tempActual < pidRanges[i].tempMax) {
                newRange = i;
                break;
            }
        }
        
        // Si cambió de rango, actualizar parámetros PID
        if (newRange != currentRange && newRange != -1) {
            currentRange = newRange;
            float Kp, Ki, Kd;
            getPIDParamsForTemperature(tempActual, &Kp, &Ki, &Kd);
            myPID.SetTunings(Kp, Ki, Kd);
            
            Serial.printf("[PID] Cambio de rango: %d, Kp=%0.3f, Ki=%0.3f, Kd=%0.3f\n",
                          currentRange, Kp, Ki, Kd);
        }
        
        // Calcular output PID
        float setpoint = getCurrentSetpoint();  // Desde TaskControlCurva
        float output = myPID.Compute(tempActual, setpoint);
        controlRelay(output);
        
        vTaskDelay(pdMS_TO_TICKS(100));  // 100ms
    }
}
```

---

## 📊 ESTRUCTURA DE DATOS COMPLETA

### En NVS (Preferences)

```
Namespace: "pid_ranges"
├── num_ranges (uint8_t): 5
├── range_0_tempMin (float): 0.0
├── range_0_tempMax (float): 250.0
├── range_0_Kp (float): 2.0
├── range_0_Ki (float): 0.02
├── range_0_Kd (float): 15.0
├── range_0_autotuned (bool): true
├── range_0_timestamp (unsigned long): 1234567890
├── range_1_tempMin (float): 250.0
├── range_1_tempMax (float): 550.0
├── range_1_Kp (float): 3.0
├── range_1_Ki (float): 0.05
├── range_1_Kd (float): 25.0
├── range_1_autotuned (bool): true
├── range_1_timestamp (unsigned long): 1234567890
└── ... (similar para rangos 2, 3, 4)
```

### Tamaño Estimado en Flash

```
Cada rango: ~30 bytes
5 rangos: ~150 bytes
Total: ~200 bytes (muy poco, NVS puede almacenar varios KB)
```

---

## 🎮 COMANDOS BLE PARA AUTOTUNE

### Comandos Propuestos

```cpp
// Desde la app, enviar comandos BLE:

"AUTOTUNE_RANGE:0"     // Autotune solo rango 0 (0-250°C)
"AUTOTUNE_RANGE:1"     // Autotune solo rango 1 (250-550°C)
"AUTOTUNE_ALL"         // Autotune todos los rangos secuencialmente
"GET_PID_RANGES"       // Obtener parámetros de todos los rangos
"SET_PID_RANGE:0:3.5:0.06:30"  // Establecer manualmente Kp, Ki, Kd para rango 0
```

### Respuestas

```json
// GET_PID_RANGES responde con:
{
  "ranges": [
    {
      "index": 0,
      "tempMin": 0,
      "tempMax": 250,
      "Kp": 2.0,
      "Ki": 0.02,
      "Kd": 15.0,
      "autotuned": true,
      "timestamp": 1234567890
    },
    // ... más rangos
  ]
}
```

---

## ✅ VENTAJAS DEL AUTOTUNE POR RANGOS

1. **Control más preciso**: Parámetros optimizados para cada rango de temperatura
2. **Mejor seguimiento de rampas**: Transiciones más suaves entre temperaturas
3. **Estabilidad mejorada**: Menos oscilaciones en remojos
4. **Adaptabilidad**: Se puede re-autotunar un rango específico sin afectar otros
5. **Flexibilidad**: Parámetros manuales por rango si es necesario

## ⚠️ CONSIDERACIONES

1. **Tiempo**: Autotune completo puede tomar 1-2 horas (5 rangos × 15 min)
2. **Temperatura**: Cada autotune requiere que el horno esté en el rango objetivo
3. **Energía**: Consumo significativo durante autotune completo
4. **Precisión**: Los rangos deben solaparse ligeramente para evitar saltos bruscos

---

## 🔄 FLUJO COMPLETO

```
1. Usuario inicia autotune desde app
   └─> Comando BLE: "AUTOTUNE_RANGE:2"

2. Sistema verifica condiciones
   └─> Temperatura actual en rango seguro
   └─> Horno en estado IDLE

3. Calentar hasta rango objetivo (si es necesario)
   └─> Rango 2: 400-500°C
   └─> Setpoint de prueba: 450°C

4. Ejecutar autotune (5-15 minutos)
   └─> QuickPID alterna relay ON/OFF
   └─> Mide oscilaciones
   └─> Calcula Kp, Ki, Kd

5. Guardar parámetros
   └─> Actualizar pidRanges[2]
   └─> Guardar en NVS
   └─> Marcar como autotuned

6. Notificar completado
   └─> BLE: "AUTOTUNE_COMPLETE:RANGE:2"
   └─> Firebase: Actualizar estado

7. Parámetros activos
   └─> Cuando temperatura entre 400-500°C
   └─> TaskControlPID usa automáticamente estos parámetros
```

---

## 📝 RESUMEN

- **Autotune actual**: Se ejecuta una vez, calcula Kp/Ki/Kd, se guarda en NVS
- **Autotune por rangos**: Múltiples autotunes, uno por rango de temperatura
- **Almacenamiento**: NVS (Preferences), persiste después de reinicio
- **Selección**: Automática según temperatura actual
- **Ventaja**: Control más preciso en todo el rango de temperaturas

---

## 🚀 PROYECCIÓN DE PARÁMETROS: AUTOTUNE EN PRIMEROS RANGOS

### ¿Por Qué Proyectar Parámetros?

Ejecutar autotune en **todos** los rangos puede ser:
- ⏱️ **Lento**: 1-2 horas (5 rangos × 15-30 min cada uno)
- ⚡ **Costoso**: Alto consumo de energía
- 🔥 **Riesgoso**: Requiere alcanzar temperaturas muy altas (1000°C+)

**Solución**: Ejecutar autotune solo en los **primeros 2 rangos** (bajas temperaturas) y **proyectar** los parámetros para los rangos superiores.

### Ventajas de la Proyección

✅ **Tiempo reducido**: ~30 minutos en lugar de 1-2 horas  
✅ **Menor consumo**: Solo calentar hasta ~500°C  
✅ **Más seguro**: No requiere temperaturas extremas  
✅ **Suficientemente preciso**: Los parámetros siguen patrones predecibles  

---

## 📊 MÉTODOS DE PROYECCIÓN

### Método 1: Extrapolación Lineal (Simple)

**Concepto**: Asumir que Kp, Ki, Kd aumentan linealmente con la temperatura.

```cpp
// Ejemplo: Autotune en Rango 0 (0-250°C) y Rango 1 (250-550°C)
// Proyectar para Rango 2 (550-900°C), Rango 3 (900-1150°C), etc.

void projectPIDParamsLinear(int numAutotunedRanges) {
    if (numAutotunedRanges < 2) {
        Serial.println("[PROJECT] Error: Se necesitan al menos 2 rangos autotunados");
        return;
    }
    
    // Calcular pendiente (m) y ordenada al origen (b) para cada parámetro
    // Usando los primeros 2 rangos autotunados
    
    PIDRange* r0 = &pidRanges[0];  // Rango 0: 0-250°C
    PIDRange* r1 = &pidRanges[1];  // Rango 1: 250-550°C
    
    // Temperatura promedio de cada rango
    float temp0 = (r0->tempMin + r0->tempMax) / 2.0;  // 125°C
    float temp1 = (r1->tempMin + r1->tempMax) / 2.0;  // 400°C
    
    // Calcular pendientes
    float mKp = (r1->Kp - r0->Kp) / (temp1 - temp0);
    float mKi = (r1->Ki - r0->Ki) / (temp1 - temp0);
    float mKd = (r1->Kd - r0->Kd) / (temp1 - temp0);
    
    // Calcular ordenadas al origen
    float bKp = r0->Kp - mKp * temp0;
    float bKi = r0->Ki - mKi * temp0;
    float bKd = r0->Kd - mKd * temp0;
    
    // Proyectar para rangos restantes
    for (int i = 2; i < MAX_PID_RANGES; i++) {
        PIDRange* r = &pidRanges[i];
        float tempAvg = (r->tempMin + r->tempMax) / 2.0;
        
        r->Kp = mKp * tempAvg + bKp;
        r->Ki = mKi * tempAvg + bKi;
        r->Kd = mKd * tempAvg + bKd;
        r->autotuned = false;  // Marcado como proyectado, no autotunado
        
        Serial.printf("[PROJECT] Rango %d proyectado: Kp=%.3f, Ki=%.3f, Kd=%.3f\n",
                      i, r->Kp, r->Ki, r->Kd);
    }
}
```

**Ejemplo Real:**
```
Rango 0 (125°C): Kp=2.0, Ki=0.02, Kd=15.0
Rango 1 (400°C): Kp=3.0, Ki=0.05, Kd=25.0

Pendiente Kp: m = (3.0 - 2.0) / (400 - 125) = 0.0036
Proyección Rango 2 (725°C): Kp = 0.0036 × 725 + 1.55 = 4.16
```

---

### Método 2: Extrapolación Exponencial (Más Realista)

**Concepto**: Los parámetros aumentan exponencialmente con la temperatura (más realista para hornos debido a pérdidas de calor por radiación T⁴).

```cpp
void projectPIDParamsExponential(int numAutotunedRanges) {
    if (numAutotunedRanges < 2) {
        return;
    }
    
    PIDRange* r0 = &pidRanges[0];
    PIDRange* r1 = &pidRanges[1];
    
    float temp0 = (r0->tempMin + r0->tempMax) / 2.0;
    float temp1 = (r1->tempMin + r1->tempMax) / 2.0;
    
    // Modelo exponencial: y = a × e^(b × T)
    // Resolver para a y b usando los dos puntos conocidos
    
    // Para Kp: Kp = a × e^(b × T)
    // ln(Kp1/Kp0) = b × (T1 - T0)
    float bKp = log(r1->Kp / r0->Kp) / (temp1 - temp0);
    float aKp = r0->Kp / exp(bKp * temp0);
    
    float bKi = log(r1->Ki / r0->Ki) / (temp1 - temp0);
    float aKi = r0->Ki / exp(bKi * temp0);
    
    float bKd = log(r1->Kd / r0->Kd) / (temp1 - temp0);
    float aKd = r0->Kd / exp(bKd * temp0);
    
    // Proyectar para rangos restantes
    for (int i = 2; i < MAX_PID_RANGES; i++) {
        PIDRange* r = &pidRanges[i];
        float tempAvg = (r->tempMin + r->tempMax) / 2.0;
        
        r->Kp = aKp * exp(bKp * tempAvg);
        r->Ki = aKi * exp(bKi * tempAvg);
        r->Kd = aKd * exp(bKd * tempAvg);
        r->autotuned = false;
        
        Serial.printf("[PROJECT] Rango %d (exp): Kp=%.3f, Ki=%.3f, Kd=%.3f\n",
                      i, r->Kp, r->Ki, r->Kd);
    }
}
```

**Ejemplo Real:**
```
Rango 0 (125°C): Kp=2.0
Rango 1 (400°C): Kp=3.0

b = ln(3.0/2.0) / (400-125) = 0.0015
a = 2.0 / e^(0.0015 × 125) = 1.66

Proyección Rango 2 (725°C): Kp = 1.66 × e^(0.0015 × 725) = 4.95
```

---

### Método 3: Modelo Basado en Física (Radiación T⁴)

**Concepto**: Las pérdidas de calor por radiación son proporcionales a T⁴ (Ley de Stefan-Boltzmann). Los parámetros PID deben compensar estas pérdidas.

```cpp
void projectPIDParamsPhysicsBased(int numAutotunedRanges) {
    if (numAutotunedRanges < 2) {
        return;
    }
    
    PIDRange* r0 = &pidRanges[0];
    PIDRange* r1 = &pidRanges[1];
    
    float temp0 = (r0->tempMin + r0->tempMax) / 2.0;
    float temp1 = (r1->tempMin + r1->tempMax) / 2.0;
    
    // Modelo: Parámetro = a × T^b
    // Resolver para a y b usando logaritmos
    
    // Para Kp: Kp = a × T^b
    // ln(Kp) = ln(a) + b × ln(T)
    float lnKp0 = log(r0->Kp);
    float lnKp1 = log(r1->Kp);
    float lnT0 = log(temp0);
    float lnT1 = log(temp1);
    
    float bKp = (lnKp1 - lnKp0) / (lnT1 - lnT0);
    float aKp = exp(lnKp0 - bKp * lnT0);
    
    float lnKi0 = log(r0->Ki);
    float lnKi1 = log(r1->Ki);
    float bKi = (lnKi1 - lnKi0) / (lnT1 - lnT0);
    float aKi = exp(lnKi0 - bKi * lnT0);
    
    float lnKd0 = log(r0->Kd);
    float lnKd1 = log(r1->Kd);
    float bKd = (lnKd1 - lnKd0) / (lnT1 - lnT0);
    float aKd = exp(lnKd0 - bKd * lnT0);
    
    // Proyectar para rangos restantes
    for (int i = 2; i < MAX_PID_RANGES; i++) {
        PIDRange* r = &pidRanges[i];
        float tempAvg = (r->tempMin + r->tempMax) / 2.0;
        
        r->Kp = aKp * pow(tempAvg, bKp);
        r->Ki = aKi * pow(tempAvg, bKi);
        r->Kd = aKd * pow(tempAvg, bKd);
        r->autotuned = false;
        
        Serial.printf("[PROJECT] Rango %d (física): Kp=%.3f, Ki=%.3f, Kd=%.3f\n",
                      i, r->Kp, r->Ki, r->Kd);
    }
}
```

---

### Método 4: Interpolación Polinómica (Más Preciso)

**Concepto**: Usar una curva polinómica que pase exactamente por los puntos conocidos.

```cpp
void projectPIDParamsPolynomial(int numAutotunedRanges) {
    if (numAutotunedRanges < 2) {
        return;
    }
    
    // Construir polinomio de grado (numAutotunedRanges - 1)
    // Para 2 puntos: polinomio de grado 1 (lineal)
    // Para 3 puntos: polinomio de grado 2 (cuadrático)
    
    // Simplificado: usar interpolación cuadrática si hay 2+ puntos
    // y = ax² + bx + c
    
    PIDRange* r0 = &pidRanges[0];
    PIDRange* r1 = &pidRanges[1];
    
    float temp0 = (r0->tempMin + r0->tempMax) / 2.0;
    float temp1 = (r1->tempMin + r1->tempMax) / 2.0;
    
    // Si solo hay 2 puntos, usar lineal (ya implementado en Método 1)
    // Si hay 3+ puntos, usar cuadrática
    
    if (numAutotunedRanges >= 3) {
        PIDRange* r2 = &pidRanges[2];
        float temp2 = (r2->tempMin + r2->tempMax) / 2.0;
        
        // Resolver sistema de ecuaciones para Kp:
        // Kp0 = a×T0² + b×T0 + c
        // Kp1 = a×T1² + b×T1 + c
        // Kp2 = a×T2² + b×T2 + c
        
        // Matriz de coeficientes (simplificado, usar método de Cramer o Gauss)
        // ... (implementación más compleja)
    }
    
    // Por ahora, usar extrapolación lineal para simplicidad
    projectPIDParamsLinear(numAutotunedRanges);
}
```

---

## 🔧 IMPLEMENTACIÓN COMPLETA: AUTOTUNE + PROYECCIÓN

### Función Principal

```cpp
// Ejecutar autotune en primeros 2 rangos y proyectar el resto
void autotuneAndProject() {
    Serial.println("[AUTOTUNE] Iniciando autotune en primeros 2 rangos...");
    Serial.println("[AUTOTUNE] ⚠️ Esto tomará aproximadamente 30-40 minutos");
    
    // Autotune Rango 0 (0-250°C)
    Serial.println("\n[AUTOTUNE] ===== RANGO 0 (0-250°C) =====");
    if (!autotuneRange(0)) {
        Serial.println("[AUTOTUNE] ❌ Falló autotune en Rango 0");
        return;
    }
    
    // Calentar hasta Rango 1
    float targetTemp1 = (pidRanges[1].tempMin + pidRanges[1].tempMax) / 2.0;
    Serial.printf("[AUTOTUNE] Calentando hasta Rango 1 (%0.1f°C)...\n", targetTemp1);
    while (readTemperature() < pidRanges[1].tempMin) {
        controlRelay(100.0);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    vTaskDelay(pdMS_TO_TICKS(60000));  // Estabilización
    
    // Autotune Rango 1 (250-550°C)
    Serial.println("\n[AUTOTUNE] ===== RANGO 1 (250-550°C) =====");
    if (!autotuneRange(1)) {
        Serial.println("[AUTOTUNE] ❌ Falló autotune en Rango 1");
        return;
    }
    
    // Proyectar parámetros para rangos restantes
    Serial.println("\n[PROJECT] Proyectando parámetros para rangos 2-4...");
    
    // Elegir método de proyección
    // Opción 1: Lineal (simple, rápido)
    projectPIDParamsLinear(2);
    
    // Opción 2: Exponencial (más realista)
    // projectPIDParamsExponential(2);
    
    // Opción 3: Basado en física (más preciso)
    // projectPIDParamsPhysicsBased(2);
    
    // Guardar todos los rangos (autotunados + proyectados)
    savePIDRanges();
    
    Serial.println("[AUTOTUNE] ✅ Autotune y proyección completados");
    Serial.println("[AUTOTUNE] Parámetros guardados en NVS");
    
    // Mostrar resumen
    printPIDRangesSummary();
}

void printPIDRangesSummary() {
    Serial.println("\n[PID] ===== RESUMEN DE PARÁMETROS =====");
    for (int i = 0; i < MAX_PID_RANGES; i++) {
        PIDRange* r = &pidRanges[i];
        const char* source = r->autotuned ? "AUTOTUNED" : "PROJECTED";
        Serial.printf("Rango %d (%0.0f-%0.0f°C) [%s]:\n", 
                      i, r->tempMin, r->tempMax, source);
        Serial.printf("  Kp=%.3f, Ki=%.3f, Kd=%.3f\n", r->Kp, r->Ki, r->Kd);
    }
    Serial.println("=======================================\n");
}
```

---

## 🎮 COMANDOS BLE ACTUALIZADOS

### Nuevos Comandos

```cpp
// Desde la app, enviar comandos BLE:

"AUTOTUNE_PROJECT"     // Autotune en rangos 0-1 y proyectar el resto
"AUTOTUNE_RANGE:0"     // Autotune solo rango 0
"AUTOTUNE_RANGE:1"     // Autotune solo rango 1
"AUTOTUNE_ALL"         // Autotune todos los rangos (sin proyección)
"PROJECT_PARAMS"       // Proyectar parámetros desde rangos autotunados
"GET_PID_RANGES"       // Obtener parámetros de todos los rangos
```

### Respuesta de GET_PID_RANGES (Actualizada)

```json
{
  "ranges": [
    {
      "index": 0,
      "tempMin": 0,
      "tempMax": 250,
      "Kp": 2.0,
      "Ki": 0.02,
      "Kd": 15.0,
      "autotuned": true,
      "projected": false,
      "timestamp": 1234567890
    },
    {
      "index": 1,
      "tempMin": 250,
      "tempMax": 550,
      "Kp": 3.0,
      "Ki": 0.05,
      "Kd": 25.0,
      "autotuned": true,
      "projected": false,
      "timestamp": 1234567890
    },
    {
      "index": 2,
      "tempMin": 550,
      "tempMax": 900,
      "Kp": 4.16,
      "Ki": 0.08,
      "Kd": 35.0,
      "autotuned": false,
      "projected": true,
      "timestamp": 0
    }
    // ... más rangos
  ]
}
```

---

## 📊 COMPARACIÓN DE MÉTODOS

| Método | Precisión | Complejidad | Tiempo | Recomendado Para |
|--------|-----------|-------------|--------|------------------|
| **Lineal** | ⭐⭐ | ⭐ | ⚡⚡⚡ | Hornos pequeños, rangos estrechos |
| **Exponencial** | ⭐⭐⭐ | ⭐⭐ | ⚡⚡⚡ | Hornos medianos, pérdidas moderadas |
| **Física (T⁴)** | ⭐⭐⭐⭐ | ⭐⭐⭐ | ⚡⚡⚡ | Hornos grandes, altas temperaturas |
| **Polinómico** | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⚡⚡ | Máxima precisión, 3+ puntos |

---

## ✅ VENTAJAS DE LA PROYECCIÓN

1. **Tiempo reducido**: 30-40 min vs 1-2 horas
2. **Menor consumo**: Solo calentar hasta ~500°C
3. **Más seguro**: No requiere temperaturas extremas
4. **Suficientemente preciso**: Los parámetros siguen patrones predecibles
5. **Flexible**: Se puede re-autotunar un rango específico después

## ⚠️ CONSIDERACIONES

1. **Precisión**: Los parámetros proyectados pueden no ser tan precisos como los autotunados
2. **Validación**: Es recomendable validar los parámetros proyectados en uso real
3. **Ajuste manual**: Si un rango proyectado no funciona bien, se puede autotunar específicamente
4. **Método**: Elegir el método de proyección según el tipo de horno y experiencia

---

## 🔄 FLUJO COMPLETO CON PROYECCIÓN

```
1. Usuario inicia autotune desde app
   └─> Comando BLE: "AUTOTUNE_PROJECT"

2. Sistema ejecuta autotune en Rango 0 (0-250°C)
   └─> Tiempo: ~15 minutos
   └─> Resultado: Kp0, Ki0, Kd0

3. Sistema calienta hasta Rango 1 (250-550°C)
   └─> Tiempo: ~10-15 minutos

4. Sistema ejecuta autotune en Rango 1 (250-550°C)
   └─> Tiempo: ~15 minutos
   └─> Resultado: Kp1, Ki1, Kd1

5. Sistema proyecta parámetros para Rangos 2-4
   └─> Usa método seleccionado (lineal/exponencial/física)
   └─> Calcula Kp2, Ki2, Kd2, Kp3, Ki3, Kd3, etc.

6. Guardar todos los parámetros en NVS
   └─> Rangos 0-1: autotuned=true
   └─> Rangos 2-4: autotuned=false, projected=true

7. Notificar completado
   └─> BLE: "AUTOTUNE_COMPLETE:PROJECTED"
   └─> Firebase: Actualizar estado

8. Parámetros activos
   └─> Sistema usa automáticamente parámetros según temperatura
   └─> Rangos autotunados: máxima precisión
   └─> Rangos proyectados: precisión suficiente
```

---

## 📝 RESUMEN FINAL

- ✅ **Autotune en primeros 2 rangos**: Más rápido y seguro
- ✅ **Proyección para rangos superiores**: Ahorra tiempo y energía
- ✅ **Múltiples métodos disponibles**: Lineal, exponencial, física, polinómico
- ✅ **Flexibilidad**: Se puede re-autotunar un rango específico después
- ✅ **Almacenamiento**: Todos los parámetros (autotunados + proyectados) en NVS


