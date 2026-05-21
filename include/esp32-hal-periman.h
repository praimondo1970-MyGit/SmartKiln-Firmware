// Stub header para evitar errores de compilación con Arduino_GFX
// Solo se usa RGB Panel, no SPI que requiere este header completo
#ifndef ESP32_HAL_PERIMAN_H
#define ESP32_HAL_PERIMAN_H

#include <Arduino.h>

// Definiciones mínimas necesarias (stub)
typedef enum {
    PERIMAN_PIN_TYPE_UNKNOWN = 0,
    PERIMAN_PIN_TYPE_INVALID
} peripheral_pin_type_t;

// Funciones stub mínimas
static inline void perimanClearPinBus(uint8_t pin) { (void)pin; }
static inline bool perimanSetPinBus(uint8_t pin, peripheral_pin_type_t type, void *bus) { (void)pin; (void)type; (void)bus; return true; }

#endif // ESP32_HAL_PERIMAN_H
