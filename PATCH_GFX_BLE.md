# Parches aplicados al proyecto

## 1. GFX Library – Arduino_ESP32RGBPanel.h

**Problema:** Con Arduino-ESP32 2.x la condición en el header tenía `ESP_ARDUINO_VERSION_MAJOR > 5`, por lo que la estructura `esp_rgb_panel_t` no se definía y la compilación fallaba.

**Solución:** En  
`.pio/libdeps/esp32-s3-devkitc-1/GFX Library for Arduino/src/databus/Arduino_ESP32RGBPanel.h`  
cambiar la línea:

```c
#if (!defined(ESP_ARDUINO_VERSION_MAJOR)) || (ESP_ARDUINO_VERSION_MAJOR >5)
```

por:

```c
#if (!defined(ESP_ARDUINO_VERSION_MAJOR)) || (ESP_ARDUINO_VERSION_MAJOR < 3)
```

**Si tras un `pio run -t fullclean` vuelve a fallar la compilación de GFX**, vuelve a aplicar este cambio en ese archivo.

## 2. BLE – Reinicio al conectar

- **sdkconfig.defaults:** `CONFIG_BT_NIMBLE_HOST_TASK_STACK_SIZE=8192`
- **main.cpp:** Callbacks BLE aligerados; guardado WiFi y KILN_INFO diferido a TaskComunicaciones (`pendingWifiConfigSave`, `pendingKilnInfoSave`).
