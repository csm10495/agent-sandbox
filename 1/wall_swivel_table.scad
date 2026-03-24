// Design 1: COMPACT Wall-Mounted Swivel Table
// Smaller dimensions for tight spaces (bedside, small workspace)
// Designed for 3D printing on Bambu P2S (240x256mm build plate)
// Material: PETG or PLA (PETG recommended for strength)

// Global parameters
$fn = 100; // Smooth circles

// Dimensions (in mm) - COMPACT VERSION
table_width = 150;  // Reduced from 200
table_depth = 100;  // Reduced from 150
table_thickness = 8;

wall_bracket_height = 120;  // Reduced from 150
wall_bracket_width = 60;    // Reduced from 80
wall_bracket_thickness = 10;

swivel_post_diameter = 20;  // Reduced from 25
swivel_post_height = 90;    // Reduced from 120
swivel_bearing_diameter = 23;
swivel_bearing_clearance = 0.25;

// Mounting hardware holes
screw_hole_diameter = 4; // M3 screws for lighter weight
wall_screw_positions = [
    [0, 25],
    [0, 70]
];

// Toggle for which part to print
print_part = "all";

// Helper module for rounded rectangle
module rounded_rect(width, depth, height, radius) {
    hull() {
        translate([radius, radius, 0])
            cylinder(h=height, r=radius);
        translate([width-radius, radius, 0])
            cylinder(h=height, r=radius);
        translate([radius, depth-radius, 0])
            cylinder(h=height, r=radius);
        translate([width-radius, depth-radius, 0])
            cylinder(h=height, r=radius);
    }
}

// Compact wall mounting bracket
module wall_bracket() {
    difference() {
        union() {
            // Main bracket plate
            cube([wall_bracket_width, wall_bracket_thickness, wall_bracket_height]);

            // Bottom support base
            translate([0, 0, 0])
                cube([wall_bracket_width, 35, 15]);

            // Swivel post socket
            translate([wall_bracket_width/2, wall_bracket_thickness + 20, wall_bracket_height - 30])
                rotate([90, 0, 0]) {
                    difference() {
                        cylinder(h=28, d=swivel_bearing_diameter + 8);
                        translate([0, 0, -1])
                            cylinder(h=30, d=swivel_bearing_diameter + swivel_bearing_clearance);
                    }
                }
        }

        // Wall mounting screw holes
        for (pos = wall_screw_positions) {
            translate([wall_bracket_width/2, -1, pos[1]])
                rotate([-90, 0, 0])
                    cylinder(h=wall_bracket_thickness + 2, d=screw_hole_diameter);
        }
    }
}

// Compact swivel post
module swivel_post() {
    union() {
        // Main rotating post
        cylinder(h=swivel_post_height, d=swivel_bearing_diameter - swivel_bearing_clearance*2);

        // Friction rings
        for (z = [10, 35, 60, 85]) {
            translate([0, 0, z])
                cylinder(h=1.5, d=swivel_bearing_diameter - swivel_bearing_clearance*0.5);
        }

        // Table mounting plate at top
        translate([0, 0, swivel_post_height])
            cylinder(h=8, d=swivel_post_diameter + 18);

        // Reinforcement ribs
        for (angle = [0:60:300]) {
            rotate([0, 0, angle])
                translate([-2, 0, swivel_post_height])
                    cube([4, swivel_post_diameter/2 + 9, 8]);
        }
    }
}

// Bearing sleeve
module bearing_sleeve() {
    difference() {
        cylinder(h=26, d=swivel_bearing_diameter + swivel_bearing_clearance*2);
        translate([0, 0, -1])
            cylinder(h=28, d=swivel_bearing_diameter - swivel_bearing_clearance);
    }
}

// Compact table top
module table_top() {
    difference() {
        union() {
            // Main table surface
            rounded_rect(table_width, table_depth, table_thickness, 8);

            // Raised edge
            difference() {
                rounded_rect(table_width, table_depth, table_thickness + 4, 8);
                translate([4, 4, -1])
                    rounded_rect(table_width - 8, table_depth - 8, table_thickness + 6, 6);
            }

            // Reinforcement underneath
            translate([0, 0, -10]) {
                translate([table_width/2 - 3, 5, 0])
                    cube([6, table_depth - 10, 10]);
                translate([5, table_depth/2 - 3, 0])
                    cube([table_width - 10, 6, 10]);
            }
        }

        // Mounting hole for swivel post
        translate([table_width/2, table_depth/2, -11])
            cylinder(h=table_thickness + 12, d=swivel_post_diameter + 20);

        // Screw holes
        for (angle = [0:120:240]) {
            rotate([0, 0, angle])
                translate([table_width/2, table_depth/2 + 20, -11])
                    cylinder(h=table_thickness + 12, d=screw_hole_diameter);
        }
    }
}

// Assembly view
module assembly() {
    color("lightblue")
        wall_bracket();
    color("gray")
        translate([wall_bracket_width/2, wall_bracket_thickness - 8, wall_bracket_height - 30])
            rotate([90, 0, 0])
                bearing_sleeve();
    color("green")
        translate([wall_bracket_width/2, wall_bracket_thickness - 8, wall_bracket_height - 30])
            rotate([90, 0, 0])
                swivel_post();
    color("orange")
        translate([wall_bracket_width/2 - table_width/2,
                   wall_bracket_thickness - 8 - swivel_post_height - 8,
                   wall_bracket_height - 30])
            rotate([90, 0, 0])
                table_top();
}

// Render the selected part
if (print_part == "all") {
    assembly();
} else if (print_part == "wall_bracket") {
    wall_bracket();
} else if (print_part == "swivel_post") {
    swivel_post();
} else if (print_part == "table_top") {
    table_top();
} else if (print_part == "bearing_sleeve") {
    bearing_sleeve();
}
