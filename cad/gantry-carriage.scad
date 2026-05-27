// =============================================================================
// GANTRY CARRIAGE BLOCK
// Auto-Emptying Cat Litter Box Project
// =============================================================================
//
// This OpenSCAD file generates the main carriage block that rides along
// the 2020 V-slot aluminum extrusion. It houses the rake cartridge mount,
// V-wheel assemblies, and belt attachment point.
//
// Design Features:
// - Four V-wheel mounting points (2 fixed, 2 with eccentric spacer clearance)
// - Female dovetail slot for rake cartridge quick-change system
// - GT2 belt clamping mechanism
// - Integrated cable management clips
// - Optimized for 256×256×256mm build volume
//
// Print Settings:
// - Layer Height: 0.2mm
// - Infill: 40% (gyroid for strength + weight balance)
// - Perimeters: 4 walls
// - Support: Minimal (only under belt clamp overhang)
//
// =============================================================================

// -----------------------
// USER-ADJUSTABLE PARAMETERS
// -----------------------

// Extrusion Interface
extrusion_width = 20;          // [mm] 2020 extrusion width
v_slot_width = 11;             // [mm] Width of V-slot groove
v_slot_tolerance = 0.3;        // [mm] Clearance for V-wheels in slot

// V-Wheel Parameters
wheel_diameter = 24;           // [mm] Delrin V-wheel outer diameter
wheel_thickness = 11;          // [mm] V-wheel thickness
wheel_axle_diameter = 5;       // [mm] M5 shaft diameter
wheel_mounting_hole = 5.2;     // [mm] M5 clearance hole
eccentric_spacer_clearance = 8; // [mm] Clearance for eccentric spacer adjustment

// Carriage Body Dimensions
carriage_length = 80;          // [mm] Length along extrusion travel direction
carriage_width = 60;           // [mm] Width perpendicular to travel
carriage_height = 40;          // [mm] Height above extrusion top surface
body_wall_thickness = 4;       // [mm] Wall thickness for structural parts

// Dovetail Slot (Female) - MUST MATCH rake cartridge dovetail key
dovetail_slot_length = 42;     // [mm] Slightly longer than key for easy insertion
dovetail_base_width = 20.5;    // [mm] Clearance for 20mm dovetail base
dovetail_top_width = 10.5;     // [mm] Clearance for 10mm dovetail top
dovetail_depth = 12;           // [mm] Depth of slot (slightly deeper than key height)

// Pin Lock Mechanism
pin_diameter = 6;              // [mm] Spring-loaded pin diameter
pin_hole_offset = 10;          // [mm] Distance from dovetail edge (must match cartridge)
pin_spring_recess = 15;        // [mm] Recess depth for spring housing

// Belt Attachment
belt_width = 6;                // [mm] GT2 belt width
belt_thickness = 1.5;          // [mm] GT2 belt thickness
belt_clamp_screw_diameter = 3.2; // [mm] M3 clearance hole
belt_clamp_length = 30;        // [mm] Length of belt clamping surface

// Cable Management
cable_clip_width = 8;          // [mm] Width of cable routing clips
cable_clip_height = 6;         // [mm] Height of clips

// Mounting Holes
mounting_hole_diameter = 3.2;  // [mm] M3 clearance holes
threaded_insert_diameter = 4.2; // [mm] M3 threaded insert outer diameter

// -----------------------
// MAIN ASSEMBLY
// -----------------------

module gantry_carriage() {
    difference() {
        union() {
            // Main carriage body
            carriage_body();

            // V-wheel mounting blocks
            v_wheel_mounts();

            // Cable management clips
            cable_clips();
        }

        // Remove material for dovetail slot
        dovetail_slot();

        // V-wheel axle holes
        v_wheel_holes();

        // Belt attachment holes
        belt_clamp_holes();

        // Pin lock hole with spring recess
        pin_lock_mechanism();
    }

    // Separate belt clamp piece (prints separately, attached with M3 screws)
    translate([0, carriage_width + 10, 0])
        belt_clamp();
}

// -----------------------
// COMPONENT MODULES
// -----------------------

// Main carriage body structure
module carriage_body() {
    // Central body block
    translate([0, 0, 0])
        cube([carriage_length, carriage_width, carriage_height]);

    // Reinforcement ribs (reduce weight while maintaining strength)
    rib_positions = [carriage_length/4, carriage_length/2, 3*carriage_length/4];
    for (x = rib_positions) {
        translate([x - 1.5, 0, 0])
            cube([3, carriage_width, carriage_height - 5]);
    }
}

// V-wheel mounting blocks (4× wheels: 2 fixed, 2 with eccentric spacer clearance)
module v_wheel_mounts() {
    wheel_positions = [
        // Front wheels (fixed)
        [15, 10, 0],
        [15, 50, 0],
        // Rear wheels (eccentric spacer clearance)
        [65, 10, 0],
        [65, 50, 0]
    ];

    for (i = [0:len(wheel_positions)-1]) {
        pos = wheel_positions[i];
        is_eccentric = (i >= 2); // Rear wheels have eccentric spacers

        translate(pos) {
            difference() {
                // Mounting boss
                cylinder(h=wheel_thickness + 6, d=wheel_diameter + 8, $fn=64);

                // Axle hole (larger for eccentric spacers on rear wheels)
                if (is_eccentric) {
                    cylinder(h=wheel_thickness + 7, d=wheel_mounting_hole + eccentric_spacer_clearance, $fn=64, center=true);
                } else {
                    cylinder(h=wheel_thickness + 7, d=wheel_mounting_hole, $fn=64, center=true);
                }
            }
        }
    }
}

// V-wheel axle holes (through-holes for M5 shafts)
module v_wheel_holes() {
    wheel_positions = [
        [15, 10, -1],
        [15, 50, -1],
        [65, 10, -1],
        [65, 50, -1]
    ];

    for (i = [0:len(wheel_positions)-1]) {
        pos = wheel_positions[i];
        is_eccentric = (i >= 2);

        translate(pos) {
            if (is_eccentric) {
                // Larger hole for eccentric spacer adjustment
                cylinder(h=carriage_height + 2, d=wheel_mounting_hole + eccentric_spacer_clearance, $fn=64);
            } else {
                // Standard M5 clearance hole
                cylinder(h=carriage_height + 2, d=wheel_mounting_hole, $fn=64);
            }
        }
    }
}

// Dovetail slot (female) for rake cartridge mounting
module dovetail_slot() {
    translate([carriage_length/2, carriage_width/2, carriage_height - dovetail_depth]) {
        // Tapered dovetail cavity
        hull() {
            translate([0, 0, 0])
                cube([dovetail_base_width, dovetail_slot_length, 0.1], center=true);
            translate([0, 0, dovetail_depth])
                cube([dovetail_top_width, dovetail_slot_length, 0.1], center=true);
        }
    }
}

// Pin lock mechanism (spring-loaded pin housing)
module pin_lock_mechanism() {
    translate([carriage_length/2, carriage_width/2 + dovetail_slot_length/2 - pin_hole_offset, carriage_height - dovetail_depth/2]) {
        // Pin through-hole
        rotate([90, 0, 0])
            cylinder(h=carriage_width, d=pin_diameter, center=true, $fn=32);

        // Spring recess (on one side)
        translate([0, carriage_width/2 - 5, 0])
            rotate([90, 0, 0])
                cylinder(h=pin_spring_recess, d=pin_diameter + 4, $fn=32);
    }
}

// Belt attachment clamp holes
module belt_clamp_holes() {
    // Belt passage slot
    translate([carriage_length/2 - belt_clamp_length/2, -1, carriage_height/2 - belt_width/2])
        cube([belt_clamp_length, body_wall_thickness + 2, belt_width + 2]);

    // M3 screw holes for belt clamp (4× holes)
    clamp_screw_positions = [
        [carriage_length/2 - 10, body_wall_thickness/2, carriage_height/2 - 8],
        [carriage_length/2 + 10, body_wall_thickness/2, carriage_height/2 - 8],
        [carriage_length/2 - 10, body_wall_thickness/2, carriage_height/2 + 8],
        [carriage_length/2 + 10, body_wall_thickness/2, carriage_height/2 + 8]
    ];

    for (pos = clamp_screw_positions) {
        translate(pos)
            rotate([90, 0, 0])
                cylinder(h=body_wall_thickness + 2, d=belt_clamp_screw_diameter, center=true, $fn=32);
    }
}

// Belt clamp piece (separate component)
module belt_clamp() {
    difference() {
        // Clamp body
        translate([carriage_length/2 - belt_clamp_length/2, 0, carriage_height/2 - 10])
            cube([belt_clamp_length, body_wall_thickness + 2, 20]);

        // Belt passage slot
        translate([carriage_length/2 - belt_clamp_length/2 - 1, body_wall_thickness/2, carriage_height/2 - belt_width/2])
            cube([belt_clamp_length + 2, belt_thickness + 1, belt_width + 2]);

        // Screw holes with countersink
        clamp_screw_positions = [
            [carriage_length/2 - 10, body_wall_thickness/2, carriage_height/2 - 8],
            [carriage_length/2 + 10, body_wall_thickness/2, carriage_height/2 - 8],
            [carriage_length/2 - 10, body_wall_thickness/2, carriage_height/2 + 8],
            [carriage_length/2 + 10, body_wall_thickness/2, carriage_height/2 + 8]
        ];

        for (pos = clamp_screw_positions) {
            translate(pos) {
                rotate([90, 0, 0])
                    cylinder(h=body_wall_thickness + 4, d=belt_clamp_screw_diameter, center=true, $fn=32);
                // Countersink for M3 socket head
                translate([0, -(body_wall_thickness/2 + 1), 0])
                    rotate([90, 0, 0])
                        cylinder(h=3, d=6, $fn=32);
            }
        }
    }
}

// Cable management clips
module cable_clips() {
    clip_positions = [
        [10, carriage_width - 5, carriage_height],
        [carriage_length - 10, carriage_width - 5, carriage_height]
    ];

    for (pos = clip_positions) {
        translate(pos) {
            difference() {
                // Clip body
                cube([cable_clip_width, 5, cable_clip_height]);

                // Cable channel
                translate([cable_clip_width/2, 0, cable_clip_height - 3])
                    rotate([-90, 0, 0])
                        cylinder(h=6, d=4, $fn=32);
            }
        }
    }
}

// -----------------------
// RENDER MAIN ASSEMBLY
// -----------------------

gantry_carriage();

// =============================================================================
// ASSEMBLY INSTRUCTIONS
// =============================================================================
//
// 1. PRINTING:
//    - Export STL: F6 (Render) → File → Export → STL
//    - Print main carriage with 0.2mm layers, 40% gyroid infill, 4 perimeters
//    - Print belt clamp separately (same settings)
//    - Support required only under belt clamp overhang (minimal)
//    - Estimated print time: 8 hours (carriage) + 1 hour (clamp)
//    - Estimated filament: 120g (carriage) + 20g (clamp)
//
// 2. POST-PROCESSING:
//    - Remove support material from belt clamp
//    - Test-fit V-wheels (should rotate freely without binding)
//    - Test-fit eccentric spacers in rear wheel holes (should adjust tension)
//    - Verify dovetail slot depth and taper with calipers
//
// 3. HARDWARE INSTALLATION:
//    - Install 4× V-wheels with M5 shafts (40mm length)
//    - On rear wheels: use eccentric spacers between wheel and carriage
//    - Adjust eccentric spacers until wheels contact V-slot with slight resistance
//    - Install spring-loaded pin in pin lock mechanism
//      - Pin: 6mm diameter, 30mm length
//      - Spring: compression spring, 6mm ID, 10mm OD, 20mm free length
//
// 4. BELT ATTACHMENT:
//    - Thread GT2 belt through belt passage slot
//    - Fold belt back on itself (~20mm overlap)
//    - Position belt clamp over folded section
//    - Secure with 4× M3×12mm screws
//    - Verify belt is centered and not twisted
//
// 5. TESTING:
//    - Mount carriage on 2020 V-slot extrusion
//    - Verify smooth rolling motion (no binding or excessive play)
//    - Adjust eccentric spacers if needed:
//      - Too loose: rotate spacer to increase tension
//      - Too tight: rotate spacer to decrease tension
//    - Test rake cartridge insertion and locking (should click firmly)
//
// =============================================================================

// =============================================================================
// DESIGN NOTES
// =============================================================================
//
// V-WHEEL CONFIGURATION:
// The carriage uses a 4-wheel design with 2 fixed and 2 eccentric spacer wheels.
// This configuration provides:
// - Stable 3-point contact with V-slot (prevents rocking)
// - Adjustable preload for wear compensation
// - High load capacity (each wheel rated ~10kg)
//
// ECCENTRIC SPACER ADJUSTMENT:
// Rear wheels use eccentric spacers (0.5mm offset) to adjust tension:
// - Rotate spacer 180° = 1mm adjustment in V-slot contact
// - Proper tension: carriage should roll smoothly with slight resistance
// - Over-tightening causes premature wheel wear and motor strain
//
// BELT ATTACHMENT METHOD:
// The belt clamp design provides:
// - Positive mechanical lock (no glue required)
// - Easy belt replacement (4 screws)
// - Even clamping force across belt width (prevents skewing)
//
// DOVETAIL SLOT TOLERANCES:
// - Base width: 0.5mm clearance for easy insertion
// - Top width: 0.5mm clearance
// - Depth: 2mm extra clearance for full seating
// - If cartridge is too loose: add thin tape to dovetail key
// - If cartridge is too tight: light sanding on dovetail surfaces
//
// CABLE MANAGEMENT:
// Integrated cable clips route motor wires along carriage:
// - Prevents snagging on frame during travel
// - Maintains consistent wire bend radius
// - Recommended: use cable carrier chain for best reliability
//
// =============================================================================

// =============================================================================
// TROUBLESHOOTING
// =============================================================================
//
// ISSUE: Carriage binds or skips during travel
// SOLUTION:
// - Check eccentric spacer tension (should not be over-tight)
// - Verify V-slot extrusion is straight (use straightedge)
// - Clean debris from V-wheels and V-slot grooves
// - Lubricate V-wheels with dry PTFE spray (not oil-based)
//
// ISSUE: Rake cartridge doesn't lock or falls out
// SOLUTION:
// - Verify pin hole alignment (should be at pin_hole_offset = 10mm)
// - Check spring tension (should hold pin with ~2-3kg force)
// - Ensure dovetail slot depth is sufficient (12mm minimum)
// - Inspect dovetail key for layer separation (reprint if needed)
//
// ISSUE: Belt slips during cleaning cycles
// SOLUTION:
// - Increase belt clamp screw torque (max 0.5 Nm for M3 in PETG)
// - Verify belt overlap is at least 20mm
// - Check belt tension at motor mount (should be 3-4kg force)
// - Replace belt if teeth are worn or damaged
//
// ISSUE: Excessive noise during travel
// SOLUTION:
// - Reduce motor speed in firmware (default: 50mm/s)
// - Check for V-wheel flat spots (replace if found)
// - Verify TMC2209 driver is in silent mode (UART configured)
// - Add PTFE dry lubricant to V-wheels
//
// =============================================================================
