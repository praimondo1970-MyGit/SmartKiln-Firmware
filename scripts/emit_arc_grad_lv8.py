#!/usr/bin/env python3
"""Generate the horseshoe arc gradient image for LVGL 8 (RGB565 + alpha)."""

from __future__ import annotations

import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT_C = ROOT / "src" / "arc_grad_img.c"
OUT_H = ROOT / "include" / "arc_grad_img.h"

ARC_SIZE = 222
ARC_STROKE_W = 12
ARC_SIDE_EXTEND_DEG = 5
ARC_SWEEP_DEG = 220 + 2 * ARC_SIDE_EXTEND_DEG
ARC_START_ANGLE = 90 + (360 - ARC_SWEEP_DEG) // 2

STOPS = [
    (0.00, 0xFF8A50),
    (0.18, 0xFF7043),
    (0.42, 0xFF5722),
    (0.68, 0xF4511E),
    (0.88, 0xE53935),
    (1.00, 0xC62828),
]


def lerp_rgb(c0: int, c1: int, t: float) -> int:
    t = max(0.0, min(1.0, t))
    r0, g0, b0 = (c0 >> 16) & 0xFF, (c0 >> 8) & 0xFF, c0 & 0xFF
    r1, g1, b1 = (c1 >> 16) & 0xFF, (c1 >> 8) & 0xFF, c1 & 0xFF
    r = int(r0 + (r1 - r0) * t)
    g = int(g0 + (g1 - g0) * t)
    b = int(b0 + (b1 - b0) * t)
    return (r << 16) | (g << 8) | b


def color_at(t: float) -> int:
    if t <= STOPS[0][0]:
        return STOPS[0][1]
    for i in range(1, len(STOPS)):
        pos, color = STOPS[i]
        if t <= pos:
            prev_pos, prev_color = STOPS[i - 1]
            span = pos - prev_pos
            local = (t - prev_pos) / span if span > 0 else 0.0
            return lerp_rgb(prev_color, color, local)
    return STOPS[-1][1]


def rgb565(r: int, g: int, b: int) -> int:
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def build_map() -> list[int]:
    cx = cy = ARC_SIZE / 2.0
    rout = ARC_SIZE / 2.0
    rin = rout - ARC_STROKE_W
    cap_pad = 10.0
    out: list[int] = []

    for y in range(ARC_SIZE):
        for x in range(ARC_SIZE):
            dx = x - cx
            dy = y - cy
            r = math.hypot(dx, dy)
            if r < rin or r > rout:
                out.extend([0x00, 0x00, 0x00])
                continue

            ang = math.degrees(math.atan2(dy, dx))
            if ang < 0:
                ang += 360.0

            rel = ang - ARC_START_ANGLE
            if rel < 0:
                rel += 360.0

            if rel >= 360.0 - cap_pad:
                t = 0.0
            elif rel <= ARC_SWEEP_DEG + cap_pad:
                t = rel / ARC_SWEEP_DEG
                t = max(0.0, min(1.0, t))
            else:
                out.extend([0x00, 0x00, 0x00])
                continue

            rgb = color_at(t)
            rr = (rgb >> 16) & 0xFF
            gg = (rgb >> 8) & 0xFF
            bb = rgb & 0xFF
            color = rgb565(rr, gg, bb)
            out.extend([color & 0xFF, (color >> 8) & 0xFF, 0xFF])

    return out


def emit() -> None:
    map_bytes = build_map()
    lines = [
        '#include "lvgl.h"',
        "",
        "#ifndef LV_ATTRIBUTE_MEM_ALIGN",
        "#define LV_ATTRIBUTE_MEM_ALIGN",
        "#endif",
        "",
        "const LV_ATTRIBUTE_MEM_ALIGN uint8_t arc_grad_img_map[] = {",
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
            "const lv_img_dsc_t arc_grad_img = {",
            "    .header.always_zero = 0,",
            f"    .header.w = {ARC_SIZE},",
            f"    .header.h = {ARC_SIZE},",
            f"    .data_size = {len(map_bytes)},",
            "    .header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA,",
            "    .data = arc_grad_img_map,",
            "};",
            "",
        ]
    )

    OUT_C.parent.mkdir(parents=True, exist_ok=True)
    OUT_C.write_text("\n".join(lines), encoding="utf-8")

    OUT_H.write_text(
        "\n".join(
            [
                "#ifndef ARC_GRAD_IMG_H",
                "#define ARC_GRAD_IMG_H",
                "",
                '#include "lvgl.h"',
                "",
                "LV_IMG_DECLARE(arc_grad_img);",
                "",
                "#endif",
                "",
            ]
        ),
        encoding="utf-8",
    )

    print(f"Wrote {OUT_C} ({len(map_bytes)} bytes)")


if __name__ == "__main__":
    emit()
