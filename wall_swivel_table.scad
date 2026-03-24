// Wall-Mounted Swivel Table Design
// Designed for 3D printing on Bambu P2S (240x256mm build plate)
// Material: PETG or PLA

// Global parameters
$fn = 100; // Smooth circles

// Dimensions (in mm)
table_width = 200;
table_depth = 150;
table_thickness = 8;

wall_bracket_height = 150;
wall_bracket_width = 80;
wall_bracket_thickness = 10;

swivel_post_diameter = 25;
swivel_post_height = 120;
swivel_bearing_diameter = 28; // Slightly larger for bearing sleeve
swivel_bearing_clearance = 0.3; // Print tolerance

// Mounting hardware holes
screw_hole_diameter = 5; // For M4 screws
wall_screw_positions = [
    [0, 30],
    [0, 90],
    [0, 130]
];

// Toggle for which part to print
print_part = "all"; // Options: "all", "wall_bracket", "swivel_post", "table_top", "bearing_sleeve"

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

// Wall mounting bracket
module wall_bracket() {
    difference() {
        union() {
            // Main bracket plate
            cube([wall_bracket_width, wall_bracket_thickness, wall_bracket_height]);

            // Reinforcement ribs
            for (y = [20:40:wall_bracket_height-20]) {
                translate([0, 0, y])
                    rotate([0, 45, 0])
                        cube([15, wall_bracket_thickness, 3]);
            }

            // Bottom support base
            translate([0, 0, 0])
                cube([wall_bracket_width, 40, 15]);

            // Swivel post socket
            translate([wall_bracket_width/2, wall_bracket_thickness + 20, wall_bracket_height - 40])
                rotate([90, 0, 0]) {
                    difference() {
                        cylinder(h=30, d=swivel_bearing_diameter + 8);
                        // Bearing socket hole
                        translate([0, 0, -1])
                            cylinder(h=32, d=swivel_bearing_diameter + swivel_bearing_clearance);
                    }
                }
        }

        // Wall mounting screw holes
        for (pos = wall_screw_positions) {
            translate([wall_bracket_width/2 + pos[0], -1, pos[1]])
                rotate([-90, 0, 0])
                    cylinder(h=wall_bracket_thickness + 2, d=screw_hole_diameter);
            // Countersink
            translate([wall_bracket_width/2 + pos[0], -1, pos[1]])
                rotate([-90, 0, 0])
                    cylinder(h=6, d1=screw_hole_diameter*2.5, d2=screw_hole_diameter);
        }
    }
}

// Swivel post (attaches to table, rotates in bearing)
module swivel_post() {
    union() {
        // Main rotating post
        cylinder(h=swivel_post_height, d=swivel_bearing_diameter - swivel_bearing_clearance*2);

        // Friction rings for smooth but controlled rotation
        for (z = [10, 30, 50, 70, 90, 110]) {
            translate([0, 0, z])
                cylinder(h=2, d=swivel_bearing_diameter - swivel_bearing_clearance);
        }

        // Table mounting plate at top
        translate([0, 0, swivel_post_height])
            cylinder(h=8, d=swivel_post_diameter + 20);

        // Reinforcement ribs on mounting plate
        for (angle = [0:60:300]) {
            rotate([0, 0, angle])
                translate([-3, 0, swivel_post_height])
                    cube([6, swivel_post_diameter/2 + 10, 8]);
        }
    }
}

// Bearing sleeve (provides smooth rotation)
module bearing_sleeve() {
    difference() {
        cylinder(h=28, d=swivel_bearing_diameter + swivel_bearing_clearance*2);
        translate([0, 0, -1])
            cylinder(h=30, d=swivel_bearing_diameter - swivel_bearing_clearance);
    }
}

// Table top
module table_top() {
    difference() {
        union() {
            // Main table surface with rounded corners
            rounded_rect(table_width, table_depth, table_thickness, 10);

            // Raised edge for items not to fall off
            difference() {
                rounded_rect(table_width, table_depth, table_thickness + 5, 10);
                translate([5, 5, -1])
                    rounded_rect(table_width - 10, table_depth - 10, table_thickness + 7, 8);
            }

            // Reinforcement structure underneath
            translate([0, 0, -10]) {
                // Cross bracing
                translate([table_width/2 - 3, 5, 0])
                    cube([6, table_depth - 10, 10]);
                translate([5, table_depth/2 - 3, 0])
                    cube([table_width - 10, 6, 10]);
            }
        }

        // Mounting hole for swivel post
        translate([table_width/2, table_depth/2, -11])
            cylinder(h=table_thickness + 12, d=swivel_post_diameter + 22);

        // Screw holes to attach to swivel post mounting plate
        for (angle = [0:120:240]) {
            rotate([0, 0, angle])
                translate([0, 25, -11])
                    cylinder(h=table_thickness + 12, d=screw_hole_diameter);
        }
    }
}

// Assembly view
module assembly() {
    // Wall bracket
    color("lightblue")
        wall_bracket();

    // Bearing sleeve in bracket
    color("gray")
        translate([wall_bracket_width/2, wall_bracket_thickness - 10, wall_bracket_height - 40])
            rotate([90, 0, 0])
                bearing_sleeve();

    // Swivel post
    color("green")
        translate([wall_bracket_width/2, wall_bracket_thickness - 10, wall_bracket_height - 40])
            rotate([90, 0, 0])
                swivel_post();

    // Table top
    color("orange")
        translate([wall_bracket_width/2 - table_width/2,
                   wall_bracket_thickness - 10 - swivel_post_height - 8,
                   wall_bracket_height - 40])
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
