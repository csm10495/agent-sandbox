// Design 5: DUAL-TIER Wall-Mounted Swivel Table
// Two-level shelf design for more storage
// Designed for 3D printing on Bambu P2S (240x256mm build plate)
// Material: PETG recommended

$fn = 100;

table_width = 180;
table_depth = 140;
table_thickness = 8;
tier_spacing = 80;  // Vertical spacing between tiers

wall_bracket_height = 200;  // Taller for two tiers
wall_bracket_width = 80;
wall_bracket_thickness = 12;

swivel_post_diameter = 25;
swivel_post_height = 160;  // Longer post
swivel_bearing_diameter = 28;
swivel_bearing_clearance = 0.25;

screw_hole_diameter = 5;
wall_screw_positions = [[0, 30], [0, 90], [0, 150], [0, 180]];

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
                cube([wall_bracket_width, 50, 20]);

            translate([wall_bracket_width/2, wall_bracket_thickness + 25, wall_bracket_height - 50])
                rotate([90, 0, 0])
                    cylinder(h=35, d=swivel_bearing_diameter + 10);
        }

        for (pos = wall_screw_positions) {
            translate([wall_bracket_width/2, -1, pos[1]])
                rotate([-90, 0, 0])
                    cylinder(h=20, d=screw_hole_diameter);
        }

        translate([wall_bracket_width/2, wall_bracket_thickness - 10, wall_bracket_height - 50])
            rotate([90, 0, 0])
                cylinder(h=40, d=swivel_bearing_diameter + swivel_bearing_clearance);
    }
}

module swivel_post() {
    union() {
        cylinder(h=swivel_post_height, d=swivel_bearing_diameter - swivel_bearing_clearance*2);

        for (z = [10:25:150]) {
            translate([0, 0, z])
                cylinder(h=2, d=swivel_bearing_diameter - swivel_bearing_clearance*0.5);
        }

        // Two mounting plates
        for (z_offset = [swivel_post_height - tier_spacing, swivel_post_height]) {
            translate([0, 0, z_offset])
                cylinder(h=8, d=swivel_post_diameter + 22);

            for (angle = [0:60:300]) {
                rotate([0, 0, angle])
                    translate([-2, 0, z_offset])
                        cube([4, swivel_post_diameter/2 + 11, 8]);
            }
        }
    }
}

module bearing_sleeve() {
    difference() {
        cylinder(h=33, d=swivel_bearing_diameter + swivel_bearing_clearance*2);
        translate([0, 0, -1])
            cylinder(h=35, d=swivel_bearing_diameter - swivel_bearing_clearance);
    }
}

module shelf_tier() {
    difference() {
        union() {
            rounded_rect(table_width, table_depth, table_thickness, 8);

            difference() {
                rounded_rect(table_width, table_depth, table_thickness + 4, 8);
                translate([4, 4, -1])
                    rounded_rect(table_width - 8, table_depth - 8, table_thickness + 6, 6);
            }

            translate([0, 0, -10]) {
                translate([table_width/2 - 3, 8, 0])
                    cube([6, table_depth - 16, 10]);
                translate([8, table_depth/2 - 3, 0])
                    cube([table_width - 16, 6, 10]);
            }
        }

        translate([table_width/2, table_depth/2, -11])
            cylinder(h=22, d=swivel_post_diameter + 24);

        for (angle = [0:120:240]) {
            rotate([0, 0, angle])
                translate([table_width/2, table_depth/2 + 22, -11])
                    cylinder(h=22, d=screw_hole_diameter);
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

    // Upper tier
    color("orange")
        translate([wall_bracket_width/2 - table_width/2,
                   wall_bracket_thickness - 10 - swivel_post_height - 8,
                   wall_bracket_height - 50])
            rotate([90, 0, 0]) shelf_tier();

    // Lower tier
    color("yellow")
        translate([wall_bracket_width/2 - table_width/2,
                   wall_bracket_thickness - 10 - swivel_post_height + tier_spacing - 8,
                   wall_bracket_height - 50])
            rotate([90, 0, 0]) shelf_tier();
}

if (print_part == "all") {
    assembly();
} else if (print_part == "wall_bracket") {
    wall_bracket();
} else if (print_part == "swivel_post") {
    swivel_post();
} else if (print_part == "table_top") {
    shelf_tier();
} else if (print_part == "bearing_sleeve") {
    bearing_sleeve();
}
