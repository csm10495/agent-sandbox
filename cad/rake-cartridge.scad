// =============================================================================
// PARAMETRIC SIFTING RAKE CARTRIDGE - TYPE A (CLUMPING CLAY)
// Auto-Emptying Cat Litter Box Project
// =============================================================================
//
// This OpenSCAD file generates a modular rake cartridge designed for
// clumping clay litter. The fine-toothed design allows clean litter to
// pass through while retaining clumps ≥10mm.
//
// Design Features:
// - Standardized dovetail mounting key for quick-change system
// - Parametric tooth spacing and thickness
// - Optimized for 256×256×256mm build volume
// - PETG recommended (impact resistance, odor resistance)
//
// Print Settings:
// - Layer Height: 0.2mm
// - Infill: 30% (grid pattern for directional rigidity)
// - Perimeters: 4 walls
// - Support: None required (optimized geometry)
//
// =============================================================================

// -----------------------
// USER-ADJUSTABLE PARAMETERS
// -----------------------

// Rake Dimensions
rake_width = 250;              // [mm] Total rake width (fits within 256mm build volume)
rake_height = 80;              // [mm] Vertical height of rake assembly
rake_depth = 15;               // [mm] Depth/thickness of rake body

// Tooth Parameters
tooth_thickness = 2;           // [mm] Thickness of each tooth
tooth_spacing = 4;             // [mm] Gap between teeth (determines sifting fineness)
tooth_height = 60;             // [mm] Length of teeth extending downward
tooth_angle = 15;              // [degrees] Forward angle of teeth (reduces resistance)

// Dovetail Mounting Key (STANDARDIZED - DO NOT MODIFY unless changing entire system)
dovetail_length = 40;          // [mm] Length of dovetail key
dovetail_base_width = 20;      // [mm] Base width of dovetail
dovetail_top_width = 10;       // [mm] Top width of dovetail (creates taper)
dovetail_height = 10;          // [mm] Height of dovetail key

// Pin Lock Hole (for spring-loaded locking pin)
pin_hole_diameter = 6;         // [mm] Diameter of pin lock hole
pin_hole_offset = 10;          // [mm] Distance from edge of dovetail

// Mounting Holes for Rake Body (M3 threaded inserts)
mounting_hole_diameter = 4.2;  // [mm] M3 threaded insert outer diameter
mounting_hole_depth = 5;       // [mm] Depth for threaded insert

// Structural Parameters
body_wall_thickness = 3;       // [mm] Wall thickness for main body
fillet_radius = 2;             // [mm] Fillets for stress distribution

// -----------------------
// CALCULATED VALUES
// -----------------------

// Calculate number of teeth that fit within rake width
tooth_pitch = tooth_thickness + tooth_spacing;
num_teeth = floor((rake_width - tooth_thickness) / tooth_pitch);
actual_rake_width = num_teeth * tooth_pitch + tooth_thickness;

echo(str("Rake Configuration:"));
echo(str("  Total Width: ", actual_rake_width, " mm"));
echo(str("  Number of Teeth: ", num_teeth));
echo(str("  Tooth Pitch: ", tooth_pitch, " mm"));

// -----------------------
// MAIN ASSEMBLY
// -----------------------

module rake_cartridge_type_a() {
    union() {
        // Main rake body
        rake_body();

        // Dovetail mounting key
        translate([actual_rake_width/2, rake_depth/2, rake_height])
            dovetail_key();

        // Teeth array
        translate([tooth_thickness/2, 0, rake_height - tooth_height - 5])
            teeth_array();
    }
}

// -----------------------
// COMPONENT MODULES
// -----------------------

// Main body structure
module rake_body() {
    difference() {
        // Solid body
        translate([0, 0, rake_height - 10])
            cube([actual_rake_width, rake_depth, 10]);

        // Mounting holes for M3 threaded inserts (4× holes on standardized grid)
        mounting_holes();
    }
}

// Dovetail key for carriage mounting
module dovetail_key() {
    difference() {
        // Tapered dovetail shape
        hull() {
            translate([0, 0, 0])
                cube([dovetail_top_width, dovetail_length, 0.1], center=true);
            translate([0, 0, dovetail_height])
                cube([dovetail_base_width, dovetail_length, 0.1], center=true);
        }

        // Pin lock hole (through-hole for spring-loaded pin)
        translate([0, dovetail_length/2 - pin_hole_offset, dovetail_height/2])
            rotate([90, 0, 0])
                cylinder(h=20, d=pin_hole_diameter, center=true, $fn=32);
    }
}

// Array of rake teeth
module teeth_array() {
    for (i = [0 : num_teeth - 1]) {
        translate([i * tooth_pitch, 0, 0])
            rake_tooth();
    }
}

// Individual rake tooth
module rake_tooth() {
    // Angled tooth for forward raking motion
    hull() {
        // Top of tooth (attached to body)
        translate([0, rake_depth/2, tooth_height])
            cube([tooth_thickness, rake_depth, 0.1], center=true);

        // Bottom of tooth (angled forward)
        translate([tooth_height * sin(tooth_angle), rake_depth/2, 0])
            cube([tooth_thickness, rake_depth, 0.1], center=true);
    }

    // Reinforcement fillet at base of tooth
    translate([0, rake_depth/2, tooth_height])
        rotate([0, 90, 0])
            cylinder(h=tooth_thickness, r=fillet_radius, center=true, $fn=16);
}

// Mounting holes for threaded inserts (standardized 30×30mm grid)
module mounting_holes() {
    hole_positions = [
        [actual_rake_width/2 - 15, rake_depth/2],
        [actual_rake_width/2 + 15, rake_depth/2]
    ];

    for (pos = hole_positions) {
        translate([pos[0], pos[1], rake_height - mounting_hole_depth])
            cylinder(h=mounting_hole_depth + 1, d=mounting_hole_diameter, $fn=32);
    }
}

// -----------------------
// RENDER MAIN ASSEMBLY
// -----------------------

rake_cartridge_type_a();

// =============================================================================
// ALTERNATIVE CONFIGURATIONS
// =============================================================================
//
// TYPE B - WIDE-SCOOP PELLET RAKE:
// Modify these parameters:
//   tooth_thickness = 4;        // Wider bars for structural rigidity
//   tooth_spacing = 8;          // Larger gaps to pass 6-7mm pellets
//   tooth_height = 50;          // Shorter teeth (less depth needed)
//
// TYPE C - HYBRID SILICA/TOFU RAKE:
// Replace teeth_array() with mesh pattern:
//   - Use rectangular slots: 3mm × 8mm
//   - Cross-bracing every 50mm
//   - See separate type_c variant file
//
// =============================================================================

// =============================================================================
// ASSEMBLY INSTRUCTIONS
// =============================================================================
//
// 1. PRINTING:
//    - Export STL: F6 (Render) → File → Export → STL
//    - Slice with 0.2mm layer height, 30% infill
//    - Print in PETG (recommended) or PLA
//    - No support required
//    - Estimated print time: 6 hours
//    - Estimated filament: 90g
//
// 2. POST-PROCESSING:
//    - Insert M3 threaded inserts into mounting holes (use soldering iron at 200°C)
//    - Verify dovetail key fits smoothly (light sanding if needed)
//    - Clean any stringing between teeth with wire cutters
//
// 3. INSTALLATION:
//    - Align dovetail key with carriage block dovetail slot
//    - Slide cartridge into carriage until pin hole aligns
//    - Release spring-loaded pin (should click into place)
//    - Verify cartridge is locked (pull test with ~5kg force)
//
// 4. TESTING:
//    - Manually test rake teeth in litter sample
//    - Verify teeth angle allows smooth sifting
//    - Check for any binding or flex under load
//
// =============================================================================

// =============================================================================
// MAINTENANCE & TROUBLESHOOTING
// =============================================================================
//
// COMMON ISSUES:
//
// 1. Teeth breaking during cleaning:
//    - Increase tooth_thickness to 2.5mm or 3mm
//    - Reduce tooth_height to 50mm (less leverage)
//    - Print with 4 perimeters for maximum strength
//
// 2. Litter passing through too easily:
//    - Reduce tooth_spacing to 3mm or 3.5mm
//    - Verify clumps are properly formed (check litter moisture)
//
// 3. Clumps getting stuck between teeth:
//    - Increase tooth_angle to 20° (more aggressive)
//    - Reduce tooth_spacing slightly
//    - Consider switching to Type B rake for pellet litter
//
// 4. Dovetail key not locking:
//    - Verify pin_hole_offset matches carriage block
//    - Check for layer adhesion issues (reprint in PETG)
//    - Lightly sand dovetail surfaces for smoother fit
//
// =============================================================================
