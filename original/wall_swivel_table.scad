// Wall-Mounted Swivel Table Design - IMPROVED VERSION
// Designed for 3D printing on Bambu P2S (240x256mm build plate)
// Material: PETG or PLA (PETG recommended for strength)

// Global parameters
$fn = 100; // Smooth circles

// Dimensions (in mm)
table_width = 200;
table_depth = 150;
table_thickness = 10; // Increased from 8mm for more strength

wall_bracket_height = 150;
wall_bracket_width = 80;
wall_bracket_thickness = 12; // Increased from 10mm for more strength

swivel_post_diameter = 25;
swivel_post_height = 120;
swivel_bearing_diameter = 28; // Slightly larger for bearing sleeve
swivel_bearing_clearance = 0.25; // Tighter tolerance for better friction

// Mounting hardware holes
screw_hole_diameter = 5; // For M4 screws
wall_screw_positions = [
    [0, 30],
    [0, 90],
    [0, 130]
];

// Toggle for which part to print
print_part = "all"; // Options: "all", "wall_bracket", "swivel_post", "table_top", "bearing_sleeve", "friction_washer"

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

// Wall mounting bracket - IMPROVED with better reinforcement
module wall_bracket() {
    difference() {
        union() {
            // Main bracket plate - thicker at the base
            hull() {
                cube([wall_bracket_width, wall_bracket_thickness, wall_bracket_height]);
                translate([0, 0, 0])
                    cube([wall_bracket_width, wall_bracket_thickness + 5, 20]);
            }

            // Stronger reinforcement ribs with triangular bracing
            for (y = [25:35:wall_bracket_height-25]) {
                translate([wall_bracket_width/2 - 15, wall_bracket_thickness, y])
                    rotate([0, 0, 0])
                        linear_extrude(height=3)
                            polygon([[0,0], [30,0], [15,25]]);
            }

            // Thicker bottom support base
            translate([0, 0, 0])
                cube([wall_bracket_width, 50, 20]);

            // Swivel post socket with enhanced support
            translate([wall_bracket_width/2, wall_bracket_thickness + 25, wall_bracket_height - 40])
                rotate([90, 0, 0]) {
                    difference() {
                        cylinder(h=35, d=swivel_bearing_diameter + 10);
                        // Bearing socket hole
                        translate([0, 0, -1])
                            cylinder(h=37, d=swivel_bearing_diameter + swivel_bearing_clearance);
                    }
                }

            // Additional support struts for swivel socket
            translate([wall_bracket_width/2, wall_bracket_thickness, wall_bracket_height - 60])
                rotate([0, 0, 0])
                    linear_extrude(height=5)
                        polygon([[-15,0], [15,0], [20,20], [-20,20]]);
        }

        // Wall mounting screw holes
        for (pos = wall_screw_positions) {
            translate([wall_bracket_width/2 + pos[0], -1, pos[1]])
                rotate([-90, 0, 0])
                    cylinder(h=wall_bracket_thickness + 10, d=screw_hole_diameter);
            // Countersink for flat head screws
            translate([wall_bracket_width/2 + pos[0], -1, pos[1]])
                rotate([-90, 0, 0])
                    cylinder(h=7, d1=screw_hole_diameter*2.5, d2=screw_hole_diameter);
        }
    }
}

// Swivel post (attaches to table, rotates in bearing) - IMPROVED
module swivel_post() {
    union() {
        // Main rotating post - tapered for easier insertion
        cylinder(h=swivel_post_height, d1=swivel_bearing_diameter - swivel_bearing_clearance*2 - 0.5,
                 d2=swivel_bearing_diameter - swivel_bearing_clearance*2);

        // Friction rings for smooth but controlled rotation - adjusted spacing
        for (z = [5, 20, 40, 60, 80, 100, 115]) {
            translate([0, 0, z])
                cylinder(h=1.5, d=swivel_bearing_diameter - swivel_bearing_clearance*0.5);
        }

        // Thicker table mounting plate at top
        translate([0, 0, swivel_post_height])
            cylinder(h=10, d=swivel_post_diameter + 25);

        // Stronger reinforcement ribs on mounting plate
        for (angle = [0:45:315]) {
            rotate([0, 0, angle])
                hull() {
                    translate([-2, 0, swivel_post_height])
                        cube([4, swivel_post_diameter/2 + 12, 10]);
                    translate([-4, swivel_post_diameter/2 + 8, swivel_post_height])
                        cube([8, 2, 10]);
                }
        }

        // Center reinforcement cone
        translate([0, 0, swivel_post_height - 15])
            cylinder(h=15, d1=swivel_bearing_diameter - swivel_bearing_clearance*2,
                     d2=swivel_post_diameter + 25);
    }
}

// Bearing sleeve (provides smooth rotation) - IMPROVED
module bearing_sleeve() {
    difference() {
        cylinder(h=33, d=swivel_bearing_diameter + swivel_bearing_clearance*2);
        translate([0, 0, -1])
            cylinder(h=35, d=swivel_bearing_diameter - swivel_bearing_clearance*0.8);

        // Grease channels for lubrication
        for (angle = [0:90:270]) {
            rotate([0, 0, angle])
                translate([swivel_bearing_diameter/2 - 1, -0.5, 5])
                    cube([2, 1, 23]);
        }
    }
}

// Friction washer for adjustable tension
module friction_washer() {
    difference() {
        cylinder(h=2, d=swivel_bearing_diameter + 3);
        translate([0, 0, -1])
            cylinder(h=4, d=swivel_bearing_diameter - swivel_bearing_clearance*2 + 0.5);
    }
}

// Table top - IMPROVED with better reinforcement
module table_top() {
    difference() {
        union() {
            // Main table surface with rounded corners
            rounded_rect(table_width, table_depth, table_thickness, 10);

            // Raised edge for items not to fall off
            difference() {
                rounded_rect(table_width, table_depth, table_thickness + 6, 10);
                translate([6, 6, -1])
                    rounded_rect(table_width - 12, table_depth - 12, table_thickness + 8, 8);
            }

            // Enhanced reinforcement structure underneath
            translate([0, 0, -15]) {
                // Cross bracing - thicker and taller
                translate([table_width/2 - 4, 10, 0])
                    cube([8, table_depth - 20, 15]);
                translate([10, table_depth/2 - 4, 0])
                    cube([table_width - 20, 8, 15]);

                // Perimeter reinforcement frame
                difference() {
                    rounded_rect(table_width - 8, table_depth - 8, 10, 8);
                    translate([6, 6, -1])
                        rounded_rect(table_width - 20, table_depth - 20, 12, 6);
                }
            }

            // Central mounting boss
            translate([table_width/2, table_depth/2, -15])
                cylinder(h=15, d=swivel_post_diameter + 30);
        }

        // Mounting hole for swivel post
        translate([table_width/2, table_depth/2, -16])
            cylinder(h=table_thickness + 17, d=swivel_post_diameter + 26);

        // Screw holes to attach to swivel post mounting plate
        for (angle = [0:45:315]) {
            rotate([0, 0, angle])
                translate([table_width/2, table_depth/2 + 28, -16])
                    cylinder(h=table_thickness + 17, d=screw_hole_diameter);
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
} else if (print_part == "friction_washer") {
    friction_washer();
}
