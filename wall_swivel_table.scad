// ============================================================
// Wall-Mounted Swivel Table
// Designed for 3D printing on Bambu Lab P2S (256x256x256mm)
// Material: PETG recommended (PLA acceptable for lighter loads)
//
// DESCRIPTION:
//   A small table that mounts to a wall via a bracket and
//   swivels 90° outward for use. When not in use, it folds
//   flat against the wall. The friction-based pivot allows
//   smooth operation while preventing unwanted movement.
//
// COMPONENTS (3 printed parts):
//   1. Wall Bracket   - L-shaped bracket, screws into wall
//   2. Swivel Platform - Rotating arm assembly with pivot hub
//   3. Table Top       - Flat surface with ribs and edge lips
//
// HARDWARE REQUIRED (non-printed):
//   - 1x M8×50mm hex bolt (pivot axle)
//   - 3x M8 flat washers (one under bolt head, one between
//         bracket and platform, one on top of platform)
//   - 1x M8 nylon washer (between bracket shelf and platform
//         for controlled friction)
//   - 1x M8 nylock nut (tighten to adjust swivel friction)
//   - 4x #10 × 2.5" wood screws OR wall anchors (to mount
//         bracket to wall — use studs if possible!)
//   - 6x M4×20mm socket head cap screws (table top to platform)
//   - 6x M4 nylock nuts (secure table top bolts)
//
// PRINT SETTINGS (per part):
//   - Layer height: 0.2mm
//   - Infill: 50%+ (gyroid or grid pattern)
//   - Walls/Perimeters: 4 minimum
//   - Top/Bottom solid layers: 5 minimum
//   - Supports: None needed (designed for supportless printing)
//   - Orientation: Print each part flat on the bed as exported
//
// ASSEMBLY:
//   1. Mount wall bracket to wall with 4 screws into studs
//   2. Place M8 bolt through bracket shelf hole from below
//      (with washer under bolt head)
//   3. Add nylon washer on top of bracket shelf
//   4. Place swivel platform over the bolt
//   5. Add flat washer on top of platform, then nylock nut
//   6. Tighten nut until swivel has firm but smooth resistance
//   7. Attach table top to platform with 6x M4 bolts & nuts
//
// BED SIZE CHECK (Bambu P2S = 256×256mm):
//   - Wall bracket:     80mm × 150mm  ✓
//   - Swivel platform: ~195mm × 175mm ✓
//   - Table top:       ~240mm × 170mm ✓
// ============================================================

/* [Render Mode] */
// Choose what to display
render_mode = "assembly"; // ["assembly","wall_bracket","swivel_platform","table_top","exploded"]

// Swing angle for assembly view (0=stowed, 90=in use)
swing_angle = 80; // [0:5:90]

/* [Table Top] */
table_width  = 240;   // Along-wall dimension (mm)
table_depth  = 170;   // Out-from-wall dimension (mm)
table_thick  = 7;     // Surface thickness (mm)
edge_lip_h   = 4;     // Raised edge height (mm)
edge_lip_w   = 3;     // Raised edge width (mm)

/* [Wall Bracket] */
wb_height     = 150;  // Vertical plate height (mm)
wb_width      = 80;   // Vertical plate width (mm)
wb_wall_t     = 10;   // Wall plate thickness (mm)
wb_shelf_dep  = 65;   // Shelf depth from wall face (mm)
wb_shelf_t    = 12;   // Shelf thickness (mm)

/* [Swivel Platform] */
hub_dia       = 44;   // Central hub diameter (mm)
arm_length    = 180;  // Arm length from hub center (mm)
arm_width     = 26;   // Each arm width (mm)
arm_gap       = 140;  // Center-to-center arm spacing (mm)
plat_thick    = 10;   // Platform thickness (mm)

/* [Hardware] */
m8_clear      = 8.5;  // M8 bolt clearance hole (mm)
m8_washer_od  = 17;   // M8 washer recess diameter (mm)
m8_washer_t   = 2;    // Washer recess depth (mm)
m4_clear      = 4.5;  // M4 bolt clearance hole (mm)
m4_head_dia   = 8;    // M4 socket head counterbore dia (mm)
m4_head_dep   = 4;    // M4 counterbore depth (mm)
screw_clear   = 5.5;  // #10 wood screw clearance (mm)

// ============== Internal Parameters ==============
$fn = 60;
_eps = 0.01;    // epsilon for clean boolean ops
_gusset_t = 8;  // gusset plate thickness
_boss_h = 3;    // pivot boss raised height
_boss_dia = hub_dia + 10; // boss diameter on shelf
_rib_h = 10;    // underside rib height
_rib_w = 3.5;   // rib width
_stop_w = 10;   // rotation stop block width
_stop_h = 16;   // rotation stop block height

// Pivot center location on bracket shelf
_pvt_x = wb_width / 2;
_pvt_y = wb_wall_t + (wb_shelf_dep - wb_wall_t) / 2;

// ============== Helper Modules ==============

// Box with rounded XY corners
module rbox(size, r=3) {
    hull() {
        for (x=[r, size[0]-r], y=[r, size[1]-r])
            translate([x, y, 0])
                cylinder(r=r, h=size[2]);
    }
}

// ============== PART 1: WALL BRACKET ==============
module wall_bracket() {
    difference() {
        union() {
            // Vertical wall plate
            rbox([wb_width, wb_wall_t, wb_height], r=2);

            // Horizontal shelf (load-bearing platform)
            rbox([wb_width, wb_shelf_dep, wb_shelf_t], r=2);

            // Triangular gussets (2×) for load transfer
            // Uses hull of two thin boxes to form a clean wedge shape
            _gh = wb_height - wb_shelf_t - 20;
            _gd = wb_shelf_dep - wb_wall_t - 10;
            for (side = [0, 1]) {
                gx = (side == 0) ? 12 : wb_width - 12 - _gusset_t;
                hull() {
                    // Base along shelf top surface
                    translate([gx, wb_wall_t, wb_shelf_t])
                        cube([_gusset_t, _gd, 2]);
                    // Top edge against wall plate
                    translate([gx, wb_wall_t, wb_shelf_t + _gh])
                        cube([_gusset_t, 2, 2]);
                }
            }

            // Pivot boss on shelf top
            translate([_pvt_x, _pvt_y, wb_shelf_t - _eps])
                cylinder(d=_boss_dia, h=_boss_h + _eps);

            // Rotation stop block
            translate([wb_width - 14, wb_wall_t + 1, wb_shelf_t])
                rbox([11, _stop_w, _stop_h], r=1.5);
        }

        // Pivot bolt hole
        translate([_pvt_x, _pvt_y, -1])
            cylinder(d=m8_clear, h=wb_shelf_t + _boss_h + 5);

        // Washer recess (bottom of shelf)
        translate([_pvt_x, _pvt_y, -_eps])
            cylinder(d=m8_washer_od + 1, h=m8_washer_t);

        // Wall mounting screw holes (4×)
        for (col = [18, wb_width-18])
            for (row = [wb_shelf_t + 25, wb_height - 25]) {
                translate([col, -1, row])
                    rotate([-90, 0, 0])
                    cylinder(d=screw_clear, h=wb_wall_t + 2);
                // Countersink
                translate([col, wb_wall_t - 2.5, row])
                    rotate([-90, 0, 0])
                    cylinder(d1=screw_clear, d2=screw_clear*2, h=3);
            }
    }
}


// ============== PART 2: SWIVEL PLATFORM ==============
module swivel_platform() {
    // Origin at pivot center. Arms extend in +Y.
    _arm_start = hub_dia/2 - 8;

    difference() {
        union() {
            // Central hub
            cylinder(d=hub_dia, h=plat_thick);

            // Left arm
            translate([-arm_gap/2 - arm_width/2, _arm_start, 0])
                rbox([arm_width, arm_length - _arm_start, plat_thick], r=4);
            // Right arm
            translate([arm_gap/2 - arm_width/2, _arm_start, 0])
                rbox([arm_width, arm_length - _arm_start, plat_thick], r=4);

            // Smooth hub-to-arm transitions
            for (sign = [-1, 1]) {
                hull() {
                    translate([sign * (hub_dia/2 - 5), 2, 0])
                        cylinder(d=14, h=plat_thick);
                    translate([sign * arm_gap/2, _arm_start + 15, 0])
                        cylinder(d=arm_width, h=plat_thick);
                }
            }

            // Cross brace near arm tips
            translate([-arm_gap/2 - arm_width/2, arm_length - 28, 0])
                rbox([arm_gap + arm_width, 20, plat_thick], r=4);

            // Cross brace at middle
            _mid = (_arm_start + arm_length) / 2;
            translate([-arm_gap/2 - arm_width/2, _mid - 8, 0])
                rbox([arm_gap + arm_width, 16, plat_thick], r=4);

            // Rotation stop tab (contacts bracket stop block)
            rotate([0, 0, -8])
                translate([-hub_dia/2 - 6, -14, 0])
                rbox([12, 16, plat_thick + 5], r=2);
        }

        // Pivot bolt hole
        translate([0, 0, -1])
            cylinder(d=m8_clear, h=plat_thick + 10);

        // Washer recess on top
        translate([0, 0, plat_thick - m8_washer_t])
            cylinder(d=m8_washer_od + 1, h=m8_washer_t + 1);

        // M4 mounting holes for table top (6×)
        for (pos = _mount_holes())
            translate([pos[0], pos[1], -1])
                cylinder(d=m4_clear, h=plat_thick + 2);
    }
}

// Mounting hole positions (shared by platform and table top)
function _mount_holes() =
    let(
        _arm_start = hub_dia/2 - 8,
        _mid = (_arm_start + arm_length) / 2
    )
    [
        [-arm_gap/2, arm_length - 18],
        [ arm_gap/2, arm_length - 18],
        [-arm_gap/2, _mid],
        [ arm_gap/2, _mid],
        [-arm_gap/2, _arm_start + 18],
        [ arm_gap/2, _arm_start + 18],
    ];


// ============== PART 3: TABLE TOP ==============
module table_top() {
    // Origin at center of back edge (wall side).
    // Table extends in +Y (away from wall).

    difference() {
        union() {
            // Main flat surface
            translate([-table_width/2, 0, 0])
                rbox([table_width, table_depth, table_thick], r=8);

            // Edge lips (3 sides — not the wall side)
            // Front
            translate([-table_width/2, table_depth - edge_lip_w, table_thick])
                rbox([table_width, edge_lip_w, edge_lip_h], r=1.5);
            // Left
            translate([-table_width/2, 8, table_thick])
                cube([edge_lip_w, table_depth - 8, edge_lip_h]);
            // Right
            translate([table_width/2 - edge_lip_w, 8, table_thick])
                cube([edge_lip_w, table_depth - 8, edge_lip_h]);

            // Structural ribs (underside)
            // 5 cross ribs
            for (i = [1:5]) {
                ry = table_depth / 6 * i;
                translate([-table_width/2 + 12, ry - _rib_w/2, -_rib_h])
                    cube([table_width - 24, _rib_w, _rib_h + _eps]);
            }
            // 2 longitudinal ribs (aligned with platform arms)
            for (lx = [-arm_gap/2, arm_gap/2])
                translate([lx - _rib_w/2, 12, -_rib_h])
                    cube([_rib_w, table_depth - 24, _rib_h + _eps]);
            // Center spine
            translate([-_rib_w/2, 10, -_rib_h])
                cube([_rib_w, table_depth - 20, _rib_h + _eps]);
        }

        // M4 bolt holes with counterbores (6×)
        for (pos = _mount_holes()) {
            // Through hole (all the way through ribs + surface + lip)
            translate([pos[0], pos[1], -_rib_h - 1])
                cylinder(d=m4_clear, h=table_thick + _rib_h + edge_lip_h + 5);
            // Counterbore on top for bolt head (flush mount)
            translate([pos[0], pos[1], table_thick - m4_head_dep])
                cylinder(d=m4_head_dia, h=m4_head_dep + edge_lip_h + 1);
        }
    }
}

// ============== ASSEMBLY ==============
module assembly() {
    // 1. Wall bracket (fixed)
    color([0.25, 0.50, 0.80])
        wall_bracket();

    // 2. Swivel platform + 3. Table top (rotating)
    _pz = wb_shelf_t + _boss_h + 1;  // Z of platform bottom
    translate([_pvt_x, _pvt_y, _pz])
        rotate([0, 0, swing_angle]) {
            // Platform
            color([0.90, 0.50, 0.15])
                swivel_platform();
            // Table on top of platform
            color([0.25, 0.70, 0.35])
                translate([0, 0, plat_thick + _rib_h + 0.5])
                table_top();
        }
}

// ============== EXPLODED VIEW ==============
module exploded() {
    // Wall bracket
    color([0.25, 0.50, 0.80])
        wall_bracket();

    // Platform lifted up
    translate([_pvt_x, _pvt_y, wb_shelf_t + _boss_h + 40])
        color([0.90, 0.50, 0.15])
        swivel_platform();

    // Table top lifted further
    translate([_pvt_x, _pvt_y, wb_shelf_t + _boss_h + 40 + plat_thick + _rib_h + 40])
        color([0.25, 0.70, 0.35])
        table_top();
}

// ============== MAIN ==============
if (render_mode == "assembly")        assembly();
else if (render_mode == "wall_bracket")    wall_bracket();
else if (render_mode == "swivel_platform") swivel_platform();
else if (render_mode == "table_top")       table_top();
else if (render_mode == "exploded")        exploded();
