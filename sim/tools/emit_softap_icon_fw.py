#!/usr/bin/env python3
"""Generate firmware LVGL8 SoftAP icon (RGB565+A8 interleaved) from Material wifi_tethering."""

from __future__ import annotations

import io
import shutil
import subprocess
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT_C = ROOT / "src" / "icons" / "icon_softap.c"
ASSETS = ROOT / "assets" / "icons"
SIZE = 18
SYMBOL = "icon_softap"
SVG_NAME = "wifi_tethering"

BASE_URL = (
    "https://raw.githubusercontent.com/google/material-design-icons/master/"
    f"symbols/web/{SVG_NAME}/materialsymbolsrounded/{SVG_NAME}_fill1_24px.svg"
)


def ensure_pillow() -> None:
    try:
        from PIL import Image  # noqa: F401
    except ImportError:
        subprocess.check_call([sys.executable, "-m", "pip", "install", "pillow", "-q"])


def svg_to_white_rgba(svg_bytes: bytes, size: int):
    from PIL import Image

    ASSETS.mkdir(parents=True, exist_ok=True)
    svg_path = ASSETS / f"{SVG_NAME}.svg"
    png_path = ASSETS / f"{SYMBOL}_{size}.png"
    svg_path.write_bytes(svg_bytes)

    npx = shutil.which("npx") or shutil.which("npx.cmd")
    if npx:
        subprocess.check_call(
            [
                npx,
                "--yes",
                "@resvg/resvg-js-cli",
                "--fit-width",
                str(size),
                "--fit-height",
                str(size),
                str(svg_path),
                str(png_path),
            ],
            cwd=ROOT,
        )
        img = Image.open(png_path).convert("RGBA")
    else:
        try:
            import cairosvg
        except ImportError:
            subprocess.check_call(
                [sys.executable, "-m", "pip", "install", "cairosvg", "-q"]
            )
            import cairosvg

        png = cairosvg.svg2png(
            bytestring=svg_bytes, output_width=size, output_height=size
        )
        img = Image.open(io.BytesIO(png)).convert("RGBA")
        img.save(png_path)

    pixels = img.load()
    for y in range(img.height):
        for x in range(img.width):
            _, _, _, a = pixels[x, y]
            if a:
                pixels[x, y] = (255, 255, 255, a)
            else:
                pixels[x, y] = (0, 0, 0, 0)
    img.save(png_path)
    return img


def rgb565(r: int, g: int, b: int) -> int:
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def emit_lvgl8_c(img, symbol: str, out_path: Path) -> None:
    w, h = img.size
    pixels = img.load()
    map_bytes: list[int] = []
    for y in range(h):
        for x in range(w):
            r, g, b, a = pixels[x, y]
            if a == 0:
                map_bytes.extend([0x00, 0x00, 0x00])
            else:
                color = rgb565(r, g, b)
                map_bytes.extend([color & 0xFF, (color >> 8) & 0xFF, a])

    lines = [
        '#include "lvgl.h"',
        "",
        "#ifndef LV_ATTRIBUTE_MEM_ALIGN",
        "#define LV_ATTRIBUTE_MEM_ALIGN",
        "#endif",
        "",
        f"const LV_ATTRIBUTE_MEM_ALIGN uint8_t {symbol}_map[] = {{",
    ]
    row = "    "
    for i, b in enumerate(map_bytes):
        row += f"0x{b:02x}, "
        if (i + 1) % 12 == 0:
            lines.append(row.rstrip())
            row = "    "
    if row.strip():
        lines.append(row.rstrip())
    lines.append("};")
    lines.append("")
    lines.extend(
        [
            f"const lv_img_dsc_t {symbol} = {{",
            "    .header.always_zero = 0,",
            f"    .header.w = {w},",
            f"    .header.h = {h},",
            f"    .data_size = {len(map_bytes)},",
            "    .header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA,",
            f"    .data = {symbol}_map,",
            "};",
            "",
        ]
    )
    out_path.write_text("\n".join(lines), encoding="utf-8")
    print(f"Wrote {out_path} ({w}x{h}, {len(map_bytes)} bytes)")


def main() -> int:
    ensure_pillow()
    print(f"Downloading {SVG_NAME}...")
    svg = urllib.request.urlopen(BASE_URL, timeout=30).read()
    img = svg_to_white_rgba(svg, SIZE)
    emit_lvgl8_c(img, SYMBOL, OUT_C)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
