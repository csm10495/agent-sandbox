// Design 8: CORNER-MOUNT Wall-Mounted Swivel Table
// Designed to mount in corners with 45° bracket
// Designed for 3D printing on Bambu P2S (240x256mm build plate)
// Material: PETG or PLA

$fn = 100;

table_width = 200;
table_depth = 150;
table_thickness = 10;

wall_bracket_height = 150;
wall_bracket_width = 90;   // Wider for corner mount
wall_bracket_thickness = 12;

swivel_post_diameter = 25;
swivel_post_height = 120;
swivel_bearing_diameter = 28;
swivel_bearing_clearance = 0.25;

screw_hole_diameter = 5;

print_part = "all";

module rounded_rect(width, depth, height, radius) {
    hull() {
        for (x = [radius, width-radius])
            for (y = [radius, depth-radius])
                translate([x, y, 0])
                    cylinder(h=height, r=radius);
    }
}

// L-shaped corner bracket
module wall_bracket() {
    difference() {
        union() {
            // Vertical plate 1
            cube([wall_bracket_width, wall_bracket_thickness, wall_bracket_height]);

            // Vertical plate 2 (perpendicular)
            translate([0, 0, 0])
                rotate([0, 0, 90])
                    cube([wall_bracket_width, wall_bracket_thickness, wall_bracket_height]);

            // Diagonal brace
            translate([wall_bracket_thickness/2, wall_bracket_thickness/2, 0])
                rotate([0, 0, 45])
                    cube([wall_bracket_width * 1.1, 8, wall_bracket_height]);

            // Bottom reinforcement
            hull() {
                cube([wall_bracket_width, wall_bracket_thickness, 20]);
                rotate([0, 0, 90])
                    cube([wall_bracket_width, wall_bracket_thickness, 20]);
            }

            // Swivel socket at 45°
            translate([wall_bracket_width/2, wall_bracket_width/2, wall_bracket_height - 40])
                rotate([90, 45, 0]) {
                    difference() {
                        cylinder(h=32, d=swivel_bearing_diameter + 10);
                        translate([0, 0, -1])
                            cylinder(h=34, d=swivel_bearing_diameter + swivel_bearing_clearance);
                    }
                }
        }

        // Screw holes on both walls
        for (z = [30, 90, 130]) {
            // Wall 1 screws
            translate([wall_bracket_width/2, -1, z])
                rotate([-90, 0, 0])
                    cylinder(h=20, d=screw_hole_diameter);

            // Wall 2 screws
            translate([-1, wall_bracket_width/2, z])
                rotate([0, 90, 0])
                    cylinder(h=20, d=screw_hole_diameter);
        }
    }
}

module swivel_post() {
    union() {
        cylinder(h=swivel_post_height, d=swivel_bearing_diameter - swivel_bearing_clearance*2);

        for (z = [10, 35, 60, 85, 110]) {
            translate([0, 0, z])
                cylinder(h=2, d=swivel_bearing_diameter - swivel_bearing_clearance*0.5);
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
        cylinder(h=30, d=swivel_bearing_diameter + swivel_bearing_clearance*2);
        translate([0, 0, -1])
            cylinder(h=32, d=swivel_bearing_diameter - swivel_bearing_clearance);
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
        translate([wall_bracket_width/2, wall_bracket_width/2, wall_bracket_height - 40])
            rotate([90, 45, 0]) bearing_sleeve();
    color("green")
        translate([wall_bracket_width/2, wall_bracket_width/2, wall_bracket_height - 40])
            rotate([90, 45, 0]) swivel_post();
    color("orange")
        translate([wall_bracket_width/2 - table_width/2,
                   wall_bracket_width/2 - swivel_post_height - 10,
                   wall_bracket_height - 40])
            rotate([90, 45, 0]) table_top();
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
