#!/usr/bin/env python3
"""Montaje fotográfico con recortes de componentes reales."""

from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ASSETS = Path(
    r"C:\Users\praim\.cursor\projects\c-Users-praim-OneDrive-Documentos-PlatformIO-Projects-IA-Kiln\assets"
)
OUT = Path(__file__).with_name("Prueba_MOC_BTA_Montaje.png")
PCB = Path(__file__).resolve().parent.parent / "pcb"


def font(size, bold=False):
    p = r"C:\Windows\Fonts\arialbd.ttf" if bold else r"C:\Windows\Fonts\arial.ttf"
    try:
        return ImageFont.truetype(p, size)
    except OSError:
        return ImageFont.load_default()


def cutout(im: Image.Image, t: int = 238) -> Image.Image:
    arr = np.array(im.convert("RGBA"))
    r, g, b = arr[:, :, 0].astype(np.int16), arr[:, :, 1].astype(np.int16), arr[:, :, 2].astype(np.int16)
    white = (r >= t) & (g >= t) & (b >= t) & (np.abs(r - g) < 22) & (np.abs(g - b) < 22)
    arr[:, :, 3] = np.where(white, 0, 255)
    out = Image.fromarray(arr)
    bbox = out.split()[-1].getbbox()
    if bbox:
        out = out.crop(bbox)
    return out


def load(name: str) -> Image.Image:
    p = ASSETS / name
    if not p.exists():
        raise FileNotFoundError(p)
    return Image.open(p).convert("RGBA")


def fit(im: Image.Image, mw: int, mh: int) -> Image.Image:
    im = im.copy()
    im.thumbnail((mw, mh), Image.Resampling.LANCZOS)
    return im


def shadow(base: Image.Image, im: Image.Image, xy, blur=10, opacity=90):
    x, y = xy
    sh = Image.new("RGBA", im.size, (0, 0, 0, 0))
    alpha = im.split()[-1].point(lambda a: opacity if a > 20 else 0)
    sh.paste((0, 0, 0, opacity), (0, 0), alpha)
    sh = sh.filter(ImageFilter.GaussianBlur(blur))
    base.alpha_composite(sh, (x + 8, y + 10))
    base.alpha_composite(im, (x, y))


def pill(d: ImageDraw.ImageDraw, xy, text, fill, fnt):
    x, y = xy
    pad_x, pad_y = 10, 6
    bb = d.textbbox((0, 0), text, font=fnt)
    w, h = bb[2] - bb[0] + pad_x * 2, bb[3] - bb[1] + pad_y * 2
    d.rounded_rectangle((x, y, x + w, y + h), radius=8, fill=fill)
    d.text((x + pad_x, y + pad_y - 1), text, font=fnt, fill=(255, 255, 255))
    return w, h


def wire(d, pts, color, width=7):
    d.line(pts, fill=color, width=width, joint="curve")
    r = width // 2 + 1
    for p in (pts[0], pts[-1]):
        d.ellipse((p[0] - r, p[1] - r, p[0] + r, p[1] + r), fill=color)


def main():
    W, H = 2400, 1500
    table = fit(load("part_table.png"), W, H)
    canvas = Image.new("RGBA", (W, H), (236, 232, 224, 255))
    canvas.paste(table, ((W - table.width) // 2, 80))
    canvas = canvas.convert("RGBA")

    bread = fit(cutout(load("part_breadboard.png"), 230), 980, 720)
    perf = fit(cutout(load("part_perfboard.png"), 230), 980, 620)
    moc = fit(cutout(load("part_moc3020.png"), 236), 420, 260)
    triac = fit(cutout(load("part_bt138.png"), 236), 260, 340)
    res = fit(cutout(load("part_resistors.png"), 236), 520, 220)
    mov = fit(cutout(load("part_mov.png"), 236), 160, 180)
    lamp = fit(cutout(load("part_lamp.png"), 236), 220, 280)
    fuse = fit(cutout(load("part_fuse_term.png"), 236), 340, 200)
    snub = fit(cutout(load("part_snubber_hs.png"), 236), 280, 180)

    bx, by = 70, 210
    px, py = 1180, 240
    shadow(canvas, bread, (bx, by), 14, 70)
    shadow(canvas, perf, (px, py), 14, 70)

    mx, my = 860, 430
    shadow(canvas, moc, (mx, my), 8, 80)
    tx, ty = 1480, 300
    shadow(canvas, triac, (tx, ty), 8, 80)
    shadow(canvas, res, (120, 860), 6, 50)
    shadow(canvas, mov, (1320, 620), 6, 60)
    shadow(canvas, lamp, (2050, 320), 8, 70)
    shadow(canvas, fuse, (1780, 700), 6, 60)
    shadow(canvas, snub, (1680, 520), 6, 50)

    d = ImageDraw.Draw(canvas)
    F18, F22 = font(18), font(22)
    F20B, F28B, F36B = font(20, True), font(28, True), font(36, True)

    d.rectangle((0, 0, W, 92), fill=(27, 54, 93, 255))
    d.text((36, 18), "IA Kiln  ·  Montaje real del prototipo", font=F36B, fill=(255, 255, 255))
    d.text((36, 62), "Fotos de componentes  ·  protoboard 3,3 V  ·  placa 220 VAC  ·  MOC a caballo", font=F18, fill=(213, 222, 236))

    pill(d, (90, 160), "PROTOBOARD  3,3 V", (29, 78, 137), F20B)
    pill(d, (1200, 190), "PLACA PERFORADA  220 VAC", (155, 28, 28), F20B)
    pill(d, (980, 400), "ISO GAP", (138, 109, 59), F20B)

    # pin callouts around MOC (notch left → pin 1 bottom-left of notch in our photo)
    pill(d, (mx - 10, my + 40), "1 ánodo", (29, 78, 137), F18)
    pill(d, (mx - 10, my + 110), "2 cátodo", (29, 78, 137), F18)
    pill(d, (mx - 10, my + 180), "3 NC", (90, 90, 90), F18)
    pill(d, (mx + 250, my + 40), "6 G", (155, 28, 28), F18)
    pill(d, (mx + 250, my + 110), "5 NC", (90, 90, 90), F18)
    pill(d, (mx + 250, my + 180), "4 MT → MT2", (155, 28, 28), F18)

    pill(d, (tx - 80, ty + 280), "MT1", (155, 28, 28), F18)
    pill(d, (tx + 70, ty + 280), "MT2  pestaña", (155, 28, 28), F18)
    pill(d, (tx + 190, ty + 280), "G", (155, 28, 28), F18)
    pill(d, (tx + 40, ty - 8), "BT138 / BTA", (30, 30, 30), F18)

    pill(d, (150, 830), "R1 120 Ω   Rpd 10 kΩ   R2 330 Ω   Rs 100 Ω 2 W", (60, 50, 40), F18)
    pill(d, (1310, 800), "RV1  14D431", (40, 70, 140), F18)
    pill(d, (2050, 280), "lámpara 25–40 W", (155, 28, 28), F18)
    pill(d, (1780, 900), "F1  2 A T   ·   L N A2", (155, 28, 28), F18)

    # wires (approximate, instructional overlay)
    ORANGE, BLUE, RED, BRN, BLK = (214, 90, 18, 230), (40, 90, 180, 230), (190, 35, 35, 230), (140, 80, 40, 230), (40, 40, 40, 230)
    wire(d, [(160, 500), (320, 500), (880, 500)], ORANGE, 8)  # GPIO → R1 area → pin1
    wire(d, [(160, 620), (320, 620), (880, 580)], BLUE, 8)    # GND → pin2
    wire(d, [(1240, 500), (1480, 500), (1680, 560)], RED, 8)  # pin6 → gate
    wire(d, [(1240, 610), (1480, 640), (1580, 640)], BRN, 8)  # pin4 → MT2
    wire(d, [(1580, 680), (1780, 780), (2050, 500)], BRN, 7)  # MT2 → lamp
    wire(d, [(1520, 680), (1780, 820)], BLK, 7)               # MT1 → N

    d.rounded_rectangle((40, 1120, 2360, 1460), radius=16, fill=(255, 255, 255, 245), outline=(27, 54, 93, 255), width=3)
    d.text((64, 1140), "Cableado  (esta foto + el PDF mandan sobre el render 3D)", font=F20B, fill=(27, 54, 93))
    lines = [
        "GPIO (naranja) → R1 120 Ω → MOC pin 1.    GND (azul) → MOC pin 2.    Rpd 10 k entre GPIO y GND.",
        "MOC pin 6 → R2 330 Ω → G.    MOC pin 4 → MT2.    MT1 → N.    L → fusible 2 A T → lámpara → MT2.",
        "TO-220 de frente, pestaña atrás: izquierda MT1 · centro MT2 · derecha G.  Invertirlos = zumbido y calor.",
        "MOC a caballo del aire.  220 V solo en la placa perforada.  No PWM.  No unir GND con N.  Probar lámpara, luego bobina.",
    ]
    yy = 1190
    for ln in lines:
        d.text((64, yy), ln, font=F22, fill=(26, 26, 26))
        yy += 58

    rgb = canvas.convert("RGB")
    rgb.save(OUT, "PNG", quality=95)
    PCB.mkdir(exist_ok=True)
    (PCB / OUT.name).write_bytes(OUT.read_bytes())
    print(OUT, OUT.stat().st_size)


if __name__ == "__main__":
    main()
