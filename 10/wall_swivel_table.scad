// Design 10: MINIMALIST ULTRA-THIN Wall-Mounted Swivel Table
// Sleek, thin profile design for modern aesthetics
// Designed for 3D printing on Bambu P2S (240x256mm build plate)
// Material: PETG or PLA (light duty only)

$fn = 100;

// Ultra-thin dimensions
table_width = 180;
table_depth = 130;
table_thickness = 6;  // Very thin

wall_bracket_height = 120;
wall_bracket_width = 60;
wall_bracket_thickness = 8;  // Minimal thickness

swivel_post_diameter = 20;
swivel_post_height = 90;
swivel_bearing_diameter = 23;
swivel_bearing_clearance = 0.25;

screw_hole_diameter = 4;  // M3 screws
wall_screw_positions = [[0, 25], [0, 70], [0, 105]];

print_part = "all";

module rounded_rect(width, depth, height, radius) {
    hull() {
        for (x = [radius, width-radius])
            for (y = [radius, depth-radius])
                translate([x, y, 0])
                    cylinder(h=height, r=radius);
    }
}

// Minimalist bracket
module wall_bracket() {
    difference() {
        union() {
            // Slim profile plate
            cube([wall_bracket_width, wall_bracket_thickness, wall_bracket_height]);

            // Minimal base
            translate([0, 0, 0])
                cube([wall_bracket_width, 28, 12]);

            // Compact socket
            translate([wall_bracket_width/2, wall_bracket_thickness + 16, wall_bracket_height - 30])
                rotate([90, 0, 0]) {
                    difference() {
                        cylinder(h=22, d=swivel_bearing_diameter + 7);
                        translate([0, 0, -1])
                            cylinder(h=24, d=swivel_bearing_diameter + swivel_bearing_clearance);
                    }
                }

            // Minimal ribs
            for (y = [30, 70]) {
                translate([wall_bracket_width/2 - 10, wall_bracket_thickness, y])
                    linear_extrude(height=2)
                        polygon([[0,0], [20,0], [10,18]]);
            }
        }

        for (pos = wall_screw_positions) {
            translate([wall_bracket_width/2, -1, pos[1]])
                rotate([-90, 0, 0])
                    cylinder(h=wall_bracket_thickness + 2, d=screw_hole_diameter);
        }
    }
}

// Minimalist swivel post
module swivel_post() {
    union() {
        cylinder(h=swivel_post_height, d=swivel_bearing_diameter - swivel_bearing_clearance*2);

        // Minimal friction rings
        for (z = [15, 45, 75]) {
            translate([0, 0, z])
                cylinder(h=1, d=swivel_bearing_diameter - swivel_bearing_clearance*0.5);
        }

        // Slim mounting plate
        translate([0, 0, swivel_post_height])
            cylinder(h=6, d=swivel_post_diameter + 16);

        // Minimal ribs
        for (angle = [0:90:270]) {
            rotate([0, 0, angle])
                translate([-1.5, 0, swivel_post_height])
                    cube([3, swivel_post_diameter/2 + 8, 6]);
        }
    }
}

module bearing_sleeve() {
    difference() {
        cylinder(h=20, d=swivel_bearing_diameter + swivel_bearing_clearance*2);
        translate([0, 0, -1])
            cylinder(h=22, d=swivel_bearing_diameter - swivel_bearing_clearance);
    }
}

// Ultra-thin table top
module table_top() {
    difference() {
        union() {
            rounded_rect(table_width, table_depth, table_thickness, 8);

            // Subtle raised edge
            difference() {
                rounded_rect(table_width, table_depth, table_thickness + 3, 8);
                translate([3, 3, -1])
                    rounded_rect(table_width - 6, table_depth - 6, table_thickness + 5, 6);
            }

            // Minimal reinforcement
            translate([0, 0, -8]) {
                // Single cross brace
                translate([table_width/2 - 2.5, 8, 0])
                    cube([5, table_depth - 16, 8]);
                translate([8, table_depth/2 - 2.5, 0])
                    cube([table_width - 16, 5, 8]);

                // Small central hub
                translate([table_width/2, table_depth/2, 0])
                    cylinder(h=8, d=swivel_post_diameter + 20);
            }
        }

        translate([table_width/2, table_depth/2, -9])
            cylinder(h=table_thickness + 10, d=swivel_post_diameter + 18);

        // Minimal screw holes
        for (angle = [0:120:240]) {
            rotate([0, 0, angle])
                translate([table_width/2, table_depth/2 + 18, -9])
                    cylinder(h=table_thickness + 10, d=screw_hole_diameter);
        }
    }
}

module assembly() {
    color("lightblue") wall_bracket();
    color("gray")
        translate([wall_bracket_width/2, wall_bracket_thickness - 6, wall_bracket_height - 30])
            rotate([90, 0, 0]) bearing_sleeve();
    color("green")
        translate([wall_bracket_width/2, wall_bracket_thickness - 6, wall_bracket_height - 30])
            rotate([90, 0, 0]) swivel_post();
    color("orange")
        translate([wall_bracket_width/2 - table_width/2,
                   wall_bracket_thickness - 6 - swivel_post_height - 6,
                   wall_bracket_height - 30])
            rotate([90, 0, 0]) table_top();
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
