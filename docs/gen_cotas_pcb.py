#!/usr/bin/env python3
"""Plano de cotas Rev.F: footprints reales para verificar piezas locales.

Las medidas salen de pcb/gen_easyeda_driver.py (Rev.F, 90 × 74 mm).
"""

from pathlib import Path

from reportlab.lib.pagesizes import A3, A4, landscape
from reportlab.lib.units import mm
from reportlab.lib.colors import HexColor, white, black
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfgen import canvas

OUT = Path(__file__).with_name("IA_Kiln_Driver_Cotas.pdf")
PCB = Path(__file__).resolve().parents[1] / "pcb" / "IA_Kiln_Driver_Cotas.pdf"

NAVY = HexColor("#1B365D")
LINE = HexColor("#1A1A1A")
MUTED = HexColor("#5C677D")
DIM = HexColor("#1D4E89")
AC = HexColor("#9B1C1C")
PSU = HexColor("#0F6B4C")
GOLD = HexColor("#B8860B")
BOARD = HexColor("#2E6B45")
ISO = HexColor("#C9A227")
FILL_LV = HexColor("#EAF2FA")
FILL_AC = HexColor("#FDF2F2")
FILL_NOTE = HexColor("#F7F7F4")
FILL_OK = HexColor("#E8F6EF")


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

H_PCB = 74.0
W_PCB = 90.0


class D:
    def __init__(self, c: canvas.Canvas):
        self.c = c

    def text(self, x, y, s, size=8, bold=False, color=LINE, align="left"):
        self.c.setFillColor(color)
        self.c.setFont(FONT_B if bold else FONT, size)
        if align == "center":
            self.c.drawCentredString(x * mm, y * mm, s)
        elif align == "right":
            self.c.drawRightString(x * mm, y * mm, s)
        else:
            self.c.drawString(x * mm, y * mm, s)

    def line(self, x1, y1, x2, y2, color=LINE, width=0.7):
        self.c.setStrokeColor(color)
        self.c.setFillColor(color)
        self.c.setLineWidth(width)
        self.c.setLineCap(1)
        self.c.line(x1 * mm, y1 * mm, x2 * mm, y2 * mm)

    def rect(self, x, y, w, h, fill=None, stroke=LINE, width=0.7, dash=None):
        self.c.setStrokeColor(stroke)
        self.c.setLineWidth(width)
        if dash:
            self.c.setDash(dash)
        else:
            self.c.setDash()
        if fill is not None:
            self.c.setFillColor(fill)
            self.c.rect(x * mm, y * mm, w * mm, h * mm, fill=1, stroke=1)
        else:
            self.c.rect(x * mm, y * mm, w * mm, h * mm, fill=0, stroke=1)
        self.c.setDash()

    def round_box(self, x, y, w, h, fill, stroke, sw=0.7, r=2.2):
        self.c.setFillColor(fill)
        self.c.setStrokeColor(stroke)
        self.c.setLineWidth(sw)
        self.c.roundRect(x * mm, y * mm, w * mm, h * mm, r * mm, fill=1, stroke=1)

    def circ(self, x, y, r, fill=None, stroke=LINE, width=0.7, dash=None):
        if dash:
            self.c.setDash(dash)
        else:
            self.c.setDash()
        self.c.setStrokeColor(stroke)
        self.c.setLineWidth(width)
        if fill is not None:
            self.c.setFillColor(fill)
            self.c.circle(x * mm, y * mm, r * mm, fill=1, stroke=1)
        else:
            self.c.circle(x * mm, y * mm, r * mm, fill=0, stroke=1)
        self.c.setDash()

    def pad(self, x, y, d_pad, d_hole, scale=1.0):
        self.circ(x, y, d_pad * scale / 2, HexColor("#D4AF37"), HexColor("#6B4F1A"), 0.4)
        self.circ(x, y, d_hole * scale / 2, HexColor("#1A1A1A"), HexColor("#1A1A1A"), 0.2)

    def dim_h(self, x1, x2, y, label, side=1, color=DIM):
        self.line(x1, y, x2, y, color, 0.55)
        self.line(x1, y - 1.3, x1, y + 1.3, color, 0.55)
        self.line(x2, y - 1.3, x2, y + 1.3, color, 0.55)
        ty = y + 1.6 if side > 0 else y - 4.0
        self.text((x1 + x2) / 2, ty, label, 7.2, True, color, "center")

    def dim_v(self, y1, y2, x, label, side=1, color=DIM):
        self.line(x, y1, x, y2, color, 0.55)
        self.line(x - 1.3, y1, x + 1.3, y1, color, 0.55)
        self.line(x - 1.3, y2, x + 1.3, y2, color, 0.55)
        self.text(x + 1.8 if side > 0 else x - 1.8, (y1 + y2) / 2 - 1.1, label, 7.2, True, color, "left" if side > 0 else "right")


def header(d: D, wm, hm, title, sub):
    d.c.setFillColor(NAVY)
    d.c.rect(0, (hm - 16) * mm, wm * mm, 16 * mm, fill=1, stroke=0)
    d.text(10, hm - 7.2, title, 14, True, white)
    d.text(10, hm - 13.2, sub, 8, False, HexColor("#D5DEEC"))
    d.text(wm - 10, hm - 7.2, "Rev. F", 11, True, white, "right")
    d.text(wm - 10, hm - 13.2, "PCB 90 × 74 mm", 8, False, HexColor("#D5DEEC"), "right")
    d.c.setFillColor(HexColor("#F3F4F6"))
    d.c.rect(0, 0, wm * mm, 12 * mm, fill=1, stroke=0)
    d.c.setStrokeColor(NAVY)
    d.c.setLineWidth(0.5)
    d.c.line(0, 12 * mm, wm * mm, 12 * mm)
    d.text(
        10,
        4.6,
        "Imprimir a escala 100 % (no “ajustar al papel”). El BT138 solo conmuta A2–N de la bobina.",
        7.4,
        False,
        NAVY,
    )


def pcb_to_paper(x, y, ox, oy, sc):
    return ox + x * sc, oy + (H_PCB - y) * sc


def draw_board(d: D, ox, oy, sc, labels=True, holes=True, courtyards=True):
    d.rect(ox, oy, W_PCB * sc, H_PCB * sc, BOARD, HexColor("#1A3324"), 1.1)
    iso_x = ox + 38.0 * sc
    d.line(iso_x, oy + (H_PCB - 50.0) * sc, iso_x, oy + (H_PCB - 20.2) * sc, ISO, 1.4)
    d.line(iso_x, oy + (H_PCB - 11.2) * sc, iso_x, oy + (H_PCB - 4.8) * sc, ISO, 1.4)

    def P(x, y):
        return pcb_to_paper(x, y, ox, oy, sc)

    def box(x0, y0, x1, y1, color=HexColor("#F4F1DE"), stroke=HexColor("#3D3D3D")):
        xa, ya = P(x0, y1)
        xb, yb = P(x1, y0)
        d.rect(min(xa, xb), min(ya, yb), abs(xb - xa), abs(yb - ya), color, stroke, 0.55)

    def pads(pts, dp, dh):
        for x, y in pts:
            px, py = P(x, y)
            d.pad(px, py, dp, dh, sc)

    if courtyards:
        box(6.0, 8.4, 14.8, 13.8, HexColor("#D8F3DC"))
        box(6.0, 16.8, 14.8, 22.2, HexColor("#D8E7F5"))
        box(21.0, 52.0, 55.0, 72.0, HexColor("#CDE7D8"))
        box(51.0, 6.2, 70.2, 22.2, HexColor("#F8E1D4"))
        box(80.2, 16.8, 87.2, 39.0, HexColor("#F8D7DA"))
        cx, cy = P(46.75, 30.0)
        d.circ(cx, cy, 8.0 * sc, None, AC, 0.7, (1.2, 1.0))
        cx, cy = P(66.75, 62.0)
        d.circ(cx, cy, 8.0 * sc, None, AC, 0.7, (1.2, 1.0))
        box(43.4, 38.6, 55.6, 43.4, HexColor("#F5E6C8"))
        box(58.0, 45.4, 74.0, 48.6, HexColor("#F5E6C8"))

    pads([(8.0, 11.0), (11.5, 11.0)], 2.2, 1.3)
    pads([(8.0, 19.5), (11.5, 19.5)], 2.2, 1.3)
    pads([(8.0, 28.0), (15.62, 28.0)], 1.8, 0.9)
    pads([(8.0, 35.5), (15.62, 35.5)], 1.8, 0.9)
    pads([(19.0, 11.0), (24.0, 11.0)], 1.8, 0.9)
    pads([(10.0, 58.0), (10.0, 54.5)], 2.0, 0.9)

    ux, uy = 30.38, 12.5
    moc = [(ux, uy + i * 2.54) for i in range(3)] + [(ux + 7.62, uy + i * 2.54) for i in range(3)]
    pads(moc, 2.0, 0.9)
    pads([(41.2, 12.5), (48.82, 12.5)], 1.8, 0.9)
    pads([(59.50, 24.0), (62.04, 24.0), (64.58, 24.0)], 2.4, 1.1)
    pads([(43.0, 30.0), (50.5, 30.0)], 2.2, 1.1)
    pads([(42.0, 41.0), (57.0, 41.0)], 2.2, 1.1)
    pads([(56.0, 47.0), (76.0, 47.0)], 2.2, 1.1)
    pads([(52.7, 59.5), (52.7, 64.5), (23.3, 54.3), (23.3, 69.7)], 2.6, 1.3)
    pads([(63.0, 62.0), (70.5, 62.0)], 2.2, 1.1)
    pads([(83.0, 20.00), (83.0, 27.62), (83.0, 35.24)], 3.2, 1.6)

    if holes:
        for x, y in ((5.0, 5.0), (85.0, 5.0), (5.0, 69.0), (85.0, 69.0), (62.04, 14.0)):
            px, py = P(x, y)
            d.circ(px, py, 1.6 * sc, HexColor("#222"), HexColor("#EEE"), 0.35)

    if labels and sc >= 1.5:
        d.text(*P(5.6, 7.0), "J1", 7, True, white)
        d.text(*P(5.6, 16.2), "J3", 7, True, white)
        d.text(*P(22.2, 50.8), "HLK-PM12", 7, True, HexColor("#083") if False else white)
        hx, hy = P(22.4, 51.6)
        d.text(hx, hy, "U2  HLK-PM12", 7.2, True, HexColor("#0B3D2A"))
        d.text(*P(52.0, 5.6), "Q2 BT138", 7, True, HexColor("#5C2A1A"))
        d.text(*P(76.6, 16.0), "J2", 8, True, AC)
        d.text(*P(38.6, 25.2), "RV1", 7, True, AC)
        d.text(*P(60.4, 53.6), "RV2", 7, True, AC)
        d.text(*P(28.4, 8.4), "U1 MOC", 7, True, NAVY)
        d.text(*P(39.2, 36.8), "Cs P15", 6.5, True, HexColor("#6B4F1A"))
        d.text(*P(58.2, 44.2), "Rs P20", 6.5, True, HexColor("#6B4F1A"))


def page1(c):
    wm, hm = landscape(A3)[0] / mm, landscape(A3)[1] / mm
    d = D(c)
    header(
        d,
        wm,
        hm,
        "IA Kiln  ·  Cotas de footprints  ·  para piezas locales",
        "PCB Rev. F  ·  EasyEDA Std  ·  todos los componentes through-hole",
    )
    sc = 2.05
    ox, oy = 28.0, 24.0
    draw_board(d, ox, oy, sc)

    # overall dims
    d.dim_h(ox, ox + W_PCB * sc, oy + H_PCB * sc + 8.5, "90,0 mm")
    d.dim_v(oy, oy + H_PCB * sc, ox - 10.0, "74,0 mm", -1)
    d.dim_h(ox, ox + 5.0 * sc, oy + H_PCB * sc + 3.5, "5,0")
    d.dim_h(ox + 85.0 * sc, ox + W_PCB * sc, oy + H_PCB * sc + 3.5, "5,0")
    d.dim_v(oy + (H_PCB - 5.0) * sc, oy + H_PCB * sc, ox - 4.5, "5,0", -1)
    iso_x = ox + 38.0 * sc
    d.dim_h(ox, iso_x, oy - 5.5, "ISO GAP  x = 38,0 mm", -1)

    d.round_box(228, 18, 178, hm - 40, FILL_NOTE, NAVY, 0.8, 3)
    d.text(236, hm - 30, "Cómo usar este plano", 11, True, NAVY)
    lines = [
        "1. Imprimí la hoja 3 a escala 1:1 y apoyá",
        "    cada pieza sobre los pads.",
        "2. Con el calibre medí el PASO entre",
        "    centros de terminales (no el cuerpo).",
        "3. El pin tiene que entrar con holgura:",
        "    Ø pin  ≤  Ø agujero − 0,2 mm.",
        "4. El cuerpo no puede tapar al vecino.",
        "    MOV: máximo Ø 14 mm (courtyard Ø 16).",
        "",
        "Si una pieza local no coincide en el",
        "paso, no la fuerces: o se cambia el",
        "footprint, o se busca otra pieza.",
        "",
        "Holguras pensadas para ferretería /",
        "electrónica local (no SMD, no CNC):",
        "  bornes 3,5  →  Ø 1,30 mm",
        "  DIP / axial →  Ø 0,90 mm",
        "  MOV / X2 / 2 W →  Ø 1,10 mm",
        "  HLK-PM12    →  Ø 1,30 mm",
        "  J2 7,62     →  Ø 1,60 mm",
        "  M3          →  Ø 3,20 mm",
        "",
        "HLK-PM12: lo crítico son los pines,",
        "no la seda. Cuerpo típico 34,8 × 20,2.",
        "Caja de seda 34 × 20 (guía).",
    ]
    yy = hm - 38
    for line in lines:
        d.text(236, yy, line, 7.5, False, LINE)
        yy -= 5.55

    d.round_box(228, 18, 178, 42, HexColor("#FDECEC"), AC, 0.8, 2)
    d.text(236, 50, "No mezclar con la prueba casera", 8.5, True, AC)
    d.text(236, 42, "Ese montaje (MOC + lámpara) no es esta placa.", 7.2, False, LINE)
    d.text(236, 35, "Acá Cs es X2 P=15, Rs P=20, dos MOV 14D431.", 7.2, False, LINE)
    d.text(236, 28, "R2 en placa = 330 Ω. En banco 220 Ω sirve.", 7.2, False, LINE)

    c.showPage()


def fp_header(d, x, y, w, h, title):
    d.round_box(x, y, w, h, white, NAVY, 0.8, 2.4)
    d.c.setFillColor(NAVY)
    d.c.rect(x * mm, (y + h - 9) * mm, w * mm, 9 * mm, fill=1, stroke=0)
    d.text(x + 4, y + h - 6.4, title, 8.5, True, white)


def page2(c):
    wm, hm = landscape(A3)[0] / mm, landscape(A3)[1] / mm
    d = D(c)
    header(
        d,
        wm,
        hm,
        "IA Kiln  ·  Detalle de cada footprint",
        "Paso entre centros  ·  Ø pad  ·  Ø agujero  ·  cuerpo máximo",
    )

    cards = [
        (12, 168, "J1 / J3   bornes KF128 / WJ350  2P"),
        (150, 168, "J2   bornes DG128 / KF762  3P"),
        (288, 168, "U1   MOC3020  DIP-6"),
        (12, 96, "Q2   BT138-600  TO-220"),
        (150, 96, "U2   HLK-PM12"),
        (288, 96, "RV1 / RV2   14D431  Ø14"),
        (12, 24, "Cs   100 nF X2  275 VAC"),
        (150, 24, "Rs   100 Ω  2 W   y   R1 / Rpd / R2"),
        (288, 24, "C12 / C13   y   tornillos M3"),
    ]
    for x, y, title in cards:
        fp_header(d, x, y, 132, 68, title)

    # J1/J3
    x0, y0 = 22, 186
    d.pad(x0, y0 + 18, 2.2, 1.3, 4)
    d.pad(x0 + 14, y0 + 18, 2.2, 1.3, 4)
    d.dim_h(x0, x0 + 14, y0 + 30, "3,50 mm")
    d.text(x0 + 36, y0 + 28, "Pad Ø 2,20   agujero Ø 1,30", 7.3, False, LINE)
    d.text(x0 + 36, y0 + 20, "Pin del borne ≤ 1,10 mm", 7.3, True, PSU)
    d.text(x0 + 36, y0 + 12, "Cuerpo del bloque ~ 8 × 7 mm", 7.3, False, MUTED)
    d.text(x0 + 36, y0 + 4, "Medir: distancia entre centros de tornillos", 7.1, False, DIM)

    # J2
    x0, y0 = 160, 184
    d.pad(x0, y0 + 32, 3.2, 1.6, 3.2)
    d.pad(x0, y0 + 32 - 7.62 * 3.2, 3.2, 1.6, 3.2)
    d.pad(x0, y0 + 32 - 15.24 * 3.2, 3.2, 1.6, 3.2)
    d.dim_v(y0 + 32 - 7.62 * 3.2, y0 + 32, x0 + 10, "7,62 mm")
    d.text(x0 + 28, y0 + 36, "Pad Ø 3,20   agujero Ø 1,60", 7.3, False, LINE)
    d.text(x0 + 28, y0 + 28, "Pin ≤ 1,40 mm", 7.3, True, PSU)
    d.text(x0 + 28, y0 + 20, "Tres polos en línea: L · N · A2", 7.3, False, MUTED)
    d.text(x0 + 28, y0 + 12, "No sirve paso 5,08 ni 5,00 mm", 7.3, True, AC)

    # MOC
    x0, y0 = 300, 192
    for i in range(3):
        d.pad(x0, y0 - i * 9, 2.0, 0.9, 3.5)
        d.pad(x0 + 26.7, y0 - i * 9, 2.0, 0.9, 3.5)
    d.dim_v(y0 - 9, y0, x0 - 8, "2,54", -1)
    d.dim_h(x0, x0 + 26.7, y0 + 10, "7,62 mm")
    d.text(x0 + 40, y0, "Agujero Ø 0,90", 7.3, False, LINE)
    d.text(x0 + 40, y0 - 8, "Pin DIP ≤ 0,70 mm", 7.3, True, PSU)
    d.text(x0 + 40, y0 - 16, "Pin 1 = ánodo (marca)", 7.3, False, MUTED)
    d.text(x0 + 40, y0 - 24, "Filas 1-2-3  y  6-5-4", 7.3, False, MUTED)

    # BT138
    x0, y0 = 28, 122
    d.pad(x0, y0, 2.4, 1.1, 4)
    d.pad(x0 + 10.2, y0, 2.4, 1.1, 4)
    d.pad(x0 + 20.3, y0, 2.4, 1.1, 4)
    d.circ(x0 + 10.2, y0 + 22, 6.4, None, LINE, 0.7)
    d.text(x0, y0 - 8, "MT1", 7, True, LINE, "center")
    d.text(x0 + 10.2, y0 - 8, "MT2", 7, True, LINE, "center")
    d.text(x0 + 20.3, y0 - 8, "G", 7, True, LINE, "center")
    d.dim_h(x0, x0 + 10.2, y0 + 10, "2,54")
    d.dim_v(y0, y0 + 22, x0 + 32, "10,0 mm  pestaña")
    d.text(x0 + 48, y0 + 20, "Pad Ø 2,40   pin Ø 1,10", 7.3, False, LINE)
    d.text(x0 + 48, y0 + 12, "Pestaña = MT2  ·  agujero PCB Ø 3,20 (M3)", 7.3, True, PSU)
    d.text(x0 + 48, y0 + 4, "Frente hacia vos, pestaña atrás:", 7.3, False, MUTED)
    d.text(x0 + 48, y0 - 4, "izquierda MT1 · centro MT2 · derecha G", 7.3, False, MUTED)

    # HLK
    x0, y0 = 162, 114
    d.rect(x0, y0, 48, 28, HexColor("#CDE7D8"), PSU, 0.8)
    d.pad(x0 + 6, y0 + 5, 2.6, 1.3, 2.2)
    d.pad(x0 + 6, y0 + 23, 2.6, 1.3, 2.2)
    d.pad(x0 + 42, y0 + 10.5, 2.6, 1.3, 2.2)
    d.pad(x0 + 42, y0 + 17.5, 2.6, 1.3, 2.2)
    d.text(x0 + 10, y0 + 2, "3  0V", 6.2, True, PSU)
    d.text(x0 + 10, y0 + 24.5, "4  +12", 6.2, True, PSU)
    d.text(x0 + 30, y0 + 8, "1 AC", 6.2, True, AC)
    d.text(x0 + 30, y0 + 19, "2 AC", 6.2, True, AC)
    d.text(x0 + 52, y0 + 22, "Filas AC–DC  29,40 mm", 7.3, True, DIM)
    d.text(x0 + 52, y0 + 14, "AC (1–2)        5,00 mm", 7.3, False, LINE)
    d.text(x0 + 52, y0 + 7, "DC (3–4)      15,40 mm", 7.3, False, LINE)
    d.text(x0 + 52, y0 + 0, "Agujero Ø 1,30   pin ≤ 1,10", 7.3, True, PSU)
    d.text(x0 + 52, y0 - 7, "Cuerpo seda 34 × 20 mm", 7.1, False, MUTED)

    # MOV
    x0, y0 = 322, 128
    d.circ(x0 + 18, y0 + 8, 16, None, AC, 0.8, (1.5, 1.2))
    d.circ(x0 + 18, y0 + 8, 14, HexColor("#F8D7DA"), AC, 0.5)
    d.pad(x0 + 18 - 7.5 * 1.1, y0 + 8, 2.2, 1.1, 2.2)
    d.pad(x0 + 18 + 7.5 * 1.1, y0 + 8, 2.2, 1.1, 2.2)
    d.dim_h(x0 + 18 - 8.25, x0 + 18 + 8.25, y0 - 4, "7,50 mm")
    d.text(x0 + 40, y0 + 18, "Cuerpo Ø 14 máx.", 7.3, True, AC)
    d.text(x0 + 40, y0 + 10, "Courtyard Ø 16 mm", 7.3, False, LINE)
    d.text(x0 + 40, y0 + 2, "Agujero Ø 1,10", 7.3, False, LINE)
    d.text(x0 + 40, y0 - 6, "No usar 20D (es Ø 20)", 7.3, True, AC)

    # Cs
    x0, y0 = 28, 48
    d.pad(x0, y0 + 18, 2.2, 1.1, 3)
    d.pad(x0 + 45, y0 + 18, 2.2, 1.1, 3)
    d.rect(x0 + 8, y0 + 10, 29, 16, HexColor("#F5E6C8"), GOLD, 0.7)
    d.dim_h(x0, x0 + 45, y0 + 36, "15,00 mm")
    d.text(x0 + 56, y0 + 28, "P=15 mm  (no P=10 ni P=22,5)", 7.3, True, PSU)
    d.text(x0 + 56, y0 + 20, "Agujero Ø 1,10   pad Ø 2,20", 7.3, False, LINE)
    d.text(x0 + 56, y0 + 12, "100 nF  X2  275 VAC", 7.3, False, LINE)
    d.text(x0 + 56, y0 + 4, "Si tu X2 es P=10, no entra derecho", 7.3, True, AC)

    # Rs + axials
    x0, y0 = 162, 48
    d.pad(x0, y0 + 22, 2.2, 1.1, 2.6)
    d.pad(x0 + 52, y0 + 22, 2.2, 1.1, 2.6)
    d.rect(x0 + 8, y0 + 16, 36, 12, HexColor("#F5E6C8"), GOLD, 0.7)
    d.dim_h(x0, x0 + 52, y0 + 40, "Rs  20,00 mm")
    d.text(x0 + 58, y0 + 30, "Rs 2 W: paso 20 mm  Øag 1,10", 7.3, True, PSU)
    d.text(x0 + 58, y0 + 22, "Cuerpo 2 W típico 16 × Ø5 — entra", 7.3, False, LINE)
    d.pad(x0, y0 + 4, 1.8, 0.9, 2.4)
    d.pad(x0 + 18.3, y0 + 4, 1.8, 0.9, 2.4)
    d.dim_h(x0, x0 + 18.3, y0 - 6, "R1 Rpd R2  7,62 mm", -1)
    d.text(x0 + 58, y0 + 8, "Axiales ¼ W: paso 7,62  Øag 0,90", 7.3, False, LINE)
    d.text(x0 + 58, y0 + 0, "R1=120 Ω  Rpd=10 k  R2=330 Ω", 7.3, False, MUTED)

    # C12 C13 M3
    x0, y0 = 300, 52
    d.pad(x0, y0 + 28, 1.8, 0.9, 3)
    d.pad(x0 + 15, y0 + 28, 1.8, 0.9, 3)
    d.dim_h(x0, x0 + 15, y0 + 40, "C12  5,00 mm")
    d.pad(x0 + 4, y0 + 8, 2.0, 0.9, 3)
    d.pad(x0 + 4, y0 + 8 + 10.5, 2.0, 0.9, 3)
    d.dim_v(y0 + 8, y0 + 18.5, x0 + 16, "C13  3,50")
    d.circ(x0 + 48, y0 + 22, 6.4, HexColor("#333"), HexColor("#EEE"), 0.5)
    d.text(x0 + 58, y0 + 24, "M3  Ø 3,20 mm", 7.3, True, LINE)
    d.text(x0 + 58, y0 + 16, "a 5,0 mm del borde", 7.3, False, MUTED)
    d.text(x0 + 22, y0 - 2, "C13 radial 220 µF 25 V  (el + hacia arriba)", 7.1, False, MUTED)

    c.showPage()


def page3(c):
    wm, hm = landscape(A4)[0] / mm, landscape(A4)[1] / mm
    d = D(c)
    header(
        d,
        wm,
        hm,
        "IA Kiln  ·  Plantilla 1:1  ·  apoyar las piezas",
        "Imprimir al 100 %. Comprobar la barra de 50 mm con una regla.",
    )
    ox, oy = 14.0, 22.0
    draw_board(d, ox, oy, 1.0, labels=True, holes=True, courtyards=True)
    d.dim_h(ox, ox + 90.0, oy + 74.0 + 6.5, "90,0 mm")
    d.dim_v(oy, oy + 74.0, ox + 90.0 + 6.0, "74,0 mm")

    # 50 mm scale bar
    d.round_box(168, 22, 118, 52, FILL_OK, PSU, 0.9, 2)
    d.text(176, 64, "Barra de escala", 9, True, PSU)
    d.line(176, 48, 226, 48, PSU, 1.6)
    d.line(176, 45.5, 176, 50.5, PSU, 1.2)
    d.line(226, 45.5, 226, 50.5, PSU, 1.2)
    d.text(201, 52.5, "50,0 mm", 8, True, PSU, "center")
    d.text(176, 36, "Si no mide 50 mm, reimprimí", 7.2, False, LINE)
    d.text(176, 29, "sin “ajustar a la página”.", 7.2, False, LINE)

    d.round_box(168, 80, 118, hm - 108, FILL_NOTE, NAVY, 0.8, 2)
    d.text(176, hm - 36, "Checklist rápido", 10, True, NAVY)
    chk = [
        "J1 / J3  paso 3,50",
        "J2        paso 7,62  (tres pines)",
        "MOC      2,54 × 7,62",
        "BT138    2,54  y pestaña M3",
        "HLK      5,00 / 15,40 / 29,40",
        "MOV      paso 7,50  cuerpo Ø14",
        "Cs        paso 15,0  (X2)",
        "Rs        paso 20,0  (2 W)",
        "R1/Rpd/R2 paso 7,62",
        "C12 5,00   C13 3,50",
    ]
    yy = hm - 46
    for line in chk:
        d.text(176, yy, "☐  " + line, 7.6, False, LINE)
        yy -= 7.15

    c.showPage()


def render_preview(pdf_path: Path):
    try:
        import pypdfium2 as pdfium
    except ImportError:
        return
    doc = pdfium.PdfDocument(str(pdf_path))
    names = (
        "IA_Kiln_Driver_Cotas_p1.png",
        "IA_Kiln_Driver_Cotas_p2.png",
        "IA_Kiln_Driver_Cotas_1a1.png",
    )
    for i, name in enumerate(names):
        if i >= len(doc):
            break
        img = doc[i].render(scale=1.55).to_pil()
        prev = pdf_path.with_name(name)
        img.save(prev, "PNG")
        print(prev)


def main():
    c = canvas.Canvas(str(OUT), pagesize=landscape(A3))
    c.setTitle("IA Kiln — Cotas footprints PCB Rev. F")
    c.setAuthor("IA Kiln")
    c.setSubject("Medidas de pads, agujeros y cuerpos para piezas locales")
    page1(c)
    page2(c)
    c.setPageSize(landscape(A4))
    page3(c)
    c.save()
    try:
        PCB.write_bytes(OUT.read_bytes())
        print(PCB)
    except OSError as exc:
        print("skip", PCB, exc)
    print(OUT)
    render_preview(OUT)


if __name__ == "__main__":
    main()
