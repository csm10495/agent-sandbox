// Design 2: LARGE Wall-Mounted Swivel Table
// Larger dimensions for more workspace
// Designed for 3D printing on Bambu P2S (240x256mm build plate) - Multi-part assembly
// Material: PETG recommended for strength

// Global parameters
$fn = 100;

// Dimensions (in mm) - LARGE VERSION
table_width = 250;  // Increased from 200
table_depth = 180;  // Increased from 150
table_thickness = 12; // Thicker for strength

wall_bracket_height = 180;  // Increased from 150
wall_bracket_width = 100;   // Increased from 80
wall_bracket_thickness = 15; // Thicker

swivel_post_diameter = 30;  // Increased from 25
swivel_post_height = 140;   // Increased from 120
swivel_bearing_diameter = 33;
swivel_bearing_clearance = 0.25;

// Mounting hardware holes
screw_hole_diameter = 6; // M5 screws for heavier loads
wall_screw_positions = [
    [0, 35],
    [0, 90],
    [0, 145],
    [0, 170]  // Extra mounting point
];

print_part = "all";

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

// Heavy-duty wall bracket
module wall_bracket() {
    difference() {
        union() {
            // Main bracket with taper
            hull() {
                cube([wall_bracket_width, wall_bracket_thickness, wall_bracket_height]);
                translate([0, 0, 0])
                    cube([wall_bracket_width, wall_bracket_thickness + 8, 25]);
            }

            // Extra thick bottom support
            translate([0, 0, 0])
                cube([wall_bracket_width, 60, 25]);

            // Large swivel socket
            translate([wall_bracket_width/2, wall_bracket_thickness + 30, wall_bracket_height - 50])
                rotate([90, 0, 0]) {
                    difference() {
                        cylinder(h=40, d=swivel_bearing_diameter + 12);
                        translate([0, 0, -1])
                            cylinder(h=42, d=swivel_bearing_diameter + swivel_bearing_clearance);
                    }
                }

            // Heavy reinforcement ribs
            for (y = [30:50:wall_bracket_height-30]) {
                translate([wall_bracket_width/2 - 20, wall_bracket_thickness, y])
                    linear_extrude(height=5)
                        polygon([[0,0], [40,0], [20,30]]);
            }
        }

        // Wall mounting screw holes
        for (pos = wall_screw_positions) {
            translate([wall_bracket_width/2, -1, pos[1]])
                rotate([-90, 0, 0])
                    cylinder(h=wall_bracket_thickness + 10, d=screw_hole_diameter);
            translate([wall_bracket_width/2, -1, pos[1]])
                rotate([-90, 0, 0])
                    cylinder(h=8, d1=screw_hole_diameter*2.5, d2=screw_hole_diameter);
        }
    }
}

// Heavy-duty swivel post
module swivel_post() {
    union() {
        cylinder(h=swivel_post_height, d=swivel_bearing_diameter - swivel_bearing_clearance*2);

        for (z = [8, 25, 45, 65, 85, 105, 125]) {
            translate([0, 0, z])
                cylinder(h=2, d=swivel_bearing_diameter - swivel_bearing_clearance*0.5);
        }

        translate([0, 0, swivel_post_height])
            cylinder(h=12, d=swivel_post_diameter + 30);

        for (angle = [0:30:330]) {
            rotate([0, 0, angle])
                hull() {
                    translate([-3, 0, swivel_post_height])
                        cube([6, swivel_post_diameter/2 + 15, 12]);
                    translate([-5, swivel_post_diameter/2 + 12, swivel_post_height])
                        cube([10, 2, 12]);
                }
        }
    }
}

module bearing_sleeve() {
    difference() {
        cylinder(h=38, d=swivel_bearing_diameter + swivel_bearing_clearance*2);
        translate([0, 0, -1])
            cylinder(h=40, d=swivel_bearing_diameter - swivel_bearing_clearance);

        for (angle = [0:90:270]) {
            rotate([0, 0, angle])
                translate([swivel_bearing_diameter/2 - 1, -0.75, 6])
                    cube([2.5, 1.5, 26]);
        }
    }
}

// Large table top (split into 2 parts for printing)
module table_top() {
    difference() {
        union() {
            rounded_rect(table_width, table_depth, table_thickness, 12);

            difference() {
                rounded_rect(table_width, table_depth, table_thickness + 8, 12);
                translate([8, 8, -1])
                    rounded_rect(table_width - 16, table_depth - 16, table_thickness + 10, 10);
            }

            translate([0, 0, -18]) {
                translate([table_width/2 - 5, 12, 0])
                    cube([10, table_depth - 24, 18]);
                translate([12, table_depth/2 - 5, 0])
                    cube([table_width - 24, 10, 18]);

                // Additional corner braces
                for (corner = [[20, 20], [table_width - 30, 20], [20, table_depth - 30], [table_width - 30, table_depth - 30]]) {
                    translate([corner[0], corner[1], 0])
                        cylinder(h=15, d=20);
                }

                difference() {
                    rounded_rect(table_width - 10, table_depth - 10, 12, 10);
                    translate([8, 8, -1])
                        rounded_rect(table_width - 26, table_depth - 26, 14, 8);
                }
            }

            translate([table_width/2, table_depth/2, -18])
                cylinder(h=18, d=swivel_post_diameter + 35);
        }

        translate([table_width/2, table_depth/2, -19])
            cylinder(h=table_thickness + 20, d=swivel_post_diameter + 32);

        for (angle = [0:40:320]) {
            rotate([0, 0, angle])
                translate([table_width/2, table_depth/2 + 32, -19])
                    cylinder(h=table_thickness + 20, d=screw_hole_diameter);
        }
    }
}

module assembly() {
    color("lightblue")
        wall_bracket();
    color("gray")
        translate([wall_bracket_width/2, wall_bracket_thickness - 10, wall_bracket_height - 50])
            rotate([90, 0, 0])
                bearing_sleeve();
    color("green")
        translate([wall_bracket_width/2, wall_bracket_thickness - 10, wall_bracket_height - 50])
            rotate([90, 0, 0])
                swivel_post();
    color("orange")
        translate([wall_bracket_width/2 - table_width/2,
                   wall_bracket_thickness - 10 - swivel_post_height - 12,
                   wall_bracket_height - 50])
            rotate([90, 0, 0])
                table_top();
}

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
