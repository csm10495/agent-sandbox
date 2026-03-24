// Design 7: ADJUSTABLE HEIGHT Wall-Mounted Swivel Table
// Features telescoping post for height adjustment
// Designed for 3D printing on Bambu P2S (240x256mm build plate)
// Material: PETG recommended

$fn = 100;

table_width = 190;
table_depth = 145;
table_thickness = 10;

wall_bracket_height = 160;
wall_bracket_width = 80;
wall_bracket_thickness = 12;

swivel_outer_diameter = 30;  // Outer tube
swivel_inner_diameter = 26;  // Inner sliding tube
swivel_post_height = 140;    // Total extended height
swivel_bearing_diameter = 32;
swivel_bearing_clearance = 0.25;

screw_hole_diameter = 5;
wall_screw_positions = [[0, 30], [0, 95], [0, 140]];

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
                cube([wall_bracket_width, 48, 20]);

            translate([wall_bracket_width/2, wall_bracket_thickness + 26, wall_bracket_height - 45])
                rotate([90, 0, 0])
                    cylinder(h=36, d=swivel_bearing_diameter + 12);
        }

        for (pos = wall_screw_positions) {
            translate([wall_bracket_width/2, -1, pos[1]])
                rotate([-90, 0, 0])
                    cylinder(h=22, d=screw_hole_diameter);
        }

        translate([wall_bracket_width/2, wall_bracket_thickness - 10, wall_bracket_height - 45])
            rotate([90, 0, 0])
                cylinder(h=42, d=swivel_bearing_diameter + swivel_bearing_clearance);
    }
}

// Outer stationary post with slots
module swivel_post_outer() {
    difference() {
        union() {
            cylinder(h=swivel_post_height * 0.6, d=swivel_outer_diameter);

            for (z = [8, 25, 42, 58, 75]) {
                translate([0, 0, z])
                    cylinder(h=2, d=swivel_bearing_diameter - swivel_bearing_clearance*0.5);
            }
        }

        // Inner bore for sliding tube
        translate([0, 0, -1])
            cylinder(h=swivel_post_height * 0.6 + 2, d=swivel_inner_diameter + 0.5);

        // Height adjustment slots (3 positions)
        for (z = [20, 40, 60]) {
            translate([swivel_outer_diameter/2 - 3, -1, z])
                cube([5, 2, 6]);
        }
    }
}

// Inner sliding post with locking pin
module swivel_post_inner() {
    union() {
        cylinder(h=swivel_post_height * 0.7, d=swivel_inner_diameter);

        // Locking pin hole
        translate([swivel_inner_diameter/2 - 2, -1.5, 50])
            cube([4, 3, 8]);

        // Table mounting plate
        translate([0, 0, swivel_post_height * 0.7])
            cylinder(h=10, d=swivel_inner_diameter + 20);

        for (angle = [0:60:300]) {
            rotate([0, 0, angle])
                translate([-2, 0, swivel_post_height * 0.7])
                    cube([4, swivel_inner_diameter/2 + 10, 10]);
        }
    }
}

module bearing_sleeve() {
    difference() {
        cylinder(h=34, d=swivel_bearing_diameter + swivel_bearing_clearance*2);
        translate([0, 0, -1])
            cylinder(h=36, d=swivel_bearing_diameter - swivel_bearing_clearance);
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

            translate([0, 0, -13]) {
                translate([table_width/2 - 4, 10, 0])
                    cube([8, table_depth - 20, 13]);
                translate([10, table_depth/2 - 4, 0])
                    cube([table_width - 20, 8, 13]);

                translate([table_width/2, table_depth/2, 0])
                    cylinder(h=13, d=swivel_inner_diameter + 24);
            }
        }

        translate([table_width/2, table_depth/2, -14])
            cylinder(h=24, d=swivel_inner_diameter + 22);

        for (angle = [0:60:300]) {
            rotate([0, 0, angle])
                translate([table_width/2, table_depth/2 + 22, -14])
                    cylinder(h=24, d=screw_hole_diameter);
        }
    }
}

module assembly() {
    color("lightblue") wall_bracket();
    color("gray")
        translate([wall_bracket_width/2, wall_bracket_thickness - 10, wall_bracket_height - 45])
            rotate([90, 0, 0]) bearing_sleeve();
    color("green")
        translate([wall_bracket_width/2, wall_bracket_thickness - 10, wall_bracket_height - 45])
            rotate([90, 0, 0]) swivel_post_outer();
    color("darkgreen")
        translate([wall_bracket_width/2, wall_bracket_thickness - 10, wall_bracket_height - 45])
            rotate([90, 0, 0]) swivel_post_inner();
    color("orange")
        translate([wall_bracket_width/2 - table_width/2,
                   wall_bracket_thickness - 10 - swivel_post_height * 0.7 - 10,
                   wall_bracket_height - 45])
            rotate([90, 0, 0]) table_top();
}

if (print_part == "all") {
    assembly();
} else if (print_part == "wall_bracket") {
    wall_bracket();
} else if (print_part == "swivel_post") {
    swivel_post_outer();
} else if (print_part == "swivel_inner") {
    swivel_post_inner();
} else if (print_part == "table_top") {
    table_top();
} else if (print_part == "bearing_sleeve") {
    bearing_sleeve();
}
