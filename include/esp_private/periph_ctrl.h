// Stub header para evitar errores de compilación con Arduino_GFX
// Solo se usa RGB Panel, no SPI que requiere este header completo
#ifndef ESP_PRIVATE_PERIPH_CTRL_H
#define ESP_PRIVATE_PERIPH_CTRL_H

#include <Arduino.h>
#include <soc/periph_defs.h>  // Incluir definición real de periph_module_t

// Stub mínimo - no implementado ya que solo usamos RGB Panel
static inline void periph_module_enable(periph_module_t module) { (void)module; }
static inline void periph_module_disable(periph_module_t module) { (void)module; }

#endif // ESP_PRIVATE_PERIPH_CTRL_H

