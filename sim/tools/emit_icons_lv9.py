#!/usr/bin/env python3
"""Emit LVGL 9 image sources for the PC simulator (RGB565 + alpha)."""

from __future__ import annotations

import io
import shutil
import subprocess
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SIM = ROOT / "sim"
ASSETS = ROOT / "assets" / "icons"
OUT = SIM / "generated" / "icons"

BASE_URL = (
    "https://raw.githubusercontent.com/google/material-design-icons/master/"
    "symbols/web/{name}/materialsymbolsrounded/{name}_fill1_24px.svg"
)

LOCAL_ICONS = {"flame-fire-svgrepo-com"}

ICONS = [
    ("wifi", "icon_wifi", 18),
    ("cloud", "icon_cloud", 18),
    ("flame-fire-svgrepo-com", "icon_flame", 16),
    ("my_location", "icon_target", 16),
    ("stairs", "icon_stairs", 16),
    ("schedule", "icon_clock", 16),
]


def ensure_deps() -> None:
    try:
        from PIL import Image  # noqa: F401
    except ImportError:
        subprocess.check_call([sys.executable, "-m", "pip", "install", "pillow", "-q"])


def download_svg(name: str) -> bytes:
    url = BASE_URL.format(name=name)
    with urllib.request.urlopen(url, timeout=30) as resp:
        return resp.read()


def svg_to_white_png(svg_path: Path, png_path: Path, size: int) -> "Image.Image":
    from PIL import Image

    npx = shutil.which("npx") or shutil.which("npx.cmd")
    if not npx:
        raise RuntimeError("npx not found in PATH")

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
    pixels = img.load()
    for y in range(img.height):
        for x in range(img.width):
            _, _, _, alpha = pixels[x, y]
            if alpha:
                pixels[x, y] = (255, 255, 255, alpha)
    img.save(png_path)
    return img


def rgb565(r: int, g: int, b: int) -> int:
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def emit_lvgl9_c(img: "Image.Image", symbol: str, out_path: Path) -> None:
    w, h = img.size
    pixels = img.load()
    rgb_bytes: list[int] = []
    alpha_bytes: list[int] = []

    for y in range(h):
        for x in range(w):
            r, g, b, a = pixels[x, y]
            if a == 0:
                rgb_bytes.extend([0x00, 0x00])
            else:
                color = rgb565(r, g, b)
                rgb_bytes.extend([color & 0xFF, (color >> 8) & 0xFF])
            alpha_bytes.append(a)

    # LVGL 9: RGB565 plane followed by A8 plane (not interleaved).
    map_bytes = rgb_bytes + alpha_bytes

    lines = [
        "#include \"lvgl.h\"",
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
        lines.append(row.rstrip().rstrip(","))

    stride = w * 2
    lines.extend(
        [
            "};",
            "",
            f"const lv_image_dsc_t {symbol} = {{",
            "    .header = {",
            "        .magic = LV_IMAGE_HEADER_MAGIC,",
            "        .cf = LV_COLOR_FORMAT_RGB565A8,",
            "        .flags = 0,",
            f"        .w = {w},",
            f"        .h = {h},",
            f"        .stride = {stride},",
            "    },",
            f"    .data_size = {len(map_bytes)},",
            f"    .data = {symbol}_map,",
            "};",
            "",
        ]
    )

    out_path.write_text("\n".join(lines), encoding="utf-8")


def write_header(symbols: list[str]) -> None:
    header = SIM / "include" / "display_icons.h"
    header.parent.mkdir(parents=True, exist_ok=True)
    body = [
        "#ifndef DISPLAY_ICONS_H",
        "#define DISPLAY_ICONS_H",
        "",
        "#include <lvgl.h>",
        "",
        "/* Material Symbols Rounded (filled), white + alpha for image_recolor. */",
    ]
    for sym in symbols:
        body.append(f"LV_IMAGE_DECLARE({sym});")
    body.extend(["", "#endif", ""])
    header.write_text("\n".join(body), encoding="utf-8")


def main() -> None:
    ensure_deps()
    ASSETS.mkdir(parents=True, exist_ok=True)
    OUT.mkdir(parents=True, exist_ok=True)

    symbols: list[str] = []
    for asset_name, symbol, size in ICONS:
        print(f"Generating LVGL9 {symbol} ({size}px)...")
        svg_path = ASSETS / f"{asset_name}.svg"
        if not svg_path.exists():
            if asset_name in LOCAL_ICONS:
                raise FileNotFoundError(f"Missing local icon: {svg_path}")
            svg_path.write_bytes(download_svg(asset_name))
        png_path = ASSETS / f"{symbol}.png"
        img = svg_to_white_png(svg_path, png_path, size)
        emit_lvgl9_c(img, symbol, OUT / f"{symbol}.c")
        symbols.append(symbol)

    write_header(symbols)
    print(f"Done. {len(symbols)} icons -> {OUT}")


if __name__ == "__main__":
    main()
