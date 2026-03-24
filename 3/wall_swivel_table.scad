// Design 3: TRIANGULAR Wall-Mounted Swivel Table
// Unique triangular design for corner mounting or aesthetic variation
// Designed for 3D printing on Bambu P2S (240x256mm build plate)
// Material: PETG or PLA

$fn = 100;

// Dimensions
table_base_width = 220;  // Triangle base
table_depth = 190;       // Triangle depth
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

module rounded_triangle(base, depth, height, radius) {
    hull() {
        // Three corners of triangle
        translate([base/2, radius, 0])
            cylinder(h=height, r=radius);
        translate([radius, depth - radius, 0])
            cylinder(h=height, r=radius);
        translate([base - radius, depth - radius, 0])
            cylinder(h=height, r=radius);
    }
}

module wall_bracket() {
    difference() {
        union() {
            cube([wall_bracket_width, wall_bracket_thickness, wall_bracket_height]);
            translate([0, 0, 0])
                cube([wall_bracket_width, 40, 15]);

            translate([wall_bracket_width/2, wall_bracket_thickness + 20, wall_bracket_height - 40])
                rotate([90, 0, 0]) {
                    difference() {
                        cylinder(h=30, d=swivel_bearing_diameter + 8);
                        translate([0, 0, -1])
                            cylinder(h=32, d=swivel_bearing_diameter + swivel_bearing_clearance);
                    }
                }

            for (y = [25:40:wall_bracket_height-25]) {
                translate([wall_bracket_width/2 - 15, wall_bracket_thickness, y])
                    linear_extrude(height=3)
                        polygon([[0,0], [30,0], [15,25]]);
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

        for (z = [10, 35, 60, 85, 110]) {
            translate([0, 0, z])
                cylinder(h=1.5, d=swivel_bearing_diameter - swivel_bearing_clearance*0.5);
        }

        translate([0, 0, swivel_post_height])
            cylinder(h=10, d=swivel_post_diameter + 25);

        for (angle = [0:60:300]) {
            rotate([0, 0, angle])
                translate([-3, 0, swivel_post_height])
                    cube([6, swivel_post_diameter/2 + 12, 10]);
        }
    }
}

module bearing_sleeve() {
    difference() {
        cylinder(h=28, d=swivel_bearing_diameter + swivel_bearing_clearance*2);
        translate([0, 0, -1])
            cylinder(h=30, d=swivel_bearing_diameter - swivel_bearing_clearance);
    }
}

// Triangular table top
module table_top() {
    difference() {
        union() {
            // Main triangular surface
            rounded_triangle(table_base_width, table_depth, table_thickness, 10);

            // Raised edge
            difference() {
                rounded_triangle(table_base_width, table_depth, table_thickness + 5, 10);
                translate([0, 0, -1])
                    offset(r=-5)
                        rounded_triangle(table_base_width, table_depth, table_thickness + 7, 10);
            }

            // Reinforcement underneath
            translate([0, 0, -12]) {
                // Y-shaped reinforcement pattern
                hull() {
                    translate([table_base_width/2, 30, 0])
                        cylinder(h=12, d=12);
                    translate([table_base_width/2, table_depth - 30, 0])
                        cylinder(h=12, d=12);
                }
                hull() {
                    translate([table_base_width/2, 30, 0])
                        cylinder(h=12, d=12);
                    translate([30, table_depth - 30, 0])
                        cylinder(h=12, d=12);
                }
                hull() {
                    translate([table_base_width/2, 30, 0])
                        cylinder(h=12, d=12);
                    translate([table_base_width - 30, table_depth - 30, 0])
                        cylinder(h=12, d=12);
                }
            }

            // Central mounting boss
            translate([table_base_width/2, table_depth/2 + 20, -12])
                cylinder(h=12, d=swivel_post_diameter + 28);
        }

        // Mounting hole
        translate([table_base_width/2, table_depth/2 + 20, -13])
            cylinder(h=table_thickness + 14, d=swivel_post_diameter + 24);

        // Screw holes in triangular pattern
        for (angle = [0:120:240]) {
            rotate([0, 0, angle])
                translate([table_base_width/2, table_depth/2 + 20 + 25, -13])
                    cylinder(h=table_thickness + 14, d=screw_hole_diameter);
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
        translate([wall_bracket_width/2 - table_base_width/2,
                   wall_bracket_thickness - 10 - swivel_post_height - 10,
                   wall_bracket_height - 40 - table_depth/2 - 20])
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
