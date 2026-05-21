# 📌 PINES DEL DISPLAY - ESP32-4827S043

## ⚠️ IMPORTANTE: El display NO usa SPI

El display **ESP32-4827S043** utiliza una **interfaz RGB paralela** (RGB Panel), no SPI.

---

## 🔌 PINES UTILIZADOS

### Señales de Control
| Señal | GPIO | Función |
|-------|------|---------|
| **DE** (Data Enable) | **40** | Habilita datos |
| **VSYNC** (Vertical Sync) | **41** | Sincronización vertical |
| **HSYNC** (Horizontal Sync) | **39** | Sincronización horizontal |
| **PCLK** (Pixel Clock) | **42** | Reloj de píxeles |
| **TFT_BL** (Backlight) | **2** | Control de retroiluminación |

### Señales de Color (RGB 5-6-5 bits)

#### Rojo (R0-R4) - 5 bits
| Bit | GPIO | Función |
|-----|------|---------|
| R0 | **45** | Red bit 0 |
| R1 | **48** | Red bit 1 |
| R2 | **47** | Red bit 2 |
| R3 | **21** | Red bit 3 |
| R4 | **14** | Red bit 4 |

#### Verde (G0-G5) - 6 bits
| Bit | GPIO | Función |
|-----|------|---------|
| G0 | **5** | Green bit 0 |
| G1 | **6** | Green bit 1 |
| G2 | **7** | Green bit 2 |
| G3 | **15** | Green bit 3 |
| G4 | **16** | Green bit 4 |
| G5 | **4** | Green bit 5 |

#### Azul (B0-B4) - 5 bits
| Bit | GPIO | Función |
|-----|------|---------|
| B0 | **8** | Blue bit 0 |
| B1 | **3** | Blue bit 1 |
| B2 | **46** | Blue bit 2 |
| B3 | **9** | Blue bit 3 |
| B4 | **1** | Blue bit 4 |

---

## 📋 RESUMEN DE PINES

**Total de pines utilizados: 25**

- **Control**: 4 pines (DE, VSYNC, HSYNC, PCLK)
- **Color Rojo**: 5 pines (R0-R4)
- **Color Verde**: 6 pines (G0-G5)
- **Color Azul**: 5 pines (B0-B4)
- **Backlight**: 1 pin (TFT_BL)
- **Total**: 21 pines de datos + 4 de control + 1 backlight = **26 pines**

---

## 💻 CÓDIGO DE CONFIGURACIÓN

```cpp
// En display_lvgl.cpp, líneas 30-38

Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
    40 /* DE */, 41 /* VSYNC */, 39 /* HSYNC */, 42 /* PCLK */,
    45 /* R0 */, 48 /* R1 */, 47 /* R2 */, 21 /* R3 */, 14 /* R4 */,
    5 /* G0 */, 6 /* G1 */, 7 /* G2 */, 15 /* G3 */, 16 /* G4 */, 4 /* G5 */,
    8 /* B0 */, 3 /* B1 */, 46 /* B2 */, 9 /* B3 */, 1 /* B4 */,
    0 /* hsync_polarity */, 1 /* hsync_front_porch */, 1 /* hsync_pulse_width */, 43 /* hsync_back_porch */,
    0 /* vsync_polarity */, 3 /* vsync_front_porch */, 1 /* vsync_pulse_width */, 12 /* vsync_back_porch */,
    1 /* pclk_active_neg */, 9000000 /* prefer_speed */
);
```

---

## 🔍 DIFERENCIAS: SPI vs RGB PARALELO

### SPI (Serial Peripheral Interface)
- **4-5 pines**: MOSI, MISO, SCK, CS, (opcional: DC, RST)
- **Comunicación serial**: Un bit a la vez
- **Velocidad**: Limitada por frecuencia SPI (típicamente 10-40 MHz)
- **Uso común**: Displays pequeños (TFT, OLED)

### RGB Paralelo (RGB Panel)
- **20+ pines**: Múltiples líneas de datos en paralelo
- **Comunicación paralela**: Múltiples bits simultáneos
- **Velocidad**: Muy alta (hasta 9 MHz pixel clock en este caso)
- **Uso común**: Displays grandes, alta resolución, video

---

## ⚙️ CONFIGURACIÓN DE TIMING

El display también requiere configuración de timing:

```cpp
// Parámetros de sincronización horizontal
hsync_polarity = 0
hsync_front_porch = 1
hsync_pulse_width = 1
hsync_back_porch = 43

// Parámetros de sincronización vertical
vsync_polarity = 0
vsync_front_porch = 3
vsync_pulse_width = 1
vsync_back_porch = 12

// Reloj de píxeles
pclk_active_neg = 1  // Reloj activo en flanco negativo
prefer_speed = 9000000  // 9 MHz
```

---

## 📊 ESPECIFICACIONES DEL DISPLAY

- **Resolución**: 480 × 272 píxeles
- **Tipo**: RGB Panel (paralelo)
- **Backlight**: Controlado por GPIO 2
- **Velocidad**: 9 MHz pixel clock
- **Formato de color**: RGB 5-6-5 (16 bits por píxel)

---

## ✅ PINES DISPONIBLES PARA OTROS USOS

Si necesitas usar SPI para otros dispositivos (como MAX31855), puedes usar los pines SPI estándar del ESP32-S3:

### SPI1 (HSPI) - Disponible
- **MOSI**: GPIO 11 (o cualquier GPIO configurable)
- **MISO**: GPIO 13 (o cualquier GPIO configurable)
- **SCK**: GPIO 12 (o cualquier GPIO configurable)
- **CS**: Cualquier GPIO (seleccionable)

### SPI2 (VSPI) - Disponible
- **MOSI**: GPIO 35 (o cualquier GPIO configurable)
- **MISO**: GPIO 37 (o cualquier GPIO configurable)
- **SCK**: GPIO 36 (o cualquier GPIO configurable)
- **CS**: Cualquier GPIO (seleccionable)

**Nota**: El ESP32-S3 permite configurar SPI en casi cualquier GPIO, así que tienes flexibilidad para elegir pines que no interfieran con el display.

---

## 🔗 REFERENCIAS

- **Librería**: `Arduino_GFX_Library` (moononournation/GFX Library for Arduino)
- **Clase**: `Arduino_ESP32RGBPanel`
- **Display**: ESP32-4827S043 (480×272 RGB Panel)
