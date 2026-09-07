#!/usr/bin/env python3
"""Esquema PDF + montaje 3D del prototipo casero MOC3020 + BT138/BTA."""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont
from reportlab.lib.pagesizes import A4, landscape
from reportlab.lib.units import mm
from reportlab.lib.colors import HexColor, white
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfgen import canvas

DOCS = Path(__file__).resolve().parent
PDF = DOCS / "Prueba_MOC_BTA_Esquema.pdf"
PNG = DOCS / "Prueba_MOC_BTA_Montaje.png"
PCB = DOCS.parent / "pcb"

NAVY = HexColor("#1B365D")
LINE = HexColor("#1A1A1A")
MUTED = HexColor("#5C677D")
LV = HexColor("#1D4E89")
AC = HexColor("#9B1C1C")
GOLD = HexColor("#8A6D3B")
FILL_LV = HexColor("#EAF2FA")
FILL_ISO = HexColor("#FBF6E9")
FILL_AC = HexColor("#FDF2F2")
FILL_NOTE = HexColor("#F7F7F4")


def register_fonts():
    for regular, bold in (
        (r"C:\Windows\Fonts\arial.ttf", r"C:\Windows\Fonts\arialbd.ttf"),
        (r"C:\Windows\Fonts\segoeui.ttf", r"C:\Windows\Fonts\segoeuib.ttf"),
    ):
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

    def round_box(self, x, y, w, h, fill, stroke, sw=0.7, r=2.4):
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
        self.circ(x, y, 1.5, white, color, 1.15)
        if side == "left":
            self.poly(
                [(x - 1.5, y), (x - 7, y + 3), (x - 17, y + 3), (x - 17, y - 3), (x - 7, y - 3)],
                color, 0.9, True, white,
            )
            self.text(x - 12, y - 1.0, name, 7, True, color, "center")
        else:
            self.poly(
                [(x + 1.5, y), (x + 7, y + 3), (x + 17, y + 3), (x + 17, y - 3), (x + 7, y - 3)],
                color, 0.9, True, white,
            )
            self.text(x + 12, y - 1.0, name, 7, True, color, "center")

    def resistor_h(self, x, y, w=16, h=5.6, label="", sub="", color=LINE):
        self.c.setFillColor(white)
        self.c.setStrokeColor(color)
        self.c.setLineWidth(1.1)
        self.c.rect(x * mm, (y - h / 2) * mm, w * mm, h * mm, fill=1, stroke=1)
        self.line(x - 5, y, x, y, color)
        self.line(x + w, y, x + w + 5, y, color)
        if label:
            self.text(x + w / 2, y + h / 2 + 1.5, label, 7, True, color, "center")
        if sub:
            self.text(x + w / 2, y - h / 2 - 4.2, sub, 6.2, False, MUTED, "center")

    def resistor_v(self, x, y, h=12, w=5.4, label="", sub="", color=LINE):
        self.c.setFillColor(white)
        self.c.setStrokeColor(color)
        self.c.setLineWidth(1.1)
        self.c.rect((x - w / 2) * mm, y * mm, w * mm, h * mm, fill=1, stroke=1)
        self.line(x, y + h, x, y + h + 4, color)
        self.line(x, y, x, y - 4, color)
        if label:
            self.text(x + w / 2 + 1.8, y + h / 2 + 1.0, label, 7, True, color)
        if sub:
            self.text(x + w / 2 + 1.8, y + h / 2 - 3.0, sub, 6.1, False, MUTED)


def triac(s: Sch, x, y, k=1.0, color=AC, gate="right"):
    s.line(x, y + 12 * k, x, y - 12 * k, color, 1.35)
    s.poly([(x, y + 1.3 * k), (x - 6.5 * k, y + 9 * k), (x + 6.5 * k, y + 9 * k)], color, 1.2, True, white)
    s.poly([(x, y - 1.3 * k), (x - 6.5 * k, y - 9 * k), (x + 6.5 * k, y - 9 * k)], color, 1.2, True, white)
    gx = x + 10 * k if gate == "right" else x - 10 * k
    s.line(x, y, gx, y, color, 1.2)
    return {"mt2": (x, y + 12 * k), "mt1": (x, y - 12 * k), "g": (gx, y)}


def lamp_h(s: Sch, x, y):
    s.circ(x, y, 6.2, white, AC, 1.2)
    s.line(x - 3.8, y - 3.8, x + 3.8, y + 3.8, AC, 1.0)
    s.line(x - 3.8, y + 3.8, x + 3.8, y - 3.8, AC, 1.0)
    s.line(x - 6.2, y, x - 10, y, AC, 1.15)
    s.line(x + 6.2, y, x + 10, y, AC, 1.15)
    s.text(x, y - 10.2, "lámpara  25–40 W", 6.0, True, AC, "center")


def fuse_h(s: Sch, x, y, w=12):
    s.line(x, y, x + 2, y, AC, 1.15)
    s.c.setFillColor(white)
    s.c.setStrokeColor(AC)
    s.c.setLineWidth(1.15)
    s.c.roundRect((x + 2) * mm, (y - 2.6) * mm, (w - 4) * mm, 5.2 * mm, 1.1 * mm, fill=1, stroke=1)
    s.line(x + 2, y, x + w - 2, y, AC, 1.05)
    s.line(x + w - 2, y, x + w, y, AC, 1.15)
    s.text(x + w / 2, y + 4.2, "F1  2 A T", 6.0, True, AC, "center")


def mov_v(s: Sch, x, y1, y2):
    top, bot = max(y1, y2), min(y1, y2)
    body_h = 16
    by = (top + bot) / 2
    s.line(x, top, x, by + body_h / 2, AC, 1.15)
    s.c.setFillColor(white)
    s.c.setStrokeColor(AC)
    s.c.setLineWidth(1.15)
    s.c.rect((x - 3.4) * mm, (by - body_h / 2) * mm, 6.8 * mm, body_h * mm, fill=1, stroke=1)
    s.line(x - 3.4, by - body_h / 2, x + 3.4, by + body_h / 2, AC, 1.15)
    s.line(x, by - body_h / 2, x, bot, AC, 1.15)
    return by


def snubber_v(s: Sch, x, y1, y2):
    top, bot = max(y1, y2), min(y1, y2)
    cap_y = top - 7.5
    s.line(x, top, x, cap_y + 1.4, AC, 1.15)
    s.line(x - 4.2, cap_y + 1.4, x + 4.2, cap_y + 1.4, LINE, 1.6)
    s.line(x - 4.2, cap_y - 1.4, x + 4.2, cap_y - 1.4, LINE, 1.6)
    ry = bot + 4
    s.line(x, cap_y - 1.4, x, ry + 12, AC, 1.1)
    s.c.setFillColor(white)
    s.c.setStrokeColor(LINE)
    s.c.setLineWidth(1.1)
    s.c.rect((x - 2.7) * mm, ry * mm, 5.4 * mm, 12 * mm, fill=1, stroke=1)
    s.line(x, ry, x, bot, AC, 1.15)
    return cap_y, ry + 6


def write_pdf():
    wm, hm = landscape(A4)[0] / mm, landscape(A4)[1] / mm
    c = canvas.Canvas(str(PDF), pagesize=landscape(A4))
    s = Sch(c)

    s.c.setFillColor(NAVY)
    s.c.rect(0, (hm - 15) * mm, wm * mm, 15 * mm, fill=1, stroke=0)
    s.text(9, hm - 6.6, "IA Kiln  ·  Prototipo casero  ·  MOC3020 + BT138 / BTA", 13, True, white)
    s.text(9, hm - 12.4, "Cs = 2J104K 100 nF 630 V  ·  no usar cerámicos 473/Z5  ·  ON/OFF, sin PWM", 7.4, False, HexColor("#D5DEEC"))
    s.text(wm - 9, hm - 6.6, "Rev. A2", 9, True, white, "right")
    s.text(wm - 9, hm - 12.4, "piezas reales  ·  2026-09-06", 7.4, False, HexColor("#D5DEEC"), "right")

    s.c.setFillColor(HexColor("#F3F4F6"))
    s.c.rect(0, 0, wm * mm, 12 * mm, fill=1, stroke=0)
    s.text(9, 4.6, "PE al chasis, nunca al GND del ESP.  Fusible 1–2 A T en L.  220 V fuera de protoboard.", 7.2, False, MUTED)

    # bloques
    s.round_box(7, 118, 88, 68, FILL_LV, LV, 0.9)
    s.text(11, 178, "1   Control  3,3 V", 9, True, LV)
    s.round_box(98, 118, 78, 68, FILL_ISO, GOLD, 0.9)
    s.text(102, 178, "2   Aislamiento", 9, True, GOLD)
    s.round_box(179, 118, 111, 68, FILL_AC, AC, 0.9)
    s.text(183, 178, "3   Llave  220 VAC", 9, True, AC)

    Yc, Yg = 166, 140

    s.flag(22, Yc, "GPIO", LV, "left")
    s.flag(22, Yg, "GND", LINE, "left")
    s.line(22, Yc, 36, Yc, LV, 1.15)
    s.resistor_h(41, Yc, 16, 5.6, "R1", "120 Ω", LV)
    s.dot(36, Yc, 1.1, LV)
    s.line(36, Yc, 36, 154, LV, 1.05)
    s.resistor_v(36, 142, 10, 5.2, "Rpd", "10 kΩ", LV)
    s.line(36, 142, 36, Yg, LV, 1.05)
    s.dot(36, Yg, 1.1, LINE)
    s.line(22, Yg, 98, Yg, LINE, 1.15)
    s.line(62, Yc, 98, Yc, LV, 1.15)

    # MOC
    mx, my, mw, mh = 108, 128, 56, 50
    s.round_box(mx, my, mw, mh, white, NAVY, 1.15, 2)
    s.text(mx + mw / 2, my + mh + 1.6, "U1  MOC3020", 8, True, GOLD, "center")
    s.line(mx + mw / 2, my + 4, mx + mw / 2, my + mh - 4, GOLD, 0.9)
    s.c.setDash(1.5, 1.4)
    s.line(mx + mw / 2, my + 4, mx + mw / 2, my + mh - 4, GOLD, 0.7)
    s.c.setDash()
    s.text(mx + 12, my + mh - 7, "LED", 6.2, True, LV, "center")
    s.text(mx + mw - 12, my + mh - 7, "foto", 6.2, True, AC, "center")

    p1 = (mx, Yc)
    p2 = (mx, Yg)
    p6 = (mx + mw, Yc)
    p4 = (mx + mw, Yg + 4)
    s.line(98, Yc, p1[0], p1[1], LV, 1.15)
    s.line(98, Yg, p2[0], p2[1], LINE, 1.15)
    s.text(mx + 3, Yc + 2.2, "1 ánodo", 6.1, True, LV)
    s.text(mx + 3, Yg + 2.2, "2 cátodo", 6.1, True, LV)
    s.text(mx + mw - 3, Yc + 2.2, "6 G", 6.3, True, AC, "right")
    s.text(mx + mw - 3, p4[1] + 2.2, "4 MT", 6.3, True, AC, "right")
    s.text(mx + mw / 2, my + 6.5, "3 y 5 NC", 6.0, False, MUTED, "center")

    # BTA: puerta a la izquierda (hacia el MOC). Protecciones // Q2. Lámpara en serie en L.
    t = triac(s, 212, 147, 1.05, AC, gate="left")
    mt2x, mt2y = t["mt2"]
    mt1x, mt1y = t["mt1"]
    gx, gy = t["g"]
    s.text(248, 178, "Q2  BT138 / BTA16  TO-220", 7.4, True, AC, "center")
    s.text(mt2x - 7.2, mt2y + 1.8, "MT2", 6.6, True, AC, "right")
    s.text(mt1x - 7.2, mt1y - 3.4, "MT1", 6.6, True, AC, "right")
    s.text(gx - 1.2, gy + 2.4, "G", 7.2, True, AC, "right")
    s.dot(mt2x, mt2y, 1.15, AC)
    s.dot(mt1x, mt1y, 1.15, AC)
    s.dot(gx, gy, 1.15, AC)

    # pin 6 → R2 → G  (horizontal a 166, baja por la pata G, no cruza MT2)
    s.line(p6[0], p6[1], 171, p6[1], AC, 1.15)
    s.resistor_h(176, p6[1], 16, 5.0, "R2  330 Ω", "", AC)
    s.line(197, p6[1], gx, p6[1], AC, 1.15)
    s.line(gx, p6[1], gx, gy, AC, 1.15)

    # pin 4 → MT2  (pasa por encima de R2, llega a la pata de arriba)
    rise = 174.5
    s.line(p4[0], p4[1], 169, p4[1], AC, 1.15)
    s.line(169, p4[1], 169, rise, AC, 1.15)
    s.line(169, rise, mt2x, rise, AC, 1.15)
    s.line(mt2x, rise, mt2x, mt2y, AC, 1.15)
    s.dot(mt2x, rise, 1.05, AC)
    s.text(188, rise + 1.6, "pin 4 → MT2", 6.0, True, AC, "center")

    # N/MT1 continuo. MT2 se corta en la lámpara: un trazo hasta L cortocircuita F1.
    end_x = 284
    lamp_l, lamp_r = 254, 274
    s.line(mt2x, mt2y, lamp_l, mt2y, AC, 1.2)
    s.line(mt1x, mt1y, end_x, mt1y, LINE, 1.2)

    # snubber real: Cs 2J104K + Rs, en paralelo con Q2. Sin MOV. Sin 473/Z5.
    sx = 226
    s.dot(sx, mt2y, 1.15, AC)
    s.dot(sx, mt1y, 1.15, LINE)
    cap_y, rmid = snubber_v(s, sx, mt2y, mt1y)
    s.text(sx + 6.4, cap_y + 1.4, "Cs  2J104K", 6.4, True, AC)
    s.text(sx + 6.4, cap_y - 3.2, "100 nF  630 V", 5.8, False, MUTED)
    s.text(sx + 6.4, rmid + 1.4, "Rs  100 Ω  2 W", 6.4, True, LINE)

    lamp_h(s, 264, mt2y)
    s.dot(lamp_l, mt2y, 1.1, AC)
    fuse_h(s, lamp_r, mt2y, 10)
    s.line(lamp_r + 10, mt2y, end_x, mt2y, AC, 1.2)
    s.circ(end_x, mt2y, 2.0, white, AC, 1.2)
    s.text(end_x, mt2y + 3.8, "L", 8, True, AC, "center")
    s.circ(end_x, mt1y, 2.0, white, LINE, 1.2)
    s.text(end_x, mt1y - 6.6, "N", 8, True, LINE, "center")

    s.text(235, 121.5, "Serie:  L → F1 → lámpara → MT2 → Q2 → MT1 → N     ·     // Q2:  Cs 2J104K + Rs     ·     no 473/Z5", 6.1, False, MUTED, "center")

    # pinouts
    s.round_box(7, 16, 138, 96, FILL_NOTE, NAVY, 0.7)
    s.text(12, 102, "Montaje físico", 9, True, NAVY)
    s.text(12, 96, "Dos islas. El MOC a caballo del aire. 220 V nunca en protoboard.", 6.4, False, MUTED)

    s.round_box(14, 52, 52, 40, white, LV, 0.8)
    s.text(40, 85, "MOC3020  DIP-6", 7.2, True, LV, "center")
    s.text(40, 80, "muesca arriba", 6.0, False, MUTED, "center")
    s.c.setFillColor(HexColor("#1C1C1C"))
    s.c.roundRect(28 * mm, 60 * mm, 24 * mm, 18 * mm, 1.2 * mm, fill=1, stroke=0)
    s.c.setFillColor(HexColor("#EEE8D0"))
    s.c.circle(40 * mm, 76.2 * mm, 1.4 * mm, fill=1, stroke=0)
    for i, name in enumerate(["1 ánodo", "2 cátodo", "3 NC"]):
        yy = 74 - i * 6
        s.c.setFillColor(HexColor("#C8C8C8"))
        s.c.rect(24.5 * mm, yy * mm, 4 * mm, 2.2 * mm, fill=1, stroke=0)
        s.text(23.5, yy - 0.2, name, 5.8, True, LV, "right")
    for i, name in enumerate(["6 G", "5 NC", "4 MT"]):
        yy = 74 - i * 6
        s.c.setFillColor(HexColor("#C8C8C8"))
        s.c.rect(51.5 * mm, yy * mm, 4 * mm, 2.2 * mm, fill=1, stroke=0)
        s.text(56.5, yy - 0.2, name, 5.8, True, AC)
    s.text(40, 55, "izq. = 3,3 V    der. = 220 V", 6.1, False, MUTED, "center")

    s.round_box(78, 52, 58, 40, white, AC, 0.8)
    s.text(107, 85, "BT138 / BTA  TO-220", 7.2, True, AC, "center")
    s.text(107, 80, "frente, pestaña atrás", 6.0, False, MUTED, "center")
    s.c.setFillColor(HexColor("#2E2E2E"))
    s.c.roundRect(97 * mm, 64 * mm, 20 * mm, 14 * mm, 0.8 * mm, fill=1, stroke=0)
    s.c.setFillColor(HexColor("#B8BCC0"))
    s.c.rect(103 * mm, 77.2 * mm, 8 * mm, 2.8 * mm, fill=1, stroke=0)
    for i, name in enumerate(["MT1", "MT2", "G"]):
        px = 100 + i * 7
        s.c.setFillColor(HexColor("#C8C8C8"))
        s.c.rect(px * mm, 58 * mm, 3 * mm, 7 * mm, fill=1, stroke=0)
        s.text(px + 1.5, 53.5, name, 6.2, True, LINE, "center")
    s.text(107, 48.5, "Pestaña = MT2. No invertir MT1/MT2.", 6.0, False, MUTED, "center")

    s.round_box(151, 16, 139, 96, FILL_NOTE, AC, 0.7)
    s.text(156, 102, "Piezas de esta prueba", 9, True, AC)
    parts = [
        ("U1", "MOC3020  DIP-6", "a caballo del aire"),
        ("Q2", "BT138 / BTA  TO-220", "frente: MT1 · MT2 · G"),
        ("R1", "120 Ω", "GPIO → pin 1"),
        ("Rpd", "10 kΩ", "GPIO – GND"),
        ("R2", "330 Ω", "pin 6 → G"),
        ("Rs", "100 Ω  2 W", "en serie con Cs"),
        ("Cs", "2J104K  100 nF  630 V", "verde rectangular"),
        ("F1", "2 A T", "en L, antes de la lámpara"),
    ]
    yy = 93.5
    for ref, val, note in parts:
        s.text(158, yy, ref, 6.4, True, AC)
        s.text(172, yy, val, 6.4, True, LINE)
        s.text(232, yy, note, 6.2, False, MUTED)
        yy -= 7.2
    s.text(156, 23.2, "No usar discos 473/Z5, 25 V, 100 V, 39 pF ni 47 pF en 220 V.", 6.3, True, AC)
    s.text(156, 17.2, "GPIO HIGH.  No unir GND con N.  Lámpara primero, bobina después.  Sin PWM.", 6.2, False, MUTED)

    c.save()
    PCB.mkdir(exist_ok=True)
    try:
        (PCB / PDF.name).write_bytes(PDF.read_bytes())
    except OSError:
        pass


def font(size, bold=False):
    path = r"C:\Windows\Fonts\arialbd.ttf" if bold else r"C:\Windows\Fonts\arial.ttf"
    try:
        return ImageFont.truetype(path, size)
    except OSError:
        return ImageFont.load_default()


def iso(x, y, z, ox=420, oy=980, sx=1.05, sy=0.62):
    """Isométrico: X derecha, Y profundidad, Z arriba. y pantalla crece hacia abajo."""
    px = ox + (x - y) * sx
    py = oy - (x + y) * sy - z
    return int(px), int(py)


def quad(d, pts, fill, outline=(30, 30, 30), width=2):
    d.polygon(pts, fill=fill, outline=outline)
    d.line(pts + [pts[0]], fill=outline, width=width)


def box3(d, x, y, z, dx, dy, dz, top, left, right, outline=(40, 40, 40)):
    a = iso(x, y, z + dz)
    b = iso(x + dx, y, z + dz)
    c = iso(x + dx, y + dy, z + dz)
    e = iso(x, y + dy, z + dz)
    a0 = iso(x, y, z)
    b0 = iso(x + dx, y, z)
    c0 = iso(x + dx, y + dy, z)
    e0 = iso(x, y + dy, z)
    quad(d, [b, c, c0, b0], right, outline)
    quad(d, [a, e, e0, a0], left, outline)
    quad(d, [a, b, c, e], top, outline)
    return {"a": a, "b": b, "c": c, "e": e}


def write_png():
    W, H = 2400, 1480
    img = Image.new("RGB", (W, H), (236, 232, 224))
    d = ImageDraw.Draw(img)
    F16, F18, F22 = font(16), font(18), font(22)
    F20B, F28B, F36B, F44B = font(20, True), font(28, True), font(36, True), font(44, True)

    d.rectangle((0, 0, W, 92), fill=(27, 54, 93))
    d.text((40, 22), "IA Kiln  ·  Montaje del prototipo casero", font=F44B, fill=(255, 255, 255))
    d.text((40, 64), "MOC3020 a caballo  ·  3,3 V a la izquierda  ·  220 VAC a la derecha  ·  lámpara de prueba", font=F18, fill=(213, 222, 236))

    # mesa
    table = box3(d, 0, 0, 0, 980, 420, 28, (186, 150, 108), (150, 112, 78), (120, 88, 58), (90, 60, 40))

    # proto LV
    bb = box3(d, 70, 70, 28, 320, 260, 22, (248, 248, 250), (210, 212, 216), (188, 190, 196))
    # agujeros proto
    for i in range(8):
        for j in range(6):
            p = iso(95 + i * 34, 95 + j * 34, 52)
            d.ellipse((p[0] - 4, p[1] - 3, p[0] + 4, p[1] + 3), fill=(40, 44, 50))

    # placa HV
    hv = box3(d, 520, 70, 28, 400, 280, 10, (168, 122, 72), (140, 98, 58), (118, 82, 48))
    for i in range(10):
        for j in range(7):
            p = iso(545 + i * 34, 95 + j * 32, 40)
            d.ellipse((p[0] - 3, p[1] - 2, p[0] + 3, p[1] + 2), fill=(70, 50, 30))

    # ISO gap band
    g0 = iso(400, 60, 30)
    g1 = iso(510, 60, 30)
    g2 = iso(510, 360, 30)
    g3 = iso(400, 360, 30)
    d.polygon([g0, g1, g2, g3], fill=(251, 246, 233))
    d.text(iso(430, 200, 80), "ISO GAP", font=F20B, fill=(138, 109, 59))

    # MOC DIP straddling
    moc = box3(d, 360, 140, 50, 180, 70, 18, (32, 32, 34), (20, 20, 22), (48, 48, 52))
    notch = iso(370, 155, 70)
    d.ellipse((notch[0] - 8, notch[1] - 6, notch[0] + 8, notch[1] + 6), fill=(236, 232, 208))
    d.text(iso(410, 155, 78), "MOC3020", font=F20B, fill=(255, 255, 255))

    # pins left 1 2 3
    for i, name in enumerate(["1 ánodo", "2 cátodo", "3 NC"]):
        p = iso(358, 152 + i * 20, 50)
        d.rectangle((p[0] - 10, p[1] - 4, p[0] + 2, p[1] + 4), fill=(196, 196, 196), outline=(40, 40, 40))
        d.text((p[0] - 14, p[1]), name, font=F16, fill=(29, 78, 137), anchor="rm")
    for i, name in enumerate(["6 G", "5 NC", "4 MT"]):
        p = iso(542, 152 + i * 20, 50)
        d.rectangle((p[0] - 2, p[1] - 4, p[0] + 10, p[1] + 4), fill=(196, 196, 196), outline=(40, 40, 40))
        d.text((p[0] + 14, p[1]), name, font=F16, fill=(155, 28, 28), anchor="lm")

    # resistors LV
    r1a, r1b = iso(160, 150, 55), iso(250, 150, 55)
    d.line([r1a, r1b], fill=(29, 78, 137), width=6)
    mid = ((r1a[0] + r1b[0]) // 2, (r1a[1] + r1b[1]) // 2)
    d.rectangle((mid[0] - 22, mid[1] - 10, mid[0] + 22, mid[1] + 10), fill=(196, 154, 90), outline=(80, 50, 20))
    d.text((mid[0], mid[1] - 18), "R1 120 Ω", font=F16, fill=(29, 78, 137), anchor="mm")

    # TO-220
    body = box3(d, 680, 150, 40, 28, 50, 70, (36, 36, 38), (24, 24, 26), (55, 55, 58))
    tab = box3(d, 686, 142, 108, 16, 18, 8, (176, 180, 184), (150, 154, 158), (130, 134, 138))
    for i, name in enumerate(["MT1", "MT2", "G"]):
        p = iso(686 + i * 8, 175, 40)
        d.rectangle((p[0] - 3, p[1], p[0] + 3, p[1] + 22), fill=(200, 200, 200), outline=(40, 40, 40))
        d.text((p[0], p[1] + 30), name, font=F16, fill=(155, 28, 28), anchor="mt")
    d.text(iso(674, 130, 130), "BT138 / BTA", font=F18, fill=(155, 28, 28))
    d.text(iso(674, 130, 112), "pestaña = MT2", font=F16, fill=(92, 103, 125))

    # lamp
    lp = iso(820, 240, 90)
    d.ellipse((lp[0] - 28, lp[1] - 34, lp[0] + 28, lp[1] + 22), fill=(255, 244, 200), outline=(155, 28, 28), width=3)
    d.rectangle((lp[0] - 10, lp[1] + 18, lp[0] + 10, lp[1] + 36), fill=(80, 80, 80))
    d.text((lp[0], lp[1] - 46), "lámpara 220 V", font=F18, fill=(155, 28, 28), anchor="mm")

    # fuse
    fu = iso(780, 110, 55)
    d.rectangle((fu[0] - 28, fu[1] - 10, fu[0] + 28, fu[1] + 10), fill=(230, 230, 235), outline=(155, 28, 28), width=2)
    d.text((fu[0], fu[1] - 20), "F1  2 A T", font=F16, fill=(155, 28, 28), anchor="mm")

    # MOV disc
    mv = iso(620, 250, 70)
    d.ellipse((mv[0] - 16, mv[1] - 20, mv[0] + 16, mv[1] + 20), fill=(50, 90, 170), outline=(20, 40, 80), width=2)
    d.text((mv[0] + 22, mv[1]), "RV1", font=F16, fill=(50, 90, 170), anchor="lm")

    # labels on table
    d.text(iso(120, 40, 70), "PROTOBOARD  3,3 V", font=F22, fill=(29, 78, 137))
    d.text(iso(560, 40, 58), "PLACA PERFORADA  220 VAC", font=F22, fill=(155, 28, 28))

    # legend
    d.rounded_rectangle((40, 1180, 2360, 1440), radius=18, fill=(255, 255, 255), outline=(27, 54, 93), width=3)
    legend = [
        "GPIO (naranja) → R1 120 Ω → MOC pin 1.   GND (azul) → MOC pin 2.   Rpd 10 k entre GPIO y GND.",
        "MOC pin 6 → R2 330 Ω → G del TRIAC.   MOC pin 4 → MT2.   MT1 → N.   L → fusible → lámpara → MT2.",
        "TO-220 de frente, pestaña atrás: izquierda MT1 · centro MT2 · derecha G.  Si se invierten, zumba y calienta.",
        "No PWM.  No unir GND con N.  PE al chasis.  Probar primero la lámpara, después la bobina, nunca el horno.",
    ]
    d.text((64, 1200), "Cableado (esta imagen manda sobre cualquier render 3D)", font=F20B, fill=(27, 54, 93))
    yy = 1244
    for ln in legend:
        d.text((64, yy), ln, font=F18, fill=(26, 26, 26))
        yy += 42

    img.save(PNG, "PNG")
    PCB.mkdir(exist_ok=True)
    (PCB / PNG.name).write_bytes(PNG.read_bytes())


if __name__ == "__main__":
    write_pdf()
    write_png()
    print(PDF)
    print(PNG)
