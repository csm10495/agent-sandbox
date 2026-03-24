// Design 9: HEAVY-DUTY Wall-Mounted Swivel Table
// Reinforced design with cavities for metal threaded inserts
// Designed for 3D printing on Bambu P2S (240x256mm build plate)
// Material: PETG strongly recommended, with metal inserts

$fn = 100;

table_width = 220;
table_depth = 160;
table_thickness = 14;  // Extra thick

wall_bracket_height = 170;
wall_bracket_width = 95;
wall_bracket_thickness = 16;  // Very thick

swivel_post_diameter = 32;  // Large diameter
swivel_post_height = 135;
swivel_bearing_diameter = 35;
swivel_bearing_clearance = 0.25;

screw_hole_diameter = 6;  // M5 screws
insert_hole_diameter = 5.5;  // For M5 threaded inserts
wall_screw_positions = [[0, 35], [0, 85], [0, 135], [0, 160]];

print_part = "all";

module rounded_rect(width, depth, height, radius) {
    hull() {
        for (x = [radius, width-radius])
            for (y = [radius, depth-radius])
                translate([x, y, 0])
                    cylinder(h=height, r=radius);
    }
}

// Heavy-duty bracket with insert cavities
module wall_bracket() {
    difference() {
        union() {
            // Extra thick main plate
            hull() {
                cube([wall_bracket_width, wall_bracket_thickness, wall_bracket_height]);
                translate([0, 0, 0])
                    cube([wall_bracket_width, wall_bracket_thickness + 10, 30]);
            }

            // Massive bottom support
            translate([0, 0, 0])
                cube([wall_bracket_width, 65, 30]);

            // Extra-large swivel socket
            translate([wall_bracket_width/2, wall_bracket_thickness + 32, wall_bracket_height - 50])
                rotate([90, 0, 0]) {
                    difference() {
                        cylinder(h=42, d=swivel_bearing_diameter + 16);
                        translate([0, 0, -1])
                            cylinder(h=44, d=swivel_bearing_diameter + swivel_bearing_clearance);
                    }
                }

            // Heavy reinforcement ribs
            for (y = [30:45:wall_bracket_height-30]) {
                translate([wall_bracket_width/2 - 22, wall_bracket_thickness, y])
                    linear_extrude(height=6)
                        polygon([[0,0], [44,0], [22,35]]);
            }
        }

        // Mounting holes with threaded insert cavities
        for (pos = wall_screw_positions) {
            translate([wall_bracket_width/2, -1, pos[1]])
                rotate([-90, 0, 0]) {
                    cylinder(h=wall_bracket_thickness + 12, d=screw_hole_diameter);
                    // Cavity for threaded insert
                    cylinder(h=10, d=insert_hole_diameter + 1.5);
                }
        }
    }
}

// Heavy-duty swivel post with insert cavities
module swivel_post() {
    union() {
        cylinder(h=swivel_post_height, d=swivel_bearing_diameter - swivel_bearing_clearance*2);

        for (z = [10, 28, 48, 68, 88, 108, 125]) {
            translate([0, 0, z])
                cylinder(h=2.5, d=swivel_bearing_diameter - swivel_bearing_clearance*0.5);
        }

        translate([0, 0, swivel_post_height])
            cylinder(h=14, d=swivel_post_diameter + 32);

        for (angle = [0:30:330]) {
            rotate([0, 0, angle])
                hull() {
                    translate([-4, 0, swivel_post_height])
                        cube([8, swivel_post_diameter/2 + 16, 14]);
                    translate([-6, swivel_post_diameter/2 + 14, swivel_post_height])
                        cube([12, 3, 14]);
                }
        }

        translate([0, 0, swivel_post_height - 20])
            cylinder(h=20, d1=swivel_bearing_diameter - swivel_bearing_clearance*2,
                     d2=swivel_post_diameter + 32);
    }
}

module bearing_sleeve() {
    difference() {
        cylinder(h=40, d=swivel_bearing_diameter + swivel_bearing_clearance*2);
        translate([0, 0, -1])
            cylinder(h=42, d=swivel_bearing_diameter - swivel_bearing_clearance);

        for (angle = [0:90:270]) {
            rotate([0, 0, angle])
                translate([swivel_bearing_diameter/2 - 1.5, -1, 8])
                    cube([3, 2, 24]);
        }
    }
}

// Heavy-duty table with insert cavities
module table_top() {
    difference() {
        union() {
            rounded_rect(table_width, table_depth, table_thickness, 12);

            difference() {
                rounded_rect(table_width, table_depth, table_thickness + 8, 12);
                translate([8, 8, -1])
                    rounded_rect(table_width - 16, table_depth - 16, table_thickness + 10, 10);
            }

            translate([0, 0, -20]) {
                translate([table_width/2 - 6, 14, 0])
                    cube([12, table_depth - 28, 20]);
                translate([14, table_depth/2 - 6, 0])
                    cube([table_width - 28, 12, 20]);

                // Corner reinforcement blocks
                for (corner = [[25, 25], [table_width - 35, 25], [25, table_depth - 35], [table_width - 35, table_depth - 35]]) {
                    translate([corner[0], corner[1], 0])
                        cylinder(h=18, d=25);
                }

                difference() {
                    rounded_rect(table_width - 12, table_depth - 12, 14, 10);
                    translate([10, 10, -1])
                        rounded_rect(table_width - 32, table_depth - 32, 16, 8);
                }

                translate([table_width/2, table_depth/2, 0])
                    cylinder(h=20, d=swivel_post_diameter + 38);
            }
        }

        translate([table_width/2, table_depth/2, -21])
            cylinder(h=table_thickness + 22, d=swivel_post_diameter + 34);

        // Screw holes with insert cavities
        for (angle = [0:30:330]) {
            rotate([0, 0, angle])
                translate([table_width/2, table_depth/2 + 34, -21]) {
                    cylinder(h=table_thickness + 22, d=screw_hole_diameter);
                    // Cavity for threaded insert (from bottom)
                    translate([0, 0, -1])
                        cylinder(h=12, d=insert_hole_diameter + 1.5);
                }
        }
    }
}

module assembly() {
    color("lightblue") wall_bracket();
    color("gray")
        translate([wall_bracket_width/2, wall_bracket_thickness - 10, wall_bracket_height - 50])
            rotate([90, 0, 0]) bearing_sleeve();
    color("green")
        translate([wall_bracket_width/2, wall_bracket_thickness - 10, wall_bracket_height - 50])
            rotate([90, 0, 0]) swivel_post();
    color("orange")
        translate([wall_bracket_width/2 - table_width/2,
                   wall_bracket_thickness - 10 - swivel_post_height - 14,
                   wall_bracket_height - 50])
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
