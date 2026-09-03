/*
╔══════════════════════════════════════════════════════╗
║   Pi_Zaff System · Console Server Case  v1.0         ║
║                                                      ║
║   Case parametrico per:                              ║
║   - ESP32 RUIZHI Mini (30-pin, USB-C)                ║
║   - Modulo TP4056H USB-C                             ║
║   - MAX3232 (modulo breakout standard)               ║
║   - Batteria 18650 (con porta-cella)                 ║
║   - Switch ON/OFF, morsettiera 3-pin                 ║
║                                                      ║
║   Genera: base + coperchio a incastro                ║
║   Stampa: PLA/PETG, 0.2mm layer, 20% infill          ║
║   Tempo stimato: 4-5 ore totali                      ║
║                                                      ║
║   Uso: apri in OpenSCAD, modifica i parametri,       ║
║         poi seleziona MODE in fondo e F6 → STL       ║
╚══════════════════════════════════════════════════════╝
*/

// ════════════════════════════════════════════════════
//   COSA STAMPARE  (cambia qui prima di esportare STL)
// ════════════════════════════════════════════════════

MODE = "preview";   // "preview" | "base" | "lid" | "base_print" | "lid_print"

// preview      → vista assemblata (per controllare)
// base         → solo base, orientamento normale
// lid          → solo coperchio, orientamento normale
// base_print   → base pronta per slicer (già orientata)
// lid_print    → coperchio pronto per slicer (capovolto)


// ════════════════════════════════════════════════════
//   DIMENSIONI CASE
// ════════════════════════════════════════════════════

case_length    = 95;   // X interno
case_width     = 65;   // Y interno
case_height    = 28;   // Z interno (totale base + coperchio)

wall           = 2.2;  // spessore pareti
floor_thick    = 2.0;  // spessore base
lid_thick      = 2.0;  // spessore coperchio
corner_radius  = 4;    // raggio angoli arrotondati

// rapporto altezza base / altezza totale (resto è coperchio)
base_ratio     = 0.65;

base_h = case_height * base_ratio;
lid_h  = case_height - base_h;


// ════════════════════════════════════════════════════
//   ESP32 RUIZHI MINI (30-pin, USB-C)
// ════════════════════════════════════════════════════

esp_length   = 51.5;
esp_width    = 25.5;
esp_thick    = 1.6;     // PCB
esp_clearance_h = 7;    // altezza componenti sopra (antenna + USB)

// posizione (angolo basso-sinistro PCB)
esp_x = 4;
esp_y = 4;
esp_z = floor_thick + 3;  // altezza standoff

// fori USB-C ESP32 (lato corto, in alto)
esp_usb_y_off = esp_width/2;   // centrato
esp_usb_w     = 10;
esp_usb_h     = 5;


// ════════════════════════════════════════════════════
//   MODULO TP4056H (USB-C)
// ════════════════════════════════════════════════════

tp_length    = 26;
tp_width     = 17;
tp_thick     = 1.4;
tp_clearance_h = 4;

tp_x = esp_x + esp_length + 4;
tp_y = 4;
tp_z = floor_thick + 3;


// ════════════════════════════════════════════════════
//   MODULO MAX3232 (breakout standard)
// ════════════════════════════════════════════════════

max_length   = 21;
max_width    = 16;
max_thick    = 1.4;
max_clearance_h = 5;

max_x = esp_x;
max_y = esp_y + esp_width + 4;
max_z = floor_thick + 3;


// ════════════════════════════════════════════════════
//   BATTERIA 18650
// ════════════════════════════════════════════════════

batt_length  = 67;    // cella 18650 standard
batt_diam    = 19;    // diametro cella (con leggero gioco)
batt_holder_thick = 2; // pareti del vano

batt_x = max_x + max_length + 6;
batt_y = max_y;
batt_z = floor_thick + batt_diam/2 + 1;


// ════════════════════════════════════════════════════
//   MORSETTIERA 3 PIN (uscita seriale)
// ════════════════════════════════════════════════════
//
// Morsettiera tipo KF301-3P, passo 5.08mm
// dimensioni tipiche: ~16×9×11mm
//

term_length  = 16;
term_width   = 9;
term_height  = 11;

// posizione: parete frontale (Y=0), centrata-destra
term_x = case_length - term_length - 10;
term_y = -wall;     // sporge fuori dal case
term_z = floor_thick + 5;


// ════════════════════════════════════════════════════
//   SWITCH ON/OFF (slide laterale)
// ════════════════════════════════════════════════════
//
// Switch SS12D00G3 stile o simile slide switch
// foro tipico: 13×8mm
//

sw_w = 13;
sw_h = 8;

sw_x = -wall;       // parete sinistra (X=0)
sw_y = case_width - 18;
sw_z = floor_thick + 8;


// ════════════════════════════════════════════════════
//   FORI ESTERNI · APERTURE
// ════════════════════════════════════════════════════

// USB-C TP4056 (parete destra)
tp_usb_w = 10;
tp_usb_h = 5;

// USB-C ESP32 (parete superiore Y=max, per debug/programmazione)
esp_usb_access_w = 12;
esp_usb_access_h = 7;

// LED status (foro piccolo sul coperchio)
led_diam = 3.5;


// ════════════════════════════════════════════════════
//   VITI E STANDOFF
// ════════════════════════════════════════════════════

screw_diam_through = 3.2;  // M3 passante
screw_diam_self    = 2.5;  // M3 autofilettante (self-tap nel PLA)
screw_head_diam    = 6;    // testa M3
screw_head_height  = 3;

standoff_diam      = 6;
standoff_inset     = 4;    // distanza dal bordo

// standoff PCB (più piccoli)
pcb_standoff_diam   = 5;
pcb_standoff_height = 3;
pcb_screw_diam      = 2.5; // M2.5 per fissare PCB


// ════════════════════════════════════════════════════
//   QUALITÀ RENDERING
// ════════════════════════════════════════════════════

$fn = 60;


// ════════════════════════════════════════════════════════════════
//   MODULI HELPER
// ════════════════════════════════════════════════════════════════

// Cubo arrotondato (per case esterno)
module rounded_box(l, w, h, r) {
    hull() {
        for (x = [r, l-r])
            for (y = [r, w-r])
                translate([x, y, 0])
                    cylinder(h=h, r=r);
    }
}

// Cubo arrotondato cavo (case)
module rounded_shell(l, w, h, r, thickness) {
    difference() {
        rounded_box(l, w, h, r);
        translate([thickness, thickness, thickness])
            rounded_box(l - 2*thickness, w - 2*thickness, h, max(0.5, r - thickness));
    }
}

// Standoff con foro per vite autofilettante
module standoff(h, d_outer, d_hole) {
    difference() {
        cylinder(h=h, d=d_outer);
        translate([0, 0, -0.1])
            cylinder(h=h+0.2, d=d_hole);
    }
}

// Standoff PCB (con piccolo gradino per appoggio)
module pcb_standoff(h, d_outer, d_hole) {
    difference() {
        union() {
            cylinder(h=h, d=d_outer);
            cylinder(h=h-1, d=d_outer+1.5); // base più larga
        }
        translate([0, 0, -0.1])
            cylinder(h=h+0.2, d=d_hole);
    }
}

// Slot rettangolare con angoli arrotondati
module slot(w, h, depth, r=1) {
    hull() {
        for (x = [r, w-r])
            for (y = [r, h-r])
                translate([x, y, 0])
                    cylinder(h=depth, r=r);
    }
}

// Logo/testo Pi_Zaff embossed
module logo_emboss(depth=0.6) {
    // testo principale
    translate([0, 5, 0])
        linear_extrude(height=depth)
            text("Pi_Zaff", size=7, font="Liberation Sans:style=Bold",
                 halign="center", valign="center");
    translate([0, -3, 0])
        linear_extrude(height=depth)
            text("SYSTEM", size=4, font="Liberation Sans:style=Bold",
                 halign="center", valign="center", spacing=1.4);
    translate([0, -10, 0])
        linear_extrude(height=depth)
            text("CONSOLE SERVER", size=2.4, font="Liberation Sans",
                 halign="center", valign="center", spacing=1.6);
}

// Griglia ventilazione (slot paralleli)
module vent_slots(rows, cols, slot_w, slot_h, gap, depth) {
    for (r = [0:rows-1])
        for (c = [0:cols-1])
            translate([c*(slot_w+gap), r*(slot_h+gap), 0])
                slot(slot_w, slot_h, depth, r=slot_h/2);
}


// ════════════════════════════════════════════════════════════════
//   BASE DEL CASE
// ════════════════════════════════════════════════════════════════

module case_base() {

    difference() {
        union() {
            // guscio esterno
            rounded_shell(case_length, case_width, base_h, corner_radius, wall);

            // pavimento
            translate([wall, wall, 0])
                rounded_box(case_length - 2*wall, case_width - 2*wall,
                            floor_thick, max(0.5, corner_radius - wall));

            // ─── STANDOFF PCB ───

            // ESP32 - 4 fori, di solito sono ai 4 angoli del PCB
            // dist. centri fori ESP32 mini 30pin: ~46.5 × 21
            esp_hole_dx = 46.5;
            esp_hole_dy = 21;
            esp_hole_offset_x = (esp_length - esp_hole_dx) / 2;
            esp_hole_offset_y = (esp_width - esp_hole_dy) / 2;

            for (dx = [esp_hole_offset_x, esp_hole_offset_x + esp_hole_dx])
                for (dy = [esp_hole_offset_y, esp_hole_offset_y + esp_hole_dy])
                    translate([esp_x + dx, esp_y + dy, floor_thick])
                        pcb_standoff(pcb_standoff_height, pcb_standoff_diam, pcb_screw_diam);

            // TP4056H - 2 fori (di solito ha 2 fori M2 ai lati)
            // Se il tuo modulo non li ha, usa biadesivo o hot-glue: rimuovi questa parte
            for (dx = [2, tp_length - 2])
                translate([tp_x + dx, tp_y + tp_width/2, floor_thick])
                    pcb_standoff(pcb_standoff_height, pcb_standoff_diam - 1, 2.2);

            // MAX3232 - 2 fori
            for (dx = [2, max_length - 2])
                translate([max_x + dx, max_y + max_width/2, floor_thick])
                    pcb_standoff(pcb_standoff_height, pcb_standoff_diam - 1, 2.2);

            // ─── VANO BATTERIA 18650 (clip a U) ───
            translate([batt_x, batt_y, floor_thick]) {
                // due pareti laterali della culla
                for (yoff = [0, batt_length - 4])
                    translate([0, yoff, 0])
                        cube([batt_diam + 4, 4, batt_diam * 0.7]);

                // culla a forma di V/U che tiene la cella
                difference() {
                    cube([batt_diam + 4, batt_length, batt_diam * 0.5]);
                    translate([(batt_diam + 4)/2, -1, batt_diam * 0.5 + batt_diam/2])
                        rotate([-90, 0, 0])
                            cylinder(h=batt_length + 2, d=batt_diam);
                }
            }

            // ─── STANDOFF VITI ANGOLI CASE ───
            for (x = [standoff_inset + wall, case_length - standoff_inset - wall])
                for (y = [standoff_inset + wall, case_width - standoff_inset - wall])
                    translate([x, y, floor_thick])
                        standoff(base_h - floor_thick - 0.5, standoff_diam, screw_diam_self);
        }

        // ─── APERTURE NELLA PARETE ───

        // USB-C TP4056 (parete destra, X = case_length)
        translate([case_length - wall - 0.5,
                   tp_y + tp_width/2 - tp_usb_w/2,
                   floor_thick + tp_thick + 0.5])
            cube([wall + 1, tp_usb_w, tp_usb_h]);

        // USB-C ESP32 (parete superiore, Y = case_width) — per debug/programmazione
        translate([esp_x + esp_length/2 - esp_usb_access_w/2,
                   case_width - wall - 0.5,
                   floor_thick + esp_thick + 1])
            cube([esp_usb_access_w, wall + 1, esp_usb_access_h]);

        // Switch (parete sinistra)
        translate([-0.5, sw_y, sw_z])
            cube([wall + 1, sw_w, sw_h]);

        // Morsettiera 3-pin (parete frontale Y=0)
        translate([term_x, -0.5, term_z])
            cube([term_length, wall + 1, term_height]);

        // Ventilazione laterale (parete sinistra-bassa)
        translate([-0.5, 5, base_h - 8])
            rotate([0, 90, 0])
                vent_slots(1, 4, 8, 2, 2, wall + 1);
    }
}


// ════════════════════════════════════════════════════════════════
//   COPERCHIO
// ════════════════════════════════════════════════════════════════

module case_lid() {

    difference() {
        union() {
            // top piatto + bordi che scendono leggermente
            translate([0, 0, 0])
                rounded_box(case_length, case_width, lid_thick, corner_radius);

            // labbro interno che entra nella base (incastro 1mm)
            translate([wall + 0.4, wall + 0.4, lid_thick])
                rounded_box(case_length - 2*(wall + 0.4),
                            case_width  - 2*(wall + 0.4),
                            lid_h - lid_thick,
                            max(0.5, corner_radius - wall - 0.4));

            // svuotamento interno labbro (per peso e materiale)
            // viene fatto nella difference sotto
        }

        // svuota internamente il labbro
        translate([wall*2 + 0.4, wall*2 + 0.4, lid_thick])
            rounded_box(case_length - 2*(wall*2 + 0.4),
                        case_width  - 2*(wall*2 + 0.4),
                        lid_h + 1,
                        max(0.3, corner_radius - wall*2 - 0.4));

        // fori per viti angoli
        for (x = [standoff_inset + wall, case_length - standoff_inset - wall])
            for (y = [standoff_inset + wall, case_width - standoff_inset - wall])
                translate([x, y, -0.1]) {
                    // foro passante
                    cylinder(h=lid_h + 1, d=screw_diam_through);
                    // svaso testa vite (countersink)
                    cylinder(h=screw_head_height, d1=screw_head_diam, d2=screw_diam_through);
                }

        // ─── LED status (sopra ESP32) ───
        translate([esp_x + esp_length - 8,
                   esp_y + esp_width - 6,
                   -0.1])
            cylinder(h=lid_thick + 0.2, d=led_diam);

        // ─── Logo Pi_Zaff embossed ───
        translate([case_length/2, case_width/2, -0.1])
            mirror([1, 0, 0])
                logo_emboss(0.6);

        // ─── griglia ventilazione sopra ESP ───
        translate([esp_x + 5, esp_y + 5, -0.1])
            vent_slots(3, 4, 6, 2, 2, lid_thick + 0.2);
    }
}


// ════════════════════════════════════════════════════════════════
//   PREVIEW ASSEMBLATO
// ════════════════════════════════════════════════════════════════

module preview_components() {
    // PCB ESP32 (azzurro)
    color("MediumBlue", 0.5)
        translate([esp_x, esp_y, esp_z])
            cube([esp_length, esp_width, esp_thick]);

    // PCB TP4056 (rosso)
    color("Crimson", 0.5)
        translate([tp_x, tp_y, tp_z])
            cube([tp_length, tp_width, tp_thick]);

    // PCB MAX3232 (verde)
    color("ForestGreen", 0.5)
        translate([max_x, max_y, max_z])
            cube([max_length, max_width, max_thick]);

    // Batteria 18650 (grigio)
    color("DimGray", 0.6)
        translate([batt_x + (batt_diam+4)/2, batt_y, batt_z])
            rotate([-90, 0, 0])
                cylinder(h=batt_length, d=batt_diam);

    // Morsettiera (verde scuro)
    color("DarkGreen", 0.6)
        translate([term_x, term_y, term_z])
            cube([term_length, term_width + wall, term_height]);
}

module preview() {
    color("LightGray", 0.4) case_base();
    preview_components();
    translate([0, 0, base_h + 8])
        color("LightSlateGray", 0.4)
            case_lid();
}


// ════════════════════════════════════════════════════════════════
//   ROUTER MODE
// ════════════════════════════════════════════════════════════════

if (MODE == "preview") {
    preview();
}
else if (MODE == "base") {
    case_base();
}
else if (MODE == "lid") {
    case_lid();
}
else if (MODE == "base_print") {
    // già ben orientata (apertura verso l'alto)
    case_base();
}
else if (MODE == "lid_print") {
    // capovolgi per stampa (parte piatta sopra al letto)
    translate([0, case_width, lid_h])
        rotate([180, 0, 0])
            case_lid();
}
else {
    // default: preview
    preview();
}


// ════════════════════════════════════════════════════════════════
//   NOTE DI STAMPA
// ════════════════════════════════════════════════════════════════
/*
SLICER SETTINGS CONSIGLIATI:

  Material:        PLA o PETG
  Layer height:    0.2 mm
  Wall count:      3 perimetri
  Top/Bottom:      4 layer
  Infill:          20-25% gyroid o cubic
  Supports:        NO (geometria pensata per non averne bisogno)
  Brim:            5mm (per sicurezza adesione su parti larghe)

HARDWARE EXTRA NECESSARIO:

  - 4× viti M3 × 8mm autofilettanti (chiudere coperchio)
  - 4× viti M2.5 × 6mm per fissare ESP32 (opzionale)
  - 2× viti M2 × 5mm per TP4056 e MAX3232 (opzionali, biadesivo va bene)
  - Porta-batteria 18650 con fili e JST PH2.0 (oppure salda direttamente)

WORKFLOW PER GENERARE STL:

  1. Apri questo file in OpenSCAD
  2. Cambia MODE = "base_print"  → premi F6 → Esporta STL come "case_base.stl"
  3. Cambia MODE = "lid_print"   → premi F6 → Esporta STL come "case_lid.stl"
  4. Importa entrambi nello slicer (Cura/PrusaSlicer/Bambu Studio)

SE QUALCOSA NON COMBACIA:

  - I moduli reali variano leggermente di marca in marca.
  - Misura con calibro i tuoi componenti e modifica i parametri in cima.
  - Lo standoff PCB è ridimensionabile, le aperture pure.
  - Se il labbro del coperchio è troppo stretto, aumenta il "+0.4" a "+0.6"
*/
