$fn = 72;

mode = "assembly"; // ["assembly", "plate"]
part = "all"; // ["all", "wall_plate", "swing_arm", "table_top", "pivot_cap"]
swing_angle = 18; // 0=open, 90=swung toward the wall
explode = 0;

table_w = 220;
table_d = 160;
table_t = 12;
table_r = 16;
mount_pad_w = 96;
mount_pad_d = 58;
mount_pad_t = 6;
insert_d = 4.8;
insert_depth = 8;

arm_len = 118;
arm_w = 46;
arm_t = 16;
arm_root_d = 62;
arm_tip_w = 72;

wall_plate_w = 78;
wall_plate_h = 182;
wall_plate_t = 12;
wall_plate_r = 14;
wall_screw_d = 5.4;

shelf_w = 72;
shelf_d = 36;
shelf_t = 14;
pivot_pedestal_d = 34;
pivot_pedestal_t = 4;
pivot_stack_z = 96;

pivot_bolt_d = 8.6;
washer_d = 24;
washer_t = 1.6;
pivot_cap_d = 56;
pivot_cap_t = 8;

open_stop_deg = -4;
closed_stop_deg = 96;
stop_pin_d = 10;
stop_pin_h = 10;
stop_radius = arm_root_d / 2 - 4;
lug_w = 12;
lug_len = 13;

mount_spacing_x = 54;
mount_spacing_y = 22;
table_center_y = arm_len + 48;

module rr2d(size = [10, 10], r = 2) {
    offset(r = r)
        square([size[0] - 2 * r, size[1] - 2 * r], center = true);
}

module rounded_block(size = [10, 10, 10], r = 2) {
    linear_extrude(height = size[2])
        rr2d([size[0], size[1]], r);
}

module countersunk_hole(h, thru_d = 5.4, head_d = 10.4, head_h = 4) {
    cylinder(h = h + 0.2, d = thru_d);
    translate([0, 0, h - head_h + 0.01])
        cylinder(h = head_h + 0.2, d1 = head_d, d2 = thru_d);
}

module mounting_hole_pattern(depth = 20, d = 5.2) {
    for (x = [-mount_spacing_x / 2, mount_spacing_x / 2])
        for (y = [-mount_spacing_y, 0, mount_spacing_y])
            translate([x, y, -0.01])
                cylinder(h = depth + 0.02, d = d);
}

module wall_plate() {
    difference() {
        union() {
            translate([0, -wall_plate_t, 0])
                rounded_block([wall_plate_w, wall_plate_t, wall_plate_h], wall_plate_r);

            translate([0, 0, pivot_stack_z])
                rounded_block([shelf_w, shelf_d, shelf_t], 10);

            translate([0, shelf_d / 2, pivot_stack_z + shelf_t])
                cylinder(h = pivot_pedestal_t, d = pivot_pedestal_d);

            for (sx = [-1, 1])
                hull() {
                    translate([sx * (shelf_w / 2 - 12), 0, pivot_stack_z + 10])
                        cube([12, 0.01, 24], center = true);
                    translate([sx * (shelf_w / 2 - 8), shelf_d - 2, pivot_stack_z + shelf_t - 6])
                        cube([16, 0.01, 12], center = true);
                }

            for (a = [open_stop_deg, closed_stop_deg]) {
                translate([
                    stop_radius * cos(a),
                    shelf_d / 2 + stop_radius * sin(a),
                    pivot_stack_z + shelf_t
                ])
                    cylinder(h = stop_pin_h, d = stop_pin_d);
            }
        }

        for (x = [-22, 22], z = [34, wall_plate_h - 34])
            translate([x, 0.01, z])
                rotate([90, 0, 0])
                    countersunk_hole(wall_plate_t + 0.4, thru_d = wall_screw_d);

        translate([0, shelf_d / 2, pivot_stack_z - 0.02])
            cylinder(h = shelf_t + pivot_pedestal_t + stop_pin_h + 0.04, d = pivot_bolt_d);

        translate([0, shelf_d / 2, pivot_stack_z - 0.02])
            cylinder(h = 6.4, d = 15.2);
    }
}

module swing_arm() {
    difference() {
        union() {
            translate([0, 0, -arm_t])
                cylinder(h = arm_t, d = arm_root_d);

            translate([0, arm_len / 2, -arm_t])
                rounded_block([arm_w, arm_len, arm_t], 12);

            translate([0, arm_len + mount_pad_d / 2, -arm_t])
                hull() {
                    rounded_block([arm_w, 24, arm_t], 10);
                    rounded_block([arm_tip_w, mount_pad_d, arm_t], 12);
                }

            translate([0, arm_root_d / 2 + 10, -arm_t])
                rotate([0, 0, -20])
                    cube([lug_w, lug_len, arm_t], center = true);
        }

        translate([0, 0, -arm_t - 0.01])
            cylinder(h = arm_t + 0.02, d = pivot_bolt_d);

        translate([0, arm_len + mount_pad_d / 2, -arm_t - 0.01])
            mounting_hole_pattern(depth = arm_t + 0.02);
    }
}

module table_top() {
    difference() {
        union() {
            rounded_block([table_w, table_d, table_t], table_r);

            translate([0, -table_d / 2 + mount_pad_d / 2 + 10, -mount_pad_t])
                rounded_block([mount_pad_w, mount_pad_d, mount_pad_t], 10);
        }

        translate([0, -table_d / 2 + mount_pad_d / 2 + 10, -mount_pad_t - 0.01])
            mounting_hole_pattern(depth = mount_pad_t + insert_depth + 0.02, d = insert_d);
    }
}

module pivot_cap() {
    difference() {
        union() {
            translate([0, 0, -pivot_cap_t])
                cylinder(h = pivot_cap_t, d = pivot_cap_d);

            translate([0, 0, -pivot_cap_t])
                cylinder(h = 3, d = 22);
        }

        translate([0, 0, -pivot_cap_t - 0.01])
            cylinder(h = pivot_cap_t + 0.02, d = pivot_bolt_d);

        translate([0, 0, -pivot_cap_t - 0.01])
            cylinder(h = 4.4, d = 15.2);
    }
}

module hardware_preview() {
    color([0.72, 0.74, 0.78])
        translate([0, shelf_d / 2, pivot_stack_z + shelf_t - 6])
            cylinder(h = arm_t + pivot_cap_t + washer_t * 3 + 8, d = 8);

    for (zv = [
        pivot_stack_z + shelf_t + pivot_pedestal_t,
        pivot_stack_z + shelf_t + pivot_pedestal_t + arm_t + washer_t,
        pivot_stack_z + shelf_t + pivot_pedestal_t + arm_t + pivot_cap_t + washer_t * 2
    ])
        color([0.86, 0.86, 0.88])
            translate([0, shelf_d / 2, zv])
                cylinder(h = washer_t, d = washer_d);
}

module table_assembly() {
    translate([0, 0, explode])
        color([0.86, 0.87, 0.9])
            wall_plate();

    translate([0, shelf_d / 2, pivot_stack_z + shelf_t + pivot_pedestal_t + washer_t])
        rotate([0, 0, swing_angle])
            union() {
                color([0.18, 0.28, 0.44])
                    swing_arm();

                translate([0, table_center_y, 0])
                    color([0.95, 0.77, 0.56])
                        table_top();

                translate([0, 0, -washer_t - explode / 2])
                    color([0.12, 0.12, 0.14])
                        pivot_cap();
            }

    hardware_preview();
}

module print_plate() {
    translate([-95, 0, 0])
        wall_plate();

    translate([65, 16, arm_t])
        rotate([180, 0, 0])
            swing_arm();

    translate([0, 176, 0])
        table_top();

    translate([98, 118, pivot_cap_t])
        rotate([180, 0, 0])
            pivot_cap();
}

if (part == "wall_plate")
    wall_plate();
else if (part == "swing_arm")
    swing_arm();
else if (part == "table_top")
    table_top();
else if (part == "pivot_cap")
    pivot_cap();
else if (mode == "plate")
    print_plate();
else
    table_assembly();
