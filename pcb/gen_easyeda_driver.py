#!/usr/bin/env python3
"""PCB EasyEDA Std Rev.F: footprints reales + ruteo sin cortos.

Placa 90 × 74 mm. Tornillos M3 a 5 mm del borde. ISO GAP en x=38.
Pistas 12 V 0,50 mm; L/N/A2 1,20 mm en top (L baja a bottom en dos vias).
MOV Ø14 con courtyard 16 mm. Rs P=20 mm. Cs X2 P=15 mm.

Redes:
  V12      J1.1 — C12.1 — C13.1 — U2.4 (+12)
  GND      J1.2 — J3.2 — C12.2 — C13.2 — Rpd.2 — U1.2 — U2.3 (0 V)
  CTRL     J3.1 — Rpd.1 — R1.1
  LED_A    R1.2 — U1.1
  GATE_DRV U1.6 — R2.1
  GATE     R2.2 — Q2.G
  L        J2.1 — U2.1 — RV2.1
  N        J2.2 — U2.2 — Q2.MT1 — RV1.2 — RV2.2 — Rs.2
  A2       J2.3 — U1.4 — Q2.MT2 — RV1.1 — Cs.1
  SNUB     Cs.2 — Rs.1

U2 = HLK-PM12 a caballo del ISO GAP (AC en 220 V, 12 V aislado).
El BT138 solo conmuta A2–N (bobina). L–N alimentan la fuente, no el triac.
"""

from __future__ import annotations

import json
import math
from pathlib import Path

OUT = Path(__file__).with_name("IA_Kiln_Driver_PCB.json")

OX, OY = 4000.0, 3000.0


def u(mm: float) -> float:
    return round(mm / 0.254, 3)


def xy(mmx: float, mmy: float) -> tuple[float, float]:
    return (round(OX + u(mmx), 3), round(OY + u(mmy), 3))


def dist_seg(px, py, x1, y1, x2, y2) -> float:
    dx, dy = x2 - x1, y2 - y1
    if dx == 0 and dy == 0:
        return math.hypot(px - x1, py - y1)
    t = max(0.0, min(1.0, ((px - x1) * dx + (py - y1) * dy) / (dx * dx + dy * dy)))
    return math.hypot(px - (x1 + t * dx), py - (y1 + t * dy))


class G:
    def __init__(self):
        self.n = 1
        self.shape: list[str] = []
        self.pads: list[tuple[float, float, str, float]] = []
        self.tracks: list[tuple[list[tuple[float, float]], str, float, int]] = []

    def id(self) -> str:
        i = f"gge{self.n}"
        self.n += 1
        return i

    def pad_s(self, mmx, mmy, d_pad, d_hole, net, number, shape="ELLIPSE", w=None, h=None):
        x, y = xy(mmx, mmy)
        pw = u(w if w is not None else d_pad)
        ph = u(h if h is not None else d_pad)
        # EasyEDA: campo 10 = radio del furo en unidades de 10 mil
        hr = u(d_hole / 2.0)
        self.pads.append((mmx, mmy, net, d_pad / 2))
        return (
            f"PAD~{shape}~{x}~{y}~{pw}~{ph}~11~{net}~{number}~{hr}~~0~{self.id()}~0~~Y~0"
        )

    def hole_s(self, mmx, mmy, d_mm):
        x, y = xy(mmx, mmy)
        # EasyEDA HOLE: el número se comporta como radio (3.2 mm → UI 6.4 si se pasa diámetro)
        return f"HOLE~{x}~{y}~{u(d_mm / 2.0):.3f}~{self.id()}~0"

    def track_s(self, pts_mm, width_mm, layer, net):
        pts = " ".join(f"{x} {y}" for x, y in (xy(*p) for p in pts_mm))
        return f"TRACK~{u(width_mm):.4f}~{layer}~{net}~{pts}~{self.id()}~0"

    def text_s(self, mmx, mmy, s, size=1.2, kind="L"):
        x, y = xy(mmx, mmy)
        stroke = round(u(max(0.12, size * 0.08)), 3)
        fs = u(size)
        return f"TEXT~{kind}~{x}~{y}~{stroke}~0~none~3~~{fs:.3f}~{s}~~{self.id()}"

    def pad(self, mmx, mmy, d_pad, d_hole, net, number, shape="ELLIPSE", w=None, h=None):
        self.shape.append(self.pad_s(mmx, mmy, d_pad, d_hole, net, number, shape, w, h))

    def silk_box(self, x0, y0, x1, y1, width=0.2):
        self.shape.append(self.rect_s(x0, y0, x1, y1, width, 3))

    def rect_s(self, x0, y0, x1, y1, width=0.2, layer=3):
        return self.track_s([(x0, y0), (x1, y0), (x1, y1), (x0, y1), (x0, y0)], width, layer, "")

    def lib(self, origin, package, ref, parts: list[str]):
        ox, oy = xy(*origin)
        gid = self.id()
        inner = "#@$".join(parts)
        self.shape.append(
            f"LIB~{ox}~{oy}~package`{package}`pre`{ref}`~0~~{gid}~0#@${inner}"
        )

    def hole(self, mmx, mmy, d_mm):
        self.shape.append(self.hole_s(mmx, mmy, d_mm))

    def track(self, pts_mm, width_mm, layer, net):
        if net:
            self.tracks.append((pts_mm, net, width_mm, layer))
        self.shape.append(self.track_s(pts_mm, width_mm, layer, net))

    def via(self, mmx, mmy, net, d_mm=1.8, hole_mm=0.8):
        x, y = xy(mmx, mmy)
        self.pads.append((mmx, mmy, net, d_mm / 2))
        self.shape.append(
            f"VIA~{x}~{y}~{u(d_mm):.3f}~{net}~{u(hole_mm / 2):.3f}~{self.id()}~0"
        )

    def silk_text(self, mmx, mmy, s, size=1.2):
        self.shape.append(self.text_s(mmx, mmy, s, size, "L"))

    def outline_rect(self, w, h):
        self.shape.append(self.track_s([(0, 0), (w, 0), (w, h), (0, h), (0, 0)], 0.15, 10, ""))

    def keepout_band(self, x, y0, y1, y_gap0, y_gap1):
        """Franja de creepage en seda (no fresa). Hueco donde va U1."""
        self.shape.append(self.track_s([(x, y0), (x, y_gap0)], 0.3, 3, ""))
        self.shape.append(self.track_s([(x, y_gap1), (x, y1)], 0.3, 3, ""))
        self.shape.append(self.track_s([(x - 1.2, y0), (x + 1.2, y0)], 0.2, 3, ""))
        self.shape.append(self.track_s([(x - 1.2, y1), (x + 1.2, y1)], 0.2, 3, ""))

    def silk_circle(self, cx, cy, r, width=0.2, n=28):
        pts = [
            (cx + r * math.cos(2 * math.pi * i / n), cy + r * math.sin(2 * math.pi * i / n))
            for i in range(n + 1)
        ]
        self.shape.append(self.track_s(pts, width, 3, ""))


def _ccw(p, q, r):
    return (r[1] - p[1]) * (q[0] - p[0]) > (q[1] - p[1]) * (r[0] - p[0])


def segs_cross(a1, a2, b1, b2) -> bool:
    if a1 == b1 or a1 == b2 or a2 == b1 or a2 == b2:
        return False
    return _ccw(a1, b1, b2) != _ccw(a2, b1, b2) and _ccw(a1, a2, b1) != _ccw(a1, a2, b2)


def check_clearance(g: G, min_clr=0.35):
    errs = []
    for pts, tnet, w, _layer in g.tracks:
        for i in range(len(pts) - 1):
            a, b = pts[i], pts[i + 1]
            for px, py, pnet, r in g.pads:
                if pnet == tnet or pnet.startswith("NC"):
                    continue
                d = dist_seg(px, py, a[0], a[1], b[0], b[1])
                need = r + w / 2 + min_clr
                if d < need:
                    errs.append(
                        f"CORTO {tnet} pista {a}->{b} vs pad {pnet} ({px:.1f},{py:.1f}) d={d:.2f} need={need:.2f}"
                    )
    for i, (pts_a, na, _wa, la) in enumerate(g.tracks):
        for pts_b, nb, _wb, lb in g.tracks[i + 1 :]:
            if na == nb or la != lb:
                continue
            for j in range(len(pts_a) - 1):
                for k in range(len(pts_b) - 1):
                    if segs_cross(pts_a[j], pts_a[j + 1], pts_b[k], pts_b[k + 1]):
                        errs.append(
                            f"CRUCE {na} x {nb}  {pts_a[j]}-{pts_a[j+1]}  vs  {pts_b[k]}-{pts_b[k+1]}"
                        )
    return errs


def check_isolation(g: G, min_creep=5.0):
    lv = {"V12", "GND", "CTRL", "LED_A"}
    hv = {"L", "N", "A2", "GATE", "GATE_DRV", "SNUB"}
    errs = []
    pads = [(x, y, n, r) for x, y, n, r in g.pads if n in lv or n in hv]
    for i, (x1, y1, n1, r1) in enumerate(pads):
        for x2, y2, n2, r2 in pads[i + 1 :]:
            if (n1 in lv) == (n2 in lv):
                continue
            moc = 29.0 < x1 < 39.5 and 11.0 < y1 < 19.5 and 29.0 < x2 < 39.5 and 11.0 < y2 < 19.5
            if moc:
                continue
            d = math.hypot(x1 - x2, y1 - y2) - r1 - r2
            if d < min_creep:
                errs.append(
                    f"CREEP {n1}({x1:.1f},{y1:.1f}) vs {n2}({x2:.1f},{y2:.1f}) gap={d:.2f} < {min_creep}"
                )
    return errs


def preview(g: G, w_mm: float, h_mm: float, iso_x=38.0):
    from PIL import Image, ImageDraw, ImageFont

    sc = 12
    mg = 40
    im = Image.new("RGB", (int(w_mm * sc + 2 * mg), int(h_mm * sc + 2 * mg)), (18, 22, 20))
    d = ImageDraw.Draw(im)

    def P(x, y):
        return (mg + x * sc, mg + y * sc)

    d.rectangle([P(0, 0), P(w_mm, h_mm)], fill=(34, 92, 52), outline=(200, 200, 200), width=2)
    colors = {
        "CTRL": (80, 160, 255),
        "V12": (40, 200, 120),
        "GND": (40, 40, 40),
        "LED_A": (80, 160, 255),
        "GATE_DRV": (230, 140, 40),
        "GATE": (230, 140, 40),
        "L": (220, 80, 40),
        "A2": (200, 40, 40),
        "N": (110, 110, 110),
        "SNUB": (180, 160, 50),
    }
    d.line([P(iso_x, 5.2), P(iso_x, 11.2)], fill=(255, 220, 80), width=3)
    d.line([P(iso_x, 20.2), P(iso_x, 50.0)], fill=(255, 220, 80), width=3)
    for pts, net, width, layer in g.tracks:
        col = (70, 110, 220) if layer == 2 else colors.get(net, (220, 40, 40))
        d.line([P(*p) for p in pts], fill=col, width=max(2, int(width * sc)), joint="curve")
    for x, y, net, r in g.pads:
        rr = max(4, r * sc)
        cx, cy = P(x, y)
        d.ellipse([cx - rr, cy - rr, cx + rr, cy + rr], fill=(212, 175, 55), outline=(80, 60, 20))
    try:
        f = ImageFont.truetype(r"C:\Windows\Fonts\arialbd.ttf", 16)
        f2 = ImageFont.truetype(r"C:\Windows\Fonts\arial.ttf", 13)
    except OSError:
        f = f2 = ImageFont.load_default()
    d.text(P(4.8, 2.0), "IA Kiln  Rev.F   (preview)", font=f, fill=(255, 255, 255))
    d.text(P(26.0, 3.2), "ISO GAP", font=f2, fill=(255, 220, 80))
    d.text(P(5.2, 6.5), "12V", font=f2, fill=(180, 255, 200))
    d.text(P(5.2, 16.0), "ESP", font=f2, fill=(180, 220, 255))
    d.text(P(76, 18), "J2", font=f2, fill=(255, 200, 200))
    d.text(P(22, 53), "HLK-PM12", font=f2, fill=(200, 255, 220))
    out = Path(__file__).with_name("IA_Kiln_Driver_PCB_preview.png")
    im.save(out, "PNG")
    print(out)


def build():
    g = G()
    W, H = 90.0, 74.0
    ISO = 38.0
    g.outline_rect(W, H)
    for mx, my in ((5.0, 5.0), (W - 5.0, 5.0), (5.0, H - 5.0), (W - 5.0, H - 5.0)):
        g.hole(mx, my, 3.2)

    g.keepout_band(ISO, 4.8, 50.0, 11.2, 20.2)
    g.silk_text(26.0, 3.0, "ISO GAP", 1.15)
    g.silk_text(5.2, 71.6, "12V AISLADO", 1.1)
    g.silk_text(5.2, 3.0, "IA Kiln  Rev.F", 1.1)
    g.silk_text(52.0, 3.0, "220 VAC", 1.15)

    g.silk_text(5.6, 7.2, "12V", 1.2)
    g.silk_text(6.6, 14.4, "12V", 0.9)
    g.silk_text(10.2, 14.4, "GND", 0.9)
    g.silk_box(6.0, 8.4, 14.8, 13.8, 0.25)
    g.pad(8.0, 11.0, 2.2, 1.3, "V12", "1")
    g.pad(11.5, 11.0, 2.2, 1.3, "GND", "2")

    g.silk_text(5.6, 16.0, "ESP", 1.2)
    g.silk_text(6.2, 22.8, "CTRL", 0.9)
    g.silk_text(10.2, 22.8, "GND", 0.9)
    g.silk_box(6.0, 16.8, 14.8, 22.2, 0.25)
    g.pad(8.0, 19.5, 2.2, 1.3, "CTRL", "1")
    g.pad(11.5, 19.5, 2.2, 1.3, "GND", "2")

    g.silk_text(7.6, 25.6, "Rpd 10k", 1.0)
    g.silk_box(9.2, 26.4, 14.4, 29.6)
    g.pad(8.0, 28.0, 1.8, 0.9, "CTRL", "1")
    g.pad(15.62, 28.0, 1.8, 0.9, "GND", "2")

    g.silk_text(7.6, 32.8, "R1 120", 1.0)
    g.silk_box(9.2, 33.8, 14.4, 37.0)
    g.pad(8.0, 35.5, 1.8, 0.9, "CTRL", "1")
    g.pad(15.62, 35.5, 1.8, 0.9, "LED_A", "2")

    g.silk_text(17.4, 8.4, "C12 100n", 0.9)
    g.pad(19.0, 11.0, 1.8, 0.9, "V12", "1")
    g.pad(24.0, 11.0, 1.8, 0.9, "GND", "2")

    g.silk_text(6.4, 61.2, "C13 220u 25V", 0.9)
    g.silk_text(8.4, 56.6, "+", 1.15)
    g.pad(10.0, 58.0, 2.0, 0.9, "V12", "1")
    g.pad(10.0, 54.5, 2.0, 0.9, "GND", "2")

    ux, uy = 30.38, 12.5
    g.silk_text(ux - 1.2, uy - 3.8, "MOC3020", 1.15)
    g.silk_box(ux + 1.1, uy - 1.4, ux + 6.5, uy + 6.6, 0.25)
    g.shape.append(g.track_s([(ux + 2.6, uy - 1.4), (ux + 5.0, uy - 1.4)], 0.35, 3, ""))
    g.silk_text(ux - 4.4, uy, "1", 1.05)
    g.silk_text(ux + 8.6, uy, "6", 1.05)
    g.pad(ux, uy, 2.0, 0.9, "LED_A", "1")
    g.pad(ux, uy + 2.54, 2.0, 0.9, "GND", "2")
    g.pad(ux, uy + 5.08, 2.0, 0.9, "NC1", "3")
    g.pad(ux + 7.62, uy, 2.0, 0.9, "GATE_DRV", "6")
    g.pad(ux + 7.62, uy + 2.54, 2.0, 0.9, "NC2", "5")
    g.pad(ux + 7.62, uy + 5.08, 2.0, 0.9, "A2", "4")

    g.silk_text(41.0, 9.0, "R2 330", 1.0)
    g.silk_box(42.4, 10.8, 47.6, 14.0)
    g.pad(41.2, 12.5, 1.8, 0.9, "GATE_DRV", "1")
    g.pad(48.82, 12.5, 1.8, 0.9, "GATE", "2")

    q_mt1, q_mt2, q_g, q_y = 59.50, 62.04, 64.58, 24.0
    g.silk_text(51.6, 5.2, "BT138", 1.2)
    g.silk_text(51.2, 26.4, "MT1  MT2  G", 1.05)
    g.silk_text(66.4, 8.2, "DISIPADOR", 0.9)
    g.silk_box(51.0, 6.2, 70.2, 22.2, 0.2)
    g.silk_box(55.2, 10.4, 68.8, 21.6, 0.25)
    g.shape.append(
        g.track_s(
            [(56.0, 10.4), (68.0, 10.4), (66.6, 6.8), (57.2, 6.8), (56.0, 10.4)],
            0.25,
            3,
            "",
        )
    )
    g.hole(q_mt2, 14.0, 3.2)
    g.pad(q_mt1, q_y, 2.4, 1.1, "N", "1")
    g.pad(q_mt2, q_y, 2.4, 1.1, "A2", "2")
    g.pad(q_g, q_y, 2.4, 1.1, "GATE", "3")

    g.silk_text(39.4, 25.4, "RV1 14D431", 1.0)
    g.silk_circle(46.75, 30.0, 8.0, 0.2)
    g.pad(43.0, 30.0, 2.2, 1.1, "A2", "1")
    g.pad(50.5, 30.0, 2.2, 1.1, "N", "2")

    g.silk_text(39.4, 37.4, "Cs 100nF X2 P15", 0.95)
    g.silk_box(43.4, 38.6, 55.6, 43.4, 0.25)
    g.pad(42.0, 41.0, 2.2, 1.1, "A2", "1")
    g.pad(57.0, 41.0, 2.2, 1.1, "SNUB", "2")

    g.silk_text(56.4, 44.6, "Rs 100 2W P20", 0.95)
    g.silk_box(58.0, 45.4, 74.0, 48.6, 0.25)
    g.pad(56.0, 47.0, 2.2, 1.1, "SNUB", "1")
    g.pad(76.0, 47.0, 2.2, 1.1, "N", "2")

    hlk_l = (52.7, 59.5)
    hlk_n = (52.7, 64.5)
    hlk_gnd = (23.3, 54.3)
    hlk_v12 = (23.3, 69.7)
    g.silk_text(22.0, 50.0, "HLK-PM12", 1.15)
    g.silk_box(21.0, 52.0, 55.0, 72.0, 0.25)
    g.silk_text(16.4, 53.6, "0V", 0.9)
    g.silk_text(16.4, 68.8, "+12", 0.9)
    g.silk_text(55.6, 58.6, "AC", 0.9)
    g.silk_text(55.6, 63.8, "AC", 0.9)
    g.pad(*hlk_l, 2.6, 1.3, "L", "1")
    g.pad(*hlk_n, 2.6, 1.3, "N", "2")
    g.pad(*hlk_gnd, 2.6, 1.3, "GND", "3")
    g.pad(*hlk_v12, 2.6, 1.3, "V12", "4")

    g.silk_text(61.0, 54.0, "RV2 14D431", 1.0)
    g.silk_circle(66.75, 62.0, 8.0, 0.2)
    g.pad(63.0, 62.0, 2.2, 1.1, "L", "1")
    g.pad(70.5, 62.0, 2.2, 1.1, "N", "2")

    g.silk_text(76.4, 16.4, "J2", 1.2)
    g.silk_text(76.6, 18.6, "L", 1.35)
    g.silk_text(76.6, 26.2, "N", 1.35)
    g.silk_text(76.0, 33.8, "A2", 1.35)
    g.silk_box(80.2, 16.8, 87.2, 39.0, 0.25)
    g.pad(83.0, 20.00, 3.2, 1.6, "L", "1")
    g.pad(83.0, 27.62, 3.2, 1.6, "N", "2")
    g.pad(83.0, 35.24, 3.2, 1.6, "A2", "3")

    lv, hv, sn = 0.50, 1.20, 0.80

    g.track([(8.0, 19.5), (8.0, 28.0), (8.0, 35.5)], lv, 1, "CTRL")

    g.track([(11.5, 11.0), (11.5, 19.5)], lv, 1, "GND")
    g.track([(11.5, 19.5), (15.62, 19.5), (15.62, 28.0)], lv, 1, "GND")
    g.track([(24.0, 11.0), (24.0, 15.04), (ux, 15.04)], lv, 1, "GND")
    g.track([(15.62, 28.0), (24.0, 28.0)], lv, 1, "GND")
    g.track([(24.0, 15.04), (24.0, 54.3), (23.3, 54.3)], lv, 1, "GND")
    g.track([(23.3, 54.3), (10.0, 54.5)], lv, 1, "GND")

    g.track([(8.0, 11.0), (8.0, 8.0), (19.0, 8.0), (19.0, 11.0)], lv, 2, "V12")
    g.track([(19.0, 11.0), (19.0, 58.0), (10.0, 58.0)], lv, 2, "V12")
    g.track([(19.0, 58.0), (19.0, 69.7), (23.3, 69.7)], lv, 2, "V12")

    g.track(
        [(15.62, 35.5), (15.62, 38.5), (5.6, 38.5), (5.6, 8.0), (ux, 8.0), (ux, 12.5)],
        lv,
        1,
        "LED_A",
    )

    g.track([(ux + 7.62, 12.5), (41.2, 12.5)], lv, 1, "GATE_DRV")
    g.track(
        [(48.82, 12.5), (48.82, 6.5), (86.0, 6.5), (86.0, 24.0), (q_g, 24.0)],
        lv,
        1,
        "GATE",
    )

    g.track([(ux + 7.62, 17.58), (ux + 7.62, 30.0), (43.0, 30.0)], hv, 1, "A2")
    g.track([(43.0, 30.0), (43.0, 21.0), (q_mt2, 21.0), (q_mt2, 24.0)], hv, 1, "A2")
    g.track([(42.0, 41.0), (42.0, 30.0)], hv, 1, "A2")
    g.track([(43.0, 30.0), (43.0, 38.8), (83.0, 38.8), (83.0, 35.24)], hv, 1, "A2")

    g.track([(83.0, 27.62), (q_mt1, 27.62), (q_mt1, 24.0)], hv, 1, "N")
    g.track([(q_mt1, 27.62), (50.5, 27.62), (50.5, 30.0)], hv, 1, "N")
    g.track([(83.0, 27.62), (87.0, 27.62), (87.0, 47.0), (76.0, 47.0)], hv, 1, "N")
    g.track([(76.0, 47.0), (76.0, 64.5), (52.7, 64.5)], hv, 1, "N")
    g.track([(70.5, 64.5), (70.5, 62.0)], hv, 1, "N")

    g.track([(83.0, 20.0), (80.0, 20.0)], hv, 1, "L")
    g.via(80.0, 20.0, "L", 2.2, 1.0)
    g.track([(80.0, 20.0), (80.0, 59.5), (54.0, 59.5)], hv, 2, "L")
    g.via(54.0, 59.5, "L", 2.2, 1.0)
    g.track([(54.0, 59.5), (52.7, 59.5)], hv, 1, "L")
    g.track([(54.0, 59.5), (63.0, 59.5), (63.0, 62.0)], hv, 1, "L")

    g.track([(57.0, 41.0), (56.0, 41.0), (56.0, 47.0)], sn, 1, "SNUB")

    errs = check_clearance(g) + check_isolation(g)
    if errs:
        print("DRC FALLA:")
        for e in errs:
            print(" ", e)
        raise SystemExit(1)
    print("DRC OK  pads", len(g.pads), "tracks", len(g.tracks))
    preview(g, W, H)

    layers = [
        "1~TopLayer~#FF0000~true~true~true~",
        "2~BottomLayer~#0000FF~true~false~true~",
        "3~TopSilkLayer~#FFCC00~true~false~true~",
        "4~BottomSilkLayer~#66CC33~true~false~true~",
        "5~TopPasteMaskLayer~#808080~false~false~false~",
        "6~BottomPasteMaskLayer~#800000~false~false~false~",
        "7~TopSolderMaskLayer~#800080~false~false~false~",
        "8~BottomSolderMaskLayer~#AA00FF~false~false~false~",
        "9~RatlinesLayer~#6464FF~true~false~true~",
        "10~BoardOutLineLayer~#FF00FF~true~false~true~",
        "11~Multi-Layer~#C0C0C0~true~true~true~",
        "12~DocumentLayer~#FFFFFF~true~false~true~",
    ]
    doc = {
        "head": {
            "docType": "3",
            "editorVersion": "6.5.46",
            "title": "IA_Kiln_Driver",
            "description": "Rev.F 90x74. ISO GAP seda. HLK-PM12. MOV courtyard 16mm. J1 12V, ESP CTRL, J2 L/N/A2.",
            "newgId": True,
            "c_para": {},
            "x": str(OX),
            "y": str(OY),
            "hasIdFlag": True,
            "importFlag": 0,
            "transformList": "",
        },
        "canvas": f"CA~1000~1000~#000000~yes~#FFFFFF~10~1000~1000~line~0.254~mm~0.4~45~visible~0.5~{OX}~{OY}~0~yes",
        "shape": g.shape,
        "layers": layers,
        "DRCRULE": {
            "trackWidth": 0.25,
            "track2Track": 0.2,
            "pad2Pad": 0.25,
            "track2Pad": 0.2,
            "hole2Hole": 0.4,
            "holeSize": 0.3,
        },
        "objects": [
            "All~true~false",
            "Component~true~true",
            "Prefix~true~true",
            "Name~true~true",
            "Track~true~true",
            "Pad~true~true",
            "Via~true~true",
            "Hole~true~true",
            "Copper_Area~true~true",
            "Circle~true~true",
            "Arc~true~true",
            "Solid_Region~true~true",
            "Text~true~true",
            "Image~true~true",
            "Rect~true~true",
            "Dimension~true~true",
            "Protractor~true~true",
        ],
        "BBox": {"x": OX, "y": OY, "width": u(W), "height": u(H)},
        "netColors": {},
    }
    OUT.write_text(json.dumps(doc, indent=2), encoding="utf-8")
    print(OUT, "shapes", len(g.shape))


if __name__ == "__main__":
    build()
