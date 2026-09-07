#include "display_ui.h"
#include "display_icons.h"
#include "inter_fonts.h"

#include <lvgl.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static lv_obj_t *mainScreen = NULL;
static lv_obj_t *bootScreen = NULL;

static lv_obj_t *imgLogo = NULL;
static lv_obj_t *labelBrandSmart = NULL;
static lv_obj_t *labelBrandKiln = NULL;
static lv_obj_t *headerDivider = NULL;
static lv_obj_t *headerKilnBlock = NULL;
static lv_obj_t *labelKilnTitle = NULL;
static lv_obj_t *labelDeviceId = NULL;
static lv_obj_t *labelConnDot = NULL;
static lv_obj_t *labelConnText = NULL;
static lv_obj_t *imgWifiIcon = NULL;

static lv_obj_t *leftPanel = NULL;
static lv_obj_t *arcTemp = NULL;
static lv_obj_t *arcGradLayer = NULL;
static lv_obj_t *labelTempCaption = NULL;
static lv_obj_t *labelTempValue = NULL;
static lv_obj_t *labelTempUnit = NULL;
static lv_obj_t *centerDivider = NULL;
static lv_obj_t *labelRampCaption = NULL;
static lv_obj_t *labelRampValue = NULL;
static lv_obj_t *labelProgressCaption = NULL;
static lv_obj_t *barCycleProgress = NULL;
static lv_obj_t *labelProgressPct = NULL;

static lv_obj_t *cardStatus = NULL;
static lv_obj_t *labelStatusHeader = NULL;
static lv_obj_t *statusPill = NULL;
static lv_obj_t *labelStatusPill = NULL;
static lv_obj_t *imgStatusFlame = NULL;

static lv_obj_t *cardSummary = NULL;
static lv_obj_t *labelSummaryHeader = NULL;
static lv_obj_t *labelObjCaption = NULL;
static lv_obj_t *labelObjValue = NULL;
static lv_obj_t *labelStageCaption = NULL;
static lv_obj_t *labelStageValue = NULL;
static lv_obj_t *labelTimeCaption = NULL;
static lv_obj_t *labelTimeValue = NULL;
static lv_obj_t *imgObjIcon = NULL;
static lv_obj_t *imgStageIcon = NULL;
static lv_obj_t *imgTimeIcon = NULL;

static lv_obj_t *cardProgram = NULL;
static lv_obj_t *labelProgramHeader = NULL;
static lv_obj_t *labelProgramValue = NULL;

typedef struct {
    float pos;
    uint32_t color;
} ArcGradStop;

/* Degradado angular: inicio más saturado, punta más roja (menos naranja). */
static const ArcGradStop ARC_GRAD_STOPS[] = {
    {0.00f, 0xFF8A50},
    {0.18f, 0xFF7043},
    {0.42f, 0xFF5722},
    {0.68f, 0xF4511E},
    {0.88f, 0xE53935},
    {1.00f, 0xC62828},
};
static const int ARC_GRAD_STOP_COUNT = sizeof(ARC_GRAD_STOPS) / sizeof(ARC_GRAD_STOPS[0]);

static lv_color_t color_hex(uint32_t hex)
{
    return lv_color_hex(hex);
}

#define COLOR_ORANGE      color_hex(0xFF5722)
#define COLOR_ORANGE_DIM  color_hex(0xD84315)
#define COLOR_ORANGE_REDDISH color_hex(0xC62828)
#define COLOR_ORANGE_LIGHT color_hex(0xFF8A65)
#define COLOR_GREEN       color_hex(0x8BC34A)
#define COLOR_BLACK       color_hex(0x000000)
#define COLOR_CARD        color_hex(0x161616)
#define COLOR_CARD_BORDER color_hex(0x2E2E2E)
#define COLOR_DIVIDER_SOFT color_hex(0x3A3A3A)
#define COLOR_DARK_GRAY   color_hex(0x404040)
#define COLOR_TRACK_GRAY  color_hex(0x2A2A2A)
#define COLOR_MID_GRAY    color_hex(0x888888)
#define COLOR_LIGHT_GRAY  color_hex(0xB0B0B0)
#define COLOR_WHITE       color_hex(0xFFFFFF)
#define COLOR_YELLOW      color_hex(0xFFCC00)
#define COLOR_RED         color_hex(0xFF4444)

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const int SCREEN_W = 480;
static const int SCREEN_H = 272;
static const int HEADER_H = 36;
static const int HEADER_KILN_MIN_W = 80;
static const int HEADER_KILN_GAP_TO_CONN = 8;
static const int HEADER_CONN_RESERVE_W = 112;
static const int HEADER_KILN_NAME_X = 129;
static const int HEADER_BRAND_GAP = 10;
static const int CARD_RIGHT_MARGIN = 8;
static const int CARD_WIDTH_EXTRA = 15;
static const int CARD_CONTENT_PAD = 8;
static const int TEMP_ARC_MAX = 1000;
#define ARC_SIZE_PX 222
static const int ARC_SIZE = ARC_SIZE_PX;
static const int ARC_SIDE_EXTEND_DEG = 5;
static const int ARC_SWEEP_DEG = 220 + 2 * ARC_SIDE_EXTEND_DEG;
static const int ARC_START_ANGLE = 90 + (360 - ARC_SWEEP_DEG) / 2;
static const int ARC_END_ANGLE = ARC_START_ANGLE + ARC_SWEEP_DEG;
static const int ARC_STROKE_W = 12;
static const int CENTER_DIVIDER_TRIM = 44;
static const int CENTER_DIVIDER_W = ARC_SIZE - 2 * ARC_STROKE_W - CENTER_DIVIDER_TRIM;
static const int CENTER_DIVIDER_Y_EXTRA = 5;
static const int ARC_Y_DOWN_BIAS = 8;
static const int ARC_X_OFFSET = -13;
static const int ARC_CENTER_BLOCK_Y_OFFSET = -15;
static const int CAPTION_Y_OFFSET = -1;
static const int TEMP_CAPTION_Y_EXTRA = -23;
static const int TEMP_VALUE_Y_EXTRA = -12;
static const int RAMP_CAPTION_Y_EXTRA = 12;
static const int RAMP_VALUE_Y_EXTRA = 12;
static const int PROGRESS_BAR_WIDTH_TRIM = 61;
static const int PROGRESS_BAR_HEIGHT = 7;
static const int STATUS_FLAME_ZOOM = 310;
static const int STATUS_FLAME_X_GAP = -12;
static const int STATUS_PILL_TEXT_X_OFFSET = 2;
static const int STATUS_PILL_HORNEANDO_TEXT_X_EXTRA = 5;
static const int SPLASH_SPINNER_Y = -34;
static const int SPLASH_SPINNER_SIZE = 88;
static const int SPLASH_GLOW_OUTER_SIZE = 95;
static const int SPLASH_GLOW_OUTER_SIZE = 95;

static void createSplashGlowLayers(lv_obj_t *parent, lv_coord_t yOffset)
{
    struct GlowLayer {
        lv_coord_t size;
        uint32_t color;
        lv_opa_t opa;
    };
    static const GlowLayer layers[] = {
        { 95, 0xFF5722, (lv_opa_t)3 },
        { 88, 0xF4511E, (lv_opa_t)4 },
        { 82, 0xF04B18, (lv_opa_t)5 },
        { 75, 0xE64A19, (lv_opa_t)6 },
        { 69, 0xDC4516, (lv_opa_t)7 },
        { 62, 0xD84315, (lv_opa_t)8 },
        { 56, 0xCB3310, (lv_opa_t)9 },
        { 49, 0xBF360C, (lv_opa_t)10 },
        { 43, 0xA52808, (lv_opa_t)11 },
        { 36, 0x5C1000, (lv_opa_t)12 },
    };

    for(size_t i = 0; i < sizeof(layers) / sizeof(layers[0]); i++) {
        lv_obj_t *glow = lv_obj_create(parent);
        lv_obj_set_size(glow, layers[i].size, layers[i].size);
        lv_obj_align(glow, LV_ALIGN_CENTER, 0, yOffset);
        lv_obj_set_style_radius(glow, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_color(glow, color_hex(layers[i].color), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(glow, layers[i].opa, LV_PART_MAIN);
        lv_obj_set_style_border_width(glow, 0, LV_PART_MAIN);
        lv_obj_remove_flag(glow, LV_OBJ_FLAG_SCROLLABLE);
    }
}

static uint8_t s_arc_grad_map[ARC_SIZE_PX * ARC_SIZE_PX * 4];
static lv_image_dsc_t s_arc_grad_img;
static bool s_arc_grad_img_ready = false;

static uint32_t lerp_rgb(uint32_t c0, uint32_t c1, float t)
{
    if(t < 0.0f) t = 0.0f;
    if(t > 1.0f) t = 1.0f;

    const int r0 = (int)((c0 >> 16) & 0xFF);
    const int g0 = (int)((c0 >> 8) & 0xFF);
    const int b0 = (int)(c0 & 0xFF);
    const int r1 = (int)((c1 >> 16) & 0xFF);
    const int g1 = (int)((c1 >> 8) & 0xFF);
    const int b1 = (int)(c1 & 0xFF);

    const int r = r0 + (int)((r1 - r0) * t);
    const int g = g0 + (int)((g1 - g0) * t);
    const int b = b0 + (int)((b1 - b0) * t);
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static uint32_t arc_grad_color_at(float t)
{
    if(t <= ARC_GRAD_STOPS[0].pos) {
        return ARC_GRAD_STOPS[0].color;
    }
    for(int i = 1; i < ARC_GRAD_STOP_COUNT; i++) {
        if(t <= ARC_GRAD_STOPS[i].pos) {
            const float span = ARC_GRAD_STOPS[i].pos - ARC_GRAD_STOPS[i - 1].pos;
            const float local = (span > 0.0f) ? (t - ARC_GRAD_STOPS[i - 1].pos) / span : 0.0f;
            return lerp_rgb(ARC_GRAD_STOPS[i - 1].color, ARC_GRAD_STOPS[i].color, local);
        }
    }
    return ARC_GRAD_STOPS[ARC_GRAD_STOP_COUNT - 1].color;
}

static void build_arc_gradient_image(void)
{
    if(s_arc_grad_img_ready) {
        return;
    }

    const float cx = (float)(ARC_SIZE_PX / 2);
    const float cy = (float)(ARC_SIZE_PX / 2);
    const float rout = (float)(ARC_SIZE_PX / 2);
    const float rin = rout - (float)ARC_STROKE_W;

    for(int y = 0; y < ARC_SIZE_PX; y++) {
        for(int x = 0; x < ARC_SIZE_PX; x++) {
            uint8_t *px = &s_arc_grad_map[((size_t)y * ARC_SIZE_PX + (size_t)x) * 4];
            const float dx = (float)x - cx;
            const float dy = (float)y - cy;
            const float r = sqrtf(dx * dx + dy * dy);

            if(r < rin || r > rout) {
                px[0] = px[1] = px[2] = px[3] = 0;
                continue;
            }

            float ang = atan2f(dy, dx) * (180.0f / (float)M_PI);
            if(ang < 0.0f) {
                ang += 360.0f;
            }

            /* Extra angular margin so rounded caps still sample opaque gradient pixels. */
            const float cap_pad_deg = 10.0f;
            float rel = ang - (float)ARC_START_ANGLE;
            if(rel < 0.0f) {
                rel += 360.0f;
            }

            float t;
            if(rel >= 360.0f - cap_pad_deg) {
                t = 0.0f; /* just before start tip */
            } else if(rel <= (float)ARC_SWEEP_DEG + cap_pad_deg) {
                t = rel / (float)ARC_SWEEP_DEG;
                if(t < 0.0f) t = 0.0f;
                if(t > 1.0f) t = 1.0f;
            } else {
                px[0] = px[1] = px[2] = px[3] = 0;
                continue;
            }
            const uint32_t rgb = arc_grad_color_at(t);
            px[0] = (uint8_t)(rgb & 0xFF);         /* B */
            px[1] = (uint8_t)((rgb >> 8) & 0xFF);  /* G */
            px[2] = (uint8_t)((rgb >> 16) & 0xFF); /* R */
            px[3] = 0xFF;                          /* A */
        }
    }

    memset(&s_arc_grad_img, 0, sizeof(s_arc_grad_img));
    s_arc_grad_img.header.magic = LV_IMAGE_HEADER_MAGIC;
    s_arc_grad_img.header.cf = LV_COLOR_FORMAT_ARGB8888;
    s_arc_grad_img.header.w = ARC_SIZE_PX;
    s_arc_grad_img.header.h = ARC_SIZE_PX;
    s_arc_grad_img.header.stride = ARC_SIZE_PX * 4;
    s_arc_grad_img.data_size = sizeof(s_arc_grad_map);
    s_arc_grad_img.data = s_arc_grad_map;
    s_arc_grad_img_ready = true;
}

static void apply_horizontal_gradient(lv_obj_t *obj, lv_color_t from, lv_color_t to)
{
    lv_obj_set_style_bg_color(obj, from, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(obj, to, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_HOR, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
}

static void style_arc_track(lv_obj_t *arc)
{
    build_arc_gradient_image();

    lv_obj_set_style_pad_all(arc, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(arc, 0, LV_PART_INDICATOR);
    lv_obj_set_style_pad_all(arc, 0, LV_PART_KNOB);

    lv_obj_set_style_arc_color(arc, COLOR_TRACK_GRAY, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, ARC_STROKE_W, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_style_arc_width(arc, ARC_STROKE_W, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(arc, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, COLOR_ORANGE, LV_PART_INDICATOR);
    lv_obj_set_style_arc_image_src(arc, &s_arc_grad_img, LV_PART_INDICATOR);

    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_border_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_width(arc, 0, LV_PART_KNOB);
    lv_obj_set_style_height(arc, 0, LV_PART_KNOB);
}

static void update_arc_gradient(int temp_c)
{
    if(temp_c < 0) temp_c = 0;
    if(temp_c > TEMP_ARC_MAX) temp_c = TEMP_ARC_MAX;

    if(arcTemp != NULL) {
        lv_arc_set_value(arcTemp, temp_c);
    }
}

static void placeSummaryColumn(lv_obj_t *caption, lv_obj_t *value,
                              int col, int colW, int capY, int valY)
{
    const int x = col * colW;
    lv_obj_set_width(caption, colW);
    lv_obj_set_pos(caption, x, capY);
    lv_obj_set_style_text_align(caption, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_width(value, colW);
    lv_obj_set_pos(value, x, valY);
    lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
}

static lv_obj_t *createTintedIcon(lv_obj_t *parent, const lv_image_dsc_t *src, lv_color_t color)
{
    lv_obj_t *img = lv_image_create(parent);
    lv_image_set_src(img, src);
    lv_obj_set_style_image_recolor(img, color, LV_PART_MAIN);
    lv_obj_set_style_image_recolor_opa(img, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
    return img;
}

static void setIconTint(lv_obj_t *img, lv_color_t color)
{
    if(img != NULL) {
        lv_obj_set_style_image_recolor(img, color, LV_PART_MAIN);
    }
}

static void styleCaption(lv_obj_t *label)
{
    lv_obj_set_style_text_color(label, COLOR_MID_GRAY, LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &inter_r10, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(label, 1, LV_PART_MAIN);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
}

static void styleSectionTitle(lv_obj_t *label)
{
    styleCaption(label);
    lv_obj_set_style_text_letter_space(label, 1, LV_PART_MAIN);
}

static void formatHeaderKilnName(char *out, size_t outLen, const char *name)
{
    if(outLen == 0) return;
    if(name == NULL || name[0] == '\0') {
        strncpy(out, "Kiln 01", outLen);
        out[outLen - 1] = '\0';
        return;
    }
    strncpy(out, name, outLen);
    out[outLen - 1] = '\0';
}

static lv_coord_t calcHeaderKilnNameWidth(void)
{
    lv_coord_t kilnLeft = HEADER_KILN_NAME_X;
    lv_coord_t connLeft = SCREEN_W - HEADER_CONN_RESERVE_W;

    if(mainScreen != NULL && headerKilnBlock != NULL && labelConnDot != NULL) {
        lv_obj_update_layout(mainScreen);
        lv_coord_t measuredLeft = lv_obj_get_x(headerKilnBlock);
        lv_coord_t measuredConn = lv_obj_get_x(labelConnDot);
        if(measuredLeft > 0) kilnLeft = measuredLeft;
        if(measuredConn > kilnLeft + HEADER_KILN_MIN_W + HEADER_KILN_GAP_TO_CONN) {
            connLeft = measuredConn;
        }
    }

    lv_coord_t width = connLeft - kilnLeft - HEADER_KILN_GAP_TO_CONN;
    if(width < HEADER_KILN_MIN_W) {
        width = SCREEN_W - HEADER_CONN_RESERVE_W - HEADER_KILN_NAME_X - HEADER_KILN_GAP_TO_CONN;
    }
    return width;
}

static void layoutHeaderKilnNameBlock(void)
{
    if(headerKilnBlock == NULL || labelKilnTitle == NULL) return;
    const lv_coord_t width = calcHeaderKilnNameWidth();
    lv_obj_set_width(headerKilnBlock, width);
    lv_obj_set_width(labelKilnTitle, width);
}

static void styleCard(lv_obj_t *card)
{
    lv_obj_set_style_bg_color(card, COLOR_CARD, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 10, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(card, COLOR_CARD_BORDER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 8, LV_PART_MAIN);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
}

static void styleValue(lv_obj_t *label, const lv_font_t *font)
{
    lv_obj_set_style_text_color(label, COLOR_WHITE, LV_PART_MAIN);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
}

static lv_obj_t *createSectionHeader(lv_obj_t *parent, const char *text, int x, int y)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    styleSectionTitle(label);
    lv_obj_set_pos(label, x, y + CAPTION_Y_OFFSET);
    return label;
}

static void create_temperature_gauge(lv_obj_t *parent, int arcX, int arcY)
{
    arcTemp = lv_arc_create(parent);
    lv_obj_set_size(arcTemp, ARC_SIZE, ARC_SIZE);
    lv_obj_set_pos(arcTemp, arcX, arcY);
    lv_arc_set_rotation(arcTemp, 0);
    lv_arc_set_bg_angles(arcTemp, ARC_START_ANGLE, ARC_END_ANGLE);
    lv_arc_set_range(arcTemp, 0, TEMP_ARC_MAX);
    lv_arc_set_mode(arcTemp, LV_ARC_MODE_NORMAL);
    lv_arc_set_value(arcTemp, 0);
    style_arc_track(arcTemp);

    arcGradLayer = NULL;
}

static void createMainUi(void)
{
    mainScreen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(mainScreen, COLOR_CARD, LV_PART_MAIN);
    lv_obj_remove_flag(mainScreen, LV_OBJ_FLAG_SCROLLABLE);
    lv_screen_load(mainScreen);

    lv_obj_t *headerBar = lv_obj_create(mainScreen);
    lv_obj_set_size(headerBar, SCREEN_W, HEADER_H);
    lv_obj_set_pos(headerBar, 0, 0);
    lv_obj_set_style_bg_color(headerBar, COLOR_BLACK, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(headerBar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(headerBar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(headerBar, 0, LV_PART_MAIN);
    lv_obj_remove_flag(headerBar, LV_OBJ_FLAG_SCROLLABLE);

    imgLogo = createTintedIcon(mainScreen, &icon_flame, COLOR_ORANGE);
    lv_image_set_scale(imgLogo, 300);
    lv_obj_set_pos(imgLogo, 6, 7);

    labelBrandSmart = lv_label_create(mainScreen);
    lv_label_set_text(labelBrandSmart, "Smart");
    lv_obj_set_style_text_color(labelBrandSmart, COLOR_ORANGE, LV_PART_MAIN);
    lv_obj_set_style_text_font(labelBrandSmart, &inter_sb18, LV_PART_MAIN);
    lv_obj_align_to(labelBrandSmart, imgLogo, LV_ALIGN_OUT_RIGHT_MID, 4, 0);

    labelBrandKiln = lv_label_create(mainScreen);
    lv_label_set_text(labelBrandKiln, "Kiln");
    lv_obj_set_style_text_color(labelBrandKiln, COLOR_WHITE, LV_PART_MAIN);
    lv_obj_set_style_text_font(labelBrandKiln, &inter_sb18, LV_PART_MAIN);
    lv_obj_align_to(labelBrandKiln, labelBrandSmart, LV_ALIGN_OUT_RIGHT_MID, 0, 0);

    headerDivider = lv_obj_create(mainScreen);
    lv_obj_set_size(headerDivider, 1, 22);
    lv_obj_set_style_bg_color(headerDivider, COLOR_MID_GRAY, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(headerDivider, LV_OPA_60, LV_PART_MAIN);
    lv_obj_set_style_border_width(headerDivider, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(headerDivider, 0, LV_PART_MAIN);
    lv_obj_remove_flag(headerDivider, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align_to(headerDivider, labelBrandKiln, LV_ALIGN_OUT_RIGHT_MID, HEADER_BRAND_GAP, 0);

    headerKilnBlock = lv_obj_create(mainScreen);
    lv_obj_set_size(headerKilnBlock, HEADER_KILN_MIN_W, 28);
    lv_obj_set_style_bg_opa(headerKilnBlock, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(headerKilnBlock, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(headerKilnBlock, 0, LV_PART_MAIN);
    lv_obj_remove_flag(headerKilnBlock, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align_to(headerKilnBlock, headerDivider, LV_ALIGN_OUT_RIGHT_MID, HEADER_BRAND_GAP, 0);

    labelKilnTitle = lv_label_create(headerKilnBlock);
    lv_label_set_text(labelKilnTitle, "Kiln 01");
    lv_obj_set_style_text_color(labelKilnTitle, COLOR_WHITE, LV_PART_MAIN);
    lv_obj_set_style_text_font(labelKilnTitle, &inter_sb14, LV_PART_MAIN);
    lv_obj_set_pos(labelKilnTitle, 0, 0);
    lv_label_set_long_mode(labelKilnTitle, LV_LABEL_LONG_DOT);
    lv_obj_set_width(labelKilnTitle, HEADER_KILN_MIN_W);

    labelDeviceId = lv_label_create(headerKilnBlock);
    lv_label_set_text(labelDeviceId, "SK-001");
    lv_obj_set_style_text_color(labelDeviceId, COLOR_MID_GRAY, LV_PART_MAIN);
    lv_obj_set_style_text_font(labelDeviceId, &inter_r10, LV_PART_MAIN);
    lv_obj_set_pos(labelDeviceId, 0, 14);

    labelConnDot = lv_label_create(mainScreen);
    lv_label_set_text(labelConnDot, LV_SYMBOL_OK);
    lv_obj_set_style_text_color(labelConnDot, COLOR_GREEN, LV_PART_MAIN);
    lv_obj_set_style_text_font(labelConnDot, &lv_font_montserrat_14, LV_PART_MAIN);

    labelConnText = lv_label_create(mainScreen);
    lv_label_set_text(labelConnText, "Conectado");
    lv_obj_set_style_text_color(labelConnText, COLOR_WHITE, LV_PART_MAIN);
    lv_obj_set_style_text_font(labelConnText, &inter_r12, LV_PART_MAIN);

    imgWifiIcon = createTintedIcon(mainScreen, &icon_wifi, COLOR_WHITE);
    lv_obj_align(imgWifiIcon, LV_ALIGN_TOP_RIGHT, -8, 9);

    lv_obj_align_to(labelConnText, imgWifiIcon, LV_ALIGN_OUT_LEFT_MID, -6, 0);
    lv_obj_align_to(labelConnDot, labelConnText, LV_ALIGN_OUT_LEFT_MID, -4, 0);
    layoutHeaderKilnNameBlock();

    const int leftW = 276;
    const int contentY = HEADER_H + 2;
    const int contentH = SCREEN_H - contentY - 8;

    leftPanel = lv_obj_create(mainScreen);
    lv_obj_set_size(leftPanel, leftW, contentH);
    lv_obj_set_pos(leftPanel, 6, contentY);
    lv_obj_set_style_bg_opa(leftPanel, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(leftPanel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(leftPanel, 0, LV_PART_MAIN);
    lv_obj_remove_flag(leftPanel, LV_OBJ_FLAG_SCROLLABLE);

    const int progressReserve = 44;
    const int arcZoneH = contentH - progressReserve;
    const int arcX = (leftW - ARC_SIZE) / 2 + ARC_X_OFFSET;
    int arcY = (arcZoneH > ARC_SIZE)
                   ? (arcZoneH - ARC_SIZE) / 2 + ARC_Y_DOWN_BIAS
                   : ARC_Y_DOWN_BIAS;

    create_temperature_gauge(leftPanel, arcX, arcY);

    labelTempCaption = lv_label_create(leftPanel);
    lv_label_set_text(labelTempCaption, "TEMPERATURA");
    styleCaption(labelTempCaption);
    lv_obj_align_to(labelTempCaption, arcTemp, LV_ALIGN_CENTER, 0,
                    -26 + ARC_CENTER_BLOCK_Y_OFFSET + CAPTION_Y_OFFSET + TEMP_CAPTION_Y_EXTRA);

    labelTempValue = lv_label_create(leftPanel);
    lv_label_set_text(labelTempValue, "--");
    styleValue(labelTempValue, &inter_sb44);
    lv_obj_align_to(labelTempValue, arcTemp, LV_ALIGN_CENTER, -10,
                    4 + ARC_CENTER_BLOCK_Y_OFFSET + TEMP_VALUE_Y_EXTRA);

    labelTempUnit = lv_label_create(leftPanel);
    lv_label_set_text(labelTempUnit, "\xC2\xB0""C");
    lv_obj_set_style_text_color(labelTempUnit, COLOR_WHITE, LV_PART_MAIN);
    lv_obj_set_style_text_font(labelTempUnit, &inter_r20, LV_PART_MAIN);
    lv_obj_align_to(labelTempUnit, labelTempValue, LV_ALIGN_OUT_RIGHT_BOTTOM, 2, -4);

    centerDivider = lv_obj_create(leftPanel);
    lv_obj_set_size(centerDivider, CENTER_DIVIDER_W, 1);
    lv_obj_set_style_bg_color(centerDivider, COLOR_DIVIDER_SOFT, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(centerDivider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(centerDivider, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(centerDivider, 0, LV_PART_MAIN);
    lv_obj_remove_flag(centerDivider, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align_to(centerDivider, arcTemp, LV_ALIGN_CENTER, 0,
                    24 + ARC_CENTER_BLOCK_Y_OFFSET + CENTER_DIVIDER_Y_EXTRA);

    labelRampCaption = lv_label_create(leftPanel);
    lv_label_set_text(labelRampCaption, "VELOCIDAD DE ASCENSO");
    styleCaption(labelRampCaption);
    lv_obj_align_to(labelRampCaption, arcTemp, LV_ALIGN_CENTER, 0,
                    34 + ARC_CENTER_BLOCK_Y_OFFSET + CAPTION_Y_OFFSET + RAMP_CAPTION_Y_EXTRA);

    labelRampValue = lv_label_create(leftPanel);
    lv_label_set_text(labelRampValue, "--\xC2\xB0""C/min");
    lv_obj_set_style_text_color(labelRampValue, COLOR_GREEN, LV_PART_MAIN);
    lv_obj_set_style_text_font(labelRampValue, &inter_r16, LV_PART_MAIN);
    lv_obj_align_to(labelRampValue, arcTemp, LV_ALIGN_CENTER, 0, 52 + ARC_CENTER_BLOCK_Y_OFFSET + RAMP_VALUE_Y_EXTRA);

    labelProgressCaption = lv_label_create(leftPanel);
    lv_label_set_text(labelProgressCaption, "AVANCE DEL CICLO");
    styleCaption(labelProgressCaption);
    lv_obj_set_pos(labelProgressCaption, 8, contentH - 34 + CAPTION_Y_OFFSET);

    barCycleProgress = lv_bar_create(leftPanel);
    lv_obj_set_size(barCycleProgress, leftW - PROGRESS_BAR_WIDTH_TRIM, PROGRESS_BAR_HEIGHT);
    lv_obj_set_pos(barCycleProgress, 8, contentH - 18);
    lv_bar_set_range(barCycleProgress, 0, 100);
    lv_bar_set_value(barCycleProgress, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(barCycleProgress, COLOR_TRACK_GRAY, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(barCycleProgress, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(barCycleProgress, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_color(barCycleProgress, COLOR_ORANGE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(barCycleProgress, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(barCycleProgress, 4, LV_PART_INDICATOR);

    labelProgressPct = lv_label_create(leftPanel);
    lv_label_set_text(labelProgressPct, "0%");
    lv_obj_set_style_text_color(labelProgressPct, COLOR_ORANGE, LV_PART_MAIN);
    lv_obj_set_style_text_font(labelProgressPct, &inter_r14, LV_PART_MAIN);
    lv_obj_align_to(labelProgressPct, barCycleProgress, LV_ALIGN_OUT_RIGHT_MID, 6, 0);

    const int rightX = 288 - CARD_WIDTH_EXTRA;
    const int rightW = SCREEN_W - rightX - CARD_RIGHT_MARGIN;
    const int barBottomY = contentY + contentH - 8;
    const int CARD_GAP = 8;
    const int CARD_H_PROGRAM = 47;
    const int CARD_H_SUMMARY = 84;
    const int cardProgramY = barBottomY - CARD_H_PROGRAM;
    const int cardSummaryY = cardProgramY - CARD_GAP - CARD_H_SUMMARY;
    const int CARD_H_STATUS = cardSummaryY - CARD_GAP - contentY;

    cardStatus = lv_obj_create(mainScreen);
    lv_obj_set_size(cardStatus, rightW, CARD_H_STATUS);
    lv_obj_set_pos(cardStatus, rightX, contentY);
    styleCard(cardStatus);
    lv_obj_set_style_pad_all(cardStatus, CARD_CONTENT_PAD, LV_PART_MAIN);
    labelStatusHeader = createSectionHeader(cardStatus, "ESTADO ACTUAL", 0, 0);

    statusPill = lv_obj_create(cardStatus);
    lv_obj_set_size(statusPill, rightW - 2 * CARD_CONTENT_PAD, 32);
    lv_obj_set_pos(statusPill, 0, 18);
    apply_horizontal_gradient(statusPill, COLOR_ORANGE_LIGHT, COLOR_ORANGE);
    lv_obj_set_style_radius(statusPill, 14, LV_PART_MAIN);
    lv_obj_set_style_border_width(statusPill, 0, LV_PART_MAIN);
    lv_obj_remove_flag(statusPill, LV_OBJ_FLAG_SCROLLABLE);

    labelStatusPill = lv_label_create(statusPill);
    lv_label_set_text(labelStatusPill, "INACTIVO");
    lv_obj_set_style_text_color(labelStatusPill, COLOR_WHITE, LV_PART_MAIN);
    lv_obj_set_style_text_font(labelStatusPill, &inter_sb14, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(labelStatusPill, 2, LV_PART_MAIN);
    lv_obj_align(labelStatusPill, LV_ALIGN_CENTER, STATUS_PILL_TEXT_X_OFFSET, 0);

    imgStatusFlame = createTintedIcon(statusPill, &icon_flame, COLOR_WHITE);
    lv_image_set_scale(imgStatusFlame, STATUS_FLAME_ZOOM);
    lv_obj_align_to(imgStatusFlame, labelStatusPill, LV_ALIGN_OUT_LEFT_MID, STATUS_FLAME_X_GAP, 0);
    lv_obj_add_flag(imgStatusFlame, LV_OBJ_FLAG_HIDDEN);

    cardSummary = lv_obj_create(mainScreen);
    lv_obj_set_size(cardSummary, rightW, CARD_H_SUMMARY);
    lv_obj_set_pos(cardSummary, rightX, cardSummaryY);
    styleCard(cardSummary);
    lv_obj_set_style_pad_all(cardSummary, CARD_CONTENT_PAD, LV_PART_MAIN);
    labelSummaryHeader = createSectionHeader(cardSummary, "RESUMEN DEL CICLO", 0, 0);

    const int colW = (rightW - 16) / 3;
    const int summaryDividerY = 14;
    const int summaryDividerH = CARD_H_SUMMARY - summaryDividerY - 10;
    for(int d = 1; d <= 2; d++) {
        lv_obj_t *divider = lv_obj_create(cardSummary);
        lv_obj_set_size(divider, 1, summaryDividerH);
        lv_obj_set_pos(divider, d * colW, summaryDividerY);
        lv_obj_set_style_bg_color(divider, COLOR_DIVIDER_SOFT, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(divider, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(divider, 0, LV_PART_MAIN);
        lv_obj_remove_flag(divider, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(divider, LV_OBJ_FLAG_CLICKABLE);
    }

    labelObjCaption = lv_label_create(cardSummary);
    lv_label_set_text(labelObjCaption, "OBJETIVO");
    styleCaption(labelObjCaption);

    labelObjValue = lv_label_create(cardSummary);
    lv_label_set_text(labelObjValue, "--");
    styleValue(labelObjValue, &inter_r14);

    labelStageCaption = lv_label_create(cardSummary);
    lv_label_set_text(labelStageCaption, "ETAPA");
    styleCaption(labelStageCaption);

    labelStageValue = lv_label_create(cardSummary);
    lv_label_set_text(labelStageValue, "--");
    styleValue(labelStageValue, &inter_r14);

    labelTimeCaption = lv_label_create(cardSummary);
    lv_label_set_text(labelTimeCaption, "TIEMPO");
    styleCaption(labelTimeCaption);

    labelTimeValue = lv_label_create(cardSummary);
    lv_label_set_text(labelTimeValue, "--");
    styleValue(labelTimeValue, &inter_r14);

    placeSummaryColumn(labelObjCaption, labelObjValue, 0, colW, 16 + CAPTION_Y_OFFSET, 30);
    placeSummaryColumn(labelStageCaption, labelStageValue, 1, colW, 16 + CAPTION_Y_OFFSET, 30);
    placeSummaryColumn(labelTimeCaption, labelTimeValue, 2, colW, 16 + CAPTION_Y_OFFSET, 30);

    imgObjIcon = createTintedIcon(cardSummary, &icon_target, COLOR_ORANGE);
    lv_obj_align_to(imgObjIcon, labelObjValue, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);

    imgStageIcon = createTintedIcon(cardSummary, &icon_stairs, COLOR_ORANGE);
    lv_obj_align_to(imgStageIcon, labelStageValue, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);

    imgTimeIcon = createTintedIcon(cardSummary, &icon_clock, COLOR_ORANGE);
    lv_obj_align_to(imgTimeIcon, labelTimeValue, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);

    cardProgram = lv_obj_create(mainScreen);
    lv_obj_set_size(cardProgram, rightW, CARD_H_PROGRAM);
    lv_obj_set_pos(cardProgram, rightX, cardProgramY);
    styleCard(cardProgram);
    lv_obj_set_style_pad_all(cardProgram, CARD_CONTENT_PAD, LV_PART_MAIN);
    labelProgramHeader = createSectionHeader(cardProgram, "PROGRAMA ACTUAL", 0, 0);

    labelProgramValue = lv_label_create(cardProgram);
    lv_label_set_text(labelProgramValue, "--");
    lv_obj_set_style_text_color(labelProgramValue, COLOR_WHITE, LV_PART_MAIN);
    lv_obj_set_style_text_font(labelProgramValue, &inter_r14, LV_PART_MAIN);
    lv_obj_set_pos(labelProgramValue, 0, 16);
    lv_label_set_long_mode(labelProgramValue, LV_LABEL_LONG_DOT);
    lv_obj_set_width(labelProgramValue, rightW - 2 * CARD_CONTENT_PAD);
}

void display_ui_show_splash(void)
{
    bootScreen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(bootScreen, COLOR_BLACK, LV_PART_MAIN);
    lv_obj_remove_flag(bootScreen, LV_OBJ_FLAG_SCROLLABLE);

    createSplashGlowLayers(bootScreen, SPLASH_SPINNER_Y);

    lv_obj_t *spinner = lv_spinner_create(bootScreen);
    lv_obj_set_size(spinner, SPLASH_SPINNER_SIZE, SPLASH_SPINNER_SIZE);
    lv_obj_align(spinner, LV_ALIGN_CENTER, 0, SPLASH_SPINNER_Y);
    lv_obj_set_style_arc_color(spinner, COLOR_TRACK_GRAY, LV_PART_MAIN);
    lv_obj_set_style_arc_width(spinner, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_color(spinner, COLOR_ORANGE, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(spinner, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(spinner, true, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(spinner, LV_OPA_TRANSP, LV_PART_MAIN);

    lv_obj_t *titleSmart = lv_label_create(bootScreen);
    lv_label_set_text(titleSmart, "Smart");
    lv_obj_set_style_text_color(titleSmart, COLOR_ORANGE, LV_PART_MAIN);
    lv_obj_set_style_text_font(titleSmart, &inter_sb18, LV_PART_MAIN);

    lv_obj_t *titleKiln = lv_label_create(bootScreen);
    lv_label_set_text(titleKiln, "Kiln");
    lv_obj_set_style_text_color(titleKiln, COLOR_WHITE, LV_PART_MAIN);
    lv_obj_set_style_text_font(titleKiln, &inter_sb18, LV_PART_MAIN);

    lv_obj_update_layout(bootScreen);
    const lv_coord_t smartW = lv_obj_get_width(titleSmart);
    const lv_coord_t kilnW = lv_obj_get_width(titleKiln);
    lv_obj_align(titleSmart, LV_ALIGN_CENTER, -(smartW + kilnW) / 4, 36);
    lv_obj_align_to(titleKiln, titleSmart, LV_ALIGN_OUT_RIGHT_MID, 0, 0);
    lv_obj_move_foreground(titleSmart);
    lv_obj_move_foreground(titleKiln);

    lv_obj_t *msg = lv_label_create(bootScreen);
    lv_label_set_text(msg, "Iniciando...");
    lv_obj_set_style_text_color(msg, COLOR_LIGHT_GRAY, LV_PART_MAIN);
    lv_obj_set_style_text_font(msg, &inter_r16, LV_PART_MAIN);
    lv_obj_align(msg, LV_ALIGN_CENTER, 0, 62);

    lv_screen_load(bootScreen);
}

void display_ui_show_main(void)
{
    createMainUi();
    if(bootScreen != NULL) {
        lv_obj_delete(bootScreen);
        bootScreen = NULL;
    }
}

static int computeCycleProgressPct(const DisplayData *data)
{
    if(data->totalMinutes > 0 && data->elapsedMinutes >= 0) {
        int pct = (data->elapsedMinutes * 100) / data->totalMinutes;
        if(pct < 0) pct = 0;
        if(pct > 100) pct = 100;
        return pct;
    }
    if(data->totalStages > 0 && data->currentStage > 0) {
        int pct = ((data->currentStage - 1) * 100) / data->totalStages;
        if(pct < 0) pct = 0;
        if(pct > 100) pct = 100;
        return pct;
    }
    return 0;
}

static void formatRemainingTime(char *out, size_t outLen, int minutes)
{
    if(minutes < 0) {
        strncpy(out, "--", outLen);
        out[outLen - 1] = '\0';
        return;
    }
    const int hours = minutes / 60;
    const int mins = minutes % 60;
    if(hours > 0) {
        snprintf(out, outLen, "%dh %02dm", hours, mins);
    } else {
        snprintf(out, outLen, "%dm", mins);
    }
}

static void formatDeviceId(char *out, size_t outLen, const char *deviceId)
{
    if(deviceId == NULL || deviceId[0] == '\0') {
        strncpy(out, "SK-001", outLen);
    } else {
        snprintf(out, outLen, "SK-%s", deviceId);
    }
    out[outLen - 1] = '\0';
}

static void apply_status_pill_style(const char *pillText)
{
    if(statusPill == NULL) return;

    if(strcmp(pillText, "HORNEANDO") == 0) {
        apply_horizontal_gradient(statusPill, COLOR_ORANGE, COLOR_ORANGE_REDDISH);
    } else if(strcmp(pillText, "PAUSADO") == 0) {
        lv_obj_set_style_bg_grad_dir(statusPill, LV_GRAD_DIR_NONE, LV_PART_MAIN);
        lv_obj_set_style_bg_color(statusPill, COLOR_YELLOW, LV_PART_MAIN);
    } else if(strcmp(pillText, "DETENIDO") == 0) {
        lv_obj_set_style_bg_grad_dir(statusPill, LV_GRAD_DIR_NONE, LV_PART_MAIN);
        lv_obj_set_style_bg_color(statusPill, COLOR_RED, LV_PART_MAIN);
    } else if(strcmp(pillText, "FINALIZADO") == 0) {
        lv_obj_set_style_bg_grad_dir(statusPill, LV_GRAD_DIR_NONE, LV_PART_MAIN);
        lv_obj_set_style_bg_color(statusPill, COLOR_GREEN, LV_PART_MAIN);
    } else if(strcmp(pillText, "INTERRUMPIDO") == 0) {
        apply_horizontal_gradient(statusPill, COLOR_ORANGE_DIM, COLOR_ORANGE);
    } else {
        lv_obj_set_style_bg_grad_dir(statusPill, LV_GRAD_DIR_NONE, LV_PART_MAIN);
        lv_obj_set_style_bg_color(statusPill, COLOR_DARK_GRAY, LV_PART_MAIN);
    }
}

void display_ui_update(const DisplayData *data)
{
    if(mainScreen == NULL || data == NULL) return;

    if(labelRampValue != NULL) {
        char rampStr[20];
        const bool activeRun = (strcmp(data->status, "CALENTANDO") == 0 ||
                                strcmp(data->status, "PAUSADO") == 0 ||
                                strcmp(data->status, "INTERRUMPIDO") == 0);

        if(activeRun && strcmp(data->currentPhase, "REMOJO") == 0) {
            snprintf(rampStr, sizeof(rampStr), "0\xC2\xB0""C/min");
            lv_obj_set_style_text_color(labelRampValue, COLOR_MID_GRAY, LV_PART_MAIN);
        } else if(activeRun && strcmp(data->currentPhase, "RAMPA") == 0 &&
                  data->stageRampCpm > -0.5f) {
            snprintf(rampStr, sizeof(rampStr), "%+.0f\xC2\xB0""C/min", data->stageRampCpm);
            lv_obj_set_style_text_color(labelRampValue,
                                        data->stageRampCpm > 0.0f ? COLOR_GREEN : color_hex(0x66AAFF),
                                        LV_PART_MAIN);
        } else {
            strncpy(rampStr, "--\xC2\xB0""C/min", sizeof(rampStr));
            lv_obj_set_style_text_color(labelRampValue, COLOR_MID_GRAY, LV_PART_MAIN);
        }
        lv_label_set_text(labelRampValue, rampStr);
    }

    if(labelKilnTitle != NULL) {
        char title[64];
        formatHeaderKilnName(title, sizeof(title), data->kilnName);
        lv_label_set_text(labelKilnTitle, title);
    }

    if(labelDeviceId != NULL) {
        char idBuf[20];
        formatDeviceId(idBuf, sizeof(idBuf), data->deviceId);
        lv_label_set_text(labelDeviceId, idBuf);
    }

    const bool online = data->connection.wifiConnected || data->connection.firebaseConnected;
    if(labelConnDot != NULL) {
        lv_obj_set_style_text_color(labelConnDot, online ? COLOR_GREEN : COLOR_MID_GRAY, LV_PART_MAIN);
    }
    if(labelConnText != NULL) {
        lv_label_set_text(labelConnText, online ? "Conectado" : "Offline");
        lv_obj_set_style_text_color(labelConnText, online ? COLOR_WHITE : COLOR_MID_GRAY, LV_PART_MAIN);
    }
    if(imgWifiIcon != NULL) {
        setIconTint(imgWifiIcon, data->connection.wifiConnected ? COLOR_WHITE : COLOR_MID_GRAY);
    }
    layoutHeaderKilnNameBlock();

    update_arc_gradient((int)data->currentTemp);

    if(labelTempValue != NULL) {
        char tempStr[12];
        snprintf(tempStr, sizeof(tempStr), "%.0f", data->currentTemp);
        lv_label_set_text(labelTempValue, tempStr);
        if(arcTemp != NULL) {
            lv_obj_align_to(labelTempValue, arcTemp, LV_ALIGN_CENTER, -10,
                            4 + ARC_CENTER_BLOCK_Y_OFFSET + TEMP_VALUE_Y_EXTRA);
        }
        if(labelTempUnit != NULL && labelTempValue != NULL) {
            lv_obj_align_to(labelTempUnit, labelTempValue, LV_ALIGN_OUT_RIGHT_BOTTOM, 2, -4);
        }
    }

    const int progressPct = computeCycleProgressPct(data);
    if(barCycleProgress != NULL) {
        lv_bar_set_value(barCycleProgress, progressPct, LV_ANIM_OFF);
    }
    if(labelProgressPct != NULL) {
        char pctStr[8];
        snprintf(pctStr, sizeof(pctStr), "%d%%", progressPct);
        lv_label_set_text(labelProgressPct, pctStr);
    }

    if(labelStatusPill != NULL && statusPill != NULL) {
        const char *pillText = "INACTIVO";

        if(strcmp(data->status, "CALENTANDO") == 0) {
            pillText = "HORNEANDO";
        } else if(strcmp(data->status, "PAUSADO") == 0) {
            pillText = "PAUSADO";
        } else if(strcmp(data->status, "DETENIDO") == 0) {
            pillText = "DETENIDO";
        } else if(strcmp(data->status, "FINALIZADO") == 0) {
            pillText = "FINALIZADO";
        } else if(strcmp(data->status, "INTERRUMPIDO") == 0) {
            pillText = "INTERRUMPIDO";
        }

        lv_label_set_text(labelStatusPill, pillText);
        apply_status_pill_style(pillText);

        const int pillTextX = (strcmp(pillText, "HORNEANDO") == 0)
            ? STATUS_PILL_TEXT_X_OFFSET + STATUS_PILL_HORNEANDO_TEXT_X_EXTRA
            : STATUS_PILL_TEXT_X_OFFSET;
        lv_obj_align(labelStatusPill, LV_ALIGN_CENTER, pillTextX, 0);

        if(imgStatusFlame != NULL) {
            const bool showFlame = (strcmp(pillText, "HORNEANDO") == 0);
            if(showFlame) {
                lv_obj_remove_flag(imgStatusFlame, LV_OBJ_FLAG_HIDDEN);
                setIconTint(imgStatusFlame, COLOR_WHITE);
                lv_obj_align_to(imgStatusFlame, labelStatusPill, LV_ALIGN_OUT_LEFT_MID, STATUS_FLAME_X_GAP, 0);
            } else {
                lv_obj_add_flag(imgStatusFlame, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }

    if(labelObjValue != NULL) {
        if(data->targetTemp > 0.0f) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%.0f\xC2\xB0""C", data->targetTemp);
            lv_label_set_text(labelObjValue, buf);
        } else {
            lv_label_set_text(labelObjValue, "--");
        }
    }
    if(labelStageValue != NULL) {
        if(data->currentStage > 0 && data->totalStages > 0) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d/%d", data->currentStage, data->totalStages);
            lv_label_set_text(labelStageValue, buf);
        } else {
            lv_label_set_text(labelStageValue, "--");
        }
    }
    if(labelTimeValue != NULL) {
        int minutesToShow = -1;
        if(data->remainingMinutes >= 0) {
            minutesToShow = data->remainingMinutes;
        } else if(data->totalMinutes > 0) {
            minutesToShow = data->totalMinutes;
        }
        char timeStr[16];
        formatRemainingTime(timeStr, sizeof(timeStr), minutesToShow);
        lv_label_set_text(labelTimeValue, timeStr);
    }

    if(labelProgramValue != NULL) {
        if(strlen(data->programName) > 0) {
            lv_label_set_text(labelProgramValue, data->programName);
        } else {
            lv_label_set_text(labelProgramValue, "--");
        }
    }
}

void display_ui_fill_demo(DisplayData *data, uint32_t tick_ms)
{
    if(data == NULL) return;

    memset(data, 0, sizeof(*data));

    const uint32_t phase = (tick_ms / 15000U) % 5U;
    strncpy(data->kilnName, "Mi horno de Prueba", sizeof(data->kilnName));
    strncpy(data->deviceId, "001", sizeof(data->deviceId));
    strncpy(data->programName, "Esmalte Cone 6", sizeof(data->programName));
    data->connection.wifiConnected = true;
    data->connection.firebaseConnected = true;
    data->targetTemp = 980.0f;
    data->totalStages = 4;
    data->totalMinutes = 240;
    data->elapsedMinutes = 163;
    data->remainingMinutes = 102;

    switch(phase) {
    case 0:
        strncpy(data->status, "CALENTANDO", sizeof(data->status));
        strncpy(data->currentPhase, "RAMPA", sizeof(data->currentPhase));
        data->stageRampCpm = 12.0f;
        data->currentStage = 2;
        data->currentTemp = 875.0f + (float)((int)(tick_ms / 500U) % 20 - 10);
        break;
    case 1:
        strncpy(data->status, "PAUSADO", sizeof(data->status));
        strncpy(data->currentPhase, "REMOJO", sizeof(data->currentPhase));
        data->currentStage = 3;
        data->currentTemp = 650.0f;
        break;
    case 2:
        strncpy(data->status, "INTERRUMPIDO", sizeof(data->status));
        strncpy(data->currentPhase, "RAMPA", sizeof(data->currentPhase));
        data->stageRampCpm = -80.0f;
        data->currentStage = 3;
        data->currentTemp = 520.0f;
        break;
    case 3:
        strncpy(data->status, "FINALIZADO", sizeof(data->status));
        strncpy(data->currentPhase, "REMOJO", sizeof(data->currentPhase));
        data->currentStage = 4;
        data->currentTemp = 85.0f;
        data->remainingMinutes = 0;
        data->elapsedMinutes = 240;
        break;
    default:
        strncpy(data->status, "DETENIDO", sizeof(data->status));
        strncpy(data->currentPhase, "RAMPA", sizeof(data->currentPhase));
        data->currentStage = 0;
        data->currentTemp = 24.0f;
        data->remainingMinutes = -1;
        data->connection.wifiConnected = false;
        break;
    }
}
