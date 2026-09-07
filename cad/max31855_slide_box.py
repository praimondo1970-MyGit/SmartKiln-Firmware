# -*- coding: utf-8 -*-
"""
SmartKiln — cápsula PLA, tapa corredera, MAX31855
=================================================
v1: salidas de cable por ORIFICIOS pequeños (no ranuras en U).

Placa: breakout genérico 25 x 25 mm, 4 agujeros M2 paso 20 mm,
borne 2 pines + header 5 pines (Dupont queda ADENTRO).

Cómo generar
------------
  "C:\\Program Files\\FreeCAD 1.1\\bin\\freecadcmd.exe" max31855_slide_box.py

O en FreeCAD: Macro → Ejecutar este archivo.

Si el PCB no entra, cambiá SOLO el bloque PARAMETROS.

Impresión (PLA, 0.2 mm, 3 perímetros, 25 % infill, sin soportes)
---------------------------------------------------------------
  MAX31855_Base.stl  → piso sobre la cama
  MAX31855_Tapa.stl  → cara de ranuras sobre la cama
"""

from __future__ import annotations

import os
import sys

import FreeCAD as App
import MeshPart
import Part

# ---------------------------------------------------------------------------
# PARAMETROS  (mm)
# ---------------------------------------------------------------------------

PCB_L = 25.0
PCB_W = 25.0
PCB_T = 1.6
HOLE_D = 2.2
HOLE_INSET = 2.5
STANDOFF_H = 1.6
STANDOFF_D = 4.2

DUPONT_EXTRA = 16.0
TC_RELIEF = 8.0

OUTER_L = 62.0
OUTER_W = 32.0
OUTER_H = 18.0
WALL = 2.2
FLOOR = 1.8
CORNER_R = 2.0

LID_T = 1.8
GROOVE_D = 1.3
GROOVE_Z_CLEAR = 0.35
LID_XY_CLEAR = 0.40

# Orificios v1 (pequeños; agrandar después si hace falta)
TC_HOLE_D = 3.2
TC_HOLE_N = 2
TC_HOLE_PITCH = 5.5
SPI_HOLE_D = 5.5
HOLE_Z_FROM_FLOOR = 6.8

VENT_N = 6
VENT_L = 18.0
VENT_W = 1.3
VENT_PITCH = 2.6

SHOW_DUMMY = True
OUT_DIR = os.path.dirname(os.path.abspath(__file__))
DOC_NAME = "MAX31855_Capsula"


def box(x, y, z, dx, dy, dz):
    return Part.makeBox(dx, dy, dz, App.Vector(x, y, z))


def cyl(x, y, z, r, h, axis=App.Vector(1, 0, 0)):
    return Part.makeCylinder(r, h, App.Vector(x, y, z), axis)


def fuse_all(shapes):
    acc = shapes[0]
    for s in shapes[1:]:
        acc = acc.fuse(s)
    return acc.removeSplitter()


def cut_all(base, tools):
    acc = base
    for t in tools:
        nxt = acc.cut(t)
        if nxt.isValid():
            acc = nxt.removeSplitter()
    return acc


def rrect(x, y, z, dx, dy, dz, r):
    r = min(float(r), dx / 2.0 - 0.05, dy / 2.0 - 0.05)
    if r < 0.4:
        return box(x, y, z, dx, dy, dz)
    core = box(x + r, y, z, dx - 2.0 * r, dy, dz)
    mid = box(x, y + r, z, dx, dy - 2.0 * r, dz)
    corners = [
        Part.makeCylinder(r, dz, App.Vector(x + r, y + r, z)),
        Part.makeCylinder(r, dz, App.Vector(x + dx - r, y + r, z)),
        Part.makeCylinder(r, dz, App.Vector(x + r, y + dy - r, z)),
        Part.makeCylinder(r, dz, App.Vector(x + dx - r, y + dy - r, z)),
    ]
    return fuse_all([core, mid] + corners)


def add_shape(doc, name, shape, rgb):
    obj = doc.addObject("Part::Feature", name)
    obj.Shape = shape
    try:
        obj.ViewObject.ShapeColor = rgb
        obj.ViewObject.DisplayMode = "Shaded"
    except Exception:
        pass
    return obj


def export_stl(shape, path):
    mesh = MeshPart.meshFromShape(
        Shape=shape.removeSplitter(),
        LinearDeflection=0.06,
        AngularDeflection=0.4,
        Relative=False,
    )
    try:
        mesh.harmonizeNormals()
    except Exception:
        pass
    mesh.write(path)
    print("  STL:", path)


def export_step(shape, path):
    shape.exportStep(path)
    print("  STEP:", path)


def layout():
    inner_w = OUTER_W - 2 * WALL
    inner_l = OUTER_L - 2 * WALL
    inner_h = OUTER_H - FLOOR
    pcb_x = WALL + TC_RELIEF
    pcb_y = (OUTER_W - PCB_W) / 2.0
    lid_z = FLOOR + inner_h - LID_T - GROOVE_Z_CLEAR
    groove_h = LID_T + GROOVE_Z_CLEAR
    lid_w = inner_w + 2 * GROOVE_D - LID_XY_CLEAR
    lid_l = OUTER_L - WALL - 0.5
    hole_z = FLOOR + HOLE_Z_FROM_FLOOR
    return {
        "inner_w": inner_w,
        "inner_l": inner_l,
        "inner_h": inner_h,
        "pcb_x": pcb_x,
        "pcb_y": pcb_y,
        "lid_z": lid_z,
        "groove_h": groove_h,
        "lid_w": lid_w,
        "lid_l": lid_l,
        "hole_z": hole_z,
    }


def make_base(L):
    solid = rrect(0, 0, 0, OUTER_L, OUTER_W, OUTER_H, CORNER_R)

    cavity = box(
        WALL,
        WALL,
        FLOOR,
        L["inner_l"],
        L["inner_w"],
        L["inner_h"] + 1.0,
    )
    solid = solid.cut(cavity)

    # Rieles: abiertos en el extremo SPI (X=OUTER_L), tope en la pared TC
    groove = box(
        WALL - 0.05,
        WALL - GROOVE_D,
        L["lid_z"],
        L["inner_l"] + WALL + 0.3,
        L["inner_w"] + 2 * GROOVE_D,
        L["groove_h"],
    )
    solid = solid.cut(groove)

    # Ranura de paso de la tapa en la pared SPI (buzón)
    lid_slot = box(
        OUTER_L - WALL - 0.2,
        (OUTER_W - L["lid_w"]) / 2.0 - 0.15,
        L["lid_z"],
        WALL + 0.6,
        L["lid_w"] + 0.30,
        L["groove_h"] + 0.15,
    )
    solid = solid.cut(lid_slot)

    # Chaflán de entrada 45° en el borde SPI (imprimible)
    ch_y0 = (OUTER_W - L["lid_w"]) / 2.0 - 0.2
    chamfer = box(OUTER_L - 1.4, ch_y0, OUTER_H - 1.4, 1.6, L["lid_w"] + 0.4, 1.6)
    solid = solid.cut(chamfer)

    cuts = []
    # 2 orificios termocupla
    y0 = OUTER_W / 2.0 - (TC_HOLE_N - 1) * TC_HOLE_PITCH / 2.0
    for i in range(TC_HOLE_N):
        cuts.append(
            cyl(-0.4, y0 + i * TC_HOLE_PITCH, L["hole_z"], TC_HOLE_D / 2.0, WALL + 0.8)
        )
    # 1 orificio SPI (cables del Dupont; el housing queda adentro)
    cuts.append(
        cyl(
            OUTER_L - WALL - 0.4,
            OUTER_W / 2.0,
            L["hole_z"],
            SPI_HOLE_D / 2.0,
            WALL + 0.8,
        )
    )
    solid = cut_all(solid, cuts)

    adds = []
    for ix in (HOLE_INSET, PCB_L - HOLE_INSET):
        for iy in (HOLE_INSET, PCB_W - HOLE_INSET):
            boss = Part.makeCylinder(
                STANDOFF_D / 2.0,
                STANDOFF_H,
                App.Vector(L["pcb_x"] + ix, L["pcb_y"] + iy, FLOOR),
            )
            hole = Part.makeCylinder(
                0.85,
                STANDOFF_H + 0.2,
                App.Vector(L["pcb_x"] + ix, L["pcb_y"] + iy, FLOOR + 0.3),
            )
            adds.append(boss.cut(hole))

    # Costilla: la placa no se va al tirar el Dupont
    adds.append(
        box(
            L["pcb_x"] + PCB_L + 0.25,
            L["pcb_y"] + 1.4,
            FLOOR,
            1.2,
            PCB_W - 2.8,
            STANDOFF_H + PCB_T + 0.3,
        )
    )
    solid = fuse_all([solid] + adds)
    return solid.removeSplitter()


def make_lid(L):
    x = 0.0
    y = (OUTER_W - L["lid_w"]) / 2.0
    solid = box(x, y, 0, L["lid_l"], L["lid_w"], LID_T)

    cuts = []
    vent0 = TC_RELIEF + 4.0
    for i in range(VENT_N):
        cuts.append(
            box(
                vent0 + i * VENT_PITCH,
                (OUTER_W - VENT_L) / 2.0,
                -0.2,
                VENT_W,
                VENT_L,
                LID_T + 0.4,
            )
        )
    # Muesca de uña, extremo SPI
    cuts.append(
        box(L["lid_l"] - 8.0, OUTER_W / 2.0 - 6.0, LID_T - 0.65, 8.2, 12.0, 0.9)
    )
    return cut_all(solid, cuts)


def make_dummy(L):
    z = FLOOR + STANDOFF_H
    pcb = box(L["pcb_x"], L["pcb_y"], z, PCB_L, PCB_W, PCB_T)
    term = box(L["pcb_x"] + 0.8, L["pcb_y"] + 6.5, z + PCB_T, 6.0, 12.0, 8.5)
    hdr = box(L["pcb_x"] + PCB_L - 2.2, L["pcb_y"] + 5.5, z + PCB_T, 2.4, 14.0, 8.0)
    return pcb, term, hdr


def main():
    L = layout()
    print("MAX31855 capsula v1")
    print("  Exterior: {:.1f} x {:.1f} x {:.1f} mm".format(OUTER_L, OUTER_W, OUTER_H))
    print("  Tapa:     {:.1f} x {:.1f} x {:.1f} mm".format(L["lid_l"], L["lid_w"], LID_T))
    print("  TC:       {:d} x Ø{:.1f} mm".format(TC_HOLE_N, TC_HOLE_D))
    print("  SPI:      1 x Ø{:.1f} mm".format(SPI_HOLE_D))

    doc = App.newDocument(DOC_NAME)
    base = make_base(L)
    lid = make_lid(L)
    lid_placed = lid.copy()
    lid_placed.translate(App.Vector(WALL, 0, L["lid_z"]))

    add_shape(doc, "Base", base, (0.20, 0.20, 0.22))
    add_shape(doc, "Tapa", lid_placed, (0.28, 0.28, 0.30))

    if SHOW_DUMMY:
        pcb, term, hdr = make_dummy(L)
        add_shape(doc, "Ref_PCB", pcb, (0.12, 0.38, 0.70))
        add_shape(doc, "Ref_Borne", term, (0.15, 0.55, 0.25))
        add_shape(doc, "Ref_Header", hdr, (0.08, 0.08, 0.08))

    doc.recompute()
    fcstd = os.path.join(OUT_DIR, "MAX31855_Capsula.FCStd")
    doc.saveAs(fcstd)
    print("  FCStd:", fcstd)

    export_stl(base, os.path.join(OUT_DIR, "MAX31855_Base.stl"))
    export_stl(lid, os.path.join(OUT_DIR, "MAX31855_Tapa.stl"))
    export_step(base, os.path.join(OUT_DIR, "MAX31855_Base.step"))
    export_step(lid, os.path.join(OUT_DIR, "MAX31855_Tapa.step"))
    print("Listo.")
    return 0


def _run():
    log_path = os.path.join(OUT_DIR, "_max31855_build.log")
    try:
        code = main() or 0
        with open(log_path, "w", encoding="utf-8") as fh:
            fh.write("OK\n")
        return code
    except Exception:
        import traceback

        text = traceback.format_exc()
        with open(log_path, "w", encoding="utf-8") as fh:
            fh.write(text)
        sys.stderr.write(text)
        raise


_run()
