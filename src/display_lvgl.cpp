// Iconos personalizados - ELIMINADOS
// #define USE_CUSTOM_ICONS 1

#include "display_lvgl.h"  // Ya define LV_FONT_MONTSERRAT_48 antes de incluir lvgl.h
#include <Arduino_GFX_Library.h>

// Variables globales de la UI
lv_obj_t* mainScreen = nullptr;
lv_obj_t* labelKilnName = nullptr;        // "SMARTKILN S1" (header izquierda)
lv_obj_t* infoCard = nullptr;             // Tarjeta contenedora (nueva)
lv_obj_t* arcProgress = nullptr;          // Arco circular naranja (lado izquierdo)
lv_obj_t* labelTempInside = nullptr;      // "875°C" (DENTRO del círculo, grande)
lv_obj_t* labelStatusText = nullptr;       // "horneando" (naranja, lado derecho arriba)
lv_obj_t* labelSetpoint = nullptr;         // "objetivo" (gris claro)
lv_obj_t* setpointBox = nullptr;           // Box para el valor de temperatura objetivo
lv_obj_t* labelSetpointValue = nullptr;    // "980°C" (blanco, más grande)
lv_obj_t* labelStage = nullptr;            // "etapa" (gris claro)
lv_obj_t* stageBox = nullptr;               // Box para el valor de etapa (2/4)
lv_obj_t* labelStageValue = nullptr;       // "2/4" (blanco)
lv_obj_t* labelTime = nullptr;             // "Tiempo restante" (gris claro) - etiqueta
lv_obj_t* timeBox = nullptr;                // Box para el valor de tiempo (01:42h)
lv_obj_t* labelTimeValue = nullptr;         // "01:42h" (blanco) - valor del tiempo
lv_obj_t* labelProgramName = nullptr;       // Nombre del programa (gris claro, posición 245, 100)
lv_obj_t* infoRectangle = nullptr;          // Rectángulo contorno (235,95 a 450,250)

// Backlight pin para ESP32-4827S043
#define TFT_BL 2

// Configuración RGB Panel para ESP32-4827S043 (según proyecto funcionando)
Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
    40 /* DE */, 41 /* VSYNC */, 39 /* HSYNC */, 42 /* PCLK */,
    45 /* R0 */, 48 /* R1 */, 47 /* R2 */, 21 /* R3 */, 14 /* R4 */,
    5 /* G0 */, 6 /* G1 */, 7 /* G2 */, 15 /* G3 */, 16 /* G4 */, 4 /* G5 */,
    8 /* B0 */, 3 /* B1 */, 46 /* B2 */, 9 /* B3 */, 1 /* B4 */,
    0 /* hsync_polarity */, 1 /* hsync_front_porch */, 1 /* hsync_pulse_width */, 43 /* hsync_back_porch */,
    0 /* vsync_polarity */, 3 /* vsync_front_porch */, 1 /* vsync_pulse_width */, 12 /* vsync_back_porch */,
    1 /* pclk_active_neg */, 9000000 /* prefer_speed */
);

// Display RGB 480x272 para ESP32-4827S043
Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
    480 /* width */, 272 /* height */, rgbpanel, 0 /* rotation */, true /* auto_flush */
);

// Variables para LVGL
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *disp_draw_buf = nullptr;
static lv_disp_drv_t disp_drv;
static bool displayInitialized = false;

// Colores personalizados (basados en la imagen)
static const lv_color_t COLOR_ORANGE = lv_color_hex(0xFF6600);      // Naranja/rojo más intenso para el arco y estado
static const lv_color_t COLOR_DARK_GRAY = lv_color_hex(0x404040);  // Gris oscuro
static const lv_color_t COLOR_LIGHT_GRAY = lv_color_hex(0xC0C0C0); // Gris claro para labels
static const lv_color_t COLOR_WHITE = lv_color_hex(0xFFFFFF);       // Blanco para texto

// Funciones de iconos eliminadas

// Callback para flush del display con Arduino_GFX
void my_disp_flush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p) {
    if (!area || !color_p || !disp) {
        if (disp) lv_disp_flush_ready(disp);
        return;
    }
    
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    
    if (w == 0 || h == 0 || w > 480 || h > 272) {
        if (disp) lv_disp_flush_ready(disp);
        return;
    }
    
    // Arduino_GFX con RGB Panel - usar draw16bitRGBBitmap (no swap para parallel)
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
    
    lv_disp_flush_ready(disp);
}

// Callback para leer touch (si es necesario en el futuro)
void my_touchpad_read(lv_indev_drv_t* indev_drv, lv_indev_data_t* data) {
    // Por ahora no hay touch, pero se puede agregar después
    data->state = LV_INDEV_STATE_REL;
}

// Crear la interfaz de usuario (basada en la imagen)
void createUI() {
    // Crear pantalla principal con fondo negro
    mainScreen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(mainScreen, lv_color_black(), LV_PART_MAIN);
    lv_obj_clear_flag(mainScreen, LV_OBJ_FLAG_SCROLLABLE);
    lv_scr_load(mainScreen);
    
    int screenW = 480;
    int screenH = 272;
    
    // ========== HEADER (Parte superior) ==========
    // Nombre del horno (izquierda): Se actualizará con el nombre del horno
    labelKilnName = lv_label_create(mainScreen);
    lv_label_set_text(labelKilnName, "--");  // Valor inicial, se actualizará con el nombre del horno
    lv_obj_set_style_text_color(labelKilnName, COLOR_LIGHT_GRAY, LV_PART_MAIN);
    lv_obj_set_style_text_font(labelKilnName, &lv_font_montserrat_16, LV_PART_MAIN);  // Aumentado de 14 a 16
    lv_obj_set_pos(labelKilnName, 10, 10);
    
    // Iconos de conexión eliminados (WiFi, Cloud)
    
    // ========== TARJETA CONTENEDORA ==========
    // Crear tarjeta con márgenes respecto a los extremos
    int cardMargin = 10;  // Margen desde los extremos
    int cardY = 45;       // Iniciar después del header (45px desde arriba)
    int cardW = screenW - (cardMargin * 2);  // Ancho con márgenes
    int cardH = screenH - cardY - cardMargin; // Altura hasta el margen inferior
    
    infoCard = lv_obj_create(mainScreen);
    lv_obj_set_size(infoCard, cardW, cardH);
    lv_obj_set_pos(infoCard, cardMargin, cardY);
    // Estilo de tarjeta: fondo oscuro, bordes redondeados, sin borde visible
    lv_obj_set_style_bg_color(infoCard, lv_color_hex(0x1a1a1a), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(infoCard, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(infoCard, 8, LV_PART_MAIN);  // Bordes redondeados
    lv_obj_set_style_border_width(infoCard, 0, LV_PART_MAIN);  // Sin borde
    lv_obj_set_style_pad_all(infoCard, 15, LV_PART_MAIN);  // Padding interno
    lv_obj_clear_flag(infoCard, LV_OBJ_FLAG_SCROLLABLE);
    
    // ========== ÁREA PRINCIPAL (Dentro de la tarjeta) ==========
    
    // --- LADO IZQUIERDO: Gauge circular con temperatura dentro ---
    // Arco circular naranja (aumentado levemente y centrado verticalmente)
    int arcSize = 195;  // Aumentado de 180 a 195
    int padding = 15;   // Padding interno de la tarjeta
    int arcY = (cardH - (2 * padding) - arcSize) / 2;  // Centrar verticalmente dentro del área disponible
    if (arcY < padding) arcY = padding;  // Asegurar mínimo margen superior
    
    // Ajustar posición: subir 15 píxeles y desplazar 15 píxeles hacia la izquierda
    arcY -= 15;  // Subir 15 píxeles
    int arcX = 5;  // Desplazar hacia la izquierda (de 20 a 5 píxeles)
    
    arcProgress = lv_arc_create(infoCard);  // Crear dentro de la tarjeta
    lv_obj_set_size(arcProgress, arcSize, arcSize);
    lv_obj_set_pos(arcProgress, arcX, arcY);  // Posición ajustada: más arriba y más a la izquierda
    lv_arc_set_rotation(arcProgress, 270);  // Rotar para que empiece desde abajo
    lv_arc_set_bg_angles(arcProgress, 0, 360);
    lv_arc_set_value(arcProgress, 75);  // 75% del arco lleno (desde abajo)
    lv_obj_set_style_arc_color(arcProgress, COLOR_ORANGE, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arcProgress, 10, LV_PART_INDICATOR);  // Reducido de 15 a 10
    lv_obj_set_style_arc_color(arcProgress, COLOR_DARK_GRAY, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arcProgress, 10, LV_PART_MAIN);  // Reducido de 15 a 10
    lv_obj_clear_flag(arcProgress, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_opa(arcProgress, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(arcProgress, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arcProgress, true, LV_PART_INDICATOR);  // Bordes redondeados
    // Ocultar el knob (círculo indicador) al final del arco
    lv_obj_set_style_bg_opa(arcProgress, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_border_opa(arcProgress, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_size(arcProgress, 0, LV_PART_KNOB);  // Establecer tamaño 0 para ocultarlo
    
    // Temperatura DENTRO del círculo (centrada) - Se actualizará con temperatura real
    labelTempInside = lv_label_create(infoCard);  // Crear dentro de la tarjeta
    lv_label_set_text(labelTempInside, "25°C");  // Valor inicial de simulación
    lv_obj_set_style_text_color(labelTempInside, COLOR_WHITE, LV_PART_MAIN);
    lv_obj_set_style_text_font(labelTempInside, &lv_font_montserrat_44, LV_PART_MAIN);
    lv_obj_align_to(labelTempInside, arcProgress, LV_ALIGN_CENTER, 0, 0);
    
    // --- LADO DERECHO: Columna de información ---
    // Ajustar rightX para el nuevo desplazamiento del círculo: arcX (5) + arcSize (195) + 10 de separación
    int rightX = 5 + 195 + 10;  // 210 (ajustado para círculo desplazado a la izquierda)
    int rightY = arcY;   // Posición vertical inicial (alineado con el círculo)
    
    // Estado "horneando" (sin icono de llama)
    labelStatusText = lv_label_create(infoCard);  // Crear dentro de la tarjeta
    lv_label_set_text(labelStatusText, "horneando");
    lv_obj_set_style_text_color(labelStatusText, COLOR_ORANGE, LV_PART_MAIN);
    lv_obj_set_style_text_font(labelStatusText, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_pos(labelStatusText, rightX, rightY);  // Posición directa sin alineación al icono
    
    // Objetivo: "objetivo" (gris claro) + Box con "980°C" (blanco, alineado a la derecha dentro del box)
    int leftMargin = 15;  // Margen desde el borde izquierdo de la tarjeta (después del padding)
    
    labelSetpoint = lv_label_create(infoCard);  // Crear dentro de la tarjeta
    lv_label_set_text(labelSetpoint, "objetivo");
    lv_obj_set_style_text_color(labelSetpoint, COLOR_LIGHT_GRAY, LV_PART_MAIN);
    lv_obj_set_style_text_font(labelSetpoint, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_pos(labelSetpoint, rightX + leftMargin + 5, 80);  // X: 230px, Y: 80px (absoluto: 140px)
    
    // Crear box para el valor de temperatura objetivo (posicionado más a la derecha)
    int boxX = 330;  // Posición X del box dentro de infoCard (absoluto: 355px, texto termina en 425px)
    int boxY = 80;   // Posición Y del box (absoluto: 140px)
    int boxW = 75;  // Ancho del box (+5px: 70 + 5 = 75)
    int boxH = 30;  // Altura del box (+5px: 25 + 5 = 30)
    
    setpointBox = lv_obj_create(infoCard);
    lv_obj_set_size(setpointBox, boxW, boxH);
    lv_obj_set_pos(setpointBox, boxX, boxY);
    lv_obj_set_style_bg_color(setpointBox, lv_color_hex(0x1a1a1a), LV_PART_MAIN);  // Fondo oscuro
    lv_obj_set_style_bg_opa(setpointBox, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(setpointBox, 0, LV_PART_MAIN);  // Sin borde
    lv_obj_set_style_radius(setpointBox, 4, LV_PART_MAIN);  // Bordes ligeramente redondeados
    lv_obj_set_style_pad_all(setpointBox, 5, LV_PART_MAIN);  // Padding interno
    lv_obj_clear_flag(setpointBox, LV_OBJ_FLAG_SCROLLABLE);
    
    labelSetpointValue = lv_label_create(setpointBox);  // Crear DENTRO del box
    lv_label_set_text(labelSetpointValue, "980°C");
    lv_obj_set_style_text_color(labelSetpointValue, COLOR_WHITE, LV_PART_MAIN);
    lv_obj_set_style_text_font(labelSetpointValue, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(labelSetpointValue, 1, LV_PART_MAIN);
    lv_obj_align(labelSetpointValue, LV_ALIGN_RIGHT_MID, -5, -2);  // Alineado a la derecha dentro del box (absoluto: X=425px, Y=138px)
    
    // Etapa: "etapa" (gris claro) + Box con "2/4" (blanco, alineado a la derecha dentro del box)
    labelStage = lv_label_create(infoCard);  // Crear dentro de la tarjeta
    lv_label_set_text(labelStage, "etapa");
    lv_obj_set_style_text_color(labelStage, COLOR_LIGHT_GRAY, LV_PART_MAIN);
    lv_obj_set_style_text_font(labelStage, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_pos(labelStage, rightX + leftMargin + 5, 120);  // X: 230px, Y: 120px (absoluto: 180px)
    
    // Crear box para el valor de etapa (posicionado igual que setpointBox pero a la altura de "etapa")
    int stageBoxX = 330;  // Posición X del box (igual que setpointBox)
    int stageBoxY = 120;  // Posición Y del box (absoluto: 180px)
    int stageBoxW = 75;   // Ancho del box (+5px: 70 + 5 = 75, igual que setpointBox)
    int stageBoxH = 30;   // Altura del box (+5px: 25 + 5 = 30, igual que setpointBox)
    
    stageBox = lv_obj_create(infoCard);
    lv_obj_set_size(stageBox, stageBoxW, stageBoxH);
    lv_obj_set_pos(stageBox, stageBoxX, stageBoxY);
    lv_obj_set_style_bg_color(stageBox, lv_color_hex(0x1a1a1a), LV_PART_MAIN);  // Fondo oscuro
    lv_obj_set_style_bg_opa(stageBox, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(stageBox, 0, LV_PART_MAIN);  // Sin borde
    lv_obj_set_style_radius(stageBox, 4, LV_PART_MAIN);  // Bordes ligeramente redondeados
    lv_obj_set_style_pad_all(stageBox, 5, LV_PART_MAIN);  // Padding interno
    lv_obj_clear_flag(stageBox, LV_OBJ_FLAG_SCROLLABLE);
    
    labelStageValue = lv_label_create(stageBox);  // Crear DENTRO del box
    lv_label_set_text(labelStageValue, "2/4");
    lv_obj_set_style_text_color(labelStageValue, COLOR_WHITE, LV_PART_MAIN);
    lv_obj_set_style_text_font(labelStageValue, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(labelStageValue, 1, LV_PART_MAIN);
    lv_obj_align(labelStageValue, LV_ALIGN_RIGHT_MID, -5, -2);  // Alineado a la derecha dentro del box (absoluto: X=425px, Y=178px)
    
    // Tiempo restante: "restante" (gris claro) + Box con "01:42h" (blanco)
    labelTime = lv_label_create(infoCard);  // Crear dentro de la tarjeta (etiqueta)
    lv_label_set_text(labelTime, "restante");
    lv_obj_set_style_text_color(labelTime, COLOR_LIGHT_GRAY, LV_PART_MAIN);  // Gris claro (igual que labelSetpoint y labelStage)
    lv_obj_set_style_text_font(labelTime, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_pos(labelTime, rightX + leftMargin + 5, 160);  // X: 230px, Y: 160px (absoluto: 220px)
    
    // Crear box para el valor de tiempo (posicionado debajo de stageBox)
    int timeBoxX = 330;  // Posición X del box (igual que otros boxes)
    int timeBoxY = 160;  // Posición Y del box (absoluto: 220px)
    int timeBoxW = 75;   // Ancho del box (igual que otros boxes)
    int timeBoxH = 30;   // Altura del box (igual que otros boxes)
    
    timeBox = lv_obj_create(infoCard);
    lv_obj_set_size(timeBox, timeBoxW, timeBoxH);
    lv_obj_set_pos(timeBox, timeBoxX, timeBoxY);
    lv_obj_set_style_bg_color(timeBox, lv_color_hex(0x1a1a1a), LV_PART_MAIN);  // Fondo oscuro
    lv_obj_set_style_bg_opa(timeBox, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(timeBox, 0, LV_PART_MAIN);  // Sin borde
    lv_obj_set_style_radius(timeBox, 4, LV_PART_MAIN);  // Bordes ligeramente redondeados
    lv_obj_set_style_pad_all(timeBox, 5, LV_PART_MAIN);  // Padding interno
    lv_obj_clear_flag(timeBox, LV_OBJ_FLAG_SCROLLABLE);
    
    labelTimeValue = lv_label_create(timeBox);  // Crear DENTRO del box (valor)
    lv_label_set_text(labelTimeValue, "01:42h");
    lv_obj_set_style_text_color(labelTimeValue, COLOR_WHITE, LV_PART_MAIN);
    lv_obj_set_style_text_font(labelTimeValue, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(labelTimeValue, 1, LV_PART_MAIN);
    lv_obj_align(labelTimeValue, LV_ALIGN_RIGHT_MID, -5, -2);  // Alineado a la derecha dentro del box (absoluto: X=425px, Y=218px)
    
    // Rectángulo contorno desde (235,95) hasta (450,250)
    // Dimensiones: ancho = 450-235 = 215px, alto = 250-95 = 155px
    // Posición en infoCard: X = 235 - 10 - 15 = 210px, Y = 95 - 45 - 15 = 35px
    infoRectangle = lv_obj_create(infoCard);
    lv_obj_set_size(infoRectangle, 215, 155);  // Ancho: 215px, Alto: 155px
    lv_obj_set_pos(infoRectangle, 210, 35);  // X: 210px en infoCard (absoluto: 235px), Y: 35px en infoCard (absoluto: 95px)
    lv_obj_set_style_bg_opa(infoRectangle, LV_OPA_TRANSP, LV_PART_MAIN);  // Sin relleno (transparente)
    lv_obj_set_style_border_color(infoRectangle, COLOR_LIGHT_GRAY, LV_PART_MAIN);  // Borde gris claro
    lv_obj_set_style_border_width(infoRectangle, 1, LV_PART_MAIN);  // Grosor del borde: 1px
    lv_obj_set_style_radius(infoRectangle, 0, LV_PART_MAIN);  // Sin bordes redondeados
    lv_obj_clear_flag(infoRectangle, LV_OBJ_FLAG_SCROLLABLE);
    
    // Nombre del programa (gris claro, posición absoluta 245, 100px)
    // Posición en infoCard: X = 245 - 10 - 15 = 220px, Y = 100 - 45 - 15 = 40px
    labelProgramName = lv_label_create(infoCard);
    lv_label_set_text(labelProgramName, "--");
    lv_obj_set_style_text_color(labelProgramName, COLOR_LIGHT_GRAY, LV_PART_MAIN);
    lv_obj_set_style_text_font(labelProgramName, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_pos(labelProgramName, 220, 40);  // X: 220px en infoCard (absoluto: 245px), Y: 40px en infoCard (absoluto: 100px)
}

// Inicializar display y LVGL
bool initDisplayLVGL() {
    // Logs comentados para enfocarse en BT y WiFi
    // Serial.println("[DISPLAY] ========== INICIO INIT DISPLAY ==========");
    // Serial.println("[DISPLAY] Configurando para ESP32-4827S043 con RGB Panel...");
    
    // Inicializar display Arduino_GFX
    // Serial.println("[DISPLAY] Inicializando Arduino_GFX RGB Panel...");
    gfx->begin();
    // Serial.println("[DISPLAY] ✅ Arduino_GFX iniciado");
    
    // Limpiar pantalla
    gfx->fillScreen(0x0000);  // Negro (RGB565)
    // Serial.println("[DISPLAY] Pantalla limpiada");
    
    // Configurar backlight
    // Serial.println("[DISPLAY] Configurando backlight...");
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    delay(100);
    // Serial.println("[DISPLAY] Backlight ON");
    
    int w = gfx->width();
    int h = gfx->height();
    // Serial.printf("[DISPLAY] Display inicializado: %dx%d\n", w, h);
    
    if (w == 0 || h == 0) {
        // Serial.println("[DISPLAY] ❌ Dimensiones inválidas del display");
        return false;
    }
    
    // Inicializar LVGL
    // Serial.println("[DISPLAY] Inicializando LVGL...");
    lv_init();
    // Serial.println("[DISPLAY] LVGL init() completado");
    
    // Configurar buffers de display (usar heap_caps_malloc en ESP32)
    // Reducir buffer a /6 para liberar memoria (~21KB en lugar de ~32KB)
    // Serial.println("[DISPLAY] Configurando buffers...");
    uint32_t buffer_size = w * h / 8;  // Reducido de /6 a /8 para liberar más memoria (~21KB en lugar de ~26KB)
    #ifdef ESP32
        disp_draw_buf = (lv_color_t *)heap_caps_malloc(sizeof(lv_color_t) * buffer_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    #else
        disp_draw_buf = (lv_color_t *)malloc(sizeof(lv_color_t) * buffer_size);
    #endif
    
    if (!disp_draw_buf) {
        // Serial.println("[DISPLAY] ❌ Error al asignar memoria para buffers");
        return false;
    }
    
    lv_disp_draw_buf_init(&draw_buf, disp_draw_buf, NULL, buffer_size);
    // Serial.println("[DISPLAY] Buffers configurados");
    
    // Configurar driver de display
    // Serial.println("[DISPLAY] Configurando driver...");
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = w;  // 480
    disp_drv.ver_res = h;  // 272
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_t* disp = lv_disp_drv_register(&disp_drv);
    if (!disp) {
        // Serial.println("[DISPLAY] ❌ Error al registrar driver");
        return false;
    }
    // Serial.println("[DISPLAY] Driver registrado");
    
    // Crear la interfaz de usuario
    // Serial.println("[DISPLAY] Creando UI...");
    createUI();
    // Serial.println("[DISPLAY] UI creada");
    
    // Forzar actualización inicial
    lv_timer_handler();
    delay(100);
    lv_timer_handler();
    
    displayInitialized = true;
    // Serial.println("[DISPLAY] ✅ LVGL inicializado correctamente");
    return true;
}

// Función updateFlameBlink eliminada (icono de llama eliminado)

// Tarea para el handler de LVGL
void displayTaskHandler(void *pvParameters) {
    for (;;) {
        lv_timer_handler(); // Llamar al handler de LVGL
        
        vTaskDelay(pdMS_TO_TICKS(5)); // Ejecutar cada 5ms
    }
}

// Actualizar datos en la pantalla
void updateDisplayData(const DisplayData& data) {
    if (!displayInitialized) {
        // Serial.println("[DISPLAY] ⚠️ Display no inicializado, no se puede actualizar");
        return;
    }
    
    // Actualizar nombre del programa (en lugar del nombre del horno)
    // Logs comentados para enfocarse en BT y WiFi
    // static unsigned long lastNameLog = 0;
    // unsigned long currentTimeName = millis();
    
    // if (currentTimeName - lastNameLog >= 3000) {
    //     Serial.printf("[DISPLAY] 📝 DEBUG NOMBRE PROGRAMA:\n");
    //     Serial.printf("  - data.programName='%s' (longitud: %d)\n", 
    //                  data.programName.c_str(), data.programName.length());
    //     Serial.printf("  - ¿Está vacío? %s\n", data.programName.isEmpty() ? "SÍ" : "NO");
    //     Serial.printf("  - ¿Es igual a ''? %s\n", (data.programName == "") ? "SÍ" : "NO");
    //     Serial.printf("  - labelKilnName existe: %s\n", labelKilnName != nullptr ? "SÍ" : "NO");
    //     if (labelKilnName != nullptr) {
    //         const char* currentText = lv_label_get_text(labelKilnName);
    //         Serial.printf("  - Texto actual en label: '%s'\n", currentText ? currentText : "(null)");
    //     }
    //     lastNameLog = currentTimeName;
    // }
    
    // PRIORIDAD: 1) Nombre del horno, 2) Nombre del programa, 3) Valor por defecto
    char nameToShow[64] = "";
    
    // Prioridad: primero nombre del horno, luego nombre del programa como fallback
    // Verificar si hay nombre del horno
    if (strlen(data.kilnName) > 0) {
        // Verificar que no sea solo espacios o caracteres especiales
        char trimmed[64];
        strncpy(trimmed, data.kilnName, sizeof(trimmed) - 1);
        trimmed[sizeof(trimmed) - 1] = '\0';
        // Trim manual: eliminar espacios al inicio y final
        int start = 0;
        while (trimmed[start] == ' ' && start < strlen(trimmed)) start++;
        int end = strlen(trimmed) - 1;
        while (trimmed[end] == ' ' && end >= start) end--;
        if (end >= start) {
            int len = end - start + 1;
            strncpy(nameToShow, trimmed + start, len);
            nameToShow[len] = '\0';
        }
    }
    
    // Si no hay nombre del horno, usar nombre del programa como fallback
    if (strlen(nameToShow) == 0 && strlen(data.programName) > 0) {
        // Verificar que no sea solo espacios o caracteres especiales
        char trimmed[64];
        strncpy(trimmed, data.programName, sizeof(trimmed) - 1);
        trimmed[sizeof(trimmed) - 1] = '\0';
        // Trim manual: eliminar espacios al inicio y final
        int start = 0;
        while (trimmed[start] == ' ' && start < strlen(trimmed)) start++;
        int end = strlen(trimmed) - 1;
        while (trimmed[end] == ' ' && end >= start) end--;
        if (end >= start) {
            int len = end - start + 1;
            strncpy(nameToShow, trimmed + start, len);
            nameToShow[len] = '\0';
        }
    }
    
    // Si tampoco hay nombre del horno, usar valor por defecto
    if (strlen(nameToShow) == 0) {
        strncpy(nameToShow, "SMARTKILN S1", sizeof(nameToShow) - 1);
        nameToShow[sizeof(nameToShow) - 1] = '\0';
        // Serial.println("[DISPLAY] ⚠️ Usando valor por defecto: 'SMARTKILN S1'");
    }
    
    // Actualizar el label
    lv_label_set_text(labelKilnName, nameToShow);
    lv_obj_invalidate(labelKilnName);
    
    // Verificar que se aplicó
    // const char* verifyName = lv_label_get_text(labelKilnName);
    // Serial.printf("[DISPLAY] ✓ Nombre final en label: '%s'\n", verifyName ? verifyName : "(null)");
    
    // Actualizar nombre del programa (labelProgramName)
    if (labelProgramName != nullptr) {
        char programNameToShow[64] = "";
        if (strlen(data.programName) > 0) {
            // Verificar que no sea solo espacios o caracteres especiales
            char trimmed[64];
            strncpy(trimmed, data.programName, sizeof(trimmed) - 1);
            trimmed[sizeof(trimmed) - 1] = '\0';
            // Trim manual: eliminar espacios al inicio y final
            int start = 0;
            while (trimmed[start] == ' ' && start < strlen(trimmed)) start++;
            int end = strlen(trimmed) - 1;
            while (trimmed[end] == ' ' && end >= start) end--;
            if (end >= start) {
                int len = end - start + 1;
                strncpy(programNameToShow, trimmed + start, len);
                programNameToShow[len] = '\0';
            }
        }
        
        // Si no hay nombre del programa, mostrar "--"
        if (strlen(programNameToShow) == 0) {
            strncpy(programNameToShow, "--", sizeof(programNameToShow) - 1);
            programNameToShow[sizeof(programNameToShow) - 1] = '\0';
        }
        
        lv_label_set_text(labelProgramName, programNameToShow);
        lv_obj_invalidate(labelProgramName);
    }
    
    // Actualizar temperatura dentro del círculo
    static float lastDisplayedTemp = -1.0;  // Variable estática para rastrear última temperatura mostrada
    // Logs comentados para enfocarse en BT y WiFi
    unsigned long currentTime = millis();
    
    if (labelTempInside != nullptr) {
        // Log detallado cada vez que hay cambio o cada 2 segundos
        // bool shouldLog = (data.currentTemp != lastDisplayedTemp) || (currentTime - lastTempDebugLog >= 2000);
        
        // if (shouldLog) {
        //     Serial.printf("[DISPLAY] 🔍 DEBUG TEMPERATURA:\n");
        //     Serial.printf("  - data.currentTemp recibido: %.2f°C\n", data.currentTemp);
        //     Serial.printf("  - Última temperatura mostrada: %.2f°C\n", lastDisplayedTemp);
        //     Serial.printf("  - ¿labelTempInside existe? %s\n", labelTempInside != nullptr ? "SÍ" : "NO");
        //     Serial.printf("  - ¿arcProgress existe? %s\n", arcProgress != nullptr ? "SÍ" : "NO");
        //     
        //     // Obtener texto actual del label para verificar
        //     const char* currentText = lv_label_get_text(labelTempInside);
        //     Serial.printf("  - Texto actual en label: '%s'\n", currentText ? currentText : "(null)");
        //     
        //     lastTempDebugLog = currentTime;
        // }
        
        // Actualizar siempre si el valor cambió o si es la primera vez
        if (data.currentTemp != lastDisplayedTemp || lastDisplayedTemp < 0) {
            char tempStr[32];
            sprintf(tempStr, "%.0f°C", data.currentTemp);
            // Serial.printf("[DISPLAY] 📝 Actualizando texto a: '%s'\n", tempStr);
            
            lv_label_set_text(labelTempInside, tempStr);
            
            // Verificar que el texto se aplicó
            // const char* verifyText = lv_label_get_text(labelTempInside);
            // Serial.printf("[DISPLAY] ✓ Texto verificado después de set_text: '%s'\n", verifyText ? verifyText : "(null)");
            
            // Re-aplicar alineación después de cambiar el texto
            if (arcProgress != nullptr) {
                lv_obj_align_to(labelTempInside, arcProgress, LV_ALIGN_CENTER, 0, 0);
                // Serial.println("[DISPLAY] ✓ Alineación re-aplicada");
            } else {
                // Serial.println("[DISPLAY] ⚠️ arcProgress es nullptr, no se puede re-aplicar alineación!");
            }
            
            // Forzar actualización del objeto y refrescar
            lv_obj_invalidate(labelTempInside);
            if (arcProgress != nullptr) {
                lv_obj_invalidate(arcProgress);  // Invalidar también el círculo para forzar refresh completo
            }
            
            // Forzar refresh inmediato de LVGL
            lv_refr_now(lv_disp_get_default());
            
            lastDisplayedTemp = data.currentTemp;
            // Serial.printf("[DISPLAY] ✅ Temperatura actualizada: %.0f°C\n", data.currentTemp);
        }
    } else {
        // static unsigned long lastErrorLog = 0;
        // if (currentTime - lastErrorLog >= 5000) {
        //     Serial.println("[DISPLAY] ⚠️ labelTempInside es nullptr!");
        //     lastErrorLog = currentTime;
        // }
    }
    
    // Actualizar estado traduciendo a español (sin icono de llama)
    char statusText[32] = "";
    
    if (strcmp(data.status, "CALENTANDO") == 0) {
        strncpy(statusText, "horneando", sizeof(statusText) - 1);
        statusText[sizeof(statusText) - 1] = '\0';
        lv_obj_set_style_text_color(labelStatusText, COLOR_ORANGE, LV_PART_MAIN);
    } else if (strcmp(data.status, "PAUSADO") == 0) {
        strncpy(statusText, "pausa", sizeof(statusText) - 1);
        statusText[sizeof(statusText) - 1] = '\0';
        lv_obj_set_style_text_color(labelStatusText, lv_color_hex(0xFFFF00), LV_PART_MAIN);  // Amarillo
    } else if (strcmp(data.status, "DETENIDO") == 0) {
        strncpy(statusText, "detenido", sizeof(statusText) - 1);
        statusText[sizeof(statusText) - 1] = '\0';
        lv_obj_set_style_text_color(labelStatusText, lv_color_hex(0xFF0000), LV_PART_MAIN);  // Rojo
    } else if (strcmp(data.status, "FINALIZADO") == 0) {
        strncpy(statusText, "finalizado", sizeof(statusText) - 1);
        statusText[sizeof(statusText) - 1] = '\0';
        lv_obj_set_style_text_color(labelStatusText, lv_color_hex(0x00FF00), LV_PART_MAIN);  // Verde
    } else if (strcmp(data.status, "INTERRUMPIDO") == 0) {
        strncpy(statusText, "interrumpido", sizeof(statusText) - 1);
        statusText[sizeof(statusText) - 1] = '\0';
        lv_obj_set_style_text_color(labelStatusText, lv_color_hex(0xFF8800), LV_PART_MAIN);
    } else {
        strncpy(statusText, "inactivo", sizeof(statusText) - 1);
        statusText[sizeof(statusText) - 1] = '\0';
        lv_obj_set_style_text_color(labelStatusText, COLOR_LIGHT_GRAY, LV_PART_MAIN);
    }
    
    lv_label_set_text(labelStatusText, statusText);
    
    // Actualizar objetivo
    if (data.targetTemp > 0) {
        char setpointStr[32];
        sprintf(setpointStr, "%.0f°C", data.targetTemp);
        lv_label_set_text(labelSetpointValue, setpointStr);
    }
    
    // Actualizar etapa (currentStage/totalStages)
    if (data.currentStage > 0 && data.totalStages > 0) {
        char stageStr[16];
        sprintf(stageStr, "%d/%d", data.currentStage, data.totalStages);
        lv_label_set_text(labelStageValue, stageStr);
    } else {
        lv_label_set_text(labelStageValue, "--");
    }
    
    // Actualizar tiempo total del programa en formato "HH:MMh" (en labelTimeValue)
    if (data.totalMinutes > 0) {
        int hours = data.totalMinutes / 60;
        int minutes = data.totalMinutes % 60;
        char timeStr[16];
        sprintf(timeStr, "%02d:%02dh", hours, minutes);
        lv_label_set_text(labelTimeValue, timeStr);
    } else {
        lv_label_set_text(labelTimeValue, "--:--h");
    }
    
    // Iconos de conexión eliminados (WiFi, Cloud)
    
}
