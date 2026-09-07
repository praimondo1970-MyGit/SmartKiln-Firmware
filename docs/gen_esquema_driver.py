#!/usr/bin/env python3
"""PDF del esquemático del driver IA Kiln Rev. F (HLK-PM12 + MOC3020 + BT138)."""

from pathlib import Path

from reportlab.lib.pagesizes import A3, landscape
from reportlab.lib.units import mm
from reportlab.lib.colors import HexColor, white, black
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfgen import canvas

OUT = Path(__file__).with_name("IA_Kiln_Driver_Esquema.pdf")
OUT_FINAL = Path(__file__).with_name("IA_Kiln_Driver_Esquema_FINAL.pdf")
OUT_PCB = Path(__file__).resolve().parents[1] / "pcb" / "IA_Kiln_Driver_Esquema.pdf"

NAVY = HexColor("#1B365D")
LINE = HexColor("#1A1A1A")
MUTED = HexColor("#5C677D")
LV = HexColor("#1D4E89")
AC = HexColor("#9B1C1C")
PSU = HexColor("#0F6B4C")
GOLD = HexColor("#8A6D3B")
FILL_PSU = HexColor("#E8F6EF")
FILL_LV = HexColor("#EAF2FA")
FILL_AC = HexColor("#FDF2F2")
FILL_ISO = HexColor("#FBF6E9")
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

    def dot(self, x, y, r=1.15, color=LINE):
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

    def flag(self, x, y, name, color=LINE, side="left"):
        """Etiqueta de red estilo schematic."""
        self.circ(x, y, 1.6, white, color, 1.2)
        if side == "left":
            self.poly([(x - 1.6, y), (x - 8, y + 3.2), (x - 18, y + 3.2), (x - 18, y - 3.2), (x - 8, y - 3.2)], color, 0.9, True, white)
            self.text(x - 13, y - 1.1, name, 7.2, True, color, "center")
        else:
            self.poly([(x + 1.6, y), (x + 8, y + 3.2), (x + 18, y + 3.2), (x + 18, y - 3.2), (x + 8, y - 3.2)], color, 0.9, True, white)
            self.text(x + 13, y - 1.1, name, 7.2, True, color, "center")

    def resistor_h(self, x, y, w=16, h=6, label="", sub="", color=LINE):
        self.c.setFillColor(white)
        self.c.setStrokeColor(color)
        self.c.setLineWidth(1.15)
        self.c.rect(x * mm, (y - h / 2) * mm, w * mm, h * mm, fill=1, stroke=1)
        self.line(x - 5, y, x, y, color)
        self.line(x + w, y, x + w + 5, y, color)
        if label:
            self.text(x + w / 2, y + h / 2 + 1.6, label, 7.2, True, color, "center")
        if sub:
            self.text(x + w / 2, y - h / 2 - 4.2, sub, 6.2, False, MUTED, "center")
        return x - 5, x + w + 5

    def resistor_v(self, x, y, h=14, w=6, label="", sub="", color=LINE):
        self.c.setFillColor(white)
        self.c.setStrokeColor(color)
        self.c.setLineWidth(1.15)
        self.c.rect((x - w / 2) * mm, y * mm, w * mm, h * mm, fill=1, stroke=1)
        self.line(x, y + h, x, y + h + 5, color)
        self.line(x, y, x, y - 5, color)
        if label:
            self.text(x + w / 2 + 2.2, y + h / 2 + 1.0, label, 7.2, True, color)
        if sub:
            self.text(x + w / 2 + 2.2, y + h / 2 - 3.4, sub, 6.1, False, MUTED)
        return y - 5, y + h + 5

    def cap_h(self, x, y, label="", sub="", color=LINE):
        self.line(x - 5, y, x - 1.2, y, color)
        self.line(x - 1.2, y - 6, x - 1.2, y + 6, color, 1.5)
        self.line(x + 1.2, y - 6, x + 1.2, y + 6, color, 1.5)
        self.line(x + 1.2, y, x + 5, y, color)
        if label:
            self.text(x, y + 8.2, label, 7.1, True, color, "center")
        if sub:
            self.text(x, y - 11.2, sub, 6.1, False, MUTED, "center")
        return x - 5, x + 5

    def cap_v_electro(self, x, y_top, h=16, label="", sub="", color=LINE):
        yt = y_top
        yb = y_top - h
        self.line(x, yt, x, yt - 3, color)
        self.line(x - 5, yt - 3, x + 5, yt - 3, color, 1.5)
        self.line(x - 4.2, yt - 5.4, x + 4.2, yt - 5.4, color, 1.5)
        self.line(x, yt - 5.4, x, yb, color)
        self.text(x + 7.5, yt - 2, "+", 8, True, color)
        if label:
            self.text(x + 7.5, yt - 9, label, 7.1, True, color)
        if sub:
            self.text(x + 7.5, yt - 15, sub, 6.1, False, MUTED)
        return yt, yb

    def mov_v(self, x, y_top, y_bot, label="RV", sub="", color=LINE, lab_side="right"):
        mid = (y_top + y_bot) / 2
        self.line(x, y_top, x, mid + 7, color)
        self.line(x, y_bot, x, mid - 7, color)
        self.poly(
            [(x, mid + 7), (x + 7, mid), (x, mid - 7), (x - 7, mid)],
            color,
            1.15,
            True,
            white,
        )
        self.line(x - 8, mid + 6, x + 8, mid - 6, color, 1.15)
        tx = x + 11 if lab_side == "right" else x - 11
        align = "left" if lab_side == "right" else "right"
        if label:
            self.text(tx, mid + 2.4, label, 7.4, True, color, align)
        if sub:
            self.text(tx, mid - 4.6, sub, 6.6, True, color, align)
        return mid


def triac_symbol(s: Sch, x, y, k=1.0, color=AC, gate="right"):
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
    gx = x + 11 * k if gate == "right" else x - 11 * k
    s.line(x, y, gx, y, color, 1.25)
    return {
        "mt2": (x, y + 12 * k),
        "mt1": (x, y - 12 * k),
        "g": (gx, y),
    }


def header_footer(s: Sch, wm, hm, title, subtitle, rev="Rev. F  FINAL"):
    s.c.setFillColor(NAVY)
    s.c.rect(0, (hm - 16) * mm, wm * mm, 16 * mm, fill=1, stroke=0)
    s.text(10, hm - 7.4, title, 15, True, white)
    s.text(10, hm - 13.4, subtitle, 8, False, HexColor("#D5DEEC"))
    s.text(wm - 10, hm - 7.4, rev, 10, True, white, "right")
    s.text(wm - 10, hm - 13.4, "PCB 90 × 74 mm   ·   2026-09-06", 8, False, HexColor("#D5DEEC"), "right")
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
        "IA Kiln  ·  Esquemático del driver  ·  FINAL",
        "PCB Rev. F  ·  HLK-PM12 12 V aislado  ·  MOC3020  ·  BT138 (solo bobina)  ·  J1 12 V  ·  J3 ESP  ·  J2 L/N/A2",
    )
    s.text(
        10,
        8.4,
        "El BT138 no alimenta el HLK: L y N de J2 van fijos a la fuente. A2–N es la única malla conmutada (bobina del contactor).",
        7.3,
        False,
        NAVY,
    )
    s.text(
        10,
        3.6,
        "GND de 12 V / CTRL no se une a N. PE al chasis del horno, no a esta placa. ISO GAP en seda (el aislamiento lo dan U1 y U2).",
        7.3,
        False,
        MUTED,
    )

    # ----- regiones -----
    s.round_box(8, 156, 128, 112, FILL_PSU, PSU, 0.85)
    s.text(12, 260, "0   Fuente  220 VAC → 12 V aislado", 9, True, PSU)
    s.round_box(140, 156, 122, 112, FILL_LV, LV, 0.85)
    s.text(144, 260, "1   Control  GPIO 3,3 V", 9, True, LV)
    s.round_box(266, 156, 146, 112, FILL_AC, AC, 0.85)
    s.text(270, 260, "2   Llave de bobina  220 VAC", 9, True, AC)

    s.c.setDash(1.5, 1.7)
    s.line(205, 164, 205, 254, GOLD, 1.05)
    s.c.setDash()
    s.text(205, 160.2, "ISO GAP", 6.2, True, GOLD, "center")

    # ========== 0. HLK-PM12 ==========
    hx, hy, hw, hh = 42, 178, 56, 58
    s.round_box(hx, hy, hw, hh, white, PSU, 1.2, 2)
    s.c.setDash(0.8, 1.2)
    s.line(hx + hw / 2, hy + 4, hx + hw / 2, hy + hh - 4, GOLD, 0.9)
    s.c.setDash()
    s.text(hx + hw / 2, hy + hh + 2.2, "U2  HLK-PM12", 8, True, PSU, "center")
    s.text(hx + 14, hy + hh - 8, "AC", 6.4, True, AC, "center")
    s.text(hx + hw - 14, hy + hh - 8, "12 V", 6.4, True, PSU, "center")
    s.text(hx + hw / 2, hy + hh / 2 - 2, "3 W  ·  250 mA", 6.1, False, MUTED, "center")
    s.text(hx + hw / 2, hy + hh / 2 - 8, "aisl. 3 kV", 6.1, False, MUTED, "center")

    ac1 = (hx, hy + 42)
    ac2 = (hx, hy + 16)
    v12 = (hx + hw, hy + 42)
    g0v = (hx + hw, hy + 16)
    s.text(hx + 5, ac1[1] - 1, "1", 6.5, True, AC)
    s.text(hx + 5, ac2[1] - 1, "2", 6.5, True, AC)
    s.text(hx + hw - 5, v12[1] - 1, "4 +V", 6.5, True, PSU, "right")
    s.text(hx + hw - 5, g0v[1] - 1, "3 0V", 6.5, True, PSU, "right")

    # flags L / N into HLK
    s.flag(24, ac1[1], "L", AC, "left")
    s.line(24, ac1[1], ac1[0], ac1[1], AC, 1.25)
    s.flag(24, ac2[1], "N", LINE, "left")
    s.line(24, ac2[1], ac2[0], ac2[1], LINE, 1.25)

    # RV2 across L–N of the HLK (labels above, not inside U2)
    s.mov_v(34, ac1[1], ac2[1], "", "", AC)
    s.dot(34, ac1[1], 1.05, AC)
    s.dot(34, ac2[1], 1.05, LINE)
    s.line(24, ac1[1], 34, ac1[1], AC, 1.05)
    s.line(24, ac2[1], 34, ac2[1], LINE, 1.05)
    s.round_box(82, 245, 50, 13, HexColor("#FDECEC"), AC, 1.0, 1.6)
    s.text(107, 253.2, "RV2  MOV 14D431", 7.0, True, AC, "center")
    s.text(107, 247.2, "entre L y N de U2", 6.0, False, MUTED, "center")

    # DC out to J1
    s.line(v12[0], v12[1], 118, v12[1], PSU, 1.25)
    s.flag(118, v12[1], "12V", PSU, "right")
    s.dot(108, v12[1], 1.1, PSU)
    s.cap_h(108, v12[1] - 18, "C12", "100 nF", PSU)
    s.line(108, v12[1], 108, v12[1] - 13, PSU, 1.05)
    s.line(108, v12[1] - 23, 108, g0v[1], LINE, 1.05)
    s.dot(108, g0v[1], 1.1, LINE)

    s.line(g0v[0], g0v[1], 118, g0v[1], LINE, 1.25)
    s.flag(118, g0v[1], "GND", LINE, "right")

    # C13 electro
    s.line(v12[0], v12[1], 124, v12[1], PSU, 1.05)
    s.dot(124, v12[1], 1.05, PSU)
    s.cap_v_electro(124, v12[1], 18, "C13", "220 µF 25 V", PSU)
    s.line(124, v12[1] - 18, 124, g0v[1], LINE, 1.05)
    s.dot(124, g0v[1], 1.05, LINE)

    s.round_box(14, 164, 36, 12, white, PSU, 0.9, 1.4)
    s.text(32, 168.2, "J1  salida 12 V", 6.6, True, PSU, "center")
    s.text(14, 158.5, "al ESP  ·  no unir GND con N", 6.1, False, MUTED)

    # ========== 1. Control ==========
    Yc, Yg = 238, 188
    s.flag(152, Yc, "CTRL", LV, "left")
    s.flag(152, Yg, "GND", LINE, "left")
    s.round_box(148, 164, 40, 12, white, LV, 0.9, 1.4)
    s.text(168, 168.2, "J3  ESP GPIO", 6.6, True, LV, "center")

    s.line(152, Yc, 168, Yc, LV)
    s.resistor_h(173, Yc, 16, 5.8, "R1", "120 Ω", LV)
    s.dot(168, Yc, 1.1, LV)
    s.line(168, Yc, 168, 210, LV, 1.05)
    s.resistor_v(168, 194, 11, 5.6, "Rpd", "10 kΩ", LV)
    s.line(168, 189, 168, Yg, LV, 1.05)
    s.dot(168, Yg, 1.1, LINE)
    s.line(152, Yg, 196, Yg, LINE)

    # MOC3020
    mx, my, mw, mh = 196, 176, 52, 70
    s.round_box(mx, my, mw, mh, white, NAVY, 1.15, 2)
    s.text(mx + mw / 2, my + mh + 2.0, "U1  MOC3020", 8, True, GOLD, "center")

    lx, ly = mx + 14, my + 36
    s.poly([(lx - 5.5, ly - 5.5), (lx + 5.5, ly), (lx - 5.5, ly + 5.5)], LV, 1.15, True, HexColor("#D6E6F7"))
    s.line(lx + 5.8, ly - 5.8, lx + 5.8, ly + 5.8, LV, 1.4)
    s.poly([(lx + 8, ly + 1.5), (lx + 14, ly + 5.5)], GOLD, 1.0)
    s.poly([(lx + 8, ly - 1.2), (lx + 14, ly + 2.8)], GOLD, 1.0)
    s.text(lx - 1, ly + 14, "LED", 6.2, True, LV, "center")
    s.text(mx + 3.5, my + mh - 7, "1 ánodo", 6.3, True, LV)
    s.text(mx + 3.5, my + 4.2, "2 cátodo", 6.3, True, LV)

    pin1 = (mx, my + 54)
    pin2 = (mx, my + 18)
    pin6 = (mx + mw, my + 54)
    pin4 = (mx + mw, my + 18)
    s.line(pin1[0], pin1[1], mx + 8.5, pin1[1], LV, 1.05)
    s.line(mx + 8.5, pin1[1], mx + 8.5, ly + 5.5, LV, 1.05)
    s.line(pin2[0], pin2[1], mx + 8.5, pin2[1], LV, 1.05)
    s.line(mx + 8.5, pin2[1], lx - 5.5, ly, LV, 1.05)

    pt = triac_symbol(s, mx + 38, my + 36, 0.68, AC)
    s.text(mx + 38, my + 56.5, "foto", 6.2, True, AC, "center")
    s.text(mx + mw - 3.5, my + mh - 7, "6 G", 6.5, True, AC, "right")
    s.text(mx + mw - 3.5, my + 4.2, "4 MT", 6.5, True, AC, "right")
    s.line(pin6[0], pin6[1], mx + mw - 7, pin6[1], AC, 1.05)
    s.line(mx + mw - 7, pin6[1], pt["g"][0], pt["g"][1], AC, 1.05)
    s.line(pin4[0], pin4[1], mx + mw - 7, pin4[1], AC, 1.05)
    s.line(mx + mw - 7, pin4[1], pt["mt1"][0], pt["mt1"][1], AC, 1.05)

    s.line(194, Yc, 194, pin1[1], LV)
    s.line(194, pin1[1], pin1[0], pin1[1], LV)
    s.dot(194, pin1[1], 1.05, LV)
    s.line(196, Yg, pin2[0], pin2[1], LINE)

    s.text(222, 164.5, "pines 3 y 5 NC", 6.1, False, MUTED, "center")

    # ========== 2. BT138 ==========
    tx, ty = 314, 210
    t = triac_symbol(s, tx, ty, 1.08, AC, gate="left")
    mt2x, mt2y = t["mt2"]
    mt1x, mt1y = t["mt1"]
    gx, gy = t["g"]
    s.text(368, 260, "Q2  BT138-600  TO-220", 9, True, AC, "center")
    s.text(368, 254.2, "pestaña = MT2", 6.2, False, MUTED, "center")
    s.text(mt2x - 8, mt2y + 1.8, "MT2", 7, True, AC, "right")
    s.text(mt1x - 8, mt1y - 3.4, "MT1", 7, True, AC, "right")
    s.text(gx - 1.4, gy + 2.6, "G", 8, True, AC, "right")
    s.dot(*t["mt2"], 1.15, AC)
    s.dot(*t["mt1"], 1.15, AC)
    s.dot(*t["g"], 1.15, AC)

    # pin 6 → R2 → G (a la izquierda, sin cruzar MT2)
    s.line(pin6[0], pin6[1], 258, pin6[1], AC)
    s.resistor_h(263, pin6[1], 14, 5.4, "R2  330 Ω", "", AC)
    s.line(282, pin6[1], gx, pin6[1], AC)
    s.line(gx, pin6[1], gx, gy, AC)

    # pin 4 → MT2 por encima de R2
    rise = 248
    s.line(pin4[0], pin4[1], 256, pin4[1], AC, 1.15)
    s.line(256, pin4[1], 256, rise, AC, 1.15)
    s.line(256, rise, mt2x, rise, AC, 1.15)
    s.line(mt2x, rise, mt2x, mt2y, AC, 1.15)
    s.dot(mt2x, rise, 1.05, AC)
    s.text(282, rise + 1.8, "pin 4 → MT2", 6.2, True, AC, "center")

    # RV1 + snubber en paralelo MT1–MT2, a la derecha del TRIAC
    vx, sx = 338, 356
    s.line(mt2x, mt2y, 388, mt2y, AC, 1.15)
    s.line(mt1x, mt1y, 400, mt1y, LINE, 1.2)
    s.dot(vx, mt2y, 1.05, AC)
    s.dot(vx, mt1y, 1.05, LINE)
    s.mov_v(vx, mt2y, mt1y, "", "", AC)
    s.round_box(328, mt2y + 3.2, 52, 14, HexColor("#FDECEC"), AC, 1.0, 1.6)
    s.text(354, mt2y + 12.0, "RV1  MOV 14D431", 7.2, True, AC, "center")
    s.text(354, mt2y + 5.6, "entre A2 y N  (MT2–MT1)", 6.1, False, MUTED, "center")
    s.dot(sx, mt2y, 1.05, AC)
    s.dot(sx, mt1y, 1.05, LINE)
    s.line(sx, mt2y, sx, 228, LINE, 1.05)
    s.line(sx - 5, 228, sx + 5, 228, LINE, 1.5)
    s.line(sx - 5, 225.6, sx + 5, 225.6, LINE, 1.5)
    s.line(sx, 225.6, sx, 218, LINE, 1.05)
    s.c.setFillColor(white)
    s.c.setStrokeColor(LINE)
    s.c.setLineWidth(1.15)
    s.c.rect((sx - 3) * mm, 202 * mm, 6 * mm, 16 * mm, fill=1, stroke=1)
    s.line(sx, 202, sx, mt1y, LINE, 1.05)
    s.text(sx, mt1y - 5.2, "Cs X2 + Rs 2 W", 6.1, True, LINE, "center")

    # J2: L no pasa por Q2. N = MT1. A2 baja a la derecha, sin pasar por MT1.
    jx = 400
    s.flag(jx, 246, "L", AC, "right")
    s.flag(jx, mt1y, "N", LINE, "right")
    s.flag(jx, 176, "A2", AC, "right")
    s.round_box(372, 157, 34, 11, white, AC, 0.9, 1.4)
    s.text(389, 160.6, "J2  L N A2", 6.6, True, AC, "center")

    s.line(jx, 246, 370, 246, AC, 1.25)
    s.text(368, 250, "L → U2, no al TRIAC", 6.1, False, MUTED, "right")

    a2x = 388
    s.dot(a2x, mt2y, 1.1, AC)
    s.line(a2x, mt2y, a2x, 176, AC, 1.2)
    s.line(a2x, 176, jx, 176, AC, 1.2)

    s.text(320, 171.2, "Q2 conmuta A2–N  ·  A1 del contactor queda en L", 6.2, True, AC, "center")

    # ========== Tablero ==========
    s.round_box(8, 20, 404, 130, HexColor("#F3F6F1"), HexColor("#1B4332"), 0.85)
    s.text(12, 142, "3   Tablero  (fuera de la PCB)  ·  220 VAC", 9, True, HexColor("#1B4332"))

    YL, YN, YPE = 124, 52, 28
    s.circ(24, YL, 2.3, white, AC, 1.3)
    s.text(30, YL - 1.2, "L  220 VAC", 8, True, AC)
    s.circ(24, YN, 2.3, white, LINE, 1.3)
    s.text(30, YN - 1.2, "N  220 VAC", 8, True, LINE)
    s.circ(24, YPE, 2.3, white, HexColor("#1B4332"), 1.3)
    s.text(30, YPE - 1.2, "PE", 8, True, HexColor("#1B4332"))

    s.line(26.4, YL, 400, YL, AC, 1.55)
    s.line(26.4, YN, 400, YN, LINE, 1.55)
    s.line(26.4, YPE, 70, YPE, HexColor("#1B4332"), 1.3)
    s.line(70, YPE, 70, 24, HexColor("#1B4332"), 1.3)
    s.line(64, 24, 76, 24, HexColor("#1B4332"), 1.5)
    s.text(82, 22.6, "chasis del horno  ·  no a GND de la PCB", 6.4, True, HexColor("#1B4332"))

    # F1
    xf = 88
    s.dot(xf, YL, 1.2, AC)
    s.line(xf, YL, xf, 108, AC, 1.25)
    s.round_box(xf - 9, 94, 18, 14, white, AC, 1.15, 1.6)
    s.line(xf - 6, 101, xf + 6, 101, AC, 1.4)
    s.text(xf, 110.5, "F1", 7.4, True, AC, "center")
    s.text(xf + 12, 99, "2 A T", 6.3, False, MUTED)
    s.line(xf, 94, xf, 82, AC, 1.25)

    # Seta
    s.dot(xf, 82, 1.1, AC)
    s.dot(xf + 16, 82, 1.1, AC)
    s.poly([(xf, 82), (xf + 14.5, 88)], AC, 1.3)
    s.line(xf + 16, 82, xf + 30, 82, AC, 1.2)
    s.text(xf + 8, 91, "S1  seta NC", 7, True, AC, "center")

    # Split to J2-L and A1
    xs = xf + 30
    s.dot(xs, 82, 1.15, AC)
    s.line(xs, 82, 168, 82, AC, 1.2)
    s.round_box(168, 74, 44, 16, white, AC, 1.1, 1.5)
    s.text(190, 84.5, "J2-L  (PCB)", 7.2, True, AC, "center")
    s.text(190, 77.5, "HLK siempre vivo", 6.1, False, MUTED, "center")

    s.line(xs, 82, xs, 68, AC, 1.15)
    s.line(xs, 68, 168, 68, AC, 1.15)
    s.round_box(168, 60, 44, 16, white, NAVY, 1.1, 1.5)
    s.text(190, 70.5, "A1  contactor", 7.2, True, NAVY, "center")
    s.text(190, 63.5, "bobina 220 VAC", 6.1, False, MUTED, "center")

    s.round_box(222, 74, 44, 16, white, LINE, 1.1, 1.5)
    s.text(244, 84.5, "J2-N  (PCB)", 7.2, True, LINE, "center")
    s.text(244, 77.5, "N + MT1 + HLK", 6.1, False, MUTED, "center")
    s.dot(244, YN, 1.15, LINE)
    s.line(244, 74, 244, YN, LINE, 1.2)

    s.round_box(222, 60, 44, 16, white, AC, 1.1, 1.5)
    s.text(244, 70.5, "J2-A2  (PCB)", 7.2, True, AC, "center")
    s.text(244, 63.5, "MT2 → A2", 6.1, False, MUTED, "center")

    # Contactor poles + heaters
    s.round_box(278, 58, 122, 72, white, NAVY, 1.1, 2)
    s.text(339, 122, "K1  Contactora  ~40 A", 8, True, NAVY, "center")
    s.text(339, 115.5, "bobina 220 VAC  ·  resistencias NO van al BT138", 6.1, False, MUTED, "center")
    s.round_box(286, 86, 20, 22, HexColor("#EEF3F9"), LINE, 1.0, 1.2)
    s.text(296, 100, "A1", 7.5, True, LINE, "center")
    s.text(296, 90, "A2", 7.5, True, LINE, "center")
    # A1 (L tras seta) → K1.A1
    s.line(212, 68, 280, 68, AC, 1.15)
    s.line(280, 68, 280, 108, AC, 1.15)
    s.line(280, 108, 296, 108, AC, 1.15)
    s.dot(296, 108, 1.1, AC)
    # K1.A2 → J2-A2 (conmutado por el BT138)
    s.line(296, 86, 310, 86, AC, 1.15)
    s.line(310, 86, 310, 48, AC, 1.15)
    s.line(310, 48, 266, 48, AC, 1.15)
    s.line(266, 48, 266, 60, AC, 1.15)
    s.dot(266, 60, 1.05, AC)
    s.dot(296, 86, 1.1, AC)

    # Wait, A2 of contactor should go to J2-A2. K1 A2 is the coil return to N via triac.
    # A1 from L (after e-stop), A2 to J2-A2.
    # Let me fix: line from J2-A2 box (244, 60) down then to K1 A2 pin.

    # poles
    def pole(px, top, bot, up, dn):
        s.dot(px, top, 1.1, AC)
        s.dot(px, bot, 1.1, AC)
        s.line(px, top, px, top - 4, AC, 1.2)
        s.line(px, bot, px, bot + 3, AC, 1.2)
        s.poly([(px, top - 4), (px + 8, bot + 3.5)], AC, 1.35)
        s.text(px - 2, top + 2.2, up, 6.8, True, AC, "right")
        s.text(px - 2, bot - 5.0, dn, 6.8, True, AC, "right")

    p1, p2 = 330, 368
    pole(p1, 104, 88, "L1", "T1")
    pole(p2, 104, 88, "L2", "T2")
    s.line(p1, 104, p1, YL, AC, 1.2)
    s.dot(p1, YL, 1.15, AC)
    s.line(p2, 104, p2, YN, LINE, 1.2)
    s.dot(p2, YN, 1.15, LINE)

    s.round_box(318, 62, 72, 16, HexColor("#FFF1E0"), AC, 1.1, 1.4)
    s.poly(
        [(324, 70), (332, 74), (340, 66), (348, 74), (356, 66), (364, 74), (372, 70), (384, 70)],
        AC,
        1.15,
    )
    s.text(354, 80.5, "Resistencias horno", 6.5, True, AC, "center")
    s.line(p1, 88, p1, 70, AC, 1.1)
    s.line(p1, 70, 318, 70, AC, 1.1)
    s.line(390, 70, 390, 88, AC, 1.1)
    s.line(390, 88, p2, 88, AC, 1.1)

    c.showPage()


def page2(c: canvas.Canvas):
    wm, hm = landscape(A3)[0] / mm, landscape(A3)[1] / mm
    s = Sch(c)
    header_footer(
        s,
        wm,
        hm,
        "IA Kiln  ·  Lista de materiales y cableado  ·  FINAL",
        "Driver PCB Rev. F  ·  HLK-PM12 + MOC3020 + BT138  ·  solo bobina del contactor",
    )
    s.text(10, 5.5, "Probar la bobina sola, con las resistencias del horno desconectadas.", 7.4, False, NAVY)

    headers = ["Ref", "Componente", "Valor / tipo", "Función"]
    rows = [
        ["U2", "Fuente AC/DC", "HLK-PM12  12 V / 3 W", "220 VAC → 12 V aislado para el ESP"],
        ["C12", "Capacitor", "100 nF", "Desacople de 12 V"],
        ["C13", "Electrolítico", "220 µF  25 V", "Filtro de salida HLK (datasheet)"],
        ["RV2", "Varistor 14D431", "MOV  Ø14 mm  430 V", "Entre L y N de U2 (picos de red a la fuente)"],
        ["J1", "Bornes 3,5 mm", "2 polos  12 V / GND", "Salida 12 V al ESP (aislado)"],
        ["J3", "Bornes 3,5 mm", "2 polos  CTRL / GND", "GPIO 3,3 V activo HIGH"],
        ["R1", "Resistencia", "120 Ω  1/4 W", "LED del MOC3020 (~17 mA a 3,3 V)"],
        ["Rpd", "Resistencia", "10 kΩ  1/4 W", "Pull-down de CTRL en el reset"],
        ["U1", "Optoacoplador", "MOC3020  DIP-6", "Aislamiento y disparo del TRIAC"],
        ["R2", "Resistencia", "330 Ω  1/4 W", "Puerta del BT138"],
        ["Q2", "TRIAC", "BT138-600  TO-220", "Llave de la bobina. Pestaña = MT2"],
        ["RV1", "Varistor 14D431", "MOV  Ø14 mm  430 V", "Entre A2 y N, junto al BT138 (picos de bobina)"],
        ["Cs", "Capacitor X2", "100 nF  275 VAC  P=15 mm", "Snubber, en serie con Rs"],
        ["Rs", "Resistencia", "100 Ω  2 W  P=20 mm", "Snubber entre MT1 y MT2"],
        ["J2", "Bornes 7,62 mm", "3 polos  L / N / A2", "Red y bobina. L no pasa por Q2"],
        ["F1", "Fusible (tablero)", "2 A T  250 V", "Inrush del HLK + bobina"],
        ["S1", "Seta NC (tablero)", "NC  220 VAC", "Corta L a J2-L y a A1"],
        ["K1", "Contactora", "Siemens ~40 A, bob. 220 V", "Resistencias del horno. No al TRIAC"],
    ]
    col_w = [18, 42, 58, 130]
    x0, y0 = 12, hm - 28
    row_h = 8.6
    s.c.setFillColor(NAVY)
    s.c.rect(x0 * mm, (y0 - 1.2) * mm, sum(col_w) * mm, 8.2 * mm, fill=1, stroke=0)
    x = x0
    for i, head in enumerate(headers):
        s.text(x + 1.6, y0 + 1.0, head, 8, True, white)
        x += col_w[i]
    y = y0 - 9.2
    highlight = {"RV1", "RV2"}
    for r, row in enumerate(rows):
        fill = HexColor("#FDECEC") if row[0] in highlight else (HexColor("#F3F6FB") if r % 2 == 0 else white)
        s.c.setFillColor(fill)
        s.c.rect(x0 * mm, (y - 2.2) * mm, sum(col_w) * mm, row_h * mm, fill=1, stroke=0)
        x = x0
        for i, cell in enumerate(row):
            s.text(x + 1.6, y, cell, 7.1, i == 0, AC if row[0] in highlight else LINE)
            x += col_w[i]
        y -= row_h

    s.round_box(268, 20, 144, hm - 42, FILL_NOTE, NAVY, 0.8, 3)
    s.text(274, hm - 30, "Pinout y reglas", 11, True, NAVY)
    notes = [
        "J1  12 V · GND     →  alimentación ESP",
        "J3  CTRL · GND    →  GPIO 3,3 V",
        "J2  L · N · A2    →  red y bobina",
        "",
        "BT138 (frente, pestaña atrás):",
        "izq. MT1 = N   ·   centro MT2 = A2",
        "der. G = R2    ·   pestaña = MT2",
        "",
        "MOC3020: 1 ánodo, 2 cátodo,",
        "4 y 6 fototriac.  3 y 5 NC.",
        "",
        "HLK-PM12: 1–2 AC, 3 −V0, 4 +V0.",
        "Cuerpo a caballo del ISO GAP.",
        "",
        "MOV 14D431  (dos en la PCB):",
        "RV2  entre L y N de U2 (fuente).",
        "    seda junto a los pines AC del HLK.",
        "RV1  entre A2 y N (MT2–MT1).",
        "    seda junto al BT138, con Cs y Rs.",
        "",
        "Firmware: solo ON/OFF de CTRL.",
        "Nada de PWM ni recorte de fase.",
        "Ventana 15–20 s, mínimo 2 s/estado.",
        "CTRL en LOW al configurar el GPIO.",
        "",
        "HLK-PM12 = 12 V / 250 mA. El panel",
        "ESP32-4827S043 suele ser 5 V: hace",
        "falta buck 12→5 V o un HLK de 5 V.",
        "",
        "S1 debe apagar aunque el ESP se",
        "cuelgue con CTRL en 1.",
    ]
    yy = hm - 40
    for line in notes:
        s.text(274, yy, line, 7.3, False, LINE)
        yy -= 6.35

    s.round_box(12, 18, 248, 36, HexColor("#FDECEC"), AC, 0.9, 2.2)
    s.text(16, 46, "220 VAC  ·  peligro", 9, True, AC)
    s.text(16, 38.2, "Montar en tablero cerrado. PE al chasis. No trabajar con la red conectada.", 7.4, False, LINE)
    s.text(16, 30.8, "El TRIAC abre A2–N (neutro de la bobina). A1 queda en L. Las resistencias van por K1 (40 A).", 7.4, False, LINE)
    s.text(16, 23.4, "Si invertís MT1/MT2: un solo semiciclo, zumbido y TRIAC caliente.", 7.4, False, LINE)

    c.showPage()


def render_preview(pdf_path: Path):
    try:
        import pypdfium2 as pdfium
    except ImportError:
        return
    doc = pdfium.PdfDocument(str(pdf_path))
    for i, name in enumerate(("IA_Kiln_Driver_Esquema_p1.png", "IA_Kiln_Driver_Esquema_p2.png")):
        if i >= len(doc):
            break
        img = doc[i].render(scale=1.6).to_pil()
        prev = pdf_path.with_name(name)
        img.save(prev, "PNG")
        print(prev)


def main():
    c = canvas.Canvas(str(OUT), pagesize=landscape(A3))
    c.setTitle("IA Kiln — Esquemático driver Rev. F FINAL")
    c.setAuthor("IA Kiln")
    c.setSubject("PCB Rev. F: HLK-PM12 + MOC3020 + BT138, bobina del contactor")
    page1(c)
    page2(c)
    c.save()
    data = OUT.read_bytes()
    OUT_FINAL.write_bytes(data)
    for dest in (OUT_PCB, Path(__file__).resolve().parents[1] / "pcb" / "IA_Kiln_Driver_Esquema_FINAL.pdf"):
        try:
            dest.write_bytes(data)
            print(dest)
        except OSError as exc:
            print("skip", dest, exc)
    print(OUT)
    print(OUT_FINAL)
    render_preview(OUT)


if __name__ == "__main__":
    main()
