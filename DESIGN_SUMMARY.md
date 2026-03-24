# Wall-Mounted Swivel Table - Design Summary

## What This Design Looks Like

### Overall Assembly
The wall-mounted swivel table consists of a vertical wall bracket that mounts securely to the wall. A swivel post rotates within a bearing sleeve, allowing a horizontal table surface to swing in and out from the wall. When not in use, the table can be rotated to sit parallel to the wall, taking up minimal space.

### Dimensions Summary
- **Table Surface**: 200mm x 150mm (approx 8" x 6")
- **Table Reach from Wall**: ~120mm (4.7") when extended
- **Wall Bracket Height**: 150mm (6")
- **Total Assembled Height**: ~180mm including table thickness

### Component Breakdown

#### 1. Wall Bracket (80×50×150mm)
- Vertical mounting plate with 3 screw holes for wall attachment
- Triangular reinforcement ribs for strength
- Socket at top for bearing sleeve
- Thick base for stability

#### 2. Swivel Post (50×50×130mm)
- Cylindrical post that rotates in bearing
- Friction rings along length for controlled movement
- Large mounting plate at top for table attachment
- 8 mounting points with reinforcing ribs

#### 3. Table Top (200×150×31mm)
- Rectangular surface with rounded corners
- Raised edge (6mm high) to prevent items from sliding off
- Extensive reinforcement underneath:
  - Cross-bracing in both directions
  - Perimeter frame
  - Central mounting boss
- 8 screw holes for attachment to swivel post

#### 4. Bearing Sleeve (28.5mm diameter, 33mm tall)
- Cylindrical sleeve that fits into wall bracket
- Grease channels for lubrication
- Provides smooth rotating interface

#### 5. Friction Washers (31mm diameter, 2mm thick)
- Optional components (print 1-3 as needed)
- Placed between swivel post and bearing to adjust rotation resistance
- Prevents unwanted movement under load

## How It Works

### Swivel Mechanism
The swivel post fits through the bearing sleeve, which is press-fit into the wall bracket. Friction rings on the swivel post contact the inner surface of the bearing sleeve, creating controlled resistance. The tightness can be adjusted by adding friction washers.

### Load Distribution
Weight on the table surface is transferred through:
1. Table's internal reinforcement ribs
2. Central mounting boss and 8 mounting screws
3. Swivel post (compression and bending)
4. Bearing sleeve
5. Wall bracket socket
6. Wall bracket main plate
7. 3 wall mounting screws into wall

### Usage Pattern
1. **Stowed Position**: Table rotated parallel to wall (minimal protrusion)
2. **Extended Position**: Table rotated 90° perpendicular to wall for use
3. **Rotation**: Smooth 180°+ rotation with controlled friction

## Material Recommendations

### PETG (Recommended)
- Higher strength and toughness
- Better layer adhesion
- More resistant to impact
- Slightly flexible under load (good for this application)
- Recommended for all structural parts

### PLA (Acceptable)
- Easier to print
- More rigid (can be brittle)
- Lower temperature resistance
- Suitable for light-duty use only
- Consider PETG for wall bracket and swivel post at minimum

## Print Considerations

### Critical Parts
1. **Wall Bracket**: Must be strong - use 50% infill, 5 perimeters
2. **Swivel Post**: Key structural element - use 40% infill, 5 perimeters
3. **Table Top**: Can use less infill (30%) due to extensive internal structure

### Support Requirements
- **Wall Bracket**: Needs support for swivel socket (overhanging cylinder)
- **Swivel Post**: May need light support for friction rings
- **Table Top**: Print face-down, minimal supports needed
- **Bearing Sleeve**: No supports needed (print vertically)

## Assembly Time
- Part preparation: 30 minutes
- Wall bracket installation: 15-30 minutes
- Table assembly: 15 minutes
- Final installation: 10 minutes
**Total**: ~1.5-2 hours including wall mounting

## Design Iterations
This is version 2 of the design with the following improvements over v1:
- Increased wall bracket thickness (10mm → 12mm)
- Better reinforcement ribs (triangular instead of simple)
- Improved swivel mechanism with friction rings
- Stronger table mounting interface (8 screws instead of 3)
- Better table reinforcement structure
- Added friction washers for adjustability
- Grease channels in bearing sleeve
- Tapered swivel post for easier assembly

## Comparison to Commercial Products
Similar to the Amazon-inspired wall-mounted fold-down tables, but advantages include:
- ✅ Fully customizable dimensions
- ✅ Open-source and free
- ✅ Repairable (print replacement parts)
- ✅ Can be modified for specific uses
- ⚠️ Lower weight capacity than metal versions
- ⚠️ Requires proper 3D printer and skills

## Use Cases
Perfect for:
- Small apartments with limited space
- Workshop tool holder
- Bedside table in small bedrooms
- Kitchen prep surface
- Laptop stand
- Display shelf for decorative items
- Plant stand
- Entry table for keys/mail

Not suitable for:
- Heavy equipment (>10 lbs)
- Permanent heavy storage
- Safety-critical applications
- Outdoor use (unless weatherproof filament used)

## Files Generated
All STL files are ready to print:
1. `wall_bracket_v2.stl` - Main wall mount
2. `swivel_post_v2.stl` - Rotating post
3. `table_top_v2.stl` - Table surface
4. `bearing_sleeve_v2.stl` - Rotation interface
5. `friction_washer.stl` - Optional friction adjustment
6. `assembly_v2.stl` - Complete assembly for visualization

Source file for customization:
- `wall_swivel_table.scad` - Fully parametric OpenSCAD file

## Rendering
The design has been rendered to STL format suitable for:
- Import into slicing software (Bambu Studio, PrusaSlicer, Cura, etc.)
- Verification in STL viewers
- 3D printing directly

All parts have been verified to fit on Bambu P2S print bed (240x256mm).
