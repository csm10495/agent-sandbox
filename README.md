# Wall-Mounted Swivel Table

This repository contains a parametric OpenSCAD design for a small wall-mounted table that swings out from the wall on a friction-adjustable pivot.

## Files

- `wall_swivel_table.scad` - the printable model and assembled preview

## Design summary

- Tabletop: 220 mm x 160 mm x 12 mm
- Intended use: light-duty bedside, plant, or drink table
- Load target: a few pounds when printed with strong settings and mounted into a stud or suitable wall anchors
- Printer fit: all parts fit within a 256 mm x 256 mm x 256 mm Bambu P2S build volume
- Storage motion: the table swivels around a vertical pivot so it can sit close to the wall when not in use

## Required hardware

- 4x M5 wall screws plus wall anchors if not mounting to a stud
- 1x M8 x 55 mm bolt
- 1x M8 nyloc nut
- 3x M8 washers total
- 2x 8 mm ID nylon or PTFE friction washers
- 6x M5 heat-set inserts for the tabletop mount
- 6x M5 x 16 mm machine screws for attaching the swing arm to the tabletop

## Recommended print settings

- Material: PETG preferred, PLA acceptable for lower heat and lighter-duty use
- Layer height: 0.20 mm
- Nozzle: 0.4 mm
- Perimeters: 5 or more
- Top and bottom layers: 6 or more
- Infill: 35% gyroid or cubic for the arm and wall bracket, 25% or more for the tabletop

## How to use the model

- Preview assembled table: open `wall_swivel_table.scad` with `mode = "assembly"`
- Show printable plate A: set `mode = "plate_a"`
- Show printable plate B: set `mode = "plate_b"`
- Export a single part: set `part` to `wall_plate`, `swing_arm`, `table_top`, or `pivot_cap`

Example CLI exports:

```sh
openscad -o wall_plate.stl -D 'part="wall_plate"' ./wall_swivel_table.scad
openscad -o print_plate_a.stl -D 'mode="plate_a"' ./wall_swivel_table.scad
openscad -o print_plate_b.stl -D 'mode="plate_b"' ./wall_swivel_table.scad
```

## Assembly notes

1. Install the six heat-set inserts into the underside mounting pad on the tabletop.
2. Fasten the swing arm to the tabletop with the M5 machine screws.
3. Mount the wall plate into a stud or suitable anchors.
4. Stack the pivot as: bolt head under shelf, steel washer, nylon washer, swing arm, nylon washer, pivot cap, steel washer, nyloc nut.
5. Tighten the nut until the table swings smoothly but stays in place on its own.

The printed stop pins and arm lug limit the swing travel so the table does not rotate too far in either direction.
