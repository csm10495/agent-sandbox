# Wall-Mounted Swivel Table - 3D Printing Guide

## Overview
This is a 3D printable wall-mounted table that swivels in and out from the wall. The design is inspired by commercial wall-mounted fold-down tables but is fully customizable and printable on a Bambu P2S printer.

## Design Features

### Structural Features
- **Reinforced wall bracket** with triangular bracing for maximum strength
- **Smooth swivel mechanism** using a bearing sleeve and friction rings
- **Strong table surface** with raised edges and extensive internal reinforcement
- **Modular design** - all parts fit on Bambu P2S print bed (240x256mm)
- **Adjustable friction** using optional friction washers

### Weight Capacity
The table is designed to hold several pounds when properly mounted:
- Estimated capacity: 5-10 lbs (2-5 kg) depending on material and print quality
- PETG recommended for maximum strength
- Proper wall mounting is critical for weight capacity

## Parts List

### 3D Printed Parts
1. **Wall Bracket** (wall_bracket_v2.stl) - 80×50×150mm
   - Mounts to wall with 3 screws
   - Contains swivel post socket

2. **Swivel Post** (swivel_post_v2.stl) - 50×50×130mm
   - Rotates in bearing sleeve
   - Attaches to table top

3. **Table Top** (table_top_v2.stl) - 200×150×31mm
   - Main surface with raised edge
   - Heavy internal reinforcement

4. **Bearing Sleeve** (bearing_sleeve_v2.stl) - 28.5×28.5×33mm
   - Provides smooth rotation interface
   - Includes grease channels

5. **Friction Washers** (friction_washer.stl) - 31×31×2mm (optional)
   - Print 1-3 for adjustable friction
   - Prevents table from rotating on its own

### Required Hardware
- **3x M4×40mm screws** - Wall mounting (adjust length based on wall type)
- **3x M4 wall anchors** - For drywall/plaster (or appropriate for your wall type)
- **8x M4×12mm screws** - Table to swivel post attachment
- **Washers** - M4 washers for screw connections
- **Grease/Lubricant** (optional) - For smoother rotation

### Recommended Tools
- Drill with 5mm bit
- Screwdriver (Phillips or hex, depending on screw choice)
- Level
- Stud finder (if mounting to studs)
- 3D printer filament (PETG or PLA)

## Print Settings

### Recommended Settings
- **Material**: PETG (preferred) or PLA
- **Layer Height**: 0.2mm
- **Infill**: 40-50% for structural parts, 20-30% for table top
- **Wall Count**: 4-5 perimeters
- **Top/Bottom Layers**: 5-6 layers
- **Supports**: Required for wall bracket's swivel socket area
- **Bed Adhesion**: Brim or raft recommended for larger parts

### Print Order Priority
1. Wall Bracket (longest print, most important)
2. Swivel Post
3. Bearing Sleeve
4. Table Top
5. Friction Washers (if needed)

### Estimated Print Times (approx.)
- Wall Bracket: 8-12 hours
- Swivel Post: 6-8 hours
- Table Top: 12-16 hours
- Bearing Sleeve: 1-2 hours
- Friction Washer: 15-30 minutes each

## Assembly Instructions

### Step 1: Prepare Parts
1. Remove all support material
2. Clean up any stringing or rough edges with sandpaper
3. Test fit swivel post into bearing sleeve (should be snug but able to rotate)
4. If friction is too high, sand the swivel post lightly

### Step 2: Install Bearing Sleeve
1. Press bearing sleeve into wall bracket's socket
2. It should fit snugly (use rubber mallet if needed)
3. Optional: Apply lubricant to grease channels

### Step 3: Mount Wall Bracket
1. Mark desired height on wall (recommend 75-90cm/30-36" from floor)
2. Use level to mark 3 screw positions
3. If mounting to drywall:
   - Drill pilot holes
   - Install wall anchors
   - Screw bracket to wall with M4×40mm screws
4. If mounting to studs:
   - Locate studs
   - Drill pilot holes
   - Screw directly into studs (stronger)

### Step 4: Attach Table to Swivel Post
1. Position table top face-down on soft surface
2. Insert swivel post mounting plate into table bottom
3. Align screw holes (8 positions around circumference)
4. Install M4×12mm screws through table into mounting plate
5. Tighten evenly in star pattern

### Step 5: Install Swivel Assembly
1. Apply light lubricant to swivel post
2. Insert swivel post into bearing sleeve (taper-end first)
3. Push until fully seated
4. Test rotation - should be smooth but controlled

### Step 6: Adjust Friction (Optional)
1. If table rotates too easily, add friction washers
2. Place 1-3 washers between swivel post and bearing sleeve
3. Test rotation after each washer
4. Find optimal balance between smooth and controlled

## Usage Tips
1. **Break-in Period**: First few rotations may be stiff - this is normal
2. **Lubrication**: Reapply lubricant every 6-12 months for smooth operation
3. **Weight Distribution**: Place heavier items closer to wall for stability
4. **Rotation**: Use steady, controlled force - don't jerk or slam
5. **Maintenance**: Periodically check screws for tightness

## Design Specifications
- **Table Surface**: 200mm × 150mm (7.9" × 5.9")
- **Height from Wall**: ~120mm (4.7") when extended
- **Rotation**: 180+ degrees (limited by wall clearance)
- **Swivel Diameter**: 28mm
- **Wall Bracket Height**: 150mm

## Customization
The OpenSCAD source file (wall_swivel_table.scad) is fully parametric. You can modify:
- Table size (width/depth/thickness)
- Wall bracket dimensions
- Swivel post length
- Bearing clearances
- Screw hole sizes

## Troubleshooting

### Table won't rotate smoothly
- Sand swivel post lightly
- Apply lubricant
- Remove friction washers

### Table rotates too easily / won't stay in position
- Add friction washers
- Tighten mounting screws
- Check bearing sleeve is properly seated

### Parts don't fit together
- Check print scaling (should be 100%)
- Adjust clearance parameters in SCAD file
- Light sanding of tight spots

### Wall bracket feels weak
- Re-print with higher infill (50%+)
- Use PETG instead of PLA
- Ensure proper wall mounting (into studs if possible)

## Safety Notes
- ⚠️ **Maximum weight capacity**: 5-10 lbs depending on material and mounting
- ⚠️ **Proper wall mounting is critical** - use appropriate anchors for your wall type
- ⚠️ **Not suitable for heavy equipment** or climbing
- ⚠️ **Check mounting screws periodically** for loosening
- ⚠️ **Supervise children** when using the table

## Files Included
- `wall_swivel_table.scad` - OpenSCAD source file (fully parametric)
- `wall_bracket_v2.stl` - STL for wall bracket
- `swivel_post_v2.stl` - STL for swivel post
- `table_top_v2.stl` - STL for table top
- `bearing_sleeve_v2.stl` - STL for bearing sleeve
- `friction_washer.stl` - STL for optional friction washers
- `assembly_v2.stl` - STL of complete assembly (for visualization)
- `check_dimensions.py` - Python script to verify parts fit print bed

## License
This design is provided as-is for personal use. Modify and adapt as needed for your specific requirements.

## Version History
- **v2** (Current) - Improved reinforcement, better swivel mechanism, friction washers
- **v1** - Initial design

---

*Designed for Bambu P2S printer - March 2026*
