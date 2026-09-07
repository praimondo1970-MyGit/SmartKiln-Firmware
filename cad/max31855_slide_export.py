# -*- coding: utf-8 -*-
"""Export STEP/STL of the v1 sliding capsule (same numbers as max31855_slide_box.py)."""
from pathlib import Path
import cadquery as cq

PCB_L, PCB_W, PCB_T = 25.0, 25.0, 1.6
HOLE_INSET = 2.5
STANDOFF_H, STANDOFF_D = 1.6, 4.2
TC_RELIEF = 8.0

OUTER_L, OUTER_W, OUTER_H = 62.0, 32.0, 18.0
WALL, FLOOR, CORNER_R = 2.2, 1.8, 2.0
LID_T, GROOVE_D, GROOVE_Z_CLEAR, LID_XY_CLEAR = 1.8, 1.3, 0.35, 0.40

TC_HOLE_D, TC_HOLE_N, TC_HOLE_PITCH = 3.2, 2, 5.5
SPI_HOLE_D, HOLE_Z_FROM_FLOOR = 5.5, 6.8
VENT_N, VENT_L, VENT_W, VENT_PITCH = 6, 18.0, 1.3, 2.6

INNER_W = OUTER_W - 2 * WALL
INNER_L = OUTER_L - 2 * WALL
INNER_H = OUTER_H - FLOOR
PCB_X = WALL + TC_RELIEF
PCB_Y = (OUTER_W - PCB_W) / 2.0
LID_Z = FLOOR + INNER_H - LID_T - GROOVE_Z_CLEAR
GROOVE_H = LID_T + GROOVE_Z_CLEAR
LID_W = INNER_W + 2 * GROOVE_D - LID_XY_CLEAR
LID_L = OUTER_L - WALL - 0.5
HOLE_Z = FLOOR + HOLE_Z_FROM_FLOOR
OUT = Path(__file__).resolve().parent


def make_base() -> cq.Workplane:
    body = (
        cq.Workplane("XY")
        .box(OUTER_L, OUTER_W, OUTER_H, centered=False)
        .edges("|Z")
        .fillet(CORNER_R)
    )
    cavity = cq.Workplane("XY").box(INNER_L, INNER_W, INNER_H + 1.0, centered=False)
    cavity = cavity.translate((WALL, WALL, FLOOR))
    body = body.cut(cavity)

    groove = cq.Workplane("XY").box(
        INNER_L + WALL + 0.4, INNER_W + 2 * GROOVE_D, GROOVE_H, centered=False
    )
    groove = groove.translate((WALL - 0.05, WALL - GROOVE_D, LID_Z))
    body = body.cut(groove)

    lid_slot = cq.Workplane("XY").box(WALL + 0.8, LID_W + 0.30, GROOVE_H + 0.15, centered=False)
    lid_slot = lid_slot.translate(
        (OUTER_L - WALL - 0.3, (OUTER_W - LID_W) / 2.0 - 0.15, LID_Z)
    )
    body = body.cut(lid_slot)

    y0 = OUTER_W / 2.0 - (TC_HOLE_N - 1) * TC_HOLE_PITCH / 2.0
    for i in range(TC_HOLE_N):
        hole = (
            cq.Workplane("YZ")
            .workplane(offset=-0.5)
            .center(y0 + i * TC_HOLE_PITCH, HOLE_Z)
            .circle(TC_HOLE_D / 2.0)
            .extrude(WALL + 1.2)
        )
        body = body.cut(hole)

    spi = (
        cq.Workplane("YZ")
        .workplane(offset=OUTER_L - WALL - 0.5)
        .center(OUTER_W / 2.0, HOLE_Z)
        .circle(SPI_HOLE_D / 2.0)
        .extrude(WALL + 1.2)
    )
    body = body.cut(spi)

    for ix in (HOLE_INSET, PCB_L - HOLE_INSET):
        for iy in (HOLE_INSET, PCB_W - HOLE_INSET):
            boss = (
                cq.Workplane("XY")
                .transformed(offset=cq.Vector(PCB_X + ix, PCB_Y + iy, FLOOR))
                .circle(STANDOFF_D / 2.0)
                .extrude(STANDOFF_H)
                .faces(">Z")
                .hole(1.7, STANDOFF_H)
            )
            body = body.union(boss)

    rib = cq.Workplane("XY").box(1.2, PCB_W - 2.8, STANDOFF_H + PCB_T + 0.3, centered=False)
    rib = rib.translate((PCB_X + PCB_L + 0.25, PCB_Y + 1.4, FLOOR))
    body = body.union(rib)
    return body


def make_lid() -> cq.Workplane:
    lid = cq.Workplane("XY").box(LID_L, LID_W, LID_T, centered=False)
    lid = lid.translate((0, (OUTER_W - LID_W) / 2.0, 0))
    vent0 = TC_RELIEF + 4.0
    for i in range(VENT_N):
        slot = cq.Workplane("XY").box(VENT_W, VENT_L, LID_T + 0.6, centered=False)
        slot = slot.translate(
            (vent0 + i * VENT_PITCH, (OUTER_W - VENT_L) / 2.0, -0.2)
        )
        lid = lid.cut(slot)
    notch = cq.Workplane("XY").box(8.2, 12.0, 0.9, centered=False)
    notch = notch.translate((LID_L - 8.0, OUTER_W / 2.0 - 6.0, LID_T - 0.65))
    lid = lid.cut(notch)
    return lid


def main():
    base = make_base()
    lid = make_lid()
    cq.exporters.export(base, str(OUT / "MAX31855_Base.step"))
    cq.exporters.export(lid, str(OUT / "MAX31855_Tapa.step"))
    cq.exporters.export(base, str(OUT / "MAX31855_Base.stl"))
    cq.exporters.export(lid, str(OUT / "MAX31855_Tapa.stl"))
    asm = base.union(lid.translate((WALL, 0, LID_Z)))
    cq.exporters.export(asm, str(OUT / "MAX31855_Ensamble.step"))
    print("INNER", INNER_L, INNER_W, INNER_H)
    print("LID", LID_L, LID_W, LID_T, "z", LID_Z)
    print("exported to", OUT)


if __name__ == "__main__":
    main()
