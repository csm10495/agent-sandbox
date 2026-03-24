// Design 6: FOLDING Wall-Mounted Table with Locking Positions
// Features detents for 3 locking positions (0°, 90°, 180°)
// Designed for 3D printing on Bambu P2S (240x256mm build plate)
// Material: PETG or PLA

$fn = 100;

table_width = 200;
table_depth = 150;
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

module rounded_rect(width, depth, height, radius) {
    hull() {
        for (x = [radius, width-radius])
            for (y = [radius, depth-radius])
                translate([x, y, 0])
                    cylinder(h=height, r=radius);
    }
}

module wall_bracket() {
    difference() {
        union() {
            cube([wall_bracket_width, wall_bracket_thickness, wall_bracket_height]);
            translate([0, 0, 0])
                cube([wall_bracket_width, 45, 18]);

            // Socket with detent grooves
            translate([wall_bracket_width/2, wall_bracket_thickness + 24, wall_bracket_height - 40])
                rotate([90, 0, 0]) {
                    difference() {
                        cylinder(h=34, d=swivel_bearing_diameter + 10);
                        translate([0, 0, -1])
                            cylinder(h=36, d=swivel_bearing_diameter + swivel_bearing_clearance);

                        // Detent grooves at 3 positions (0°, 90°, 180°)
                        for (angle = [0, 90, 180]) {
                            rotate([0, 0, angle])
                                translate([swivel_bearing_diameter/2 - 1, -1, 12])
                                    cube([3, 2, 10]);
                        }
                    }
                }
        }

        for (pos = wall_screw_positions) {
            translate([wall_bracket_width/2, -1, pos[1]])
                rotate([-90, 0, 0])
                    cylinder(h=20, d=screw_hole_diameter);
        }
    }
}

module swivel_post() {
    union() {
        // Main post with detent bumps
        difference() {
            cylinder(h=swivel_post_height, d=swivel_bearing_diameter - swivel_bearing_clearance*2);

            // Add detent bumps at 3 positions
            for (angle = [0, 90, 180]) {
                rotate([0, 0, angle])
                    translate([swivel_bearing_diameter/2 - swivel_bearing_clearance - 0.8, -0.75, 50])
                        cube([2, 1.5, 10]);
            }
        }

        // Detent bumps (protruding parts)
        for (angle = [0, 90, 180]) {
            rotate([0, 0, angle])
                translate([swivel_bearing_diameter/2 - swivel_bearing_clearance*2 - 0.3, -0.5, 52])
                    cube([0.8, 1, 6]);
        }

        for (z = [10, 35, 85, 110]) {
            translate([0, 0, z])
                cylinder(h=1.5, d=swivel_bearing_diameter - swivel_bearing_clearance*0.5);
        }

        translate([0, 0, swivel_post_height])
            cylinder(h=10, d=swivel_post_diameter + 24);

        for (angle = [0:45:315]) {
            rotate([0, 0, angle])
                translate([-2, 0, swivel_post_height])
                    cube([4, swivel_post_diameter/2 + 12, 10]);
        }
    }
}

module bearing_sleeve() {
    difference() {
        cylinder(h=32, d=swivel_bearing_diameter + swivel_bearing_clearance*2);
        translate([0, 0, -1])
            cylinder(h=34, d=swivel_bearing_diameter - swivel_bearing_clearance);
    }
}

module table_top() {
    difference() {
        union() {
            rounded_rect(table_width, table_depth, table_thickness, 10);

            difference() {
                rounded_rect(table_width, table_depth, table_thickness + 5, 10);
                translate([5, 5, -1])
                    rounded_rect(table_width - 10, table_depth - 10, table_thickness + 7, 8);
            }

            translate([0, 0, -14]) {
                translate([table_width/2 - 4, 10, 0])
                    cube([8, table_depth - 20, 14]);
                translate([10, table_depth/2 - 4, 0])
                    cube([table_width - 20, 8, 14]);

                translate([table_width/2, table_depth/2, 0])
                    cylinder(h=14, d=swivel_post_diameter + 30);
            }
        }

        translate([table_width/2, table_depth/2, -15])
            cylinder(h=26, d=swivel_post_diameter + 26);

        for (angle = [0:45:315]) {
            rotate([0, 0, angle])
                translate([table_width/2, table_depth/2 + 27, -15])
                    cylinder(h=26, d=screw_hole_diameter);
        }
    }
}

module assembly() {
    color("lightblue") wall_bracket();
    color("gray")
        translate([wall_bracket_width/2, wall_bracket_thickness - 10, wall_bracket_height - 40])
            rotate([90, 0, 0]) bearing_sleeve();
    color("green")
        translate([wall_bracket_width/2, wall_bracket_thickness - 10, wall_bracket_height - 40])
            rotate([90, 0, 0]) swivel_post();
    color("orange")
        translate([wall_bracket_width/2 - table_width/2,
                   wall_bracket_thickness - 10 - swivel_post_height - 10,
                   wall_bracket_height - 40])
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
