#!/usr/bin/env python3
"""Cotas medidas sobre el JSON exportado de EasyEDA (2026-09-06).

Fuente: pcb/PCB_IA_Kiln_Driver_PCB_2026-09-06.json
No usa el generador Rev.F: lee pads, agujeros y pasos reales del archivo.
"""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path

from reportlab.lib.pagesizes import A3, A4, landscape
from reportlab.lib.units import mm
from reportlab.lib.colors import HexColor, white
from reportlab.pdfgen import canvas

DOCS = Path(__file__).resolve().parent
sys.path.insert(0, str(DOCS))
from gen_cotas_pcb import (  # noqa: E402
    AC,
    BOARD,
    D,
    DIM,
    FILL_NOTE,
    FILL_OK,
    GOLD,
    ISO,
    LINE,
    MUTED,
    NAVY,
    PSU,
)

SRC = DOCS.parent / "pcb" / "PCB_IA_Kiln_Driver_PCB_2026-09-06.json"
OUT = DOCS / "IA_Kiln_Driver_Cotas_2026-09-06.pdf"
OX, OY = 4000.0, 3000.0
U = 0.254
W_PCB, H_PCB = 82.0, 64.0


def to_mm(u, origin):
    return (float(u) - origin) * U


def parse(path: Path):
    doc = json.loads(path.read_text(encoding="utf-8"))
    pads = []
    holes = []
    for s in doc["shape"]:
        t = s.split("~")
        if t[0] == "PAD":
            pads.append(
                {
                    "x": to_mm(t[2], OX),
                    "y": to_mm(t[3], OY),
                    "dp": float(t[4]) * U,
                    "dh": float(t[9]) * U * 2,
                    "net": t[7],
                    "num": t[8],
                }
            )
        elif t[0] == "HOLE":
            holes.append((to_mm(t[1], OX), to_mm(t[2], OY), float(t[3]) * U * 2))
    return pads, holes


def pitch(a, b):
    return math.hypot(a["x"] - b["x"], a["y"] - b["y"])


def fmt(v):
    s = f"{v:.2f}".replace(".", ",")
    if s.endswith("0"):
        s = s[:-1]
    if s.endswith("0") and "," in s:
        s = s[:-1].rstrip(",")
        if "," not in s:
            return s
    return s


def header(d: D, wm, hm, title, sub, rev="EasyEDA  2026-09-06"):
    d.c.setFillColor(NAVY)
    d.c.rect(0, (hm - 16) * mm, wm * mm, 16 * mm, fill=1, stroke=0)
    d.text(10, hm - 7.2, title, 13, True, white)
    d.text(10, hm - 13.2, sub, 8, False, HexColor("#D5DEEC"))
    d.text(wm - 10, hm - 7.2, rev, 10, True, white, "right")
    d.text(wm - 10, hm - 13.2, "PCB 82 × 64 mm  ·  Rev. E editada", 8, False, HexColor("#D5DEEC"), "right")
    d.c.setFillColor(HexColor("#F3F4F6"))
    d.c.rect(0, 0, wm * mm, 12 * mm, fill=1, stroke=0)
    d.c.setStrokeColor(NAVY)
    d.c.setLineWidth(0.5)
    d.c.line(0, 12 * mm, wm * mm, 12 * mm)
    d.text(
        10,
        4.6,
        "Medidas leídas del JSON. Imprimir al 100 %. Esta no es la Rev. F (90 × 74).",
        7.4,
        False,
        NAVY,
    )


def Pxy(x, y, ox, oy, sc):
    return ox + x * sc, oy + (H_PCB - y) * sc


def draw_board(d: D, pads, holes, ox, oy, sc, labels=True):
    d.rect(ox, oy, W_PCB * sc, H_PCB * sc, BOARD, HexColor("#1A3324"), 1.1)
    iso_x = ox + 32.3 * sc
    d.line(iso_x, oy + (H_PCB - 39.4) * sc, iso_x, oy + (H_PCB - 19.8) * sc, ISO, 1.3)
    d.line(iso_x, oy + (H_PCB - 6.6) * sc, iso_x, oy + (H_PCB - 4.6) * sc, ISO, 1.3)

    hlk = (21.0, 42.4, 34.2, 20.2)
    hx, hy = Pxy(hlk[0], hlk[1] + hlk[3], ox, oy, sc)
    d.rect(hx, hy, hlk[2] * sc, hlk[3] * sc, HexColor("#CDE7D8"), PSU, 0.5)

    for p in pads:
        px, py = Pxy(p["x"], p["y"], ox, oy, sc)
        d.pad(px, py, p["dp"], p["dh"], sc)
    for x, y, dia in holes:
        px, py = Pxy(x, y, ox, oy, sc)
        d.circ(px, py, dia * sc / 2, HexColor("#222"), HexColor("#EEE"), 0.35)

    if labels and sc >= 1.6:
        d.text(*Pxy(5.2, 7.2, ox, oy, sc), "J1", 7, True, white)
        d.text(*Pxy(5.2, 18.5, ox, oy, sc), "J3", 7, True, white)
        d.text(*Pxy(22.0, 41.2, ox, oy, sc), "U2 HLK-PM12", 7, True, HexColor("#0B3D2A"))
        d.text(*Pxy(51.5, 4.8, ox, oy, sc), "Q2 BT138", 7, True, HexColor("#5C2A1A"))
        d.text(*Pxy(71.5, 25.2, ox, oy, sc), "J2", 8, True, AC)
        d.text(*Pxy(40.2, 24.4, ox, oy, sc), "RV1", 7, True, AC)
        d.text(*Pxy(62.2, 51.2, ox, oy, sc), "RV2", 7, True, AC)
        d.text(*Pxy(27.2, 7.6, ox, oy, sc), "U1 MOC", 7, True, NAVY)
        d.text(*Pxy(41.0, 29.6, ox, oy, sc), "Cs", 6.5, True, HexColor("#6B4F1A"))
        d.text(*Pxy(38.2, 38.6, ox, oy, sc), "Rs", 6.5, True, HexColor("#6B4F1A"))


def fp_header(d, x, y, w, h, title, warn=False):
    stroke = AC if warn else NAVY
    d.round_box(x, y, w, h, white, stroke, 0.8, 2.4)
    d.c.setFillColor(AC if warn else NAVY)
    d.c.rect(x * mm, (y + h - 9) * mm, w * mm, 9 * mm, fill=1, stroke=0)
    d.text(x + 4, y + h - 6.4, title, 8.4, True, white)


def page1(c, pads, holes):
    wm, hm = landscape(A3)[0] / mm, landscape(A3)[1] / mm
    d = D(c)
    header(
        d,
        wm,
        hm,
        "IA Kiln  ·  Cotas medidas del archivo EasyEDA",
        "PCB_IA_Kiln_Driver_PCB_2026-09-06.json  ·  pads y agujeros reales",
    )
    sc = 2.35
    ox, oy = 26.0, 28.0
    draw_board(d, pads, holes, ox, oy, sc)
    d.dim_h(ox, ox + W_PCB * sc, oy + H_PCB * sc + 8.2, "82,0 mm")
    d.dim_v(oy, oy + H_PCB * sc, ox - 10.0, "64,0 mm", -1)
    d.dim_h(ox, ox + 3.5 * sc, oy + H_PCB * sc + 3.2, "3,5")
    d.dim_h(ox + 78.5 * sc, ox + W_PCB * sc, oy + H_PCB * sc + 3.2, "3,5")
    d.dim_h(ox, ox + 32.3 * sc, oy - 5.2, "seda ISO  x ≈ 32,3 mm", -1)

    d.round_box(236, 18, 170, hm - 40, FILL_NOTE, NAVY, 0.8, 3)
    d.text(244, hm - 30, "Qué es este archivo", 11, True, NAVY)
    lines = [
        "Export de EasyEDA Std 6.5.57, basado en",
        "la Rev. E compacta (82 × 64 mm).",
        "Alguien movió bornes, R2, Cs, Rs y RV2.",
        "",
        "No es la Rev. F (90 × 74). Si mandás",
        "esta a fabricar, las cotas de abajo",
        "son las que van a salir en el cobre.",
        "",
        "Coincide con pieza local típica:",
        "  J1/J3 paso 3,50  (girados 90°)",
        "  J2 paso 7,62",
        "  MOC 2,54 × 7,62",
        "  BT138 paso 2,54",
        "  HLK 5,00 / 15,40 / 29,40",
        "  C13 paso 3,50   R1/Rpd 7,62",
        "",
        "No coincide (medir la pieza):",
        "  RV2  6,78 mm  (14D431 = 7,50)",
        "  RV1  7,74 mm  (típico 7,50)",
        "  Cs   14,26 mm (X2 P=10 o P=15)",
        "  Rs   17,32 mm (2 W suele 15–20)",
        "  R2   8,25 mm  (axial ¼ W = 7,62)",
        "  C12  6,84 mm  (cerámico ~5,0)",
        "",
        "Agujeros más justos que Rev. F:",
        "  J1/J3 Ø 1,10   DIP Ø 0,80",
        "  HLK Ø 1,20     MOV/X2/Rs Ø 1,00",
        "  M3 a 3,5 mm del borde",
    ]
    yy = hm - 38
    for line in lines:
        col = AC if line.startswith("  RV2") or line.startswith("No coincide") else LINE
        if line.startswith("  RV2") or line.startswith("  Cs") or line.startswith("  C12"):
            col = AC
        d.text(244, yy, line, 7.35, False, col)
        yy -= 5.15

    c.showPage()


def page2(c, pads):
    wm, hm = landscape(A3)[0] / mm, landscape(A3)[1] / mm
    d = D(c)
    header(d, wm, hm, "IA Kiln  ·  Detalle medido  ·  2026-09-06", "Paso real  vs  pieza local típica")

    rows = [
        ["Ref", "Paso en ESTA placa", "Ø pad", "Ø agujero", "Pieza local típica", "¿Entra?"],
        ["J1 / J3", "3,50 mm  (vertical)", "2,00", "1,10", "KF128 / WJ350  P=3,50  pin ≤1,0", "Sí, bloque a 90°"],
        ["J2", "7,62 + 7,62 mm", "2,80", "1,60", "DG128 / KF762  3P  P=7,62", "Sí"],
        ["U1 MOC3020", "2,54 × 7,62 mm", "2,00", "0,80", "DIP-6  pin ≤0,70", "Justo (mejor 0,90)"],
        ["R2 330 Ω", "8,25 mm  (vertical)", "1,80", "0,90", "axial ¼ W  P=7,62 o 10", "Hay que abrir patas"],
        ["R1 / Rpd", "7,62 / 7,61 mm", "1,80", "0,90", "axial ¼ W  P=7,62", "Sí"],
        ["Q2 BT138", "2,54 + 2,54 mm", "2,20", "1,00", "TO-220  paso 2,54", "Sí"],
        ["  pestaña", "9,50 mm a la fila", "—", "3,20 (M3)", "agujero tab ~Ø3,6", "Tornillo M3"],
        ["U2 HLK-PM12", "AC 5,00  DC 15,40", "2,40", "1,20", "filas 29,40  pin ≤1,10", "Sí (agujero justo)"],
        ["  filas", "29,40 mm AC–DC", "—", "—", "cuerpo ~34,8 × 20,2", "Seda 34,2 × 20,2"],
        ["RV1 14D431", "7,74 mm", "2,20", "1,00", "Ø14  P=7,50", "Holgura 0,24 mm"],
        ["RV2 14D431", "6,78 mm  (vertical)", "2,20", "1,00", "Ø14  P=7,50", "NO: 0,72 mm corto"],
        ["Cs X2 100 nF", "14,26 mm", "2,00", "1,00", "P=10 o P=15", "P=15 apretado / P=10 no"],
        ["Rs 100 Ω 2 W", "17,32 mm", "2,00", "1,00", "cuerpo ~16 × Ø5  P=15–20", "Sí para 2 W"],
        ["C12 100 nF", "6,84 mm", "1,80", "0,80", "cerámico P=5,08 / 5,0", "Hay que abrir patas"],
        ["C13 220 µF", "3,50 mm", "2,00", "0,90", "radial P=3,5 o 5,0", "Sí si P=3,5"],
        ["M3 (4×)", "3,50 mm al borde", "—", "3,20", "tornillo M3", "Cerca del borde"],
    ]

    x0, y0 = 12, hm - 30
    col_w = [32, 48, 22, 28, 70, 52]
    row_h = 8.15
    d.c.setFillColor(NAVY)
    d.c.rect(x0 * mm, (y0 - 1.4) * mm, sum(col_w) * mm, 8.4 * mm, fill=1, stroke=0)
    x = x0
    for i, head in enumerate(rows[0]):
        d.text(x + 1.4, y0 + 0.8, head, 7.6, True, white)
        x += col_w[i]

    warn_refs = {"RV2 14D431", "Cs X2 100 nF", "C12 100 nF", "R2 330 Ω"}
    y = y0 - 9.0
    for r, row in enumerate(rows[1:]):
        bad = row[0] in warn_refs
        fill = HexColor("#FDECEC") if bad else (HexColor("#F3F6FB") if r % 2 == 0 else white)
        d.c.setFillColor(fill)
        d.c.rect(x0 * mm, (y - 2.0) * mm, sum(col_w) * mm, row_h * mm, fill=1, stroke=0)
        x = x0
        for i, cell in enumerate(row):
            col = AC if bad and i in (1, 5) else LINE
            d.text(x + 1.4, y, cell, 7.15, i == 0, col)
            x += col_w[i]
        y -= row_h

    d.round_box(12, 16, 248, 28, HexColor("#FDECEC"), AC, 0.85, 2)
    d.text(16, 36, "Antes de producir ESTE gerber", 9, True, AC)
    d.text(16, 28.2, "RV2 a 6,78 mm no acepta un 14D431 de 7,50 mm sin forzar. Cs a 14,26 mm no es P=10 ni P=15 limpio.", 7.3, False, LINE)
    d.text(16, 21.0, "Si las piezas locales son las de la lista Rev. F, usá esa placa (90 × 74), no este JSON.", 7.3, False, LINE)

    d.round_box(268, 16, 138, 52, FILL_OK, PSU, 0.8, 2)
    d.text(274, 56, "Holgura de pin", 9, True, PSU)
    d.text(274, 47, "Ø pin  ≤  Ø agujero − 0,2 mm", 7.3, True, LINE)
    d.text(274, 39, "DIP 0,80 y C12 0,80 son justos:", 7.2, False, LINE)
    d.text(274, 32, "el esmalte del pin DIP a veces", 7.2, False, LINE)
    d.text(274, 25, "no entra. Mejor 0,90 mm.", 7.2, False, LINE)

    c.showPage()


def page3(c, pads, holes):
    wm, hm = landscape(A4)[0] / mm, landscape(A4)[1] / mm
    d = D(c)
    header(
        d,
        wm,
        hm,
        "IA Kiln  ·  Plantilla 1:1  ·  archivo 2026-09-06",
        "Apoyá las piezas sobre los pads. Si no calzan, no mandes este gerber.",
    )
    ox, oy = 12.0, 22.0
    draw_board(d, pads, holes, ox, oy, 1.0, labels=True)
    d.dim_h(ox, ox + 82.0, oy + 64.0 + 6.2, "82,0 mm")
    d.dim_v(oy, oy + 64.0, ox + 82.0 + 6.0, "64,0 mm")

    d.round_box(168, 18, 118, 48, FILL_OK, PSU, 0.9, 2)
    d.text(176, 56, "Barra de escala", 9, True, PSU)
    d.line(176, 42, 226, 42, PSU, 1.6)
    d.line(176, 39.6, 176, 44.4, PSU, 1.2)
    d.line(226, 39.6, 226, 44.4, PSU, 1.2)
    d.text(201, 46.5, "50,0 mm", 8, True, PSU, "center")
    d.text(176, 30, "Si no mide 50 mm, reimprimí", 7.2, False, LINE)
    d.text(176, 23.5, "sin “ajustar a la página”.", 7.2, False, LINE)

    d.round_box(168, 72, 118, hm - 100, FILL_NOTE, NAVY, 0.8, 2)
    d.text(176, hm - 36, "Checklist de ESTA placa", 10, True, NAVY)
    chk = [
        ("J1/J3  3,50 vertical", False),
        ("J2  7,62  tres pines", False),
        ("MOC  2,54 × 7,62", False),
        ("BT138  2,54 + pestaña M3", False),
        ("HLK  5,00 / 15,40 / 29,40", False),
        ("RV1  7,74  (típico 7,50)", True),
        ("RV2  6,78  ≠ 7,50  OJO", True),
        ("Cs  14,26  ≠ P=10 ni 15", True),
        ("Rs  17,32  (2 W OK)", False),
        ("R2  8,25  ≠ 7,62", True),
        ("C12  6,84  ≠ 5,0", True),
        ("C13  3,50", False),
    ]
    yy = hm - 46
    for line, bad in chk:
        d.text(176, yy, ("☐  " + line), 7.5, bad, AC if bad else LINE)
        yy -= 6.55

    c.showPage()


def render_preview(pdf_path: Path):
    try:
        import pypdfium2 as pdfium
    except ImportError:
        return
    doc = pdfium.PdfDocument(str(pdf_path))
    names = (
        "IA_Kiln_Driver_Cotas_2026-09-06_p1.png",
        "IA_Kiln_Driver_Cotas_2026-09-06_p2.png",
        "IA_Kiln_Driver_Cotas_2026-09-06_1a1.png",
    )
    for i, name in enumerate(names):
        if i >= len(doc):
            break
        img = doc[i].render(scale=1.55).to_pil()
        prev = pdf_path.with_name(name)
        img.save(prev, "PNG")
        print(prev)


def main():
    pads, holes = parse(SRC)
    c = canvas.Canvas(str(OUT), pagesize=landscape(A3))
    c.setTitle("IA Kiln — Cotas EasyEDA 2026-09-06")
    c.setAuthor("IA Kiln")
    c.setSubject("Medidas reales del JSON exportado de EasyEDA Rev.E")
    page1(c, pads, holes)
    page2(c, pads)
    c.setPageSize(landscape(A4))
    page3(c, pads, holes)
    c.save()
    dest = SRC.with_name("IA_Kiln_Driver_Cotas_2026-09-06.pdf")
    try:
        dest.write_bytes(OUT.read_bytes())
        print(dest)
    except OSError as exc:
        print("skip", dest, exc)
    print(OUT)
    render_preview(OUT)


if __name__ == "__main__":
    main()
