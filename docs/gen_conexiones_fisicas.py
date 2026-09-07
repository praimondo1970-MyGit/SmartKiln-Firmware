#!/usr/bin/env python3
"""Imagen de cableado con encapsulados físicos (MOC3020, BT138, contactora)."""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

OUT = Path(__file__).with_name("Conexiones_Fisicas_Potencia.png")

W, H = 2800, 1880
NAVY = (27, 54, 93)
WHITE = (255, 255, 255)
BG = (246, 247, 249)
INK = (26, 26, 26)
MUTED = (92, 103, 125)
LV = (29, 78, 137)
LIVE = (176, 32, 32)
NEUT = (52, 58, 64)
PE = (27, 103, 58)
GOLD = (166, 124, 50)
ORANGE = (196, 98, 22)
BODY = (36, 36, 38)
TAB = (168, 172, 176)
CREAM = (236, 224, 196)
BLUE_MOV = (62, 98, 168)
CAP = (40, 48, 58)
PLASTIC = (210, 214, 218)
CONTACTOR = (92, 98, 104)


def font(size, bold=False):
    path = r"C:\Windows\Fonts\arialbd.ttf" if bold else r"C:\Windows\Fonts\arial.ttf"
    try:
        return ImageFont.truetype(path, size)
    except OSError:
        return ImageFont.load_default()


F14, F16, F18, F20 = font(14), font(16), font(18), font(20)
F22, F24, F28, F32 = font(22), font(24), font(28), font(32)
F18B, F20B, F22B, F24B = font(18, True), font(20, True), font(22, True), font(24, True)
F28B, F36B, F42B = font(28, True), font(36, True), font(42, True)


def rr(d, box, r, fill, outline=None, width=2):
    d.rounded_rectangle(box, radius=r, fill=fill, outline=outline, width=width)


def text(d, xy, s, f, fill=INK, anchor="lt"):
    d.text(xy, s, font=f, fill=fill, anchor=anchor)


def wire(d, pts, color, width=6):
    d.line(pts, fill=color, width=width, joint="curve")
    # rounded caps
    r = max(2, width // 2)
    d.ellipse((pts[0][0] - r, pts[0][1] - r, pts[0][0] + r, pts[0][1] + r), fill=color)
    d.ellipse((pts[-1][0] - r, pts[-1][1] - r, pts[-1][0] + r, pts[-1][1] + r), fill=color)


def dot(d, xy, r=8, fill=INK):
    x, y = xy
    d.ellipse((x - r, y - r, x + r, y + r), fill=fill, outline=WHITE, width=2)


def pin_label(d, xy, s, side="left"):
    x, y = xy
    if side == "left":
        text(d, (x - 10, y), s, F16, MUTED, "rm")
    elif side == "right":
        text(d, (x + 10, y), s, F16, MUTED, "lm")
    else:
        text(d, (x, y + 16), s, F16, MUTED, "mt")


def draw_header(d):
    d.rectangle((0, 0, W, 88), fill=NAVY)
    text(d, (36, 28), "IA Kiln  ·  Conexiones físicas", F42B, WHITE, "lt")
    text(
        d,
        (36, 64),
        "MOC3020 DIP-6  +  BT138 TO-220  +  contactora Siemens 40 A     ·     placa: solo J1 (ESP) y J2 (bobina)",
        F18,
        (210, 220, 232),
        "lt",
    )
    text(d, (W - 36, 44), "Rev. B", F24B, WHITE, "rt")
    d.rectangle((0, H - 70, W, H), fill=(237, 238, 241))
    text(
        d,
        (36, H - 48),
        "Cables:  azul = 3,3 V   ·   negro = GND   ·   rojo = vivo 220 V   ·   gris = neutro   ·   naranja = puerta TRIAC   ·   verde/amarillo = PE",
        F18,
        NAVY,
        "lt",
    )
    text(
        d,
        (36, H - 24),
        "No unir GND de control con el neutro. Distancia ≥ 6 mm entre 3,3 V y 220 V. Probar la bobina antes de conectar las resistencias.",
        F16,
        MUTED,
        "lt",
    )


def draw_terminal_block(d, x, y):
    """Bornes CTRL y GND. Devuelve centros de tornillo."""
    rr(d, (x, y, x + 150, y + 168), 10, (230, 236, 232), NAVY, 3)
    text(d, (x + 75, y + 16), "Bornes control", F18B, LV, "mt")
    positions = []
    for i, (name, col) in enumerate([("CTRL", LV), ("GND", INK)]):
        yy = y + 50 + i * 56
        d.rounded_rectangle((x + 18, yy, x + 132, yy + 44), 6, fill=WHITE, outline=col, width=3)
        d.ellipse((x + 32, yy + 10, x + 56, yy + 34), outline=(120, 120, 120), width=3, fill=(200, 200, 200))
        d.ellipse((x + 38, yy + 16, x + 50, yy + 28), fill=(80, 80, 80))
        text(d, (x + 70, yy + 22), name, F20B, col, "lm")
        positions.append((x + 44, yy + 22))
    return {"CTRL": positions[0], "GND": positions[1]}


def draw_resistor(d, x, y, label, value, horizontal=True):
    """Resistencia física. Devuelve (izq/arriba, der/abajo) de los cables."""
    if horizontal:
        d.line((x, y, x + 22, y), fill=(160, 140, 90), width=5)
        d.rounded_rectangle((x + 22, y - 16, x + 98, y + 16), 8, fill=CREAM, outline=(120, 100, 60), width=2)
        for bx, col in ((34, (40, 40, 40)), (48, (80, 50, 20)), (62, (200, 40, 40)), (78, (180, 160, 40))):
            d.rectangle((x + bx, y - 16, x + bx + 6, y + 16), fill=col)
        d.line((x + 98, y, x + 120, y), fill=(160, 140, 90), width=5)
        text(d, (x + 60, y - 28), label, F18B, INK, "mb")
        text(d, (x + 60, y + 28), value, F16, MUTED, "mt")
        return (x, y), (x + 120, y)
    d.line((x, y, x, y + 18), fill=(160, 140, 90), width=5)
    d.rounded_rectangle((x - 16, y + 18, x + 16, y + 86), 8, fill=CREAM, outline=(120, 100, 60), width=2)
    for by, col in ((28, (40, 40, 40)), (42, (80, 50, 20)), (56, (200, 40, 40)), (70, (180, 160, 40))):
        d.rectangle((x - 16, y + by, x + 16, y + by + 6), fill=col)
    d.line((x, y + 86, x, y + 104), fill=(160, 140, 90), width=5)
    text(d, (x + 26, y + 40), label, F18B, INK, "lm")
    text(d, (x + 26, y + 62), value, F16, MUTED, "lm")
    return (x, y), (x, y + 104)


def draw_moc(d, x, y):
    """DIP-6 visto desde arriba, muesca arriba. Pines: 1 ánodo, 2 cátodo, 4 y 6 fototriac."""
    bw, bh = 150, 210
    rr(d, (x, y, x + bw, y + bh), 8, BODY, (10, 10, 10), 2)
    d.ellipse((x + bw / 2 - 16, y + 8, x + bw / 2 + 16, y + 28), fill=(18, 18, 18))
    d.pieslice((x + bw / 2 - 18, y - 6, x + bw / 2 + 18, y + 30), 0, 180, fill=(18, 18, 18))
    text(d, (x + bw / 2, y + 48), "MOC3020", F22B, WHITE, "mt")
    text(d, (x + bw / 2, y + 74), "opto-TRIAC", F16, (180, 180, 180), "mt")
    text(d, (x + bw / 2, y + bh / 2 + 8), "DIP-6", F16, GOLD, "mt")
    text(d, (x + bw / 2, y + bh - 22), "muesca = pin 1", F14, (160, 160, 160), "mb")

    pins = {}
    # left 1,2,3 top to bottom; right 6,5,4 top to bottom
    left = [(1, "ánodo"), (2, "cátodo"), (3, "NC")]
    right = [(6, "foto"), (5, "NC"), (4, "foto")]
    for i, (n, lab) in enumerate(left):
        py = y + 50 + i * 52
        d.rectangle((x - 28, py - 8, x + 4, py + 8), fill=TAB, outline=(90, 90, 90))
        pins[n] = (x - 28, py)
        text(d, (x + 14, py), f"{n}", F18B, WHITE, "lm")
        text(d, (x - 34, py), lab, F14, MUTED, "rm")
    for i, (n, lab) in enumerate(right):
        py = y + 50 + i * 52
        d.rectangle((x + bw - 4, py - 8, x + bw + 28, py + 8), fill=TAB, outline=(90, 90, 90))
        pins[n] = (x + bw + 28, py)
        text(d, (x + bw - 14, py), f"{n}", F18B, WHITE, "rm")
        text(d, (x + bw + 34, py), lab, F14, MUTED, "lm")
    return pins


def draw_bt138(d, x, y):
    """TO-220 de frente, pestaña atrás. Pines izq→der: MT1, MT2, G."""
    # tab
    d.polygon([(x + 18, y), (x + 122, y), (x + 112, y + 28), (x + 28, y + 28)], fill=TAB, outline=(90, 90, 90))
    d.ellipse((x + 62, y + 6, x + 78, y + 22), outline=(70, 70, 70), width=3)
    # body
    rr(d, (x + 10, y + 26, x + 130, y + 150), 6, BODY, (10, 10, 10), 2)
    text(d, (x + 70, y + 48), "BT138", F22B, WHITE, "mt")
    text(d, (x + 70, y + 74), "12 A / 600 V", F14, (180, 180, 180), "mt")
    text(d, (x + 70, y + 96), "TO-220", F16, GOLD, "mt")
    text(d, (x + 70, y + 132), "pestaña = MT2", F14, (160, 160, 160), "mt")
    pins = {}
    names = [(1, "MT1"), (2, "MT2"), (3, "G")]
    for i, (n, lab) in enumerate(names):
        px = x + 34 + i * 36
        d.rectangle((px - 6, y + 148, px + 6, y + 198), fill=TAB, outline=(90, 90, 90))
        pins[lab] = (px, y + 198)
        text(d, (px, y + 212), lab, F18B, LIVE if lab != "G" else ORANGE, "mt")
        text(d, (px, y + 230), f"pin {n}", F14, MUTED, "mt")
    return pins


def draw_mov(d, x, y):
    d.ellipse((x, y, x + 70, y + 70), fill=BLUE_MOV, outline=(30, 50, 90), width=3)
    text(d, (x + 35, y + 28), "MOV", F18B, WHITE, "mt")
    text(d, (x + 35, y + 48), "14D431", F14, (220, 230, 255), "mt")
    d.line((x + 18, y + 70, x + 18, y + 96), fill=TAB, width=4)
    d.line((x + 52, y + 70, x + 52, y + 96), fill=TAB, width=4)
    text(d, (x + 35, y - 8), "RV1", F18B, INK, "mb")
    return {"a": (x + 18, y + 96), "b": (x + 52, y + 96)}


def draw_cap(d, x, y):
    rr(d, (x, y, x + 56, y + 78), 4, CAP, (20, 24, 30), 2)
    text(d, (x + 28, y + 18), "Cs", F18B, WHITE, "mt")
    text(d, (x + 28, y + 40), "100 nF", F14, (200, 200, 200), "mt")
    text(d, (x + 28, y + 58), "X2", F14, GOLD, "mt")
    d.line((x + 14, y + 78, x + 14, y + 100), fill=TAB, width=4)
    d.line((x + 42, y + 78, x + 42, y + 100), fill=TAB, width=4)
    return {"a": (x + 14, y + 100), "b": (x + 42, y + 100)}


def draw_fuse(d, x, y):
    rr(d, (x, y, x + 130, y + 54), 8, WHITE, LIVE, 3)
    d.rounded_rectangle((x + 18, y + 16, x + 112, y + 38), 10, fill=(245, 230, 180), outline=(140, 100, 40), width=2)
    text(d, (x + 65, y + 27), "F1  1 A gG", F18B, INK, "mm")
    d.ellipse((x + 8, y + 20, x + 22, y + 34), fill=TAB, outline=(90, 90, 90))
    d.ellipse((x + 108, y + 20, x + 122, y + 34), fill=TAB, outline=(90, 90, 90))
    text(d, (x + 65, y - 8), "portafusible bobina", F16, MUTED, "mb")
    return {"in": (x + 15, y + 27), "out": (x + 115, y + 27)}


def draw_esta(d, x, y):
    d.ellipse((x, y, x + 78, y + 78), fill=(196, 32, 32), outline=(120, 16, 16), width=4)
    d.ellipse((x + 18, y + 18, x + 60, y + 60), fill=(220, 60, 60), outline=(255, 200, 200), width=2)
    text(d, (x + 39, y + 39), "S1", F20B, WHITE, "mm")
    text(d, (x + 39, y + 90), "SETA  NC", F18B, LIVE, "mt")
    text(d, (x + 39, y + 112), "abre al pulsar", F14, MUTED, "mt")
    return {"a": (x + 10, y + 70), "b": (x + 68, y + 70)}


def draw_plug(d, x, y):
    rr(d, (x, y, x + 150, y + 150), 16, (240, 240, 242), NAVY, 3)
    text(d, (x + 75, y + 16), "Enchufe 220 V", F18B, NAVY, "mt")
    # IRAM-like: earth round top, two flat below
    d.ellipse((x + 60, y + 40, x + 90, y + 70), outline=PE, width=4, fill=WHITE)
    d.rounded_rectangle((x + 38, y + 88, x + 58, y + 118), 3, fill=WHITE, outline=LIVE, width=3)
    d.rounded_rectangle((x + 92, y + 88, x + 112, y + 118), 3, fill=WHITE, outline=NEUT, width=3)
    text(d, (x + 75, y + 36), "PE", F14, PE, "mb")
    text(d, (x + 48, y + 128), "L", F18B, LIVE, "mt")
    text(d, (x + 102, y + 128), "N", F18B, NEUT, "mt")
    return {"L": (x + 48, y + 150), "N": (x + 102, y + 150), "PE": (x + 75, y + 70)}


def draw_contactor(d, x, y):
    rr(d, (x, y, x + 420, y + 260), 12, CONTACTOR, (40, 44, 48), 3)
    d.rectangle((x, y, x + 420, y + 44), fill=(60, 64, 70))
    text(d, (x + 210, y + 22), "K1  Contactora Siemens  40 A", F22B, WHITE, "mm")
    text(d, (x + 210, y + 58), "bobina 220 VAC     ·     usar 2 polos     ·     L3/T3 libre", F16, (210, 210, 210), "mt")

    def screw(px, py, label, col):
        d.rounded_rectangle((px - 28, py - 22, px + 28, py + 22), 6, fill=(230, 230, 232), outline=col, width=3)
        d.ellipse((px - 10, py - 10, px + 10, py + 10), outline=(90, 90, 90), width=3, fill=(190, 190, 190))
        d.ellipse((px - 4, py - 4, px + 4, py + 4), fill=(70, 70, 70))
        text(d, (px, py - 34), label, F18B, col, "mb")
        return (px, py)

    a1 = screw(x + 70, y + 110, "A1  bobina", LIVE)
    a2 = screw(x + 70, y + 200, "A2  bobina", LIVE)
    l1 = screw(x + 190, y + 110, "L1", LIVE)
    t1 = screw(x + 190, y + 200, "T1", LIVE)
    l2 = screw(x + 280, y + 110, "L2", NEUT)
    t2 = screw(x + 280, y + 200, "T2", NEUT)
    l3 = screw(x + 360, y + 110, "L3", MUTED)
    t3 = screw(x + 360, y + 200, "T3", MUTED)
    text(d, (x + 360, y + 236), "sin uso", F14, (180, 180, 180), "mt")
    # mechanical link
    d.line((x + 108, y + 110, x + 155, y + 110), fill=(180, 180, 180), width=2)
    return {"A1": a1, "A2": a2, "L1": l1, "T1": t1, "L2": l2, "T2": t2, "L3": l3, "T3": t3}


def draw_heater(d, x, y):
    rr(d, (x, y, x + 210, y + 90), 10, (255, 236, 214), LIVE, 3)
    # zig zag
    pts = []
    for i in range(9):
        pts.append((x + 18 + i * 20, y + (28 if i % 2 == 0 else 62)))
    d.line(pts, fill=ORANGE, width=5, joint="curve")
    text(d, (x + 105, y + 12), "Resistencias del horno", F18B, LIVE, "mt")
    text(d, (x + 105, y + 78), "hasta 40 A  ·  no al TRIAC", F14, MUTED, "mb")
    return {"a": (x, y + 45), "b": (x + 210, y + 45)}


def draw_breaker(d, x, y):
    rr(d, (x, y, x + 80, y + 100), 8, WHITE, LIVE, 3)
    d.rounded_rectangle((x + 28, y + 18, x + 52, y + 70), 4, fill=(220, 50, 50), outline=(140, 20, 20), width=2)
    d.polygon([(x + 32, y + 28), (x + 48, y + 28), (x + 40, y + 48)], fill=WHITE)
    text(d, (x + 40, y + 84), "Q1  40 A", F18B, LIVE, "mt")
    return {"in": (x + 40, y), "out": (x + 40, y + 100)}


def draw_earth(d, x, y):
    d.line((x, y, x, y + 14), fill=PE, width=4)
    d.line((x - 22, y + 14, x + 22, y + 14), fill=PE, width=5)
    d.line((x - 14, y + 24, x + 14, y + 24), fill=PE, width=4)
    d.line((x - 7, y + 34, x + 7, y + 34), fill=PE, width=3)
    text(d, (x, y + 48), "PE  chasis horno", F16, PE, "mt")


def main():
    im = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(im)
    draw_header(d)

    # Region boxes
    rr(d, (24, 108, 1380, 820), 16, (234, 242, 250), LV, 2)
    text(d, (44, 124), "1  Lado 3,3 V y disparo del TRIAC", F24B, LV, "lt")
    rr(d, (1400, 108, 2776, 820), 16, (253, 242, 242), LIVE, 2)
    text(d, (1420, 124), "2  Protecciones del TRIAC  (en paralelo MT1–MT2)", F24B, LIVE, "lt")
    rr(d, (24, 840, 2776, 1790), 16, (236, 244, 236), PE, 2)
    text(d, (44, 856), "3  Red 220 VAC, contactora y resistencias", F24B, PE, "lt")

    # --- Physical parts row 1 ---
    tb = draw_terminal_block(d, 50, 180)
    r1a, r1b = draw_resistor(d, 250, 252, "R1", "120 Ω  1/4 W", True)
    rpda, rpdb = draw_resistor(d, 170, 280, "Rpd", "10 kΩ", False)
    moc = draw_moc(d, 460, 200)
    r2a, r2b = draw_resistor(d, 720, 250, "R2", "330 Ω", True)
    tri = draw_bt138(d, 920, 170)

    mov = draw_mov(d, 1520, 280)
    cap = draw_cap(d, 1680, 270)
    rs_a, rs_b = draw_resistor(d, 1800, 430, "Rs", "100 Ω  2 W", True)
    text(d, (1680, 200), "Montar RV1 y el snubber Cs+Rs", F20B, LIVE, "lt")
    text(d, (1680, 228), "lo más cerca posible de MT1 y MT2 del BT138.", F18, MUTED, "lt")

    # Control wiring
    # CTRL -> Rpd top and R1 left
    wire(d, [tb["CTRL"], (tb["CTRL"][0] + 70, tb["CTRL"][1]), (r1a[0], r1a[1])], LV, 6)
    dot(d, (170, 252), 7, LV)
    wire(d, [(170, 252), rpda], LV, 5)
    wire(d, [r1b, (r1b[0] + 20, r1b[1]), (r1b[0] + 20, moc[1][1]), moc[1]], LV, 6)
    # GND
    wire(d, [tb["GND"], (170, tb["GND"][1]), (170, rpdb[1])], INK, 6)
    dot(d, (170, rpdb[1]), 7, INK)
    wire(d, [(170, rpdb[1]), (170, moc[2][1]), moc[2]], INK, 6)

    # MOC 6 -> R2 -> Gate (naranja)
    wire(d, [moc[6], r2a], ORANGE, 6)
    wire(d, [r2b, (tri["G"][0], r2b[1]), tri["G"]], ORANGE, 6)
    text(d, ((r2a[0] + r2b[0]) // 2, r2a[1] - 46), "pin 6 → G", F16, ORANGE, "mb")

    # MOC 4 -> solo MT2 (centro), sin pasar por MT1
    p4 = moc[4]
    mt2 = tri["MT2"]
    wire(d, [p4, (p4[0] + 36, p4[1]), (p4[0] + 36, mt2[1] + 36), (mt2[0], mt2[1] + 36), mt2], LIVE, 6)
    text(d, (p4[0] + 44, p4[1] + 18), "pin 4 → MT2", F16, LIVE, "lt")

    # Bajadas separadas: MT2 a la derecha, MT1 a la izquierda
    j_mt2 = (mt2[0] + 90, 700)
    j_mt1 = (tri["MT1"][0] - 50, 700)
    wire(d, [mt2, (mt2[0], 560), (j_mt2[0], 560), j_mt2], LIVE, 7)
    wire(d, [tri["MT1"], (tri["MT1"][0], 560), (j_mt1[0], 560), j_mt1], NEUT, 7)
    dot(d, j_mt2, 8, LIVE)
    dot(d, j_mt1, 8, NEUT)
    text(d, (j_mt2[0] + 8, 700), "MT2 / A2", F16, LIVE, "lm")
    text(d, (j_mt1[0] - 8, 700), "MT1 / N", F16, NEUT, "rm")

    # MOV entre MT1 y MT2
    wire(d, [j_mt2, (j_mt2[0], 520), (mov["b"][0], 520), mov["b"]], LIVE, 5)
    wire(d, [j_mt1, (j_mt1[0], 500), (mov["a"][0], 500), mov["a"]], NEUT, 5)

    # Snubber Cs + Rs en serie, paralelo a MT1–MT2
    wire(d, [j_mt2, (cap["a"][0], 700), (cap["a"][0], cap["a"][1])], LIVE, 5)
    wire(d, [cap["b"], (cap["b"][0], rs_a[1]), rs_a], INK, 5)
    wire(d, [rs_b, (2160, rs_b[1]), (2160, 500), (j_mt1[0], 500)], NEUT, 5)
    dot(d, (j_mt1[0], 500), 7, NEUT)

    text(d, (1520, 170), "RV1  +  Cs  +  Rs  van entre las patas MT1 y MT2", F18, MUTED, "lt")
    text(d, (50, 760), "Pin 1 MOC = ánodo (desde R1).  Pin 2 = cátodo (GND).  Pines 3 y 5 sin conectar.", F18, MUTED, "lt")
    text(d, (50, 788), "BT138 de frente, pestaña atrás:  izquierda MT1 · centro MT2 · derecha G.", F18, MUTED, "lt")

    # --- Row 3 power ---
    plug = draw_plug(d, 50, 930)
    fuse = draw_fuse(d, 250, 1000)
    esta = draw_esta(d, 430, 930)
    k1 = draw_contactor(d, 580, 910)
    q1 = draw_breaker(d, 1120, 930)
    heat = draw_heater(d, 1080, 1288)
    draw_earth(d, 168, 1220)

    lx = plug["L"][0]
    # L hacia bobina (F1 + seta + A1) — por debajo de los bornes superiores
    wire(d, [plug["L"], (lx, fuse["in"][1]), fuse["in"]], LIVE, 8)
    dot(d, (lx, fuse["in"][1]), 8, LIVE)
    wire(d, [fuse["out"], (esta["a"][0], fuse["out"][1])], LIVE, 7)
    wire(d, [esta["b"], (esta["b"][0], 980), (k1["A1"][0], 980), k1["A1"]], LIVE, 7)

    # L hacia potencia: bus superior, entra a Q1, baja solo a L1
    wire(d, [(lx, fuse["in"][1]), (lx, 888), (q1["in"][0], 888), q1["in"]], LIVE, 8)
    wire(d, [q1["out"], (q1["out"][0], 888), (k1["L1"][0], 888), k1["L1"]], LIVE, 8)
    text(d, (k1["L1"][0] + 8, 888), "solo L1", F14, LIVE, "lt")

    # A2 sube a MT2, sin pegarse al neutro
    wire(d, [k1["A2"], (k1["A2"][0], 1196), (j_mt2[0], 1196), (j_mt2[0], 820)], LIVE, 7)
    wire(d, [(j_mt2[0], 820), j_mt2], LIVE, 7)
    dot(d, j_mt2, 8, LIVE)
    text(d, (j_mt2[0] + 10, 1196), "A2 → MT2", F16, LIVE, "lm")

    # MT1 baja a N; N sube solo a L2
    n_bus = 1410
    wire(d, [j_mt1, (j_mt1[0], 860), (j_mt1[0], n_bus)], NEUT, 7)
    wire(d, [plug["N"], (plug["N"][0], n_bus), (k1["L2"][0], n_bus)], NEUT, 8)
    dot(d, (j_mt1[0], n_bus), 8, NEUT)
    wire(d, [(k1["L2"][0], n_bus), k1["L2"]], NEUT, 8)
    text(d, (k1["L2"][0] + 8, n_bus - 14), "solo L2", F14, NEUT, "lt")

    # Resistencias entre T1 y T2 (cada uno baja en vertical)
    wire(d, [k1["T1"], (k1["T1"][0], heat["a"][1]), heat["a"]], LIVE, 8)
    wire(d, [heat["b"], (heat["b"][0] + 20, heat["b"][1]), (heat["b"][0] + 20, k1["T2"][1] + 50),
             (k1["T2"][0], k1["T2"][1] + 50), k1["T2"]], LIVE, 8)

    # PE
    wire(d, [plug["PE"], (168, plug["PE"][1]), (168, 1220)], PE, 6)

    # Callouts
    notes = [
        (1980, 920, "Cómo cablear, en orden"),
        (1980, 960, "1.  CTRL → R1 → pin 1 MOC.   GND → pin 2."),
        (1980, 996, "2.  Pin 6 MOC → R2 → pata G del BT138."),
        (1980, 1032, "3.  Pin 4 MOC → pata MT2 (centro) del BT138."),
        (1980, 1068, "4.  MOV y snubber entre MT1 (izq) y MT2 (centro)."),
        (1980, 1104, "5.  En tablero: L → F1 → S1 → A1.  A2 → J2-A2."),
        (1980, 1140, "6.  J2-N → N del enchufe.  (F1 y seta NO van en la placa.)"),
        (1980, 1176, "7.  L → Q1 → L1.    N → L2."),
        (1980, 1212, "8.  T1 y T2 a las resistencias."),
        (1980, 1248, "9.  PE al chasis.  L3 y T3 vacíos."),
        (1980, 1300, "La seta S1 es NC: en reposo deja pasar."),
        (1980, 1336, "Al pulsar, abre y la contactora se cae"),
        (1980, 1372, "aunque el ESP siga en HIGH."),
    ]
    rr(d, (1948, 880, 2756, 1410), 12, WHITE, PE, 2)
    for i, (x, y, s) in enumerate(notes):
        text(d, (x, y), s, F20B if i == 0 else F18, PE if i == 0 else INK, "lt")

    rr(d, (1948, 1430, 2756, 1760), 12, (255, 244, 244), LIVE, 2)
    text(d, (1980, 1456), "No hacer", F22B, LIVE, "lt")
    text(d, (1980, 1500), "·  No conectar las resistencias al BT137/BT138.", F18, INK, "lt")
    text(d, (1980, 1536), "·  No intercambiar MT1 y MT2 (zumbido / no cierra).", F18, INK, "lt")
    text(d, (1980, 1572), "·  No usar PWM ni recorte de fase sobre la bobina.", F18, INK, "lt")
    text(d, (1980, 1608), "·  No compartir GND del ESP con el neutro de 220 V.", F18, INK, "lt")
    text(d, (1980, 1644), "·  Pines 3 y 5 del MOC3020: no conectar.", F18, INK, "lt")
    text(d, (1980, 1680), "·  Si la bobina es 24 VDC, este cableado no sirve.", F18, INK, "lt")
    text(d, (1980, 1716), "·  Trabajar siempre con el enchufe desconectado.", F18, INK, "lt")

    im.save(OUT, "PNG", dpi=(150, 150))
    print(OUT)


if __name__ == "__main__":
    main()
