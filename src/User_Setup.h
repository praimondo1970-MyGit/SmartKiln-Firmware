// User_Setup.h para ESP32-4827S043 (4.27 pulgadas)
// Esta placa típicamente usa ILI9488 con interfaz SPI

#define USER_SETUP_LOADED

// Controlador de display - ESP32-4827S043 generalmente usa ILI9488
#define ILI9488_DRIVER      // Controlador para 4.27 pulgadas

// Resolución típica para 4.27 pulgadas
#define TFT_WIDTH  480
#define TFT_HEIGHT 320

// Pines para ESP32-4827S043 (4.27 pulgadas)
// Usando pines estándar SPI válidos para ESP32-S3
// NOTA: Estos pueden necesitar ajuste según tu placa específica
#define TFT_MOSI 11  // GPIO11 - SPI MOSI
#define TFT_MISO 13  // GPIO13 - SPI MISO  
#define TFT_SCLK 12  // GPIO12 - SPI SCLK
#define TFT_CS   10  // GPIO10 - Chip Select
#define TFT_DC    9  // GPIO9  - Data/Command
#define TFT_RST  48  // GPIO48 - Reset (o puede ser -1 si no se usa)

// Backlight
#define TFT_BL   14  // GPIO14 - Backlight (o ajustar según tu placa)
#define TFT_BACKLIGHT_ON HIGH  // Nivel para encender backlight

// Colores
#define TFT_BLACK       0x0000
#define TFT_NAVY        0x000F
#define TFT_DARKGREEN   0x03E0
#define TFT_DARKCYAN    0x03EF
#define TFT_MAROON      0x7800
#define TFT_PURPLE      0x780F
#define TFT_OLIVE       0x7BE0
#define TFT_LIGHTGREY  0xC618
#define TFT_DARKGREY    0x7BEF
#define TFT_BLUE        0x001F
#define TFT_GREEN       0x07E0
#define TFT_CYAN        0x07FF
#define TFT_RED         0xF800
#define TFT_MAGENTA     0xF81F
#define TFT_YELLOW      0xFFE0
#define TFT_WHITE       0xFFFF
#define TFT_ORANGE      0xFDA0
#define TFT_GREENYELLOW 0xB7E0
#define TFT_PINK        0xFC9F

// Frecuencia SPI
#define SPI_FREQUENCY  40000000  // 40 MHz para ESP32-S3
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000

// Optimizaciones
#define LOAD_GLCD   // Font 1, Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
#define LOAD_FONT2  // Font 2, Small 16 pixel high font, needs ~3534 bytes in FLASH, 96 characters
#define LOAD_FONT4  // Font 4, Medium 26 pixel high font, needs ~5848 bytes in FLASH, 96 characters
#define LOAD_FONT6  // Font 6, Large 48 pixel font, needs ~2666 bytes in FLASH, only characters 1234567890:-.apm
#define LOAD_FONT7  // Font 7, 7 segment 48 pixel font, needs ~2438 bytes in FLASH, only characters 1234567890:.
#define LOAD_FONT8  // Font 8, Large 75 pixel font needs ~3256 bytes in FLASH, only characters 1234567890:-.
#define LOAD_GFXFF  // FreeFonts. Include access to 48 Adafruit_GFX free fonts FF1 to FF48 and custom fonts

// Configuración para ESP32-S3 - NO usar parallel, usar SPI estándar
// #define ESP32_PARALLEL  // Comentado - usar SPI estándar

