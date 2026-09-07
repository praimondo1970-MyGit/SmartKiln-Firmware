# -*- coding: utf-8 -*-
"""
SmartKiln — caja 3D para display + controlador
==============================================
Placa: ESP32-4827S043N/R/C (Jingcai, 4.3" 480x272 + ESP32-S3)
Cotas según "ESP32-4827S043 Specifications-EN.pdf":
  PCB 123.0 x 74.0 mm, 4 agujeros D3.2 a 3.5 mm de cada borde
  LCD 105.5 x 67.2, área activa 95.04 x 53.86
  USB-C y conector 4P 1.25 a la DERECHA (vista de frente)
  TF arriba; BOOT/RST a la IZQUIERDA arriba

Cómo generar el modelo
----------------------
  "C:\\Program Files\\FreeCAD 1.1\\bin\\freecadcmd.exe" smartkiln_box.py

O en FreeCAD: Macro → Ejecutar este archivo.

Si tu placa no entra o el USB no alinea, cambiá SOLO el bloque PARAMETROS
y volvé a ejecutar. El .FCStd se regenera completo.

Impresión (FDM, 0.2 mm, sin soportes en orientación indicada)
-------------------------------------------------------------
  Frente.stl  → cara de la pantalla sobre la cama
  Fondo.stl   → cara trasera sobre la cama
  4 tornillos M3 por los agujeros de la placa (M3x30 aprox.)
  PETG preferible cerca del horno; PLA va bien si la caja no toca el horno.

Ensamble
--------
  1) Display entra por atrás del frente, vidrio contra el marco de la ventana.
     Los 4 agujeros D3.2 alinean con los insertos del bisel.
  2) Cables de alimentación y del MAX31855 (el MAX va FUERA) entran por el orificio de la tapa.
  3) USB-C a la derecha.
  4) Cerrar con 4 tornillos M3 desde la tapa trasera, atravesando la placa.
"""

from __future__ import annotations

import os
import sys

import FreeCAD as App
import MeshPart
import Part

# ---------------------------------------------------------------------------
# PARAMETROS  (todo en milímetros)
# ---------------------------------------------------------------------------

# PCB completo según plano "Product Size": 123.0 x 74.0 mm
# (la tabla del PDF dice 105.5x74; eso es el LCD, no la placa).
PCB_W = 123.0
PCB_H = 74.0
PCB_T = 1.6
LCD_W = 105.5
LCD_H = 67.2
LCD_T = 3.0
GLASS_T = 1.6  # overlay táctil (resistivo o capacitivo)
HOLE_D = 3.2
HOLE_INSET = 3.5  # centro del agujero desde el borde del PCB

# Área activa 95.04 x 53.86. La ventana tapa el marco muerto del LCD.
WIN_W = 96.0
WIN_H = 55.0
# +Y = hacia arriba, alejándose del borde inferior / lado SD.
WIN_SHIFT_Y = 3.0

# Marco mínimo: solo pared + holgura. Abajo 3 mm entre PCB y pared interior.
CLEAR = 0.40
CLEAR_BOTTOM = 3.00
WALL = 2.20
FRAME = WALL
LIP = 1.40
LIP_CLEAR = 0.25
FRONT_FACE = 2.20
# Perímetro del frente: +1.5 mm de profundidad (paredes), sin tocar bisel ni postes.
FRONT_PERIM_EXTRA = 1.5
# Tapa: placa 1.5 mm. El borde calza en la muesca del labio (1.4 mm, con 0.2 mm de holgura).
BACK_WALL = 1.5
BACK_TONGUE = LIP - 0.2  # 1.2 mm
TONGUE_CLEAR = 0.15  # holgura radial en la muesca
# Filete interno en el ángulo labio–placa.
BACK_FILLET_R = 0.80

# Esquinas del rectángulo exterior: vivas.
CORNER_R = 0.0
CHAMFER_WIN = 0.7

# Postes del frente (sujetan la placa). Original Ø7.8 x 5.2 mm.
FRONT_BOSS_D = 7.80 * 0.70  # 5.46 mm (−30 %)
FRONT_BOSS_H = 4.0
FRONT_BOSS_EMBED = 0.8  # se meten en el bisel para fusionar
FRONT_BOSS_HOLE_D = 2.00  # paso de tornillo

# Columnas del fondo: mismos diámetros que el frente.
BOSS_D = FRONT_BOSS_D
SCREW_THRU = FRONT_BOSS_HOLE_D
BACK_BOSS_EXTRA = 4.0
# Cara posterior: hueco para la cabeza, un poco mayor que el paso.
COUNTERBORE_D = SCREW_THRU + 1.20  # Ø3.20 mm
COUNTERBORE_H = 2.00

# Origen del PCB: esquina inferior-izquierda, vista de FRENTE.
# USB-C borde derecho (vista de frente).
ENABLE_USB_SLOT = True
USB_Y_FROM_PCB_BOTTOM = 33.5  # conector real en la placa (dummy)
USB_SLOT_Y = 10.0  # alto de la ventana
USB_SLOT_Z = 9.0
USB_SLOT_CENTER_FROM_BOX_BOTTOM = 45.0  # centro de la ventana, desde el borde inferior de la caja

# Conector 4P 1.25 (GND/RX/TX/5V) en el mismo borde, arriba del USB-C.
ENABLE_PWR_SLOT = False
PWR_Y_FROM_PCB_BOTTOM = 50.8
PWR_SLOT_Y = 12.0
PWR_SLOT_Z = 6.5

# TF / micro-SD: cerrado (sin ventana tipo SD).
ENABLE_TF_SLOT = False
TF_X_FROM_PCB_LEFT = 50.3
TF_SLOT_X = 18.0
TF_SLOT_Z = 4.5

# Ranura BOOT/RST en el borde IZQUIERDO.
ENABLE_SERVICE_SLOT = False
SERVICE_FROM_PCB_TOP = 4.0
SERVICE_SLOT_Y = 20.0
SERVICE_SLOT_Z = 10.0

# MAX31855 y bornes van FUERA. Orificio en la cara trasera para alimentación + MAX.
CABLE_HOLE_D = 9.0
ENABLE_VENTS = False

# Ojal para colgar en pared (M4)
ENABLE_WALL_KEYS = False
KEY_SPAN = 50.0
KEY_SHAFT = 4.6
KEY_HEAD = 8.4

# Dummy de referencia (no se imprime)
SHOW_DUMMY = True

OUT_DIR = os.path.dirname(os.path.abspath(__file__))
DOC_NAME = "SmartKiln_Caja"


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def box(x, y, z, dx, dy, dz):
    if dx <= 0 or dy <= 0 or dz <= 0:
        raise ValueError("box size must be > 0")
    return Part.makeBox(dx, dy, dz, App.Vector(x, y, z))


def cyl(x, y, z, r, h, axis=App.Vector(0, 0, 1)):
    return Part.makeCylinder(r, h, App.Vector(x, y, z), axis)


def fuse_all(shapes):
    acc = shapes[0]
    for s in shapes[1:]:
        acc = acc.fuse(s)
    return acc.removeSplitter()


def rounded_rect_prism(x, y, z, dx, dy, dz, r):
    """Caja con las 4 aristas verticales del rectángulo mayor redondeadas."""
    r = min(float(r), dx / 2.0 - 0.05, dy / 2.0 - 0.05)
    if r < 0.5:
        return box(x, y, z, dx, dy, dz)
    core = box(x + r, y, z, dx - 2.0 * r, dy, dz)
    mid = box(x, y + r, z, dx, dy - 2.0 * r, dz)
    corners = [
        cyl(x + r, y + r, z, r, dz),
        cyl(x + dx - r, y + r, z, r, dz),
        cyl(x + r, y + dy - r, z, r, dz),
        cyl(x + dx - r, y + dy - r, z, r, dz),
    ]
    return fuse_all([core, mid] + corners)


def inner_right_angle_fillet(w, h, inner_i, z_face, r):
    """Refuerzo en el ángulo interno 90° entre el labio y la placa."""
    r = min(float(r), BACK_TONGUE - 0.15)
    if r < 0.35:
        return None
    x0, x1 = inner_i, w - inner_i
    y0, y1 = inner_i, h - inner_i
    lx = x1 - x0
    ly = y1 - y0
    zc = z_face - r

    sq_b = box(x0, y0, zc, lx, r, r)
    cut_b = Part.makeCylinder(r, lx + 0.4, App.Vector(x0 - 0.2, y0 + r, zc), App.Vector(1, 0, 0))
    sq_t = box(x0, y1 - r, zc, lx, r, r)
    cut_t = Part.makeCylinder(r, lx + 0.4, App.Vector(x0 - 0.2, y1 - r, zc), App.Vector(1, 0, 0))
    sq_l = box(x0, y0, zc, r, ly, r)
    cut_l = Part.makeCylinder(r, ly + 0.4, App.Vector(x0 + r, y0 - 0.2, zc), App.Vector(0, 1, 0))
    sq_r = box(x1 - r, y0, zc, r, ly, r)
    cut_r = Part.makeCylinder(r, ly + 0.4, App.Vector(x1 - r, y0 - 0.2, zc), App.Vector(0, 1, 0))
    return fuse_all([sq_b.cut(cut_b), sq_t.cut(cut_t), sq_l.cut(cut_l), sq_r.cut(cut_r)])


def cut_all(base, tools):
    acc = base
    for t in tools:
        acc = acc.cut(t)
    return acc.removeSplitter()


def rounded_slot_yz(x, y, z, dx, dy, dz, r=None):
    """Prisma a lo largo de X con extremos redondeados en Y (ranura USB, etc.)."""
    r = min(dy / 2 - 0.05, dz / 2 - 0.05) if r is None else r
    r = max(0.2, r)
    core = box(x, y + r, z, dx, dy - 2 * r, dz)
    c1 = cyl(x, y + r, z + dz / 2, r, dx, App.Vector(1, 0, 0))
    c2 = cyl(x, y + dy - r, z + dz / 2, r, dx, App.Vector(1, 0, 0))
    # cilindros centrados en Z: desplazar porque makeCylinder arranca en el eje
    c1 = Part.makeCylinder(r, dx, App.Vector(x, y + r, z + dz / 2), App.Vector(1, 0, 0))
    c2 = Part.makeCylinder(r, dx, App.Vector(x, y + dy - r, z + dz / 2), App.Vector(1, 0, 0))
    mid = box(x, y, z + dz / 2 - r, dx, dy, 2 * r)
    return fuse_all([core, c1, c2, mid])


def through_slot_x(y, z, x0, x1, dy, dz):
    """Agujero holgado a lo largo de X, caja simple (imprescindible que corte)."""
    return box(min(x0, x1) - 0.5, y, z, abs(x1 - x0) + 1.0, dy, dz)


def through_slot_y(x, z, y0, y1, dx, dz):
    return box(x, min(y0, y1) - 0.5, z, dx, abs(y1 - y0) + 1.0, dz)


def fillet_outer_z(shape, radius):
    bb = shape.BoundBox
    picked = []
    for e in shape.Edges:
        vs = e.Vertexes
        if len(vs) != 2:
            continue
        if abs(vs[0].X - vs[1].X) > 0.2 or abs(vs[0].Y - vs[1].Y) > 0.2:
            continue
        if abs(vs[0].Z - vs[1].Z) < 4.0:
            continue
        x, y = vs[0].X, vs[0].Y
        if (abs(x - bb.XMin) < 0.25 or abs(x - bb.XMax) < 0.25) and (
            abs(y - bb.YMin) < 0.25 or abs(y - bb.YMax) < 0.25
        ):
            picked.append(e)
    if not picked:
        return shape
    try:
        return shape.makeFillet(radius, picked)
    except Exception as exc:
        print("  Fillete omitido:", exc)
        return shape


def chamfer_window_front(shape, win, radius):
    x0, y0, w, h = win
    picked = []
    for e in shape.Edges:
        bb = e.BoundBox
        if abs(bb.ZMin) > 0.25 or abs(bb.ZMax) > 0.25:
            continue
        # aristas de la ventana en Z=0
        on_x = (abs(bb.XMin - x0) < 0.4 and abs(bb.XMax - (x0 + w)) < 0.4)
        on_y = (abs(bb.YMin - y0) < 0.4 and abs(bb.YMax - (y0 + h)) < 0.4)
        if (on_x and abs(bb.YMin - y0) < 0.4 or abs(bb.YMin - (y0 + h)) < 0.4) and abs(
            bb.XLength - w
        ) < 0.6:
            picked.append(e)
        elif (on_y and (abs(bb.XMin - x0) < 0.4 or abs(bb.XMin - (x0 + w)) < 0.4)) and abs(
            bb.YLength - h
        ) < 0.6:
            picked.append(e)
    if not picked:
        return shape
    try:
        return shape.makeChamfer(radius, picked)
    except Exception as exc:
        print("  Chaflán ventana omitido:", exc)
        return shape


def add_shape(doc, name, shape, rgb):
    obj = doc.addObject("Part::Feature", name)
    obj.Shape = shape
    try:
        obj.ViewObject.ShapeColor = rgb
        obj.ViewObject.DisplayMode = "Shaded"
    except Exception:
        pass
    return obj


def clean_solid(shape):
    s = shape.removeSplitter()
    try:
        s.fix(1e-4, 1e-4, 1e-3)
    except Exception:
        pass
    return s


def export_stl(shape, path):
    shape = clean_solid(shape)
    mesh = MeshPart.meshFromShape(
        Shape=shape,
        LinearDeflection=0.07,
        AngularDeflection=0.4,
        Relative=False,
    )
    try:
        mesh.harmonizeNormals()
    except Exception:
        pass
    mesh.write(path)
    print("  STL:", path, "solid" if mesh.isSolid() else "NO-SOLID")


def export_step(shape, path):
    shape.exportStep(path)
    print("  STEP:", path)


def flip_z_to_bed(shape, z_face):
    """Espeja en Z para que z_face quede sobre la cama (Z=0)."""
    m = App.Matrix()
    m.A33 = -1.0
    m.A34 = z_face
    out = shape.transformGeometry(m)
    out.translate(App.Vector(0, 0, -out.BoundBox.ZMin))
    return out


# ---------------------------------------------------------------------------
# Layout derivado
# ---------------------------------------------------------------------------

def layout():
    pocket_d = GLASS_T + LCD_T + PCB_T + 0.4
    front_core = FRONT_FACE + pocket_d
    front_z = front_core + FRONT_PERIM_EXTRA
    outer_w = PCB_W + 2 * CLEAR + 2 * WALL
    outer_h = PCB_H + CLEAR + CLEAR_BOTTOM + 2 * WALL
    pcb_x = WALL + CLEAR
    pcb_y = WALL + CLEAR_BOTTOM
    lcd_x = pcb_x + (PCB_W - LCD_W) / 2.0
    lcd_y = pcb_y + (PCB_H - LCD_H) / 2.0
    win_x = lcd_x + (LCD_W - WIN_W) / 2.0
    win_y = lcd_y + (LCD_H - WIN_H) / 2.0 + WIN_SHIFT_Y
    usb_y = USB_SLOT_CENTER_FROM_BOX_BOTTOM - USB_SLOT_Y / 2.0
    usb_z = FRONT_FACE + GLASS_T + LCD_T + PCB_T - 1.2
    back_plate = BACK_WALL
    back_z = BACK_TONGUE + BACK_WALL
    pwr_y = pcb_y + PWR_Y_FROM_PCB_BOTTOM - PWR_SLOT_Y / 2.0
    tf_x = pcb_x + TF_X_FROM_PCB_LEFT - TF_SLOT_X / 2.0
    tf_z = usb_z + 0.4
    service_y = pcb_y + PCB_H - SERVICE_FROM_PCB_TOP - SERVICE_SLOT_Y
    holes = [
        (pcb_x + HOLE_INSET, pcb_y + HOLE_INSET),
        (pcb_x + PCB_W - HOLE_INSET, pcb_y + HOLE_INSET),
        (pcb_x + HOLE_INSET, pcb_y + PCB_H - HOLE_INSET),
        (pcb_x + PCB_W - HOLE_INSET, pcb_y + PCB_H - HOLE_INSET),
    ]
    screws = list(holes)
    return {
        "pocket_d": pocket_d,
        "front_z": front_z,
        "back_z": back_z,
        "back_plate": back_plate,
        "outer_w": outer_w,
        "outer_h": outer_h,
        "pcb_x": pcb_x,
        "pcb_y": pcb_y,
        "lcd_x": lcd_x,
        "lcd_y": lcd_y,
        "win_x": win_x,
        "win_y": win_y,
        "screws": screws,
        "holes": holes,
        "usb_y": usb_y,
        "usb_z": usb_z,
        "pwr_y": pwr_y,
        "tf_x": tf_x,
        "tf_z": tf_z,
        "service_y": service_y,
        "total_z": front_z + LIP + BACK_WALL,
    }


# ---------------------------------------------------------------------------
# Geometría
# ---------------------------------------------------------------------------

def make_frente(L):
    w, h, fz = L["outer_w"], L["outer_h"], L["front_z"]
    solid = rounded_rect_prism(0, 0, 0, w, h, fz, CORNER_R)

    # Bolsillo del módulo (entra por atrás). Abajo: CLEAR_BOTTOM.
    # El extra de perímetro solo alarga las paredes; el hueco sigue abierto atrás.
    pocket = box(
        L["pcb_x"] - CLEAR,
        L["pcb_y"] - CLEAR_BOTTOM,
        FRONT_FACE,
        PCB_W + 2 * CLEAR,
        PCB_H + CLEAR_BOTTOM + CLEAR,
        L["pocket_d"] + FRONT_PERIM_EXTRA + 1.0,
    )
    solid = solid.cut(pocket)

    # Ventana
    window = box(L["win_x"], L["win_y"], -1, WIN_W, WIN_H, fz + 2)
    solid = solid.cut(window)

    # Labio de encastre (sigue el redondeo exterior)
    inset = WALL * 0.55
    lip_outer = rounded_rect_prism(0, 0, fz, w, h, LIP, CORNER_R)
    lip_cut = rounded_rect_prism(
        inset,
        inset,
        fz - 0.05,
        w - 2 * inset,
        h - 2 * inset,
        LIP + 0.2,
        max(0.0, CORNER_R - inset),
    )
    lip = lip_outer.cut(lip_cut)
    solid = solid.fuse(lip)

    # Postes que sujetan la placa (bisel → bolsillo)
    bosses = []
    for hx, hy in L["holes"]:
        bosses.append(
            cyl(hx, hy, FRONT_FACE - FRONT_BOSS_EMBED, FRONT_BOSS_D / 2.0, FRONT_BOSS_H)
        )
    solid = solid.fuse(fuse_all(bosses))

    cuts = []

    for hx, hy in L["holes"]:
        cuts.append(
            cyl(
                hx,
                hy,
                FRONT_FACE - FRONT_BOSS_EMBED - 0.2,
                FRONT_BOSS_HOLE_D / 2.0,
                FRONT_BOSS_H + LIP + 1.0,
            )
        )
        cuts.append(
            cyl(hx, hy, fz - 0.2, (FRONT_BOSS_D + 0.8) / 2.0, LIP + 0.8)
        )

    # USB: muesca en labio + pared derecha
    if ENABLE_USB_SLOT:
        cuts.append(
            through_slot_x(
                L["usb_y"],
                L["usb_z"],
                w - WALL - 4,
                w + 4,
                USB_SLOT_Y,
                USB_SLOT_Z,
            )
        )

    if ENABLE_PWR_SLOT:
        cuts.append(
            through_slot_x(
                L["pwr_y"],
                L["usb_z"] + 0.5,
                w - WALL - 4,
                w + 4,
                PWR_SLOT_Y,
                PWR_SLOT_Z,
            )
        )

    if ENABLE_TF_SLOT:
        cuts.append(
            through_slot_y(
                L["tf_x"],
                L["tf_z"],
                h - WALL - 4,
                h + 4,
                TF_SLOT_X,
                TF_SLOT_Z,
            )
        )

    solid = cut_all(solid, cuts)
    if CHAMFER_WIN >= 0.3:
        solid = chamfer_window_front(solid, (L["win_x"], L["win_y"], WIN_W, WIN_H), CHAMFER_WIN)
    if not solid.isValid():
        raise RuntimeError("Frente inválido")
    return solid


def _ok(shape, name):
    if not shape.isValid():
        raise RuntimeError("Sólido inválido en: " + name)
    return shape.removeSplitter()


def make_fondo(L):
    w, h = L["outer_w"], L["outer_h"]
    plate = L["back_plate"]
    # El borde entra en el labio del frente; la placa queda por fuera.
    z0 = L["front_z"] + LIP - BACK_TONGUE
    z1 = z0 + L["back_z"]

    tapa = rounded_rect_prism(0, 0, z0 + BACK_TONGUE, w, h, plate, CORNER_R)

    # Anillo que calza en la muesca del labio: entre el hueco del labio y la pared interior.
    lip_inset = WALL * 0.55
    outer_i = lip_inset + TONGUE_CLEAR
    inner_i = WALL - TONGUE_CLEAR
    ring_outer = rounded_rect_prism(
        outer_i, outer_i, z0, w - 2 * outer_i, h - 2 * outer_i, BACK_TONGUE, CORNER_R
    )
    ring_inner = box(
        inner_i, inner_i, z0 - 0.05, w - 2 * inner_i, h - 2 * inner_i, BACK_TONGUE + 0.2
    )
    borde = ring_outer.cut(ring_inner)
    solid = _ok(tapa.fuse(borde), "fondo placa+borde")
    fillet = inner_right_angle_fillet(w, h, inner_i, z0 + BACK_TONGUE, BACK_FILLET_R)
    if fillet is not None:
        solid = _ok(solid.fuse(fillet), "fondo filete")

    adds = []
    cuts = []

    boss_z = z0 - BACK_BOSS_EXTRA
    boss_h = L["back_z"] + BACK_BOSS_EXTRA
    for sx, sy in L["screws"]:
        adds.append(cyl(sx, sy, boss_z, BOSS_D / 2.0, boss_h))
        cuts.append(cyl(sx, sy, boss_z - 1, SCREW_THRU / 2.0, boss_h + 2))
        cuts.append(cyl(sx, sy, z1 - COUNTERBORE_H, COUNTERBORE_D / 2.0, COUNTERBORE_H + 0.4))

    solid = _ok(fuse_all([solid] + adds), "fondo postes")

    if ENABLE_PWR_SLOT:
        cuts.append(
            through_slot_x(L["pwr_y"], L["usb_z"] + 0.5, w - WALL - 6, w + 6, PWR_SLOT_Y, PWR_SLOT_Z)
        )
    if ENABLE_TF_SLOT:
        cuts.append(
            through_slot_y(L["tf_x"], L["tf_z"], h - WALL - 6, h + 6, TF_SLOT_X, TF_SLOT_Z)
        )
    if ENABLE_SERVICE_SLOT:
        cuts.append(
            through_slot_x(
                L["service_y"],
                L["usb_z"] - 1.0,
                -6,
                WALL + 6,
                SERVICE_SLOT_Y,
                SERVICE_SLOT_Z,
            )
        )

    hole_y = WALL + CABLE_HOLE_D / 2.0 + 2.0
    cuts.append(cyl(w / 2.0, hole_y, z1 - plate - 0.2, CABLE_HOLE_D / 2.0, plate + 1.2))

    if ENABLE_VENTS:
        for i in range(5):
            vx = WALL + 14 + i * 18
            cuts.append(box(vx, h - WALL - 0.2, z1 - plate - 16, 12, WALL + 1.2, 2.2))

    if ENABLE_WALL_KEYS:
        for sign in (1, -1):
            kx = w / 2.0 + sign * KEY_SPAN / 2.0
            kz = z1 - plate - 0.2
            cuts.append(cyl(kx, h / 2.0, kz, KEY_SHAFT / 2.0, plate + 1.2))
            cuts.append(box(kx - KEY_SHAFT / 2.0, h / 2.0, kz, KEY_SHAFT, 8.0, plate + 1.2))
            cuts.append(cyl(kx, h / 2.0 + 8.0, kz, KEY_HEAD / 2.0, plate + 1.2))

    for i, c in enumerate(cuts):
        nxt = solid.cut(c)
        if not nxt.isValid():
            print("  aviso: corte", i, "omitido")
            continue
        solid = nxt.removeSplitter()

    return _ok(solid, "fondo final")


def make_dummy(L):
    parts = []
    z = FRONT_FACE + 0.05
    glass = box(L["lcd_x"], L["lcd_y"], z, LCD_W, LCD_H, GLASS_T)
    z += GLASS_T
    lcd = box(L["lcd_x"], L["lcd_y"], z, LCD_W, LCD_H, LCD_T)
    z += LCD_T
    pcb = box(L["pcb_x"], L["pcb_y"], z, PCB_W, PCB_H, PCB_T)
    usb = box(
        L["pcb_x"] + PCB_W - 0.2,
        L["pcb_y"] + USB_Y_FROM_PCB_BOTTOM - 4.5,
        z + PCB_T,
        2.2,
        9.0,
        3.2,
    )
    return glass, lcd, pcb, usb


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    L = layout()
    print("SmartKiln caja")
    print("  Exterior: {:.1f} x {:.1f} x {:.1f} mm".format(L["outer_w"], L["outer_h"], L["total_z"]))
    print(
        "  Postes placa: Ø{:.2f} x {:.1f} mm, orificio Ø{:.1f} mm".format(
            FRONT_BOSS_D, FRONT_BOSS_H, FRONT_BOSS_HOLE_D
        )
    )
    print(
        "  Columnas fondo: Ø{:.2f} mm, orificio Ø{:.1f} mm, extra +{:.1f} mm".format(
            BOSS_D, SCREW_THRU, BACK_BOSS_EXTRA
        )
    )
    print(
        "  Cabeza tornillo: avellanado Ø{:.2f} x {:.1f} mm (cara posterior)".format(
            COUNTERBORE_D, COUNTERBORE_H
        )
    )
    print("  PCB:      {:.1f} x {:.1f} mm  (caja +{:.1f} x +{:.1f} mm)".format(
        PCB_W, PCB_H, L["outer_w"] - PCB_W, L["outer_h"] - PCB_H
    ))
    print("  Ventana:  {:.1f} x {:.1f} mm, +{:.1f} mm hacia arriba".format(
        WIN_W, WIN_H, WIN_SHIFT_Y
    ))
    print("  Esquinas: vivas")
    print("  USB-C:    ranura {:.1f} x {:.1f} mm, centro a {:.1f} mm del borde inferior".format(
        USB_SLOT_Y, USB_SLOT_Z, USB_SLOT_CENTER_FROM_BOX_BOTTOM
    ))
    print("  Frente:   perímetro +{:.1f} mm".format(FRONT_PERIM_EXTRA))
    print("  Tapa:     placa {:.1f} mm + borde {:.1f} mm (muesca labio {:.1f} mm)".format(
        BACK_WALL, BACK_TONGUE, LIP
    ))
    print("  Cables:   orificio trasero Ø{:.1f} mm (alim. + MAX)".format(CABLE_HOLE_D))

    doc = App.newDocument(DOC_NAME)

    frente = make_frente(L)
    fondo = make_fondo(L)

    add_shape(doc, "Frente", frente, (0.18, 0.18, 0.20))
    add_shape(doc, "Fondo", fondo, (0.22, 0.22, 0.25))

    if SHOW_DUMMY:
        glass, lcd, pcb, usb = make_dummy(L)
        add_shape(doc, "Ref_Vidrio", glass, (0.05, 0.05, 0.08))
        add_shape(doc, "Ref_LCD", lcd, (0.10, 0.18, 0.35))
        add_shape(doc, "Ref_PCB", pcb, (0.12, 0.42, 0.18))
        add_shape(doc, "Ref_USB", usb, (0.75, 0.75, 0.78))

    doc.recompute()

    fcstd = os.path.join(OUT_DIR, "SmartKiln_Caja.FCStd")
    doc.saveAs(fcstd)
    print("  FCStd:", fcstd)

    frente_stl = os.path.join(OUT_DIR, "SmartKiln_Frente.stl")
    fondo_stl = os.path.join(OUT_DIR, "SmartKiln_Fondo.stl")
    export_stl(frente, frente_stl)
    # Fondo: cara trasera sobre la cama
    fondo_print = flip_z_to_bed(fondo, fondo.BoundBox.ZMax)
    export_stl(fondo_print, fondo_stl)
    export_step(frente, os.path.join(OUT_DIR, "SmartKiln_Frente.step"))
    export_step(fondo, os.path.join(OUT_DIR, "SmartKiln_Fondo.step"))

    print("Listo.")
    return 0


def _run():
    log_path = os.path.join(OUT_DIR, "_build.log")
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


# FreeCADCmd a veces no usa __name__ == "__main__"
_run()
