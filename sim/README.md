# SmartKiln UI Simulator (LVGL 9 + SDL2)

Emulador de escritorio para iterar el dashboard **480×272** sin flashear el ESP32.
El firmware en la placa sigue usando **LVGL 8**; este entorno es solo para diseño.

## Requisitos

1. **CMake** 3.20+
2. **Compilador C** (Visual Studio 2022 con “Desktop development with C++”, o MinGW)
3. **Python 3** (ya lo tenés)
4. **Node/npx** (para generar iconos con resvg; opcional si ya existen en `sim/generated/`)

La primera configuración de CMake descarga **LVGL 9.2.2** y **SDL2** automáticamente (necesita internet).

## Pasos rápidos (PowerShell)

```powershell
cd "C:\Users\praim\OneDrive\Documentos\PlatformIO\Projects\IA Kiln\sim"

# 1) Iconos en formato LVGL 9 (solo la primera vez o si cambian assets)
python tools/emit_icons_lv9.py

# 2) Configurar y compilar
.\build.ps1

# 3) Ejecutar
.\build\smartkiln_sim.exe
```

## Qué hace el simulador

- Ventana SDL **480×272** (misma resolución que el panel)
- Splash ~1.5 s, luego dashboard V2 Final A
- Datos de demo que rotan cada 12 s: horneando, pausado, interrumpido, finalizado, detenido
- Temperatura animada en modo horneado

## Archivos

| Archivo | Rol |
|---------|-----|
| `main.c` | SDL + loop LVGL |
| `display_ui.c` | UI portada a API LVGL 9 |
| `lv_conf.h` | Config simulador |
| `tools/emit_icons_lv9.py` | Iconos Material → C (LVGL 9) |
| `../src/fonts/*.c` | Fuentes Inter (compartidas con firmware) |

## Sincronizar cambios con el ESP32

El simulador y el firmware son **código paralelo** por ahora:

- Ajustás layout/estilos en `sim/display_ui.c` (LVGL 9)
- Replicás el mismo cambio en `src/display_lvgl.cpp` (LVGL 8)

Más adelante se puede extraer un módulo común si conviene.

## Troubleshooting

- **cmake no reconocido**: instalá con `winget install Kitware.CMake` y reiniciá la terminal.
- **Sin compilador**: instalá Visual Studio Build Tools con workload C++.
- **Error SDL**: borrá `sim/build` y volvé a correr `build.ps1`.
- **Iconos faltantes**: ejecutá `python tools/emit_icons_lv9.py`.
