// SmartKiln — cápsula PLA con tapa corredera para MAX31855
// ========================================================
// Placa objetivo: breakout genérico azul MAX6675/MAX31855,
// 25 x 25 mm, 4 agujeros M2 (paso 20 mm), borne 2 pines + header 5 pines.
//
// Imprimir en PLA, 0.2 mm, 3 perímetros, 25 % infill, sin soportes:
//   Base  → cara inferior sobre la cama
//   Tapa  → cara superior (ranuras) sobre la cama
//
// Verificar con calibre PCB_L / PCB_W / DUPONT_EXTRA antes de imprimir
// la pieza final. El resto escala solo.

$fn = 32;

// ---- Placa y conectores (medidos / típicos del módulo de las fotos) ----
PCB_L          = 25.0;   // largo, borne → header
PCB_W          = 25.0;
PCB_T          = 1.6;
HOLE_D         = 2.2;    // paso M2
HOLE_INSET     =  2.5;   // centro del agujero desde el borde → paso 20 mm
STANDOFF_H     =  1.6;
STANDOFF_D     =  4.2;

TERM_H         = 10.5;   // borne verde sobre la placa
DUPONT_EXTRA   = 16.0;   // housing Dupont 5P más allá del borde del PCB
DUPONT_H       =  9.5;
DUPONT_W       = 14.0;

TC_RELIEF      =  8.0;   // holgura de cables entre pared TC y PCB

// ---- Caja (números redondos para imprimir) ----
OUTER_L        = 62.0;
OUTER_W        = 32.0;
OUTER_H        = 18.0;
WALL           =  2.2;
FLOOR          =  1.8;
CLEAR          =  0.70;  // holgura PLA alrededor del PCB

LID_T          =  1.8;
GROOVE_D       =  1.3;   // profundidad del riel
GROOVE_Z_CLEAR =  0.35;  // holgura vertical de la tapa
LID_XY_CLEAR   =  0.40;

// v1: orificios pequeños (el Dupont queda adentro)
TC_HOLE_D      =  3.2;
TC_HOLE_N      =  2;
TC_HOLE_PITCH  =  5.5;
SPI_HOLE_D     =  5.5;
HOLE_Z         =  FLOOR + 6.8;

VENT_N         =  6;
VENT_L         = 18.0;
VENT_W         =  1.3;
VENT_PITCH     =  2.6;

DETENT         =  0.30;  // resalto chaflanado, no gancho

// ---- derivados ----
INNER_W = OUTER_W - 2 * WALL;
INNER_H = OUTER_H - FLOOR;
PCB_X   = WALL + TC_RELIEF;
PCB_Y   = (OUTER_W - PCB_W) / 2;
LID_Z   = FLOOR + INNER_H - LID_T - GROOVE_Z_CLEAR;
GROOVE_H = LID_T + GROOVE_Z_CLEAR;
LID_W   = INNER_W + 2 * GROOVE_D - LID_XY_CLEAR;
LID_L   = OUTER_L - WALL - 0.6;

echo("INNER_W", INNER_W, "INNER_H", INNER_H, "LID_W", LID_W, "LID_L", LID_L);

module rrect(l, w, h, r) {
    r = min(r, w / 2 - 0.05, l / 2 - 0.05);
    hull() {
        translate([r, r, 0]) cylinder(h = h, r = r);
        translate([l - r, r, 0]) cylinder(h = h, r = r);
        translate([r, w - r, 0]) cylinder(h = h, r = r);
        translate([l - r, w - r, 0]) cylinder(h = h, r = r);
    }
}

module u_slot(w, h, t) {
    translate([-w / 2, 0, -0.2]) cube([w, t + 0.4, h + 0.2]);
}

module base() {
    difference() {
        union() {
            rrect(OUTER_L, OUTER_W, OUTER_H - 0.15, 2.0);
            // peines exteriores de alivio (solo textura / agarre)
            translate([-1.2, (OUTER_W - 16) / 2, 0])
                cube([1.2, 16, OUTER_H - 4]);
            translate([OUTER_L, (OUTER_W - 20) / 2, 0])
                cube([1.2, 20, OUTER_H - 4]);
        }

        // cavidad
        translate([WALL, WALL, FLOOR])
            cube([OUTER_L - 2 * WALL, INNER_W, INNER_H + 1]);

        // rieles: ranura en las dos paredes largas
        translate([WALL - GROOVE_D, WALL - GROOVE_D, LID_Z])
            cube([OUTER_L, INNER_W + 2 * GROOVE_D, GROOVE_H]);

        // chaflán 45° de entrada (imprimible sin soportes)
        translate([WALL - GROOVE_D, WALL - GROOVE_D, OUTER_H - 1.3])
            rotate([45, 0, 0])
                cube([OUTER_L, 2.2, 2.2]);
        translate([WALL - GROOVE_D, OUTER_W - WALL + GROOVE_D, OUTER_H - 1.3])
            rotate([-45, 0, 0])
                cube([OUTER_L, 2.2, 2.2]);

        // orificios v1 (pequeños)
        for (i = [0 : TC_HOLE_N - 1])
            translate([-0.4, OUTER_W / 2 - (TC_HOLE_N - 1) * TC_HOLE_PITCH / 2 + i * TC_HOLE_PITCH, HOLE_Z])
                rotate([0, 90, 0])
                    cylinder(h = WALL + 1.0, d = TC_HOLE_D);
        translate([OUTER_L - WALL - 0.4, OUTER_W / 2, HOLE_Z])
            rotate([0, 90, 0])
                cylinder(h = WALL + 1.2, d = SPI_HOLE_D);

        // buzón de la tapa en la pared SPI
        translate([OUTER_L - WALL - 0.2, (OUTER_W - (INNER_W + 2 * GROOVE_D - LID_XY_CLEAR)) / 2 - 0.15, LID_Z])
            cube([WALL + 0.6, INNER_W + 2 * GROOVE_D - LID_XY_CLEAR + 0.3, GROOVE_H + 0.15]);
    }

    // piso de la ranura (estante) — se imprime sólido
    translate([WALL, WALL - GROOVE_D, LID_Z - 0.01])
        cube([OUTER_L - WALL, GROOVE_D, 0.9]);
    translate([WALL, WALL + INNER_W, LID_Z - 0.01])
        cube([OUTER_L - WALL, GROOVE_D, 0.9]);

    // postes de la placa (no hace falta tornillo; tapa no los aprieta)
    for (ix = [HOLE_INSET, PCB_L - HOLE_INSET])
        for (iy = [HOLE_INSET, PCB_W - HOLE_INSET])
            translate([PCB_X + ix, PCB_Y + iy, FLOOR])
                difference() {
                    cylinder(h = STANDOFF_H, d = STANDOFF_D);
                    translate([0, 0, 0.4])
                        cylinder(h = STANDOFF_H, d = 1.7);
                }

    // tope de la placa: no se va con el Dupont al abrir la tapa
    translate([PCB_X + PCB_L + 0.3, PCB_Y + 1.5, FLOOR])
        cube([1.2, PCB_W - 3.0, STANDOFF_H + PCB_T + 0.4]);

    // resalto de detent (chaflán, flexión < 0.3 mm)
    translate([WALL + 6, WALL - GROOVE_D, LID_Z + 0.4])
        rotate([0, 45, 0]) cube([DETENT * 2, GROOVE_D, DETENT * 2]);
    translate([WALL + 6, WALL + INNER_W, LID_Z + 0.4])
        rotate([0, 45, 0]) cube([DETENT * 2, GROOVE_D, DETENT * 2]);
}

module lid() {
    difference() {
        translate([0, (OUTER_W - LID_W) / 2, 0])
            cube([LID_L, LID_W, LID_T]);

        // ranuras de ventilación sobre el IC (junta fría)
        vent0 = PCB_X + 4 - WALL;
        for (i = [0 : VENT_N - 1])
            translate([
                vent0,
                (OUTER_W - VENT_L) / 2,
                -0.2
            ])
                translate([i * VENT_PITCH, 0, 0])
                    cube([VENT_W, VENT_L, LID_T + 0.4]);

        // muesca de uña, extremo SPI
        translate([LID_L - 8, OUTER_W / 2 - 6, LID_T - 0.7])
            cube([8.2, 12, 1.0]);
    }
}

// ensamble de referencia (no imprimir junto)
module preview() {
    color([0.22, 0.22, 0.24]) base();
    color([0.28, 0.28, 0.30])
        translate([WALL + 0.3, 0, LID_Z + 0.05])
            lid();
}

preview();
// Para exportar STL por separado, comentar preview() y descomentar uno:
// base();
// translate([0, OUTER_W + 8, 0]) lid();
