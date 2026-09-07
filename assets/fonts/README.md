# Fuentes Inter para LVGL (SmartKiln display)

Tipografía del mockup: **Inter** (SIL Open Font License).

## Modo elegido: bitmap bpp 4 (lv_font_conv)

Mejor equilibrio en ESP32-S3: misma nitidez que FreeType en tamaños fijos,
sin RAM extra ni render lento en cada frame.

## Regenerar fuentes

Requisitos: Node.js, `npx lv_font_conv`.

```powershell
cd "IA Kiln"
$reg = "assets/fonts/Inter-Regular.ttf"
$sb  = "assets/fonts/Inter-SemiBold.ttf"
$sym = "0123456789 ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz.:+-/%°CminhHRóáéíúñÑ…!?,|"
$out = "src/fonts"

npx lv_font_conv --font $reg --size 10 --bpp 4 --format lvgl --lv-include lvgl.h --no-compress --symbols $sym -o "$out/inter_r10.c" --lv-font-name inter_r10
npx lv_font_conv --font $reg --size 12 --bpp 4 --format lvgl --lv-include lvgl.h --no-compress --symbols $sym -o "$out/inter_r12.c" --lv-font-name inter_r12
npx lv_font_conv --font $reg --size 14 --bpp 4 --format lvgl --lv-include lvgl.h --no-compress --symbols $sym -o "$out/inter_r14.c" --lv-font-name inter_r14
npx lv_font_conv --font $sb  --size 14 --bpp 4 --format lvgl --lv-include lvgl.h --no-compress --symbols $sym -o "$out/inter_sb14.c" --lv-font-name inter_sb14
npx lv_font_conv --font $reg --size 16 --bpp 4 --format lvgl --lv-include lvgl.h --no-compress --symbols $sym -o "$out/inter_r16.c" --lv-font-name inter_r16
npx lv_font_conv --font $sb  --size 18 --bpp 4 --format lvgl --lv-include lvgl.h --no-compress --symbols $sym -o "$out/inter_sb18.c" --lv-font-name inter_sb18
npx lv_font_conv --font $reg --size 20 --bpp 4 --format lvgl --lv-include lvgl.h --no-compress --symbols $sym -o "$out/inter_r20.c" --lv-font-name inter_r20
npx lv_font_conv --font $sb  --size 44 --bpp 4 --format lvgl --lv-include lvgl.h --no-compress --symbols $sym -o "$out/inter_sb44.c" --lv-font-name inter_sb44
```

TTF de origen: [Fontsource Inter](https://fontsource.org/fonts/inter) (latin 400 / 600).

Montserrat 14 se mantiene solo para iconos `LV_SYMBOL_*` (WiFi, check).
