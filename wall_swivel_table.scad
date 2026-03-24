/*
 * ============================================================
 *  Wall-Mounted Horizontal Swivel Table  –  v2.0
 *  Designed for Bambu P2S  (256 × 256 × 256 mm build plate)
 * ============================================================
 *
 * CONCEPT
 * -------
 * A wall-mounted table that swivels horizontally (like a door) on a
 * vertical pivot post.  Swinging 90° out from the wall deploys the
 * table; swinging it back stores it flush against the wall.
 * Rated for 5+ lbs continuous load.  Material: PETG recommended for
 * structural parts, PLA+ minimum.
 *
 * ─────────────────────────────────────────────────────────────
 *  PARTS TO PRINT
 * ─────────────────────────────────────────────────────────────
 *  Name           | Qty | Material | Walls | Infill | Notes
 *  ───────────────+─────+──────────+───────+────────+─────────────────
 *  wall_bracket   |  1  | PETG     |  5    | 45 %   | critical structural
 *  swivel_arm     |  1  | PETG     |  5    | 45 %   | critical structural
 *  table_top      |  1  | PLA/PETG |  3    | 20 %   | decorative ribs add stiffness
 *  pivot_cap      |  1  | PETG     |  4    | 30 %   | bolt-head cover
 *
 * ─────────────────────────────────────────────────────────────
 *  HARDWARE REQUIRED
 * ─────────────────────────────────────────────────────────────
 *  • 1× M8 × 110 mm bolt  (grade 8.8 steel or stainless) – main pivot
 *  • 2× M8 flat washer  (one above socket, one below post)
 *  • 1× M8 nylon-insert lock nut  (adjusts swivel friction)
 *  • 4× M5 × 50 mm screws + wall anchors  (wall mount – use studs!)
 *  • 4× M4 × 20 mm screws + M4 hex nuts   (table top ↔ arm)
 *  • Optional: 2× M4 × 12 mm self-tapping screws (pivot_cap clip)
 *
 * ─────────────────────────────────────────────────────────────
 *  PRINT PLATES  (set VIEW below)
 * ─────────────────────────────────────────────────────────────
 *  Plate 1  –  wall_bracket + pivot_cap
 *  Plate 2  –  swivel_arm   (on its side, socket bore up)
 *  Plate 3  –  table_top  (flat on bed)
 *
 *  VIEW values:
 *    "assembled"      full 90° deployed view
 *    "stored"         view with table folded flat against wall
 *    "wall_bracket"   bracket part only
 *    "swivel_arm"     arm part only
 *    "table_top"      table top only
 *    "pivot_cap"      cap only
 *    "plate1"         print plate 1
 *    "plate2"         print plate 2
 *    "plate3"         print plate 3
 *
 * ─────────────────────────────────────────────────────────────
 *  ASSEMBLY ORDER
 * ─────────────────────────────────────────────────────────────
 *  1. Mount wall_bracket to wall with 4× M5 screws into studs/anchors.
 *     Height suggestion: pivot post centre at ~950 mm from floor gives
 *     a table surface at ~1000 mm (standing height).
 *  2. Slide swivel_arm socket over the pivot post (it should rotate freely).
 *  3. Place one M8 washer on top of the arm socket.
 *  4. Drop M8 × 110 mm bolt down through washer → socket → post.
 *  5. Add second M8 washer then nylon lock nut from below the bracket.
 *  6. Tighten lock nut until the arm stays in any swivelled position by
 *     itself yet can be moved with a gentle push (~8-10 N applied at
 *     the table edge).  Re-adjust any time by loosening/tightening.
 *  7. Press pivot_cap onto the bolt head (snap-fit + optional M4 screw).
 *  8. Place table_top on top of the arm flange, holes aligned.
 *     Insert 4× M4 screws from below; thread into M4 nuts in the table.
 */

// ═══════════════════════════════════════════════════════
//  ►  CHANGE THIS to switch the displayed part / view
// ═══════════════════════════════════════════════════════
VIEW = "assembled";

// ═══════════════════════════════════════════════════════
//  GLOBAL
// ═══════════════════════════════════════════════════════
$fn    = 80;
TOL    = 0.35;          // general mating clearance (mm)
EPS    = 0.01;          // tiny overlap for clean boolean ops

// ─── Wall bracket ──────────────────────────────────────
WB_W   = 80;            // bracket plate width   (X)
WB_H   = 200;           // bracket plate height  (Z)
WB_D   = 14;            // bracket plate depth   (Y – into wall)

// pivot post dimensions
POST_DIA   = 26;        // outer ∅ (mm)
POST_H     = 88;        // height  (mm)
POST_BORE  = 8.8;       // M8 clearance hole ∅
POST_BASE_Z = (WB_H - POST_H) / 2;  // centre post vertically on bracket

// ─── Swivel arm ────────────────────────────────────────
ARM_LEN   = 250;        // pivot centre → table-end distance
ARM_W     = 54;         // box-section width  (X in assembled view)
ARM_H     = 38;         // box-section height (Z in assembled view)
ARM_WALL  = 5;          // hollow-box wall thickness

SOCK_ID   = POST_DIA + TOL*2;      // socket inner ∅ (clearance over post)
SOCK_OD   = POST_DIA + 20;         // socket outer ∅
SOCK_H    = POST_H - 4;            // socket depth (slightly less than post)

// arm_start_z: socket origin is at 0; arm box is centred on socket height
ARM_Z0    = (SOCK_H - ARM_H) / 2;  // Z of arm bottom face (local arm coords)

// ─── Table top ─────────────────────────────────────────
TT_W  = 240;            // width  (X)
TT_D  = 200;            // depth  (Y)
TT_H  = 9;              // thickness
TT_R  = 14;             // corner radius
TT_RIB_H = 6;           // underside rib height (stiffening)

// ─── Hardware clearances ───────────────────────────────
M4C  = 4.5;             // M4 clearance hole ∅
M4HD = 8.2;             // M4 head ∅  (for countersink)
M5C  = 5.5;             // M5 clearance hole ∅
M5HD = 10.0;            // M5 head ∅  (for countersink)
M4N  = 7.0;             // M4 nut across-flats (hex, +0.2 mm)
M4ND = 3.5;             // M4 nut thickness
CSK  = 4.5;             // countersink cone depth

// ═══════════════════════════════════════════════════════
//  HELPERS
// ═══════════════════════════════════════════════════════

// Rounded rectangle (XY, origin at centre-bottom)
module rrect(w, d, h, r) {
    hull()
        for(sx=[-1,1]) for(sy=[-1,1])
            translate([sx*(w/2-r), sy*(d/2-r), 0])
                cylinder(h=h, r=r);
}

// Countersunk hole  (axis = +Z, countersink flares downward at Z=0)
module csk_hole(dia, hd, depth, thru) {
    cylinder(h=thru+EPS, d=dia);
    translate([0,0,-EPS]) cylinder(h=depth+EPS, d1=hd, d2=dia);
}

// M4 nut trap (hexagonal, recessed from –Z face)
module nut_trap(depth=M4ND+0.3) {
    cylinder(h=depth, d=M4N/cos(30)+0.4, $fn=6);
}

// ═══════════════════════════════════════════════════════
//  PART 1 – WALL BRACKET
// ═══════════════════════════════════════════════════════
// Local origin: back-bottom-left of wall plate.
// +Y → away from wall (into room),  +Z → up,  +X → right.

module wall_bracket() {
    difference() {
        union() {
            // ── Plate ──────────────────────────────────
            cube([WB_W, WB_D, WB_H]);

            // ── Vertical stiffening ribs on plate face ─
            for(xi = [WB_W*0.22, WB_W*0.78])
                translate([xi-2, 0, 0]) cube([4, WB_D, WB_H]);

            // ── Pivot post (at plate front face, centred) ──
            translate([WB_W/2, WB_D, POST_BASE_Z])
                cylinder(h=POST_H, d=POST_DIA);

            // ── Gusset fillets at post base ─────────────
            for(angle=[0,90,180,270])
                translate([WB_W/2, WB_D, POST_BASE_Z])
                rotate([0, 0, angle])
                translate([POST_DIA/2, 0, 0])
                    rotate([0, 0, 45])
                        cube([5, 5, min(POST_H*0.35, 25)]);

            // ── Horizontal shelf under post (stops arm sliding down) ──
            translate([WB_W/2 - SOCK_OD/2 - 2, WB_D, POST_BASE_Z - 4])
                cube([SOCK_OD + 4, POST_DIA + 4, 4]);
        }

        // ── 4× M5 countersunk wall-mount holes ──────────
        for(xi = [WB_W*0.18, WB_W*0.82])
            for(zi = [WB_H*0.12, WB_H*0.88])
                translate([xi, 0, zi]) rotate([-90,0,0]) {
                    cylinder(h=WB_D+EPS, d=M5C);
                    translate([0,0,-EPS]) cylinder(h=CSK, d1=M5HD, d2=M5C);
                }

        // ── M8 clearance bore through post (and shelf) ──
        translate([WB_W/2, WB_D, POST_BASE_Z - 5])
            cylinder(h=POST_H + 6, d=POST_BORE);
    }
}

// ═══════════════════════════════════════════════════════
//  PART 2 – SWIVEL ARM
// ═══════════════════════════════════════════════════════
// Local origin: centre of pivot bore at Z=0 (bottom of socket).
// Socket extends 0…SOCK_H in Z.  Arm extends in +Y direction.

module swivel_arm() {
    flange_w = ARM_W + 22;
    flange_d = 34;

    difference() {
        union() {
            // ── Socket cylinder ─────────────────────────
            cylinder(h=SOCK_H, d=SOCK_OD);

            // ── Blended gusset (socket → arm side) ──────
            hull() {
                cylinder(h=ARM_H, d=SOCK_OD);
                translate([-ARM_W/2, SOCK_OD*0.6, ARM_Z0])
                    cube([ARM_W, EPS, ARM_H]);
            }

            // ── Box arm ─────────────────────────────────
            translate([-ARM_W/2, SOCK_OD*0.6, ARM_Z0])
                cube([ARM_W, ARM_LEN - SOCK_OD*0.6 - flange_d, ARM_H]);

            // ── Table-end flange ─────────────────────────
            translate([-flange_w/2, ARM_LEN - flange_d, ARM_Z0])
                cube([flange_w, flange_d, ARM_H]);

            // ── Fillet ribs along arm for stiffness ──────
            for(xi = [-ARM_W/2, ARM_W/2 - 4])
                translate([xi, SOCK_OD*0.6, ARM_Z0])
                    cube([4, ARM_LEN - SOCK_OD*0.6 - flange_d, ARM_H]);
        }

        // ── Hollow arm interior ──────────────────────────
        translate([-ARM_W/2 + ARM_WALL,
                   SOCK_OD*0.6 + ARM_WALL,
                   ARM_Z0 + ARM_WALL])
            cube([ARM_W - 2*ARM_WALL,
                  ARM_LEN - SOCK_OD*0.6 - flange_d - ARM_WALL,
                  ARM_H  - 2*ARM_WALL]);

        // ── Socket bore (clearance over pivot post) ──────
        cylinder(h=SOCK_H + EPS, d=SOCK_ID);

        // ── M8 bolt clearance (axial, full depth) ────────
        cylinder(h=SOCK_H + EPS, d=POST_BORE);

        // ── Top chamfer on socket lip ────────────────────
        translate([0, 0, SOCK_H - 2])
            cylinder(h=3, d1=SOCK_ID, d2=SOCK_ID + 4);

        // ── 4× M4 countersunk holes for table top ────────
        // (accessed from below the arm flange, csk faces down)
        for(xi = [-(flange_w/2-11), (flange_w/2-11)])
            for(yi = [ARM_LEN - flange_d + 7, ARM_LEN - 10])
                translate([xi, yi, ARM_Z0 + ARM_H + EPS])
                    rotate([0,0,0]) mirror([0,0,1])
                        csk_hole(M4C, M4HD, CSK, ARM_H+1);

        // ── M4 nut traps (in flange top face) ────────────
        for(xi = [-(flange_w/2-11), (flange_w/2-11)])
            for(yi = [ARM_LEN - flange_d + 7, ARM_LEN - 10])
                translate([xi, yi, ARM_Z0 + ARM_H - M4ND - 0.3])
                    nut_trap();
    }
}

// ═══════════════════════════════════════════════════════
//  PART 3 – TABLE TOP
// ═══════════════════════════════════════════════════════
// Origin: centre-bottom.

module table_top() {
    flange_w = ARM_W + 22;
    difference() {
        union() {
            // ── Main slab ────────────────────────────────
            rrect(TT_W, TT_D, TT_H, TT_R);

            // ── Underside stiffening ribs ─────────────────
            // 2 lengthwise ribs
            for(xi = [-TT_W/4, TT_W/4])
                translate([xi, 0, -TT_RIB_H])
                    rrect(4, TT_D - TT_R*2, TT_RIB_H, 1);
            // 1 cross rib
            translate([0, 0, -TT_RIB_H])
                rrect(TT_W - TT_R*2, 4, TT_RIB_H, 1);
        }

        // ── 4× M4 clearance + countersink holes ──────────
        for(xi = [-(flange_w/2-11), (flange_w/2-11)])
            for(yi = [-TT_D/2 + 10, -TT_D/2 + 27])
                translate([xi, yi, TT_H + EPS])
                    mirror([0,0,1])
                        csk_hole(M4C, M4HD, CSK, TT_H + 1);
    }
}

// ═══════════════════════════════════════════════════════
//  PART 4 – PIVOT CAP
// ═══════════════════════════════════════════════════════
// Snap-fits (press-fit) over the top of the swivel arm socket,
// covering the M8 bolt head.  Origin: bottom centre.

module pivot_cap() {
    cap_h  = 14;
    cap_od = SOCK_OD + 5;
    snap_id = SOCK_OD + TOL*2;   // slight clearance over socket outer wall

    difference() {
        union() {
            // body
            cylinder(h=cap_h, d=cap_od);
            // brim flange
            translate([0, 0, cap_h - 3.5])
                cylinder(h=3.5, d=cap_od + 7);
        }
        // clearance bore (slides over socket top)
        cylinder(h=cap_h - 6, d=snap_id);
        // bolt-head recess (M8 hex: 14 mm A/F, ~16 mm OD)
        translate([0, 0, cap_h - 6 - EPS])
            cylinder(h=7, d=16);
        // M8 bolt shank clearance
        cylinder(h=cap_h + EPS, d=POST_BORE);
    }
}

// ═══════════════════════════════════════════════════════
//  ASSEMBLED VIEW
// ═══════════════════════════════════════════════════════

module assemble(swivel_deg = 90) {
    pivot_x = WB_W / 2;
    pivot_y = WB_D;
    pivot_z = POST_BASE_Z;

    // ── Wall bracket ──────────────────────────────────────
    color([0.27, 0.51, 0.71], 0.97)   // steel blue
        wall_bracket();

    // ── Rotating assembly ─────────────────────────────────
    translate([pivot_x, pivot_y, pivot_z])
    rotate([0, 0, swivel_deg]) {

        // swivel arm (socket sits over post; slight Z lift for clearance)
        arm_z_lift = 2;
        color([0.93, 0.56, 0.11], 0.97)  // orange
            translate([0, 0, arm_z_lift])
                swivel_arm();

        // pivot cap on top of socket
        color([0.55, 0.55, 0.55], 0.95)
            translate([0, 0, arm_z_lift + SOCK_H + 3])
                pivot_cap();

        // table top (sits on top of arm flange)
        tt_z = arm_z_lift + ARM_Z0 + ARM_H;
        color([0.96, 0.88, 0.68], 0.97)  // warm wood
            translate([-TT_W/2, ARM_LEN - TT_D/2, tt_z])
                table_top();
    }
}

// ═══════════════════════════════════════════════════════
//  PRINT PLATES
// ═══════════════════════════════════════════════════════

module plate1() {
    // wall_bracket – flat on bed (wall-back face down)
    // Rotate so back (Y=0) of bracket is on the build plate
    rotate([90, 0, 0])
        translate([0, 0, -WB_D])
            wall_bracket();

    // pivot_cap beside bracket
    translate([WB_W + 18, 20, 0])
        pivot_cap();
}

module plate2() {
    // swivel_arm – on its side (socket bore now faces up for clean bridging)
    // Arm long axis in Y, socket bore in Z
    translate([0, SOCK_H, 0])
    rotate([90, 0, 0])
        swivel_arm();
}

module plate3() {
    // table top – flat on bed (top face up; ribs are underneath = no supports)
    translate([0, 0, TT_RIB_H])
    mirror([0, 0, 1])
        table_top();
}

// ═══════════════════════════════════════════════════════
//  DISPATCHER
// ═══════════════════════════════════════════════════════
if      (VIEW=="assembled")    assemble(0);   // arm perpendicular to wall = deployed
else if (VIEW=="stored")       assemble(90);  // arm parallel to wall = stored against wall
else if (VIEW=="wall_bracket") wall_bracket();
else if (VIEW=="swivel_arm")   swivel_arm();
else if (VIEW=="table_top")    table_top();
else if (VIEW=="pivot_cap")    pivot_cap();
else if (VIEW=="plate1")       plate1();
else if (VIEW=="plate2")       plate2();
else if (VIEW=="plate3")       plate3();
