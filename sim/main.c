#include <SDL.h>

#include <lvgl.h>
#include "src/others/snapshot/lv_snapshot.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "display_ui.h"

#define SCREEN_W 480
#define SCREEN_H 272
#define SPLASH_MS 1500
#define DEMO_UPDATE_MS 200

static void save_draw_buf_bmp(const lv_draw_buf_t *draw_buf, const char *path)
{
    if(draw_buf == NULL || draw_buf->data == NULL) {
        return;
    }

    const int w = (int)draw_buf->header.w;
    const int h = (int)draw_buf->header.h;
    const uint32_t stride = draw_buf->header.stride;
    const uint8_t *src = draw_buf->data;

    FILE *fp = fopen(path, "wb");
    if(fp == NULL) {
        return;
    }

    const uint32_t row_bytes = (uint32_t)w * 3;
    const uint32_t file_size = 54U + row_bytes * (uint32_t)h;
    const uint8_t header[54] = {
        'B', 'M',
        (uint8_t)(file_size), (uint8_t)(file_size >> 8), (uint8_t)(file_size >> 16), (uint8_t)(file_size >> 24),
        0, 0, 0, 0,
        54, 0, 0, 0,
        40, 0, 0, 0,
        (uint8_t)(w), (uint8_t)(w >> 8), (uint8_t)(w >> 16), (uint8_t)(w >> 24),
        (uint8_t)(h), (uint8_t)(h >> 8), (uint8_t)(h >> 16), (uint8_t)(h >> 24),
        1, 0,
        24, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
    };

    fwrite(header, 1, sizeof(header), fp);

    for(int y = h - 1; y >= 0; y--) {
        const uint8_t *row = src + (size_t)y * stride;
        for(int x = 0; x < w; x++) {
            const uint8_t b = row[x * 4 + 0];
            const uint8_t g = row[x * 4 + 1];
            const uint8_t r = row[x * 4 + 2];
            fputc(b, fp);
            fputc(g, fp);
            fputc(r, fp);
        }
    }

    fclose(fp);
}

static int run_verify(lv_display_t *disp)
{
    LV_UNUSED(disp);

    display_ui_show_main();

    static const struct {
        uint32_t tick_ms;
        const char *name;
    } shots[] = {
        {0U, "875c"},
        {15000U, "650c"},
        {45000U, "85c"},
    };

    for(size_t i = 0; i < sizeof(shots) / sizeof(shots[0]); i++) {
        DisplayData demo = {0};
        display_ui_fill_demo(&demo, shots[i].tick_ms);
        display_ui_update(&demo);
        lv_refr_now(NULL);

        lv_obj_t *screen = lv_screen_active();
        lv_draw_buf_t *shot = lv_snapshot_take(screen, LV_COLOR_FORMAT_ARGB8888);
        if(shot != NULL) {
            char path[128];
            snprintf(path, sizeof(path), "verify_%s.bmp", shots[i].name);
            save_draw_buf_bmp(shot, path);
            lv_draw_buf_destroy(shot);
        }
    }

    return 0;
}

int main(int argc, char **argv)
{
    lv_init();

    lv_display_t *disp = lv_sdl_window_create(SCREEN_W, SCREEN_H);
    if(disp == NULL) {
        return 1;
    }
    lv_sdl_window_set_title(disp, "SmartKiln UI Simulator (LVGL 9)");
    lv_sdl_mouse_create();

    if(argc > 1 && strcmp(argv[1], "--verify") == 0) {
        return run_verify(disp);
    }

    display_ui_show_splash();

    const uint32_t boot_start = lv_tick_get();
    bool main_ready = false;
    uint32_t last_demo_ms = 0;
    DisplayData demo = {0};

    while(1) {
        const uint32_t now = lv_tick_get();

        if(!main_ready && lv_tick_elaps(boot_start) >= SPLASH_MS) {
            display_ui_show_main();
            main_ready = true;
            last_demo_ms = now;
            display_ui_fill_demo(&demo, 0);
            display_ui_update(&demo);
        }

        if(main_ready && lv_tick_elaps(last_demo_ms) >= DEMO_UPDATE_MS) {
            display_ui_fill_demo(&demo, now);
            display_ui_update(&demo);
            last_demo_ms = now;
        }

        lv_timer_handler();
        lv_delay_ms(5);
    }

    return 0;
}
