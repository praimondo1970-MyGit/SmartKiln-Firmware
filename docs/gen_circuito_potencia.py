#!/usr/bin/env python3
"""Genera el PDF del circuito de potencia (MOC3020 + BT138 + contactora)."""

from pathlib import Path

from reportlab.lib.pagesizes import A3, landscape
from reportlab.lib.units import mm
from reportlab.lib.colors import HexColor, white
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfgen import canvas

OUT = Path(__file__).with_name("Circuito_Potencia_Horno.pdf")

NAVY = HexColor("#1B365D")
LINE = HexColor("#1A1A1A")
MUTED = HexColor("#5C677D")
LV = HexColor("#1D4E89")
AC = HexColor("#9B1C1C")
PE = HexColor("#1B4332")
GOLD = HexColor("#8A6D3B")
FILL_LV = HexColor("#EAF2FA")
FILL_ISO = HexColor("#FBF6E9")
FILL_AC = HexColor("#FDF2F2")
FILL_PWR = HexColor("#F3F6F1")
FILL_NOTE = HexColor("#F7F7F4")


def register_fonts():
    candidates = [
        (r"C:\Windows\Fonts\arial.ttf", r"C:\Windows\Fonts\arialbd.ttf"),
        (r"C:\Windows\Fonts\segoeui.ttf", r"C:\Windows\Fonts\segoeuib.ttf"),
        (r"C:\Windows\Fonts\calibri.ttf", r"C:\Windows\Fonts\calibrib.ttf"),
    ]
    for regular, bold in candidates:
        if Path(regular).exists() and Path(bold).exists():
            pdfmetrics.registerFont(TTFont("Body", regular))
            pdfmetrics.registerFont(TTFont("Body-Bold", bold))
            return "Body", "Body-Bold"
    return "Helvetica", "Helvetica-Bold"


FONT, FONT_B = register_fonts()


class Sch:
    def __init__(self, c: canvas.Canvas):
        self.c = c

    def stroke(self, color=LINE, width=1.2):
        self.c.setStrokeColor(color)
        self.c.setFillColor(color)
        self.c.setLineWidth(width)
        self.c.setLineCap(1)
        self.c.setLineJoin(1)

    def line(self, x1, y1, x2, y2, color=LINE, width=1.2):
        self.stroke(color, width)
        self.c.line(x1 * mm, y1 * mm, x2 * mm, y2 * mm)

    def poly(self, pts, color=LINE, width=1.2, close=False, fill=None):
        p = self.c.beginPath()
        p.moveTo(pts[0][0] * mm, pts[0][1] * mm)
        for x, y in pts[1:]:
            p.lineTo(x * mm, y * mm)
        if close:
            p.close()
        self.c.setStrokeColor(color)
        self.c.setLineWidth(width)
        if fill is not None:
            self.c.setFillColor(fill)
            self.c.drawPath(p, fill=1, stroke=1)
        else:
            self.c.drawPath(p, fill=0, stroke=1)

    def dot(self, x, y, r=1.2, color=LINE):
        self.c.setFillColor(color)
        self.c.setStrokeColor(color)
        self.c.circle(x * mm, y * mm, r * mm, fill=1, stroke=0)

    def circ(self, x, y, r, fill=white, stroke=LINE, width=1.2):
        self.c.setFillColor(fill)
        self.c.setStrokeColor(stroke)
        self.c.setLineWidth(width)
        self.c.circle(x * mm, y * mm, r * mm, fill=1, stroke=1)

    def round_box(self, x, y, w, h, fill, stroke, sw=0.7, r=2.6):
        self.c.setFillColor(fill)
        self.c.setStrokeColor(stroke)
        self.c.setLineWidth(sw)
        self.c.roundRect(x * mm, y * mm, w * mm, h * mm, r * mm, fill=1, stroke=1)

    def text(self, x, y, s, size=8, bold=False, color=LINE, align="left"):
        self.c.setFillColor(color)
        self.c.setFont(FONT_B if bold else FONT, size)
        if align == "center":
            self.c.drawCentredString(x * mm, y * mm, s)
        elif align == "right":
            self.c.drawRightString(x * mm, y * mm, s)
        else:
            self.c.drawString(x * mm, y * mm, s)

    def term(self, x, y, label, color=LINE, side="right"):
        self.circ(x, y, 2.2, white, color, 1.35)
        self.text(
            x + (5 if side == "right" else -5),
            y - 1.1,
            label,
            8,
            True,
            color,
            "left" if side == "right" else "right",
        )

    def resistor_h(self, x, y, w=16, h=6, label="", sub="", color=LINE):
        self.c.setFillColor(white)
        self.c.setStrokeColor(color)
        self.c.setLineWidth(1.15)
        self.c.rect(x * mm, (y - h / 2) * mm, w * mm, h * mm, fill=1, stroke=1)
        self.line(x - 5, y, x, y, color)
        self.line(x + w, y, x + w + 5, y, color)
        if label:
            self.text(x + w / 2, y + h / 2 + 1.8, label, 7.4, True, color, "center")
        if sub:
            self.text(x + w / 2, y - h / 2 - 4.4, sub, 6.4, False, MUTED, "center")
        return x - 5, x + w + 5

    def resistor_v(self, x, y, h=14, w=6, label="", sub="", color=LINE):
        self.c.setFillColor(white)
        self.c.setStrokeColor(color)
        self.c.setLineWidth(1.15)
        self.c.rect((x - w / 2) * mm, y * mm, w * mm, h * mm, fill=1, stroke=1)
        self.line(x, y + h, x, y + h + 5, color)
        self.line(x, y, x, y - 5, color)
        if label:
            self.text(x + w / 2 + 2, y + h / 2 + 1.2, label, 7.4, True, color)
        if sub:
            self.text(x + w / 2 + 2, y + h / 2 - 3.2, sub, 6.3, False, MUTED)
        return y - 5, y + h + 5


def triac_symbol(s: Sch, x, y, k=1.0, color=AC):
    """MT2 arriba, MT1 abajo, G a la derecha. Centro en (x, y)."""
    s.line(x, y + 12 * k, x, y - 12 * k, color, 1.35)
    s.poly(
        [(x, y + 1.4 * k), (x - 7 * k, y + 9.5 * k), (x + 7 * k, y + 9.5 * k)],
        color,
        1.25,
        close=True,
        fill=white,
    )
    s.poly(
        [(x, y - 1.4 * k), (x - 7 * k, y - 9.5 * k), (x + 7 * k, y - 9.5 * k)],
        color,
        1.25,
        close=True,
        fill=white,
    )
    s.line(x, y, x + 11 * k, y, color, 1.25)
    return {
        "mt2": (x, y + 12 * k),
        "mt1": (x, y - 12 * k),
        "g": (x + 11 * k, y),
    }


def header_footer(s: Sch, wm, hm, title, subtitle):
    s.c.setFillColor(NAVY)
    s.c.rect(0, (hm - 16) * mm, wm * mm, 16 * mm, fill=1, stroke=0)
    s.text(10, hm - 7.4, title, 15, True, white)
    s.text(10, hm - 13.4, subtitle, 8, False, HexColor("#D5DEEC"))
    s.text(wm - 10, hm - 7.4, "Rev. A", 10, True, white, "right")
    s.text(wm - 10, hm - 13.4, "2026-09-04", 8, False, HexColor("#D5DEEC"), "right")
    s.c.setFillColor(HexColor("#F3F4F6"))
    s.c.rect(0, 0, wm * mm, 14 * mm, fill=1, stroke=0)
    s.c.setStrokeColor(NAVY)
    s.c.setLineWidth(0.5)
    s.c.line(0, 14 * mm, wm * mm, 14 * mm)


def page1(c: canvas.Canvas):
    wm, hm = landscape(A3)[0] / mm, landscape(A3)[1] / mm
    s = Sch(c)
    header_footer(
        s,
        wm,
        hm,
        "IA Kiln  ·  Circuito de potencia",
        "MOC3020 + BT138 + contactora Siemens 40 A   |   220 VAC 50 Hz   |   control 3,3 V (ESP no dibujado)",
    )
    s.text(
        10,
        8.2,
        "El TRIAC solo conmuta la bobina. Las resistencias van por la contactora. No invertir MT1/MT2 del BT138.",
        7.3,
        False,
        NAVY,
    )
    s.text(
        10,
        3.4,
        "Vivo y neutro del enchufe pueden estar cruzados: por eso K1 corta L y N. Distancia ≥ 6 mm entre 3,3 V y 220 V.",
        7.3,
        False,
        MUTED,
    )

    # --- Regions ---
    s.round_box(8, 168, 86, 108, FILL_LV, LV, 0.8)
    s.text(12, 268, "1  Control  3,3 V", 9, True, LV)
    s.round_box(98, 168, 78, 108, FILL_ISO, GOLD)
    s.text(102, 268, "2  MOC3020", 9, True, GOLD)
    s.round_box(180, 168, 232, 108, FILL_AC, AC)
    s.text(184, 268, "3  Llave de bobina  220 VAC", 9, True, AC)
    s.round_box(8, 18, 404, 144, FILL_PWR, PE)
    s.text(12, 154, "4  Red, contactora y resistencias del horno", 9, True, PE)

    # Isolation barrier
    s.c.setDash(1.6, 1.8)
    s.line(137, 176, 137, 262, GOLD, 1.05)
    s.c.setDash()
    s.text(137, 172.5, "aislamiento", 6.2, True, GOLD, "center")

    # ========== 1. Control ==========
    Yc, Yg = 248, 192
    s.term(18, Yc, "CTRL", LV)
    s.term(18, Yg, "GND", LV)
    s.text(30, 258, "GPIO 3,3 V  ·  activo HIGH", 6.5, False, MUTED)

    s.line(20.3, Yc, 30, Yc, LV)
    s.resistor_h(35, Yc, 18, 6.2, "R1", "120 Ω  1/4 W", LV)
    s.dot(30, Yc, 1.15, LV)
    s.line(30, Yc, 30, 218, LV, 1.05)
    s.resistor_v(30, 200, 13, 6, "Rpd", "10 kΩ", LV)
    s.line(30, 195, 30, Yg, LV, 1.05)
    s.dot(30, Yg, 1.15, LV)
    s.line(20.3, Yg, 98, Yg, LV)

    # ========== 2. MOC3020 ==========
    mx, my, mw, mh = 108, 186, 58, 70
    s.round_box(mx, my, mw, mh, white, NAVY, 1.15, 2)
    s.text(mx + mw / 2, my + mh + 1.5, "opto-TRIAC  ·  disparo aleatorio  ·  400 V", 6.3, False, MUTED, "center")

    # LED
    lx, ly = mx + 16, my + 36
    s.poly([(lx - 6, ly - 6), (lx + 6, ly), (lx - 6, ly + 6)], LV, 1.2, True, HexColor("#D6E6F7"))
    s.line(lx + 6.3, ly - 6.3, lx + 6.3, ly + 6.3, LV, 1.45)
    s.poly([(lx + 9, ly + 2), (lx + 16, ly + 6.5)], GOLD, 1.05)
    s.poly([(lx + 9, ly - 1), (lx + 16, ly + 3.5)], GOLD, 1.05)
    s.text(lx - 1, ly + 16, "LED", 6.5, True, LV, "center")
    s.text(mx + 4, my + mh - 7, "1 ánodo", 6.5, True, LV)
    s.text(mx + 4, my + 4.5, "2 cátodo", 6.5, True, LV)

    pin1 = (mx, Yc)          # anode to CTRL height
    pin2 = (mx, Yg)
    pin6 = (mx + mw, Yc)
    pin4 = (mx + mw, 214)

    # Force pin y to box internals
    pin1 = (mx, my + 56)
    pin2 = (mx, my + 16)
    pin6 = (mx + mw, my + 56)
    pin4 = (mx + mw, my + 16)

    # Internal LED wiring
    s.line(pin1[0], pin1[1], mx + 10, pin1[1], LV, 1.05)
    s.line(mx + 10, pin1[1], mx + 10, ly + 6, LV, 1.05)
    s.line(pin2[0], pin2[1], mx + 10, pin2[1], LV, 1.05)
    s.line(mx + 10, pin2[1], lx - 6, ly, LV, 1.05)

    # Phototriac inside
    pt = triac_symbol(s, mx + 42, my + 36, 0.72, AC)
    s.text(mx + 42, my + 58, "fototriac", 6.5, True, AC, "center")
    s.text(mx + mw - 4, my + mh - 7, "6", 7, True, AC, "right")
    s.text(mx + mw - 4, my + 4.5, "4", 7, True, AC, "right")
    s.line(pin6[0], pin6[1], mx + mw - 8, pin6[1], AC, 1.05)
    s.line(mx + mw - 8, pin6[1], pt["g"][0], pt["g"][1], AC, 1.05)
    s.line(pin4[0], pin4[1], mx + mw - 8, pin4[1], AC, 1.05)
    s.line(mx + mw - 8, pin4[1], pt["mt1"][0], pt["mt1"][1], AC, 1.05)

    # External control to MOC — match actual pin y
    s.line(58, Yc, 58, pin1[1], LV)
    s.line(58, pin1[1], pin1[0], pin1[1], LV)
    s.dot(58, pin1[1], 1.05, LV)
    s.line(98, Yg, pin2[0], pin2[1], LV)

    # ========== 3. BT138 + snubber + MOV ==========
    tx, ty = 228, 220
    s.text(tx, ty + 28, "BT138", 10, True, AC, "center")
    s.text(tx, ty + 22.5, "TRIAC  12 A / 600 V", 6.4, False, MUTED, "center")
    t = triac_symbol(s, tx, ty, 1.15, AC)
    s.text(tx - 10, t["mt2"][1] - 1, "MT2", 7.2, True, AC, "right")
    s.text(tx - 10, t["mt1"][1] - 1, "MT1", 7.2, True, AC, "right")
    s.text(t["g"][0] + 2.2, t["g"][1] - 1, "G", 8, True, AC)
    s.dot(*t["mt2"], 1.2, AC)
    s.dot(*t["mt1"], 1.2, AC)
    s.dot(*t["g"], 1.2, AC)

    # R2 pin6 -> G
    s.line(pin6[0], pin6[1], 182, pin6[1], AC)
    s.resistor_h(187, pin6[1], 14, 5.6, "R2", "330 Ω", AC)
    s.line(206, pin6[1], t["g"][0] + 4, pin6[1], AC)
    s.line(t["g"][0] + 4, pin6[1], t["g"][0], t["g"][1], AC)

    # pin4 -> MT2
    s.line(pin4[0], pin4[1], 200, pin4[1], AC, 1.1)
    s.line(200, pin4[1], 200, t["mt2"][1], AC, 1.1)
    s.line(200, t["mt2"][1], t["mt2"][0], t["mt2"][1], AC, 1.1)
    s.dot(200, t["mt2"][1], 1.1, AC)

    # MOV across MT2-MT1 (right of triac)
    vx = 258
    s.line(t["mt2"][0], t["mt2"][1], vx, t["mt2"][1], AC, 1.05)
    s.line(t["mt1"][0], t["mt1"][1], vx, t["mt1"][1], AC, 1.05)
    s.dot(vx, t["mt2"][1], 1.05, AC)
    s.dot(vx, t["mt1"][1], 1.05, AC)
    # varistor diamond
    s.poly([(vx, t["mt2"][1] - 4), (vx + 7, (t["mt2"][1] + t["mt1"][1]) / 2),
            (vx, t["mt1"][1] + 4), (vx - 7, (t["mt2"][1] + t["mt1"][1]) / 2)],
           LINE, 1.15, True, white)
    s.line(vx - 8, t["mt2"][1] - 8, vx + 8, t["mt1"][1] + 8, LINE, 1.15)
    s.line(vx, t["mt2"][1], vx, t["mt2"][1] - 4, LINE, 1.05)
    s.line(vx, t["mt1"][1], vx, t["mt1"][1] + 4, LINE, 1.05)
    s.text(vx + 12, (t["mt2"][1] + t["mt1"][1]) / 2 + 3, "RV1  MOV", 7.3, True)
    s.text(vx + 12, (t["mt2"][1] + t["mt1"][1]) / 2 - 3.2, "14D431", 6.4, False, MUTED)

    # Snubber series RC next to MOV
    sx = 292
    s.line(vx, t["mt2"][1], sx, t["mt2"][1], LINE, 1.05)
    s.dot(sx, t["mt2"][1], 1.05, LINE)
    s.line(sx, t["mt2"][1], sx, 238, LINE, 1.05)
    # capacitor
    s.line(sx - 5, 238, sx + 5, 238, LINE, 1.5)
    s.line(sx - 5, 235.5, sx + 5, 235.5, LINE, 1.5)
    s.line(sx, 235.5, sx, 228, LINE, 1.05)
    s.c.setFillColor(white)
    s.c.setStrokeColor(LINE)
    s.c.setLineWidth(1.15)
    s.c.rect((sx - 3) * mm, 214 * mm, 6 * mm, 14 * mm, fill=1, stroke=1)
    s.line(sx, 214, sx, t["mt1"][1], LINE, 1.05)
    s.dot(sx, t["mt1"][1], 1.05, LINE)
    s.line(sx, t["mt1"][1], t["mt1"][0], t["mt1"][1], LINE, 1.05)
    s.text(sx + 8, 236, "Cs  100 nF X2", 6.6, True)
    s.text(sx + 8, 220, "Rs  100 Ω  2 W", 6.6, True)
    s.text(sx + 8, 212, "snubber MT1–MT2", 6.2, False, MUTED)

    # Pinout BT138
    s.round_box(348, 176, 58, 78, white, NAVY, 0.8, 2)
    s.text(377, 245, "BT138  TO-220", 7.3, True, NAVY, "center")
    s.text(377, 239, "frente, pestaña atrás", 6.1, False, MUTED, "center")
    s.c.setFillColor(HexColor("#3A3A3A"))
    s.c.roundRect(367 * mm, 210 * mm, 20 * mm, 18 * mm, 0.8 * mm, fill=1, stroke=0)
    s.c.setFillColor(HexColor("#8A8A8A"))
    s.c.rect(373 * mm, 226.5 * mm, 8 * mm, 3.2 * mm, fill=1, stroke=0)
    for i, name in enumerate(["MT1", "MT2", "G"]):
        px = 370 + i * 7
        s.c.setFillColor(HexColor("#C8C8C8"))
        s.c.rect(px * mm, 198 * mm, 3 * mm, 12 * mm, fill=1, stroke=0)
        s.text(px + 1.5, 190, name, 6.4, True, LINE, "center")
    s.text(377, 182, "Pestaña = MT2", 6.2, False, MUTED, "center")
    s.text(377, 176.8, "MOC pines 3 y 5: NC", 6.1, False, MUTED, "center")

    # ========== 4. Power section ==========
    YL, YN = 138, 36
    s.term(22, YL, "L   220 VAC", AC)
    s.term(22, YN, "N   220 VAC", LINE)
    s.term(22, 22, "PE", PE)
    s.text(54, 148, "Enchufe de pared", 8, True, NAVY)

    s.line(24.3, YL, 404, YL, AC, 1.7)
    s.line(24.3, YN, 404, YN, LINE, 1.7)
    s.line(24.3, 22, 56, 22, PE, 1.35)
    s.line(56, 22, 56, 18.5, PE, 1.35)
    s.stroke(PE, 1.55)
    s.c.line(50 * mm, 18.5 * mm, 62 * mm, 18.5 * mm)
    s.text(72, 20.5, "chasis horno", 6.4, True, PE)

    # F1 + SETA (rama bobina)
    xF = 78
    s.dot(xF, YL, 1.2, AC)
    s.line(xF, YL, xF, 126, AC, 1.3)
    s.round_box(xF - 8, 112, 16, 14, white, AC, 1.2, 2)
    s.line(xF - 5, 119, xF + 5, 119, AC, 1.45)
    s.text(xF, 128.5, "F1", 7.4, True, AC, "center")
    s.text(xF + 11, 116.5, "1 A gG", 6.2, False, MUTED)
    s.line(xF, 112, xF, 100, AC, 1.3)

    s.dot(xF, 100, 1.1, AC)
    s.dot(xF + 15, 100, 1.1, AC)
    s.poly([(xF, 100), (xF + 13.8, 106.5)], AC, 1.3)
    s.line(xF + 15, 100, xF + 26, 100, AC, 1.25)
    s.text(xF + 8, 108.5, "S1  SETA NC", 7, True, AC, "center")

    # K1 — a la izquierda, sin cruzar el TRIAC (x=228) ni el neutro
    kx, ky, kw, kh = 108, 44, 112, 66
    s.round_box(kx, ky, kw, kh, white, NAVY, 1.15, 2.2)
    s.text(kx + kw / 2, 130, "K1  Contactora Siemens  40 A", 8.2, True, NAVY, "center")
    s.text(kx + kw / 2, ky + kh - 6, "bobina 220 VAC  ·  2 polos NA  ·  3.er polo libre", 6.1, False, MUTED, "center")

    s.round_box(kx + 7, ky + 28, 22, 24, HexColor("#EEF3F9"), LINE, 1.1, 1.2)
    s.text(kx + 18, ky + 42, "A1", 8, True, LINE, "center")
    s.text(kx + 18, ky + 32, "A2", 8, True, LINE, "center")
    s.text(kx + 18, ky + 22, "bobina", 6.2, False, MUTED, "center")
    a1 = (kx + 18, ky + 52)
    a2 = (kx + 18, ky + 28)

    s.line(xF + 26, 100, a1[0], 100, AC, 1.25)
    s.line(a1[0], 100, a1[0], a1[1], AC, 1.25)
    s.dot(*a1, 1.15, AC)

    def pole(px, top, bot, up, dn):
        s.dot(px, top, 1.15, AC)
        s.dot(px, bot, 1.15, AC)
        s.line(px, top, px, top - 5, AC, 1.25)
        s.line(px, bot, px, bot + 3.5, AC, 1.25)
        s.poly([(px, top - 5), (px + 8.5, bot + 4.5)], AC, 1.4)
        s.text(px - 2.2, top + 2.4, up, 7.2, True, AC, "right")
        s.text(px - 2.2, bot - 5.4, dn, 7.2, True, AC, "right")

    p1x, p2x = kx + 54, kx + 88
    ptop, pbot = ky + 48, ky + 30
    pole(p1x, ptop, pbot, "L1", "T1")
    pole(p2x, ptop, pbot, "L2", "T2")
    s.c.setDash(1.3, 1.5)
    s.line(kx + 29, ky + 40, p1x - 7, ky + 40, MUTED, 0.85)
    s.c.setDash()
    s.text(kx + 71, ky + 8, "contactos NA  40 A", 6.3, False, MUTED, "center")

    # A2 → MT2  (columna a la derecha de K1, alineada con el TRIAC)
    x_up = t["mt2"][0]
    s.dot(*a2, 1.15, AC)
    s.line(a2[0], a2[1], x_up, a2[1], AC, 1.25)
    s.line(x_up, a2[1], x_up, t["mt2"][1], AC, 1.25)
    s.dot(x_up, t["mt2"][1], 1.2, AC)

    # MT1 → N  (misma columna)
    s.line(t["mt1"][0], t["mt1"][1], t["mt1"][0], YN, LINE, 1.25)
    s.dot(t["mt1"][0], YN, 1.2, LINE)

    # Q1 a la derecha de K1, entra a L1
    xQ = 270
    s.dot(xQ, YL, 1.2, AC)
    s.line(xQ, YL, xQ, 124, AC, 1.3)
    s.round_box(xQ - 8, 110, 16, 14, white, AC, 1.2, 1.5)
    s.dot(xQ, 121, 1.0, AC)
    s.dot(xQ, 113, 1.0, AC)
    s.poly([(xQ, 121), (xQ + 6, 115)], AC, 1.3)
    s.text(xQ, 126.5, "Q1", 7.4, True, AC, "center")
    s.text(xQ + 12, 115, "40 A", 6.2, False, MUTED)
    s.line(xQ, 110, xQ, ptop, AC, 1.3)
    s.line(xQ, ptop, p1x, ptop, AC, 1.3)
    s.dot(p1x, ptop, 1.15, AC)

    # Resistencias entre T1 y T2, a la derecha de K1
    hx, hy, hw = 248, 58, 46
    s.round_box(hx, hy, hw, 20, HexColor("#FFF1E0"), AC, 1.2, 1.8)
    s.poly(
        [(hx + 5, hy + 10), (hx + 10, hy + 16), (hx + 15, hy + 4), (hx + 20, hy + 16),
         (hx + 25, hy + 4), (hx + 30, hy + 16), (hx + 35, hy + 4), (hx + 41, hy + 10)],
        AC,
        1.2,
    )
    s.text(hx + hw / 2, hy + 23, "Resistencias del horno", 7.2, True, AC, "center")
    s.text(hx + hw / 2, hy - 5.6, "hasta 40 A   ·   no conectar al TRIAC", 6.2, False, MUTED, "center")

    s.line(p1x, pbot, p1x, hy + 10, AC, 1.25)
    s.line(p1x, hy + 10, hx, hy + 10, AC, 1.25)
    s.line(hx + hw, hy + 10, 308, hy + 10, AC, 1.25)
    s.line(308, hy + 10, 308, pbot, AC, 1.25)
    s.line(308, pbot, p2x, pbot, AC, 1.25)

    s.line(p2x, ptop, p2x, YN, LINE, 1.3)
    s.dot(p2x, ptop, 1.15, LINE)
    s.dot(p2x, YN, 1.2, LINE)

    # Notas
    s.round_box(320, 48, 86, 82, white, PE, 0.7, 2)
    s.text(324, 121, "Recorrido de corriente", 7.4, True, PE)
    s.text(324, 112, "Bobina", 6.8, True, LINE)
    s.text(324, 105.5, "L → F1 → S1 → A1–A2", 6.3, False, MUTED)
    s.text(324, 99.5, "→ MT2 → BT138 → MT1 → N", 6.3, False, MUTED)
    s.text(324, 90, "Horno", 6.8, True, LINE)
    s.text(324, 83.5, "L → Q1 → L1–T1 → carga", 6.3, False, MUTED)
    s.text(324, 77.5, "→ T2–L2 → N", 6.3, False, MUTED)
    s.text(324, 68, "S1 corta la bobina sin el ESP.", 6.3, False, MUTED)
    s.text(324, 61.5, "K1 abre L y N de las resistencias.", 6.3, False, MUTED)
    s.text(324, 54, "PE al chasis, siempre.", 6.3, False, MUTED)

    c.showPage()


def page2(c: canvas.Canvas):
    wm, hm = landscape(A3)[0] / mm, landscape(A3)[1] / mm
    s = Sch(c)
    header_footer(
        s,
        wm,
        hm,
        "IA Kiln  ·  Materiales y montaje",
        "Circuito de potencia  ·  MOC3020 + BT138 + contactora 40 A",
    )
    s.text(10, 5.5, "Probar primero la bobina sola, con las resistencias del horno desconectadas.", 7.4, False, NAVY)

    headers = ["Ref", "Componente", "Valor / tipo", "Función"]
    rows = [
        ["R1", "Resistencia", "120 Ω  1/4 W", "LED del MOC3020 (~17 mA a 3,3 V)"],
        ["Rpd", "Resistencia", "10 kΩ  1/4 W", "Pull-down: CTRL en 0 durante el reset"],
        ["R2", "Resistencia", "330 Ω  1/4 W", "Puerta del BT138"],
        ["Rs", "Resistencia", "100 Ω  2 W", "Snubber, en serie con Cs entre MT1 y MT2"],
        ["Cs", "Capacitor", "100 nF  X2  275 VAC", "Snubber de la bobina (carga inductiva)"],
        ["U1", "Optoacoplador", "MOC3020", "Aislamiento y disparo aleatorio del TRIAC"],
        ["Q2", "TRIAC", "BT138-600  (TO-220)", "Llave de la bobina. BT137 de reserva"],
        ["RV1", "Varistor", "MOV 14D431", "Absorbe picos al abrir la bobina"],
        ["F1", "Fusible", "1 A gG", "Protege solo el circuito de bobina"],
        ["S1", "Seta NC", "NC  220 VAC", "Emergencia: corta la bobina sin el ESP"],
        ["K1", "Contactora", "Siemens ~40 A, bobina 220 VAC", "2 polos NA cortan L y N de las resistencias"],
        ["Q1", "Termomagnética", "40 A curva C", "Protección de las resistencias"],
        ["—", "Entrada red", "L / N / PE  220 VAC", "Enchufe doméstico; PE al chasis"],
        ["—", "Carga", "Resistencias del horno", "Nunca al BT137/BT138"],
    ]
    col_w = [20, 40, 62, 128]
    x0, y0 = 12, hm - 28
    row_h = 9.4
    s.c.setFillColor(NAVY)
    s.c.rect(x0 * mm, (y0 - 1.4) * mm, sum(col_w) * mm, 8.6 * mm, fill=1, stroke=0)
    x = x0
    for i, head in enumerate(headers):
        s.text(x + 2, y0 + 1.2, head, 8, True, white)
        x += col_w[i]
    y = y0 - 10
    for r, row in enumerate(rows):
        s.c.setFillColor(HexColor("#F3F6FB") if r % 2 == 0 else white)
        s.c.rect(x0 * mm, (y - 2.4) * mm, sum(col_w) * mm, row_h * mm, fill=1, stroke=0)
        x = x0
        for i, cell in enumerate(row):
            s.text(x + 2, y, cell, 7.4, i == 0, LINE)
            x += col_w[i]
        y -= row_h

    s.round_box(268, 22, 140, hm - 44, FILL_NOTE, NAVY, 0.8, 3)
    s.text(273, hm - 32, "Montaje y uso", 11, True, NAVY)
    notes = [
        "Lado izquierdo del MOC: solo 3,3 V. No unir GND del",
        "control con el neutro de 220 V.",
        "",
        "MOC3020 es de 400 V. En 220 VAC el pico es 311 V:",
        "snubber y MOV no son opcionales.",
        "",
        "Pinout BT138 (TO-220, frente, pestaña atrás):",
        "izquierdo MT1 · centro MT2 · derecho G.",
        "Si invertís MT1 y MT2, dispara un solo semiciclo:",
        "zumbido, cierre incompleto, TRIAC caliente.",
        "",
        "Vivo/neutro del enchufe no hace falta identificarlos",
        "para el TRIAC. K1 corta los dos conductores.",
        "",
        "Firmware: solo ON/OFF. Nada de PWM ni recorte de",
        "fase. Ventana 15–20 s, mínimo 2 s por estado.",
        "Poner CTRL en LOW antes de configurar el GPIO.",
        "",
        "Si la bobina fuera 24 VDC este circuito no sirve:",
        "usar MOSFET + diodo flyback.",
        "",
        "S1 y Q1 deben apagar el horno aunque el ESP se",
        "cuelgue o deje CTRL en 1.",
    ]
    yy = hm - 42
    for line in notes:
        s.text(273, yy, line, 7.4, False, LINE)
        yy -= 6.7

    s.round_box(12, 18, 248, 32, HexColor("#FDECEC"), AC, 0.9, 2.2)
    s.text(16, 41, "220 VAC  ·  peligro", 9, True, AC)
    s.text(16, 33.5, "Montar en tablero cerrado. PE al chasis del horno y a K1. No trabajar con la red conectada.", 7.5, False, LINE)
    s.text(16, 26.5, "Probar la bobina sola antes de conectar las resistencias. El BT137 no entra en el esquema; es repuesto del BT138.", 7.5, False, LINE)
    s.text(16, 19.8, "Pines MOC3020: 1 ánodo, 2 cátodo, 4 y 6 fototriac. Pines 3 y 5 sin conectar.", 7.5, False, LINE)

    c.showPage()


def main():
    c = canvas.Canvas(str(OUT), pagesize=landscape(A3))
    c.setTitle("IA Kiln — Circuito de potencia MOC3020 + BT138 + contactora 40 A")
    c.setAuthor("IA Kiln")
    c.setSubject("Esquema de potencia para control de horno cerámico")
    page1(c)
    page2(c)
    c.save()
    print(OUT)


if __name__ == "__main__":
    main()
