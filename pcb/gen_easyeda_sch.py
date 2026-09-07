#!/usr/bin/env python3
"""Esquemático EasyEDA Std (JSON) del driver MOC3020 + BT138 + HLK-PM12."""

from __future__ import annotations

import json
from pathlib import Path

OUT = Path(__file__).with_name("IA_Kiln_Driver_SCH.json")


class Sch:
    def __init__(self):
        self.n = 1
        self.shape: list[str] = []

    def id(self) -> str:
        i = f"gge{self.n}"
        self.n += 1
        return i

    def wire(self, *pts: tuple[float, float], color="#008800"):
        s = " ".join(f"{x} {y}" for x, y in pts)
        self.shape.append(f"W~{s}~{color}~1~0~none~{self.id()}~0")

    def junction(self, x, y):
        self.shape.append(f"J~{x}~{y}~2.5~#CC0000~{self.id()}~0")

    def noconnect(self, x, y):
        self.shape.append(
            f"O~{x}~{y}~{self.id()}~M {x-4} {y-4} L {x+4} {y+4} M {x-4} {y+4} L {x+4} {y-4}~#880000~0"
        )

    def netlabel(self, x, y, text, rot=0, color="#0000FF"):
        self.shape.append(
            f"N~{x}~{y}~{rot}~{color}~{text}~{self.id()}~start~{x + 2}~{y - 3}~~8pt~0"
        )

    def text(self, x, y, s, size="11pt", color="#000080"):
        self.shape.append(
            f"T~L~{x}~{y}~0~{color}~Arial~{size}~~~~comment~{s}~1~start~{self.id()}~0"
        )

    def rect(self, x, y, w, h, color="#000000"):
        self.shape.append(f"R~{x}~{y}~~~{w}~{h}~{color}~1~0~none~{self.id()}~0")

    def pin(self, px, py, num, name, rot, pid=None):
        """Pata como línea de hoja. EasyEDA Std duplica LIB/P en (0,0)."""
        if rot == 0:
            x2, y2, nx, ny = px - 10, py, px - 14, py - 4
        elif rot == 180:
            x2, y2, nx, ny = px + 10, py, px + 14, py - 4
        elif rot == 90:
            x2, y2, nx, ny = px, py - 10, px + 4, py - 12
        else:
            x2, y2, nx, ny = px, py + 10, px + 4, py + 8
        out = [f"PL~{px:g} {py:g} {x2:g} {y2:g}~#880000~1~0~none~{self.id()}~0"]
        if name and name not in ("1", "2"):
            out.append(
                f"T~L~{nx:g}~{ny:g}~0~#0000AA~Arial~6pt~~~~comment~{name}~1~start~{self.id()}~0"
            )
        return out

    def lib(self, x, y, c_para, parts: list, rot=""):
        """Sin LIB: Std pinta una copia de cada símbolo en el origen."""
        for p in parts:
            if isinstance(p, list):
                self.shape.extend(p)
            else:
                self.shape.append(p)
        return ""

    def resistor(self, x, y, ref, value):
        parts = [
            f"T~L~{x-8:g}~{y-16:g}~0~#000080~Arial~7pt~~~~comment~{ref}~1~start~{self.id()}~0",
            f"T~L~{x-8:g}~{y+14:g}~0~#000080~Arial~7pt~~~~comment~{value}~1~start~{self.id()}~0",
            f"R~{x-12:g}~{y-5:g}~~~24~10~#880000~1~0~none~{self.id()}~0",
            f"PL~{x-20:g} {y:g} {x-12:g} {y:g}~#880000~1~0~none~{self.id()}~0",
            f"PL~{x+12:g} {y:g} {x+20:g} {y:g}~#880000~1~0~none~{self.id()}~0",
        ]
        return self.lib(x, y, "", parts)

    def resistor_v(self, x, y, ref, value, half=20):
        body = min(24, max(10, half * 2 - 8))
        y1, y2 = y - half, y + half
        yt, yb = y - body / 2, y + body / 2
        parts = [
            f"T~L~{x+10:g}~{y-6:g}~0~#000080~Arial~7pt~~~~comment~{ref}~1~start~{self.id()}~0",
            f"T~L~{x+10:g}~{y+8:g}~0~#000080~Arial~7pt~~~~comment~{value}~1~start~{self.id()}~0",
            f"R~{x-5:g}~{yt:g}~~~10~{body:g}~#880000~1~0~none~{self.id()}~0",
            f"W~{x:g} {y1:g} {x:g} {yt:g}~#008800~1~0~none~{self.id()}~0",
            f"W~{x:g} {yb:g} {x:g} {y2:g}~#008800~1~0~none~{self.id()}~0",
        ]
        return self.lib(x, y, "", parts)

    def mov_v(self, x, y, ref, value, half=20):
        body = min(28, max(12, half * 2 - 12))
        y1, y2 = y - half, y + half
        yt, yb = y - body / 2, y + body / 2
        parts = [
            f"T~L~{x+14:g}~{y-8:g}~0~#000080~Arial~7pt~~~~comment~{ref}~1~start~{self.id()}~0",
            f"T~L~{x+14:g}~{y+6:g}~0~#000080~Arial~7pt~~~~comment~{value}~1~start~{self.id()}~0",
            f"R~{x-10:g}~{yt:g}~~~20~{body:g}~#0000AA~1.2~0~none~{self.id()}~0",
            f"T~L~{x-8:g}~{y-2:g}~0~#0000AA~Arial~6pt~~~~comment~MOV~1~start~{self.id()}~0",
            f"W~{x:g} {y1:g} {x:g} {yt:g}~#008800~1~0~none~{self.id()}~0",
            f"W~{x:g} {yb:g} {x:g} {y2:g}~#008800~1~0~none~{self.id()}~0",
        ]
        return self.lib(x, y, "", parts)

    def capacitor_h(self, x, y, ref, value):
        parts = [
            f"T~L~{x-10:g}~{y-18:g}~0~#000080~Arial~7pt~~~~comment~{ref}~1~start~{self.id()}~0",
            f"T~L~{x-10:g}~{y+16:g}~0~#000080~Arial~7pt~~~~comment~{value}~1~start~{self.id()}~0",
            f"PL~{x-20:g} {y:g} {x-3:g} {y:g}~#880000~1~0~none~{self.id()}~0",
            f"PL~{x-3:g} {y-12:g} {x-3:g} {y+12:g}~#880000~1.8~0~none~{self.id()}~0",
            f"PL~{x+3:g} {y-12:g} {x+3:g} {y+12:g}~#880000~1.8~0~none~{self.id()}~0",
            f"PL~{x+3:g} {y:g} {x+20:g} {y:g}~#880000~1~0~none~{self.id()}~0",
        ]
        return self.lib(x, y, "", parts)

    def capacitor_v(self, x, y, ref, value):
        parts = [
            f"T~L~{x+14:g}~{y-8:g}~0~#000080~Arial~7pt~~~~comment~{ref}~1~start~{self.id()}~0",
            f"T~L~{x+14:g}~{y+6:g}~0~#000080~Arial~7pt~~~~comment~{value}~1~start~{self.id()}~0",
            f"PL~{x:g} {y-20:g} {x:g} {y-4:g}~#880000~1~0~none~{self.id()}~0",
            f"PL~{x-12:g} {y-4:g} {x+12:g} {y-4:g}~#880000~1.8~0~none~{self.id()}~0",
            f"PL~{x-12:g} {y+4:g} {x+12:g} {y+4:g}~#880000~1.8~0~none~{self.id()}~0",
            f"PL~{x:g} {y+4:g} {x:g} {y+20:g}~#880000~1~0~none~{self.id()}~0",
        ]
        return self.lib(x, y, "", parts)

    def connector2(self, x, y, ref, n1, n2, extra="", dy=10):
        h = dy * 2 + 16
        parts = [
            f"T~L~{x-6:g}~{y - dy - 18:g}~0~#000080~Arial~8pt~~~~comment~{ref}~1~start~{self.id()}~0",
            f"R~{x-12:g}~{y - dy - 8:g}~~~24~{h}~#006600~1.2~0~none~{self.id()}~0",
            *self.pin(x - 20, y - dy, "1", n1, 180),
            *self.pin(x - 20, y + dy, "2", n2, 180),
        ]
        if extra:
            parts.insert(1, f"T~L~{x-6:g}~{y + dy + 14:g}~0~#006600~Arial~6pt~~~~comment~{extra}~1~start~{self.id()}~0")
        return self.lib(x, y, "", parts)

    def connector3(self, x, y, ref, n1, n2, n3, extra=""):
        parts = [
            f"T~L~{x-6:g}~{y-40:g}~0~#000080~Arial~8pt~~~~comment~{ref}~1~start~{self.id()}~0",
            f"R~{x-12:g}~{y-30:g}~~~24~60~#006600~1.2~0~none~{self.id()}~0",
            *self.pin(x - 20, y - 20, "1", n1, 180),
            *self.pin(x - 20, y, "2", n2, 180),
            *self.pin(x - 20, y + 20, "3", n3, 180),
        ]
        if extra:
            parts.insert(1, f"T~L~{x-6:g}~{y+36:g}~0~#006600~Arial~6pt~~~~comment~{extra}~1~start~{self.id()}~0")
        return self.lib(x, y, "", parts)

    def hlk_pm12(self, x, y):
        parts = [
            f"T~L~{x-16:g}~{y-52:g}~0~#000080~Arial~8pt~~~~comment~U2~1~start~{self.id()}~0",
            f"T~L~{x-20:g}~{y+52:g}~0~#000080~Arial~7pt~~~~comment~HLK-PM12~1~start~{self.id()}~0",
            f"R~{x-28:g}~{y-42:g}~~~56~84~#000000~1.2~0~none~{self.id()}~0",
            f"PL~{x:g} {y-42:g} {x:g} {y+42:g}~#C4A35A~1~1~none~{self.id()}~0",
            f"T~L~{x-24:g}~{y-28:g}~0~#AA0000~Arial~6pt~~~~comment~AC~1~start~{self.id()}~0",
            f"T~L~{x+6:g}~{y-28:g}~0~#006633~Arial~6pt~~~~comment~12V~1~start~{self.id()}~0",
            *self.pin(x - 38, y - 30, "1", "AC", 180),
            *self.pin(x - 38, y + 10, "2", "AC", 180),
            *self.pin(x + 38, y + 10, "3", "-V0", 0),
            *self.pin(x + 38, y - 30, "4", "+V0", 0),
        ]
        return self.lib(x, y, "", parts)

    def moc3020(self, x, y):
        parts = [
            f"T~L~{x-8:g}~{y-48:g}~0~#000080~Arial~8pt~~~~comment~U1~1~start~{self.id()}~0",
            f"T~L~{x-8:g}~{y+48:g}~0~#000080~Arial~7pt~~~~comment~MOC3020~1~start~{self.id()}~0",
            f"R~{x-22:g}~{y-40:g}~~~44~80~#000000~1.2~0~none~{self.id()}~0",
            f"PL~{x:g} {y-40:g} {x:g} {y+40:g}~#C4A35A~1~1~none~{self.id()}~0",
            f"T~L~{x-18:g}~{y-28:g}~0~#0000AA~Arial~6pt~~~~comment~LED~1~start~{self.id()}~0",
            f"T~L~{x+4:g}~{y-28:g}~0~#AA0000~Arial~6pt~~~~comment~foto~1~start~{self.id()}~0",
            *self.pin(x - 30, y - 30, "1", "ANODE", 180),
            *self.pin(x - 30, y - 10, "2", "CATH", 180),
            *self.pin(x - 30, y + 10, "3", "NC", 180),
            *self.pin(x + 30, y + 30, "4", "MT", 0),
            *self.pin(x + 30, y + 10, "5", "NC", 0),
            *self.pin(x + 30, y - 30, "6", "G", 0),
        ]
        return self.lib(x, y, "", parts)

    def triac(self, x, y):
        parts = [
            f"T~L~{x-24:g}~{y-42:g}~0~#000080~Arial~8pt~~~~comment~Q2~1~start~{self.id()}~0",
            f"T~L~{x-28:g}~{y+42:g}~0~#000080~Arial~7pt~~~~comment~BT138~1~start~{self.id()}~0",
            f"PL~{x:g} {y-20:g} {x:g} {y+20:g}~#AA0000~1.4~0~none~{self.id()}~0",
            f"PG~{x-8:g} {y-16:g} {x+8:g} {y-16:g} {x:g} {y-4:g}~#AA0000~1.2~0~none~{self.id()}~0",
            f"PG~{x-8:g} {y+16:g} {x+8:g} {y+16:g} {x:g} {y+4:g}~#AA0000~1.2~0~none~{self.id()}~0",
            f"PL~{x:g} {y:g} {x+16:g} {y:g}~#AA0000~1.2~0~none~{self.id()}~0",
            *self.pin(x, y - 30, "1", "MT1", 270),
            *self.pin(x, y + 30, "2", "MT2", 90),
            *self.pin(x + 20, y, "3", "G", 0),
        ]
        return self.lib(x, y, "", parts)


def build_final():
    s = Sch()
    s.text(40, 24, "IA Kiln  ·  Driver de bobina + 12 V", "16pt")
    s.text(40, 44, "Rev. E  ·  L por arriba  ·  N por abajo  ·  12 V solo a la derecha de U2  ·  CTRL y GND no se cruzan", "8pt", "#555")

    s.rect(20, 55, 400, 430, "#0F6B4C")
    s.text(30, 72, "0  Fuente  220 VAC → 12 V aislado", "10pt", "#0F6B4C")
    s.rect(430, 55, 380, 340, "#1D4E89")
    s.text(440, 72, "1  Control  (GPIO 3,3 V)", "10pt", "#1D4E89")
    s.rect(810, 55, 720, 430, "#9B1C1C")
    s.text(820, 72, "2  Llave de bobina  220 VAC  (BT138)", "10pt", "#9B1C1C")

    # --- piezas ---
    # U2 (200,250): AC1 (162,220) AC2 (162,260) +V0 (238,220) -V0 (238,260)
    s.hlk_pm12(200, 250)
    s.mov_v(110, 240, "RV2", "14D431")          # (110,220) L  (110,260) N
    s.capacitor_v(290, 240, "C12", "100n")       # (290,220) (290,260)
    s.capacitor_v(330, 240, "C13", "220u 25V")
    s.connector2(390, 240, "J1", "12V", "GND", "salida 12 V", dy=20)  # (370,220) (370,260)

    s.connector2(500, 180, "J3", "CTRL", "GND", "ESP GPIO", dy=10)  # (480,170) (480,190)
    s.resistor_v(560, 180, "Rpd", "10k", half=10)  # (560,170) CTRL  (560,190) GND
    s.resistor(640, 170, "R1", "120")            # (620,170) (660,170)
    # U1 (800,200): A (770,170) K (770,190) NC3 (770,210) MT (830,230) NC5 (830,210) G (830,170)
    s.moc3020(800, 200)

    s.resistor(920, 170, "R2", "330")            # (900,170) (940,170)
    # Q2 (1080,250): MT1 (1080,220) MT2 (1080,280) G (1100,250)
    s.triac(1080, 250)
    s.mov_v(1180, 250, "RV1", "14D431", half=30)  # (1180,220) N  (1180,280) A2
    s.capacitor_h(1280, 360, "Cs", "100n X2")    # (1260,360) (1300,360)
    s.resistor(1380, 360, "Rs", "100 2W")        # (1360,360) (1400,360)
    s.connector3(1480, 250, "J2", "L", "N", "A2", "L N A2")  # (1460,230/250/270)

    # ===== 12 V (solo derecha de U2, y=220) =====
    s.netlabel(250, 220, "12V")
    s.wire((238, 220), (370, 220))
    s.junction(238, 220)
    s.junction(290, 220)
    s.junction(330, 220)
    s.junction(370, 220)

    # ===== GND aislado (solo derecha de U2, y=260) =====
    s.netlabel(250, 260, "GND")
    s.wire((238, 260), (370, 260), (480, 260), (480, 190))
    s.junction(238, 260)
    s.junction(290, 260)
    s.junction(330, 260)
    s.junction(370, 260)
    s.junction(480, 260)

    # ===== L por arriba (y=90) → U2 AC1 y RV2. No pasa por el triac =====
    s.netlabel(80, 90, "L")
    s.wire((1460, 230), (1460, 90), (110, 90), (110, 220))
    s.junction(110, 90)
    s.wire((110, 220), (162, 220))
    s.junction(110, 220)
    s.junction(162, 220)

    # ===== N por abajo (y=500) → U2 AC2, RV2, MT1. No cruza GND =====
    s.netlabel(80, 455, "N")
    s.wire((1460, 250), (1460, 455), (110, 455), (110, 260))
    s.junction(110, 455)
    s.wire((110, 260), (162, 260))
    s.junction(110, 260)
    s.junction(162, 260)

    # N a MT1, RV1 y snubber
    s.wire((1080, 220), (1180, 220), (1400, 220), (1400, 250), (1460, 250))
    s.junction(1080, 220)
    s.junction(1180, 220)
    s.junction(1400, 250)
    s.junction(1460, 250)

    # ===== A2: MT2, RV1, J2-A2, snubber =====
    s.netlabel(1420, 280, "A2")
    s.wire((1080, 280), (1180, 280), (1220, 280), (1220, 360), (1260, 360))
    s.junction(1080, 280)
    s.junction(1180, 280)
    s.junction(1220, 280)
    s.wire((1080, 280), (1400, 280), (1400, 270), (1460, 270))
    s.junction(1400, 280)

    # snubber Cs-Rs → N
    s.wire((1300, 360), (1360, 360))
    s.wire((1400, 360), (1400, 220))
    s.junction(1400, 220)
    s.junction(1400, 360)

    # ===== Control: CTRL y GND en Y distintas =====
    s.netlabel(440, 170, "CTRL")
    s.wire((480, 170), (560, 170), (620, 170))
    s.junction(480, 170)
    s.junction(560, 170)
    s.wire((660, 170), (770, 170))
    s.junction(770, 170)

    s.netlabel(440, 190, "GND")
    s.wire((480, 190), (560, 190), (770, 190))
    s.junction(480, 190)
    s.junction(560, 190)
    s.junction(770, 190)

    s.noconnect(770, 210)
    s.noconnect(830, 210)

    # MOC G → R2 → Q2 G
    s.wire((830, 170), (900, 170))
    s.wire((940, 170), (1100, 170), (1100, 250))
    s.junction(1100, 250)

    # MOC MT → Q2 MT2 (A2)
    s.wire((830, 230), (1000, 230), (1000, 280), (1080, 280))
    s.junction(1000, 280)

    s.text(30, 510, "J1 12 V/GND al ESP. Mismo GND que J3. Nunca unir con N.", "8pt", "#0F6B4C")
    s.text(30, 528, "J3 GPIO CTRL. U1 a caballo del ISO GAP (LED izq. / foto der.).", "8pt", "#1D4E89")
    s.text(30, 546, "Q2: MT1=N · MT2=A2 · G=R2 · pestaña=MT2", "8pt", "#1D4E89")
    s.text(820, 510, "Tablero: L → fusible 2 A T → seta NC → J2-L y A1.", "8pt", "#9B1C1C")
    s.text(820, 528, "J2-N → neutro.  J2-A2 → A2 contactor.  PE al chasis.", "8pt", "#9B1C1C")
    s.text(820, 546, "El BT138 solo abre A2. La fuente vive entre L y N.", "8pt", "#9B1C1C")

    return s


def main():
    s = build_final()
    doc = {
        "head": {
            "docType": "1",
            "editorVersion": "6.5.46",
            "title": "IA_Kiln_Driver",
            "description": "Rev.E  HLK-PM12, J1 12V, J3 ESP, J2 L/N/A2. BT138 solo bobina.",
            "c_para": {"Prefix Start": "1"},
            "newgId": True,
            "x": "0",
            "y": "0",
            "hasIdFlag": True,
            "importFlag": 0,
        },
        "canvas": "CA~1700~600~#FFFFFF~yes~#CCCCCC~10~1700~600~line~10~pixel~5~0~0",
        "shape": s.shape,
        "BBox": {"x": 0, "y": 0, "width": 1700, "height": 600},
        "colors": {},
    }
    OUT.write_text(json.dumps(doc, indent=2), encoding="utf-8")
    print(OUT, "shapes", len(s.shape))


if __name__ == "__main__":
    main()
