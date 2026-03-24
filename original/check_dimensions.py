#!/usr/bin/env python3
"""Check STL file dimensions to ensure they fit on Bambu P2S print bed (240x256mm)"""

import struct
import sys

def read_stl_ascii(filename):
    """Read ASCII STL file and return min/max coordinates"""
    min_x = min_y = min_z = float('inf')
    max_x = max_y = max_z = float('-inf')

    with open(filename, 'r') as f:
        for line in f:
            if 'vertex' in line:
                parts = line.strip().split()
                if len(parts) == 4:
                    x, y, z = float(parts[1]), float(parts[2]), float(parts[3])
                    min_x = min(min_x, x)
                    max_x = max(max_x, x)
                    min_y = min(min_y, y)
                    max_y = max(max_y, y)
                    min_z = min(min_z, z)
                    max_z = max(max_z, z)

    return (min_x, max_x, min_y, max_y, min_z, max_z)

def check_stl(filename, bed_x=240, bed_y=256):
    """Check if STL fits on print bed"""
    try:
        min_x, max_x, min_y, max_y, min_z, max_z = read_stl_ascii(filename)

        width = max_x - min_x
        depth = max_y - min_y
        height = max_z - min_z

        fits = width <= bed_x and depth <= bed_y

        print(f"\n{filename}:")
        print(f"  Dimensions: {width:.2f} x {depth:.2f} x {height:.2f} mm")
        print(f"  Fits on bed: {'✓ YES' if fits else '✗ NO'}")

        if not fits:
            print(f"  PROBLEM: Exceeds bed size (max {bed_x}x{bed_y}mm)")

        return fits
    except Exception as e:
        print(f"Error reading {filename}: {e}")
        return False

if __name__ == "__main__":
    print("Bambu P2S Print Bed: 240 x 256 mm")
    print("=" * 50)

    files = ["wall_bracket_v2.stl", "swivel_post_v2.stl", "table_top_v2.stl", "bearing_sleeve_v2.stl", "friction_washer.stl"]
    all_fit = True

    for filename in files:
        if not check_stl(filename):
            all_fit = False

    print("\n" + "=" * 50)
    if all_fit:
        print("✓ All parts fit on the Bambu P2S print bed!")
    else:
        print("✗ Some parts need to be resized or split!")
