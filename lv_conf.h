/**
 * @file lv_conf.h
 * Configuration file for LVGL v8.3.x
 * Optimized for ESP32-S3 with 8MB PSRAM and 800x480 display
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/

/*Color depth: 1 (1 byte per pixel), 8 (RGB332), 16 (RGB565), 32 (ARGB8888)*/
#define LV_COLOR_DEPTH     16

/*Swap the 2 bytes of RGB565 color. Useful if the display has a 8 bit interface (e.g. SPI)*/
#define LV_COLOR_16_SWAP   0

/*Enable more complex drawing routines to manage screens transparency.
 *Can be used if the UI is above another layer, e.g. an OSD menu or video player.*/
#define LV_COLOR_SCREEN_TRANSP    0

/* Adjust color mix functions rounding. GPUs might calculate color mix (blending) differently.
 * 0: Round down, 64: Round up, 128: Round nearest*/
#define LV_COLOR_MIX_ROUND_OFS    128

/*====================
   MEMORY SETTINGS
 *====================*/

/*1: use custom malloc/free, 0: use the built-in `lv_mem_alloc()` and `lv_mem_free()`*/
#define LV_MEM_CUSTOM      0
#if LV_MEM_CUSTOM == 0
    /*Size of the memory available for `lv_mem_alloc()` in bytes (>= 2kB)*/
    #define LV_MEM_SIZE    (96U * 1024U)          /*96 KB - Reducido de 128KB para liberar memoria*/

    /*Set an address for the memory pool instead of allocating it as a normal array. Can be in external SRAM too.*/
    #define LV_MEM_ADR      0     /*0: unused*/
    /*Instead of an address give a memory allocator that will be called to get a memory pool for LVGL. E.g. my_malloc*/
    #if LV_MEM_ADR == 0
        #undef LV_MEM_POOL_INCLUDE
        #undef LV_MEM_POOL_ALLOC
    #endif

#else       /*LV_MEM_CUSTOM*/
    #define LV_MEM_CUSTOM_INCLUDE <stdlib.h>   /*Header for the dynamic memory function*/
    #define LV_MEM_CUSTOM_ALLOC   malloc
    #define LV_MEM_CUSTOM_FREE    free
    #define LV_MEM_CUSTOM_REALLOC realloc
#endif     /*LV_MEM_CUSTOM*/

/*Number of the intermediate memory buffer used during rendering and other internal processing.
 *You will see an error log message if there wasn't enough buffers. */
#define LV_MEM_BUF_MAX_NUM     8  /* Reducido de 16 a 8 para liberar memoria */

/*Use the standard `memcpy` and `memset` instead of LVGL's own functions. (Might or might not be faster).*/
#define LV_MEMCPY_MEMSET_STD    0

/*====================
   HAL SETTINGS
 *====================*/

/*Default display refresh period. LVG will redraw changed areas with this period time*/
#define LV_DISP_DEF_REFR_PERIOD     33      /*[ms]*/

/*Input device read period in milliseconds*/
#define LV_INDEV_DEF_READ_PERIOD    30      /*[ms]*/

/*Use a custom tick source that tells the elapsed time in milliseconds.
 *It removes the need to manually update the tick with `lv_tick_inc()`)*/
#define LV_TICK_CUSTOM     0
#if LV_TICK_CUSTOM
    #define LV_TICK_CUSTOM_INCLUDE  "Arduino.h"         /*Header for the system time function*/
    #define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())     /*Expression evaluating to current system time in ms*/
#endif   /*LV_TICK_CUSTOM*/

/*Default Dot Per Inch. Used to initialize default sizes such as widgets sized, style paddings.
 *(Not so important, you can adjust it to modify default sizes and spaces)*/
#define LV_DPI_DEF        130     /*[px/inch]*/

/*====================
 * FEATURE CONFIGURATION
 *====================*/

/*-------------
 * Drawing
 *-----------*/

/*Enable complex draw engine.
 *Required to draw shadow, gradient, rounded corners, circles, arc, skew, image transformations or any masks*/
#define LV_DRAW_COMPLEX 1
#if LV_DRAW_COMPLEX != 0
    /*Allow buffering some shadow calculation.
    *LV_SHADOW_CACHE_SIZE is the max. shadow size to buffer, where shadow size is `shadow_width + radius`
    *Caching has LV_SHADOW_CACHE_SIZE^2 RAM cost*/
    #define LV_SHADOW_CACHE_SIZE    0

    /* Set number of maximally cached circle data.
    * The circumference of 1/4 circle are saved for anti-aliasing
    * radius * 4 bytes are used per circle */
    #define LV_CIRCLE_CACHE_SIZE    2  /* Reducido de 4 a 2 para liberar memoria */
#endif /*LV_DRAW_COMPLEX*/

/*Default image cache size. Image caching keeps the images opened.
 *If only the built-in image formats are used there is no real advantage of caching. (I.e. if no new image decoder is added)
 *With complex image decoders (e.g. PNG or JPG) caching can save the continuous open/decode of images.
 *However the opened images might consume additional RAM.
 *0: to disable caching*/
#define LV_IMG_CACHE_DEF_SIZE       0

/*Number of stops allowed per gradient. Increase this to allow more stops.
 *This adds (sizeof(lv_color_t) + 1) bytes per additional stop*/
#define LV_GRADIENT_MAX_STOPS       2

/* Adjust color mix functions rounding. GPUs might calculate color mix (blending) differently.
 * 0: Round down, 64: Round up, 128: Round nearest*/
#define LV_GRADIENT_MIX_ROUND_OFS   128

/*-------------
 * GPU
 *-----------*/

/*Enable Arm2D GPU acceleration*/
#define LV_USE_DRAW_ARM2D           0

/*-------------
 * Logging
 *-----------*/

/*Enable the log module*/
#define LV_USE_LOG      1
#if LV_USE_LOG

    /*How important log should be added:
    *LV_LOG_LEVEL_TRACE       A lot of logs to give detailed information
    *LV_LOG_LEVEL_INFO        Log important events
    *LV_LOG_LEVEL_WARN        Log if something unwanted happened but didn't cause a problem
    *LV_LOG_LEVEL_ERROR       Only critical issue, when the system may fail
    *LV_LOG_LEVEL_USER        Only logs added by the user
    *LV_LOG_LEVEL_NONE        Do not log anything*/
    #define LV_LOG_LEVEL    LV_LOG_LEVEL_WARN

    /*1: Print the log with 'printf';
    *0: User need to register a callback with `lv_log_register_print_cb()`*/
    #define LV_LOG_PRINTF   1

    /*Enable/disable LV_LOG_TRACE in modules that produces a huge number of logs*/
    #define LV_LOG_TRACE_MEM        0  /* Deshabilitado para reducir uso de memoria */
    #define LV_LOG_TRACE_TIMER      0  /* Deshabilitado para reducir uso de memoria */
    #define LV_LOG_TRACE_INDEV      0  /* Deshabilitado para reducir uso de memoria */
    #define LV_LOG_TRACE_DISP_REFR  0  /* Deshabilitado para reducir uso de memoria */
    #define LV_LOG_TRACE_EVENT      0  /* Deshabilitado para reducir uso de memoria */
    #define LV_LOG_TRACE_OBJ_CREATE 0  /* Deshabilitado para reducir uso de memoria */
    #define LV_LOG_TRACE_LAYOUT     0  /* Deshabilitado para reducir uso de memoria */
    #define LV_LOG_TRACE_ANIM       0  /* Deshabilitado para reducir uso de memoria */

#endif  /*LV_USE_LOG*/

/*-------------
 * Asserts
 *-----------*/

/*Enable asserts if an operation is failed or an invalid data is found.
 *If LV_USE_LOG is enabled an error message will be printed on failure*/
#define LV_USE_ASSERT_NULL          1   /*Check if the parameter is NULL. (Very fast, recommended)*/
#define LV_USE_ASSERT_MALLOC        1   /*Checks is the memory is successfully allocated or no. (Very fast, recommended)*/
#define LV_USE_ASSERT_STYLE         0   /*Check if the styles are properly initialized. (Very fast, recommended)*/
#define LV_USE_ASSERT_MEM_INTEGRITY 0   /*Check the integrity of `lv_mem` after critical operations. (Slow)*/
#define LV_USE_ASSERT_OBJ           0   /*Check the object's type and existence (e.g. not deleted). (Slow)*/

/*Add a custom handler when assert happens e.g. to restart the MCU*/
#define LV_ASSERT_HANDLER_INCLUDE   <stdint.h>
#define LV_ASSERT_HANDLER   while(1);   /*Halt by default*/

/*-------------
 * Others
 *-----------*/

/*1: Enable CPU time and cycles monitoring*/
#define LV_USE_PERF_MONITOR     0
#if LV_USE_PERF_MONITOR
    #define LV_USE_PERF_MONITOR_POS    LV_ALIGN_BOTTOM_RIGHT
#endif

/*1: Enable the runtime performance profiler*/
#define LV_USE_PROFILER     0
#if LV_USE_PROFILER
    /*1: Enable the built-in profiler*/
    #define LV_USE_PROFILER_BUILTIN   1
    #if LV_USE_PROFILER_BUILTIN
        /*Default profiler trace buffer size*/
        #define LV_USE_PROFILER_BUILTIN_BUFFER_SIZE    (16 * 1024)     /*[bytes]*/
    #endif

    /*Header to include for the profiler*/
    #define LV_USE_PROFILER_INCLUDE "lvgl/src/misc/lv_profiler_builtin.h"
#endif

/*1: Enable the memory monitor*/
#define LV_USE_MEM_MONITOR      0
#if LV_USE_MEM_MONITOR
    #define LV_USE_MEM_MONITOR_POS      LV_ALIGN_BOTTOM_LEFT
#endif

/*1: Enable system monitor component*/
#define LV_USE_SYSMON       0

/*1: Enable the built-in memory snapshot feature*/
#define LV_USE_BUILTIN_MALLOC    0

/*1: Enable the garbage collector for lv_obj_class*/
#define LV_USE_OBJ_BUILTIN_GC    0

/*1: Enable line snapshot feature*/
#define LV_USE_LINE_SNAPSHOT     0

/*1: Enable the observer pattern*/
#define LV_USE_OBSERVER          0

/*1: Enable the file system interfaces for images and fonts*/
#define LV_USE_FS_STDIO      0
#if LV_USE_FS_STDIO
    #define LV_FS_STDIO_LETTER '\0'     /*Set an upper cased letter on which the drive will accessible (e.g. 'A')*/
    #define LV_FS_STDIO_PATH ""         /*Set the working directory. File/directory paths will be appended to it.*/
    #define LV_FS_STDIO_CACHE_SIZE  0   /*>0 to cache this number of bytes in lv_fs_read()*/
#endif

/*1: Enable PNG decoder (libpng library)*/
#define LV_USE_PNG      0

/*1: Enable BMP decoder (bmp files)*/
#define LV_USE_BMP      0

/*1: Enable JPG decoder (sjpg library). */
#define LV_USE_SJPG     0

/*1: Enable Split JPG decoder.
 *Split JPG is a custom format optimized for embedded systems. */
#define LV_USE_SJPG     0

/*====================
 *  WIDGETS
 *====================*/

/*1: Enable the Animations */
#define LV_USE_ANIMATION        0  /* Deshabilitado - no se usa en el código */
#if LV_USE_ANIMATION
    #define LV_USE_PATH     1
    #define LV_USE_PATH_MAX_BEZIER_ORDER  2
#endif

/*1: Enable object grouping (for keyboard/encoder navigation) */
#define LV_USE_GROUP           0
#if LV_USE_GROUP
    #define LV_USE_GROUP_ITER     0
#endif

/*1: Enable GPU to accelerate widget opacity rendering */
#define LV_USE_OPA_SCALE       0

/*1: Enable widget layout using flexbox */
#define LV_USE_FLEX            0  /* Deshabilitado - no se usa en el código */

/*1: Enable widget layout using grid */
#define LV_USE_GRID            0  /* Deshabilitado - no se usa en el código */

/*1: Enable using log2 function on fixed points. It slightly improves calculation precision but reduces performance */
#define LV_USE_LOG2            0

/*1: Enable shadow drawing on widgets*/
#define LV_USE_SHADOW          0  /* Deshabilitado - no se usa en el código */
#if LV_USE_SHADOW && LV_DRAW_COMPLEX
    /*Allow buffering shadow for larger radius.
     *LV_SHADOW_CACHE_SIZE defines the max. shadow size to buffer, where shadow size is `shadow_width + radius`
     *Caching has LV_SHADOW_CACHE_SIZE^2 RAM cost*/
    #define LV_SHADOW_CACHE_SIZE        0
#endif

/*1: Enable object groups (for keyboard/encoder navigation) */
#define LV_USE_OBJ_ID          0

/*1: Enable to make the screen to support touchpad*/
#define LV_USE_OBJ_CLASSIC      1

/*1: Enable widget boundaries in debug mode*/
#define LV_USE_OBJ_BOUNDARY     0

/*1: Enable the arc widget*/
#define LV_USE_ARC              1

/*1: Enable the bar widget*/
#define LV_USE_BAR              0

/*1: Enable the button widget*/
#define LV_USE_BTN              0

/*1: Enable the button matrix widget*/
#define LV_USE_BTNMATRIX        0

/*1: Enable the calendar widget*/
#define LV_USE_CALENDAR         0

/*1: Enable the canvas widget*/
#define LV_USE_CANVAS           0

/*1: Enable the chart widget*/
#define LV_USE_CHART            0
#if LV_USE_CHART
    #define LV_CHART_DEF_POINT_COUNT    100
    #define LV_CHART_DEF_WIDTH          LV_DPI_DEF * 2
    #define LV_CHART_DEF_HEIGHT         LV_DPI_DEF
#endif

/*1: Enable the color picker widgets*/
#define LV_USE_COLORWHEEL       0

/*1: Enable the color picker widget*/
#define LV_USE_COLORPICKER      0

/*1: Enable the dialog widget*/
#define LV_USE_DIALOG           0

/*1: Enable the image widget*/
#define LV_USE_IMG              1

/*1: Enable the keyboard widget*/
#define LV_USE_KEYBOARD         0

/*1: Enable the label widget*/
#define LV_USE_LABEL            1
#if LV_USE_LABEL
    #define LV_LABEL_TEXT_SELECTION     0   /*Disable - no se necesita selección de texto*/
    #define LV_LABEL_LONG_TXT_HINT      0   /*Disable - textos cortos, no necesita optimización*/
#endif

/*1: Enable the LED widget*/
#define LV_USE_LED              0

/*1: Enable the line widget*/
#define LV_USE_LINE             0

/*1: Enable the list widget*/
#define LV_USE_LIST             0

/*1: Enable the meter widget*/
#define LV_USE_METER            0

/*1: Enable the msgbox widget*/
#define LV_USE_MSGBOX           0

/*1: Enable the roller widget*/
#define LV_USE_ROLLER           0

/*1: Enable the slider widget*/
#define LV_USE_SLIDER           0

/*1: Enable the span widget*/
#define LV_USE_SPAN             0
#if LV_USE_SPAN
    /*A line text can contain maximum num of span descriptor */
    #define LV_SPAN_SNIPPET_STACK_SIZE  64
#endif

/*1: Enable the spinbox widget*/
#define LV_USE_SPINBOX          0

/*1: Enable the spinner widget*/
#define LV_USE_SPINNER          0

/*1: Enable the switch widget*/
#define LV_USE_SWITCH           0

/*1: Enable the table widget*/
#define LV_USE_TABLE            0

/*1: Enable the tabview widget*/
#define LV_USE_TABVIEW          0

/*1: Enable the textarea widget*/
#define LV_USE_TEXTAREA         0

/*1: Enable the tileview widget*/
#define LV_USE_TILEVIEW         0

/*1: Enable the win widget*/
#define LV_USE_WIN              0

/*1: Enable the marquee widget*/
#define LV_USE_MARQUEE          0

/*1: Enable the progressbar widget*/
#define LV_USE_PROGRESSBAR      0

/*1: Enable the list dropdown widget*/
#define LV_USE_LIST_DROPDOWN    0

/*==================
 * THEMES
 *==================*/

/*A simple, impressive and very complete theme*/
#define LV_USE_THEME_DEFAULT    1
#if LV_USE_THEME_DEFAULT

    /*0: Light mode; 1: Dark mode*/
    #define LV_THEME_DEFAULT_DARK     1

    /*1: Enable grow on press*/
    #define LV_THEME_DEFAULT_GROW            1

    /*Default transition time in [ms]*/
    #define LV_THEME_DEFAULT_TRANSITION_TIME 80
#endif /*LV_USE_THEME_DEFAULT*/

/*A very simple theme that is a good starting point for a custom theme*/
#define LV_USE_THEME_BASIC      0

/*A theme designed for monochrome displays*/
#define LV_USE_THEME_MONO       0

/*==================
 * LAYOUTS
 *==================*/

/*1: Enable the flex layout*/
#define LV_USE_FLEX     1

/*1: Enable the grid layout*/
#define LV_USE_GRID     1

/*==================
 * 3RD PARTS
 *==================*/

/*File system interfaces for common APIs */

/*API for fopen, fread, etc*/
#define LV_USE_FS_STDIO 0
#if LV_USE_FS_STDIO
    #define LV_FS_STDIO_LETTER '\0'     /*Set an upper cased letter on which the drive will accessible (e.g. 'A')*/
    #define LV_FS_STDIO_PATH ""         /*Set the working directory. File/directory paths will be appended to it.*/
    #define LV_FS_STDIO_CACHE_SIZE 0    /*>0 to cache this number of bytes in lv_fs_read()*/
#endif

/*API for open, read, etc*/
#define LV_USE_FS_POSIX 0
#if LV_USE_FS_POSIX
    #define LV_FS_POSIX_LETTER '\0'     /*Set an upper cased letter on which the drive will accessible (e.g. 'A')*/
    #define LV_FS_POSIX_PATH ""         /*Set the working directory. File/directory paths will be appended to it.*/
    #define LV_FS_POSIX_CACHE_SIZE 0   /*>0 to cache this number of bytes in lv_fs_read()*/
#endif

/*API for CreateFile, ReadFile, etc*/
#define LV_USE_FS_WIN32 0
#if LV_USE_FS_WIN32
    #define LV_FS_WIN32_LETTER '\0'     /*Set an upper cased letter on which the drive will accessible (e.g. 'A')*/
    #define LV_FS_WIN32_PATH ""         /*Set the working directory. File/directory paths will be appended to it.*/
    #define LV_USE_FS_WIN32_CACHE_SIZE 0
#endif

/*API for FATFS (needs to be added separately). Uses f_open, f_read, etc*/
#define LV_USE_FS_FATFS 0
#if LV_USE_FS_FATFS
    #define LV_FS_FATFS_LETTER '\0'     /*Set an upper cased letter on which the drive will accessible (e.g. 'A')*/
    #define LV_FS_FATFS_CACHE_SIZE 0    /*>0 to cache this number of bytes in lv_fs_read()*/
#endif

/*1: Enable FreeType library*/
#define LV_USE_FREETYPE         0
#if LV_USE_FREETYPE
    /*Memory used by FreeType to cache characters [bytes] (-1: no limit)*/
    #define LV_FREETYPE_CACHE_SIZE (16 * 1024)
    #if LV_FREETYPE_DEFAULT_CACHE_SIZE >= 0
        #undef LV_FREETYPE_CACHE_FT_OUTLINE
    #endif
#endif

/*==================
 * OTHERS
 *==================*/

/*1: Enable the garbage collector for lv_obj_class */
#define LV_USE_OBJ_BUILTIN_GC  0

/*1: Enable API to take snapshot for object*/
#define LV_USE_OBJ_SNAPSHOT    0

/*1: Enable system monitor component*/
#define LV_USE_SYSMON          0

/*1: Enable the runtime performance profiler*/
#define LV_USE_PROFILER        0
#if LV_USE_PROFILER
    #define LV_USE_PROFILER_INCLUDE "lvgl/src/misc/lv_profiler_builtin.h"
    #define LV_USE_PROFILER_BUILTIN 1
#endif

/*1: Enable the observer pattern*/
#define LV_USE_OBSERVER         1

/*1: Enable the file system interfaces for images and fonts*/
#define LV_USE_FS_STDIO         0
#if LV_USE_FS_STDIO
    #define LV_FS_STDIO_LETTER 'A'
    #define LV_FS_STDIO_PATH "/"
    #define LV_FS_STDIO_CACHE_SIZE 0
#endif

/*1: Enable Png library*/
#define LV_USE_PNG      0

/*1: Enable Bmp library*/
#define LV_USE_BMP      0

/*1: Enable JPG + split JPG functionality.
 *Split JPG is used for devices with low memory. Instead of loading the whole image,
 *only a part of it is loaded at a time, which saves RAM. */
#define LV_USE_TJPGD        0

/*1: Enable Vector Graphic library for custom drawing (SVG)*/
#define LV_USE_VECTOR_GRAPHIC    0

/*==================
* EXAMPLES
*==================*/

/*Enable the examples to be built with the library*/
#define LV_BUILD_EXAMPLES    0

/*===================
 * DEMO USAGE
 ====================*/

/*Show some widget. It might be required to increase `LV_MEM_SIZE` */
#define LV_USE_DEMO_WIDGETS        0
#if LV_USE_DEMO_WIDGETS
#define LV_DEMO_WIDGETS_SLIDESHOW  0
#endif

/*Demonstrate the usage of encoder and keyboard*/
#define LV_USE_DEMO_KEYPAD_AND_ENCODER     0

/*Benchmark your system*/
#define LV_USE_DEMO_BENCHMARK   0

/*Stress test for LVGL*/
#define LV_USE_DEMO_STRESS      0

/*Music player demo*/
#define LV_USE_DEMO_MUSIC       0
#if LV_USE_DEMO_MUSIC
    #define LV_DEMO_MUSIC_SQUARE       0
    #define LV_DEMO_MUSIC_LANDSCAPE    0
    #define LV_USE_DEMO_MUSIC_LARGE    0
#endif

/*--END OF LV_CONF_H--*/

#endif /*LV_CONF_H*/

