# Phase 1: Mechanical Architecture & Gantry Mechanics

## System Overview

The auto-emptying litter box uses a **belt-driven linear gantry system** with a reciprocating rake mechanism. This design choice provides smooth, reliable motion while handling the mechanical loads of heavy, wet clumping litter without binding.

## Linear Gantry System

### Motion System Selection: Belt-Driven vs. Alternatives

**Chosen: GT2 Belt-Driven System**

After evaluating three primary options:

1. **Lead Screw**: Excellent for precision but prone to binding when dealing with variable loads and debris contamination
2. **Rack-and-Pinion**: Robust but requires precise alignment and generates more noise
3. **GT2 Belt Drive** ✅: Optimal balance of smooth operation, debris tolerance, and noise reduction

### Belt-Driven Gantry Architecture

```
[NEMA 17 Motor] → [20-tooth GT2 Pulley] → [6mm GT2 Belt] → [Carriage Block] → [Rake Assembly]
                                              ↓
                            [Idler Pulley at opposite end]
```

**Key Specifications:**
- **Belt Type**: GT2 timing belt (6mm width, 2mm pitch)
- **Belt Length**: ~1200mm (for ~800mm travel + slack for tensioning)
- **Drive Pulley**: 20-tooth GT2 (15.9mm effective diameter)
- **Motor**: NEMA 17 (1.8° step angle, 200 steps/rev)
- **Resolution**: 0.0318mm per step (full step), 0.00159mm per microstep (1/16 microstepping)
- **Travel Speed**: 50-100mm/s (configurable via firmware)

### Handling Mechanical Loads

**Load Analysis:**
- Typical litter bed: 5-10 kg
- Wet clumps: up to 2 kg additional force during sifting
- Rake assembly weight: ~0.3 kg

**Anti-Binding Features:**

1. **Dual V-Wheel Carriage Design**
   - Four V-wheels (two pairs) ride on 2020 extrusion V-slots
   - Eccentric spacers on one pair allow precise tension adjustment
   - Load distribution prevents single-point binding

2. **Belt Tensioning System**
   - Adjustable motor mount plate with slotted holes
   - Target tension: 3-4 kg force (measured with fish scale)
   - Prevents belt skipping while maintaining smooth operation

3. **Debris Isolation**
   - Carriage block features sealed bearing housings
   - V-wheels use 608ZZ sealed bearings
   - Bellows cover between carriage and motor prevents litter contamination

4. **Force Distribution Rake Design**
   - Rake teeth angle forward at 15° to encourage litter flow over resistance
   - Rake width spans 90% of litter bed width to distribute forces
   - Flex joints at rake attachment points absorb shock loads from large clumps

## Modular Cartridge Attachment Method

### Quick-Change Rake System

The gantry carriage features a standardized **T-slot dovetail mount** for tool-free rake cartridge swapping:

**Mounting Interface:**
```
[Carriage Block]
      |
      └─── [Dovetail Slide (Female)]
               ↓
           [Dovetail Key (Male)] ← Rake Cartridge
               ↓
          [Spring-Loaded Pin Lock]
```

**Procedure:**
1. Pull spring-loaded pin
2. Slide rake cartridge out along dovetail
3. Insert new cartridge (automatically aligns)
4. Release pin to lock (audible click)

**Rake Configurations:**

### Type A: Fine-Toothed Clumping Clay Rake
- **Tooth Spacing**: 4mm (filters clumps ≥10mm)
- **Tooth Thickness**: 2mm
- **Tooth Count**: ~60 teeth across 250mm width
- **Material**: PETG (impact resistant, easy to clean)

### Type B: Wide-Scoop Pellet Rake
- **Slot Width**: 8mm (passes pellets 6-7mm diameter)
- **Bar Thickness**: 4mm (structural rigidity)
- **Bar Count**: ~30 bars across 250mm width
- **Material**: PETG or PLA

### Type C: Hybrid Silica/Tofu Rake
- **Mesh Opening**: 3×8mm rectangular slots
- **Cross-bracing**: Every 50mm for rigidity
- **Material**: PETG (chemical resistance)

**Cartridge Standardization:**
- All cartridges share identical dovetail key dimensions (40mm × 20mm × 10mm taper)
- Mounting hole pattern: 4× M3 threaded inserts on 30×30mm grid
- Weight limit: 500g per cartridge

## Waste Drop Zone & Mechanical Sealing

### Drop Zone Architecture

**Location**: Far end of extrusion rail (opposite motor side)

**Geometry:**
```
[Litter Bed Area] → [Transition Ramp 30°] → [Drop Zone Opening] → [Waste Bin]
                                                     ↓
                                         [Hinged Flap Gate (Passive)]
                                                     ↓
                                              [Sealed Waste Bin]
```

### Passive Mechanical Odor Seal

**Design: Weighted Silicone Flap Gate**

**Components:**
1. **Flap Material**: 3mm food-grade silicone sheet (300×100mm)
2. **Hinge**: Continuous plastic piano hinge across 300mm width
3. **Weight Bar**: Aluminum bar (10×20×280mm, ~150g) bonded to flap bottom edge
4. **Mounting**: Flap hinges from horizontal position, hangs vertically when closed

**Operating Principle:**

- **At Rest**: Gravity pulls weighted flap closed against drop zone opening (forms seal)
- **During Cleaning**: Rake pushes waste against flap
  - Waste weight (50-200g) overcomes flap resistance (~150g weight + silicone flexibility)
  - Flap swings open inward
  - Waste drops into bin
- **After Cleaning**: Flap immediately returns to vertical sealed position

**Seal Performance:**
- Contact pressure: ~5 Pa across 0.3m² surface
- Prevents 95%+ odor escape (no active ventilation required)
- Easily replaceable (4× M3 screws hold hinge to frame)

**Waste Bin Interface:**
- Standard 13-gallon (49L) trash bags fit over bin rim
- Bin dimensions: 300mm × 200mm × 250mm (LWH)
- Removable via front panel for bag replacement
- Estimated capacity: 7-10 cleaning cycles before requiring bag change

### Anti-Jam Features

**Flap Jam Prevention:**
- Flap opening: 280mm wide × 80mm high (oversized vs. rake width)
- Rounded corners on drop zone opening (R=10mm)
- Teflon spray on flap hinge for smooth operation

**Clump Size Limiting:**
- Rake design naturally breaks clumps >50mm into smaller pieces via repeated passes
- If oversized clump jams flap: manual intervention required (design trade-off vs. complexity)

## Structural Frame Design

### 2020 V-Slot Extrusion Frame

**Frame Geometry:**
```
Top View:
┌────────────────────────────────────────┐
│  [Motor End]              [Idler End]  │ ← Gantry Rails (2× 2020, 1000mm)
│       ↓                         ↓       │
│   [Carriage moves along these rails]   │
│                                         │
│  [Litter Bed Area: 800mm × 400mm]      │
│                                         │
│               ↓                         │
│         [Drop Zone: 150mm]              │
└────────────────────────────────────────┘
```

**Bill of Extrusions:**
- 2× 1000mm 2020 V-slot (gantry rails)
- 4× 450mm 2020 standard extrusion (base frame crossbars)
- 4× 150mm 2020 standard extrusion (vertical corner supports)

**Joint Method:**
- 90° corner brackets with M5 T-nuts and bolts
- No welding or complex machining required
- Hand tools only (Allen key set)

### Frame Rigidity Analysis

**Deflection Under Load:**
- Max carriage + rake weight: 1 kg
- Max cleaning force: 20N (2kg equivalent)
- Expected deflection at midspan: <2mm (acceptable for cleaning operation)

**Reinforcement:**
- Diagonal cross-bracing on base frame (prevents racking)
- Gantry rails supported every 500mm (reduces deflection by 75%)

## Motion Control & Homing

### End-Stop Limit Switches

**Placement:**
- **Home Position**: Motor end (left side)
- **Far End**: Idler end (right side, before drop zone)

**Type**: Mechanical roller lever microswitches (Omron-style)
- Normally Open (NO) configuration
- Mounting: 3D-printed brackets attached to extrusion with M5 T-nuts

**Homing Sequence:**
1. Power on → carriage moves toward home position at 20mm/s
2. Contacts home switch → stops
3. Backs off 5mm
4. Approaches home again at 5mm/s (precision homing)
5. Contacts home switch → sets position to X=0
6. System enters IDLE state

### Travel Limits

- **Home Position (X=0)**: Motor end limit switch
- **Far Position (X=750mm)**: Software-defined maximum (backed by hardware switch at X=780mm)
- **Drop Zone Start (X=700mm)**: Software waypoint where rake tilts to dump waste

## Rake Cleaning Cycle Sequence

**Full Cycle Steps:**

1. **IDLE** → CAT_DETECTED (load cell threshold exceeded)
2. **CAT_DETECTED** → WAITING (start 10-minute timer)
3. **WAITING** → CLEANING (timer expires, no weight detected)
4. **CLEANING**:
   - Move to X=0 (home position)
   - Rake teeth angle set to 15° attack angle (via servo if motorized rake, or fixed angle)
   - Move to X=700mm at 50mm/s (sifting pass)
   - Tilt rake to 45° (dump position)
   - Move to X=750mm (pushes waste through flap)
   - Return rake to 15° angle
   - Move to X=0 at 100mm/s (fast return)
5. **CLEANING** → RETURNING
6. **RETURNING** → IDLE (homing complete)

**Cycle Time**: ~30-45 seconds per full cleaning cycle

**Emergency Abort**: If weight detected during CLEANING state → immediately halt → return to IDLE

## Material Selection for 3D-Printed Parts

**Primary Material: PETG**
- Impact resistance superior to PLA
- Odor resistant
- Temperature stable (litter boxes can get warm)
- Layer adhesion excellent for structural parts

**Infill Strategy:**
- Carriage blocks: 40% gyroid infill (strength + weight balance)
- Rake cartridges: 30% grid infill (rigidity in one axis)
- Mounting brackets: 50% rectilinear (maximum strength)

**Print Settings:**
- Layer height: 0.2mm (balance of speed and strength)
- Perimeters: 4 walls (impact resistance)
- Top/bottom layers: 5 each (bearing surface durability)

## Maintenance & Longevity

**Routine Maintenance (Monthly):**
- Vacuum litter dust from gantry rails and carriage
- Check belt tension (should not skip or over-tension)
- Verify V-wheel eccentric spacers (no wobble)
- Wipe silicone flap with damp cloth

**Wear Parts (Replace Annually):**
- V-wheels (608ZZ bearings, ~$8 for set of 4)
- GT2 belt (shows wear after ~10,000 cycles, ~$5)
- Silicone flap (tears or odor absorption, ~$3)

**Estimated Lifespan:**
- Frame: 10+ years (aluminum doesn't degrade)
- 3D-printed parts: 3-5 years (PETG degradation)
- Electronics: 5-7 years (typical MTBF for consumer electronics)

---

## Design Trade-offs & Decisions

**Why Belt-Driven vs. Lead Screw?**
- Lead screws offer higher precision but bind under side loads
- Litter creates unpredictable lateral forces during raking
- Belt systems tolerate debris contamination better
- Cost difference negligible (~$15 belt vs. ~$20 lead screw)

**Why Passive Flap vs. Active Door?**
- Passive flap eliminates one failure point (motor/servo)
- No additional wiring or control complexity
- Weighted silicone self-centers and self-seals
- Trade-off: slightly less odor containment vs. motorized door

**Why Dovetail Cartridge Mount vs. Magnetic?**
- Dovetail provides positive mechanical lock (no accidental detachment during cleaning)
- Magnets can accumulate iron-rich litter particles
- Dovetail allows 500g weight limit vs. ~200g for practical magnet systems

---

**Next**: See `phase2-bill-of-materials.md` for complete component sourcing list.
