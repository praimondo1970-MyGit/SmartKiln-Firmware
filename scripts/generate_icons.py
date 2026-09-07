#!/usr/bin/env python3
"""Download Material Symbols Rounded (filled) SVGs and emit LVGL 8 C sources."""

from __future__ import annotations

import io
import shutil
import subprocess
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets" / "icons"
OUT = ROOT / "src" / "icons"

BASE_URL = (
    "https://raw.githubusercontent.com/google/material-design-icons/master/"
    "symbols/web/{name}/materialsymbolsrounded/{name}_fill1_24px.svg"
)

LOCAL_ICONS = {"flame-fire-svgrepo-com"}

# (asset_name, lvgl symbol name, pixel size)
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


def emit_lvgl_c(img: "Image.Image", symbol: str, out_path: Path) -> None:
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

    lines.extend(
        [
            "};",
            "",
            f"const lv_img_dsc_t {symbol} = {{",
            "    .header.always_zero = 0,",
            "    .header.w = %d," % w,
            "    .header.h = %d," % h,
            "    .data_size = %d," % len(map_bytes),
            "    .header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA,",
            f"    .data = {symbol}_map,",
            "};",
            "",
        ]
    )

    out_path.write_text("\n".join(lines), encoding="utf-8")


def write_header(symbols: list[str]) -> None:
    header = ROOT / "include" / "display_icons.h"
    body = [
        "#ifndef DISPLAY_ICONS_H",
        "#define DISPLAY_ICONS_H",
        "",
        "#include <lvgl.h>",
        "",
        "/* Material Symbols Rounded (filled), white + alpha for img_recolor. */",
    ]
    for sym in symbols:
        body.append(f"LV_IMG_DECLARE({sym});")
    body.extend(["", "#endif", ""])
    header.write_text("\n".join(body), encoding="utf-8")


def main() -> None:
    ensure_deps()
    ASSETS.mkdir(parents=True, exist_ok=True)
    OUT.mkdir(parents=True, exist_ok=True)

    symbols: list[str] = []
    for asset_name, symbol, size in ICONS:
        print(f"Generating {symbol} ({size}px)...")
        svg_path = ASSETS / f"{asset_name}.svg"
        if asset_name in LOCAL_ICONS:
            if not svg_path.exists():
                raise FileNotFoundError(f"Missing local icon: {svg_path}")
        else:
            svg_path.write_bytes(download_svg(asset_name))
        png_path = ASSETS / f"{symbol}.png"
        img = svg_to_white_png(svg_path, png_path, size)
        emit_lvgl_c(img, symbol, OUT / f"{symbol}.c")
        symbols.append(symbol)

    write_header(symbols)
    print(f"Done. {len(symbols)} icons -> {OUT}")


if __name__ == "__main__":
    main()
