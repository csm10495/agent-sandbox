// Design 4: OVAL Wall-Mounted Swivel Table
// Smooth oval design with no sharp corners
// Designed for 3D printing on Bambu P2S (240x256mm build plate)
// Material: PETG or PLA

$fn = 100;

// Dimensions
table_width = 210;
table_depth = 160;
table_thickness = 10;

wall_bracket_height = 150;
wall_bracket_width = 80;
wall_bracket_thickness = 12;

swivel_post_diameter = 25;
swivel_post_height = 120;
swivel_bearing_diameter = 28;
swivel_bearing_clearance = 0.25;

screw_hole_diameter = 5;
wall_screw_positions = [[0, 30], [0, 90], [0, 130]];

print_part = "all";

// Oval shape module
module oval_shape(width, depth, height) {
    scale([width/depth, 1, 1])
        cylinder(h=height, d=depth);
}

module wall_bracket() {
    difference() {
        union() {
            hull() {
                cube([wall_bracket_width, wall_bracket_thickness, wall_bracket_height]);
                translate([0, 0, 0])
                    cube([wall_bracket_width, wall_bracket_thickness + 5, 20]);
            }

            translate([0, 0, 0])
                cube([wall_bracket_width, 45, 18]);

            translate([wall_bracket_width/2, wall_bracket_thickness + 22, wall_bracket_height - 40])
                rotate([90, 0, 0]) {
                    difference() {
                        cylinder(h=32, d=swivel_bearing_diameter + 10);
                        translate([0, 0, -1])
                            cylinder(h=34, d=swivel_bearing_diameter + swivel_bearing_clearance);
                    }
                }

            for (y = [25:38:wall_bracket_height-25]) {
                translate([wall_bracket_width/2 - 15, wall_bracket_thickness, y])
                    linear_extrude(height=4)
                        polygon([[0,0], [30,0], [15,26]]);
            }
        }

        for (pos = wall_screw_positions) {
            translate([wall_bracket_width/2, -1, pos[1]])
                rotate([-90, 0, 0])
                    cylinder(h=wall_bracket_thickness + 10, d=screw_hole_diameter);
            translate([wall_bracket_width/2, -1, pos[1]])
                rotate([-90, 0, 0])
                    cylinder(h=6, d1=screw_hole_diameter*2.5, d2=screw_hole_diameter);
        }
    }
}

module swivel_post() {
    union() {
        cylinder(h=swivel_post_height, d=swivel_bearing_diameter - swivel_bearing_clearance*2);

        for (z = [8, 28, 48, 68, 88, 108]) {
            translate([0, 0, z])
                cylinder(h=2, d=swivel_bearing_diameter - swivel_bearing_clearance*0.5);
        }

        translate([0, 0, swivel_post_height])
            cylinder(h=10, d=swivel_post_diameter + 24);

        for (angle = [0:45:315]) {
            rotate([0, 0, angle])
                hull() {
                    translate([-2, 0, swivel_post_height])
                        cube([4, swivel_post_diameter/2 + 12, 10]);
                    translate([-4, swivel_post_diameter/2 + 10, swivel_post_height])
                        cube([8, 2, 10]);
                }
        }
    }
}

module bearing_sleeve() {
    difference() {
        cylinder(h=30, d=swivel_bearing_diameter + swivel_bearing_clearance*2);
        translate([0, 0, -1])
            cylinder(h=32, d=swivel_bearing_diameter - swivel_bearing_clearance);

        for (angle = [0:90:270]) {
            rotate([0, 0, angle])
                translate([swivel_bearing_diameter/2 - 1, -0.5, 5])
                    cube([2, 1, 20]);
        }
    }
}

// Oval table top
module table_top() {
    difference() {
        union() {
            // Main oval surface
            translate([table_width/2, table_depth/2, 0])
                oval_shape(table_width, table_depth, table_thickness);

            // Raised oval edge
            difference() {
                translate([table_width/2, table_depth/2, 0])
                    oval_shape(table_width, table_depth, table_thickness + 6);
                translate([table_width/2, table_depth/2, -1])
                    oval_shape(table_width - 12, table_depth - 12, table_thickness + 8);
            }

            // Radial reinforcement underneath
            translate([0, 0, -14]) {
                for (angle = [0:60:300]) {
                    rotate([0, 0, angle])
                        translate([table_width/2 - 3, table_depth/2, 0])
                            cube([6, table_depth/2 - 15, 14]);
                }

                // Central mounting hub
                translate([table_width/2, table_depth/2, 0])
                    cylinder(h=14, d=swivel_post_diameter + 32);

                // Circular perimeter frame
                difference() {
                    translate([table_width/2, table_depth/2, 0])
                        oval_shape(table_width - 10, table_depth - 10, 10);
                    translate([table_width/2, table_depth/2, -1])
                        oval_shape(table_width - 25, table_depth - 25, 12);
                }
            }
        }

        // Mounting hole
        translate([table_width/2, table_depth/2, -15])
            cylinder(h=table_thickness + 16, d=swivel_post_diameter + 28);

        // Screw holes in circular pattern
        for (angle = [0:45:315]) {
            rotate([0, 0, angle])
                translate([table_width/2, table_depth/2 + 26, -15])
                    cylinder(h=table_thickness + 16, d=screw_hole_diameter);
        }
    }
}

module assembly() {
    color("lightblue")
        wall_bracket();
    color("gray")
        translate([wall_bracket_width/2, wall_bracket_thickness - 10, wall_bracket_height - 40])
            rotate([90, 0, 0])
                bearing_sleeve();
    color("green")
        translate([wall_bracket_width/2, wall_bracket_thickness - 10, wall_bracket_height - 40])
            rotate([90, 0, 0])
                swivel_post();
    color("orange")
        translate([wall_bracket_width/2 - table_width/2,
                   wall_bracket_thickness - 10 - swivel_post_height - 10,
                   wall_bracket_height - 40 - table_depth/2])
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
