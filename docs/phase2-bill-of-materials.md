# Phase 2: Complete Bill of Materials (BoM)

## BoM Overview

All components selected for maximum availability through common suppliers (Amazon, AliExpress, McMaster-Carr, local hardware stores). Estimated total cost: **$180-220 USD** (excluding 3D printer filament).

---

## 1. Microcontroller & Electronics

| Component | Specification | Quantity | Unit Price | Supplier | Notes |
|-----------|--------------|----------|------------|----------|-------|
| ESP32 DevKit | ESP32-WROOM-32, 30-pin | 1 | $8 | Amazon/AliExpress | Main controller, WiFi optional for future app integration |
| TMC2209 Stepper Driver | V3.0, UART mode, heatsink included | 1 | $12 | Amazon | Silent stepper control, 2A peak current |
| HX711 Load Cell Amplifier | 24-bit ADC, 80Hz update rate | 1 | $3 | AliExpress | Digital load cell interface |
| Buck Converter Module | LM2596, 24V→5V, 3A output | 1 | $4 | Amazon | Powers ESP32 from 24V rail |
| Logic Level Shifter | 4-channel, bidirectional, 3.3V↔5V | 1 | $2 | Amazon | Interface ESP32 (3.3V) with 5V peripherals if needed |
| Prototype PCB | 70×90mm, solderable | 1 | $3 | Amazon | Clean wiring consolidation (optional) |
| Screw Terminals | 2-pin, 5mm pitch, panel mount | 5 | $5 | Amazon | Power connections |

**Total Electronics: ~$37**

---

## 2. Motors & Actuators

| Component | Specification | Quantity | Unit Price | Supplier | Notes |
|-----------|--------------|----------|------------|----------|-------|
| NEMA 17 Stepper Motor | 1.8° step angle, 1.5A, 42mm body | 1 | $12 | Amazon/StepperOnline | Gantry drive motor, 40Ncm holding torque |
| GT2 Pulley (Drive) | 20-tooth, 5mm bore, aluminum | 1 | $4 | Amazon | Mounts to NEMA 17 shaft |
| GT2 Idler Pulley | 20-tooth, 5mm bore, bearing-mounted | 1 | $5 | Amazon | Free-spinning tension pulley |
| SG90 Micro Servo | 9g, 180° rotation (optional) | 1 | $3 | Amazon | Optional: motorized rake tilt (can be replaced with fixed angle) |

**Total Motors/Actuators: ~$24**

---

## 3. Sensors

| Component | Specification | Quantity | Unit Price | Supplier | Notes |
|-----------|--------------|----------|------------|----------|-------|
| Load Cells | 50kg capacity, half-bridge strain gauge | 4 | $12 (set) | Amazon | Placed under each corner of litter bed platform |
| Mechanical Limit Switches | SPDT roller lever, Omron-style | 2 | $4 (pair) | Amazon | Home and far-end gantry limits |
| Resistors (Pull-up) | 10kΩ, 1/4W | 5 | $1 (pack) | Amazon | Switch debouncing |
| Capacitors (Decoupling) | 100nF ceramic | 5 | $1 (pack) | Amazon | Noise filtering |

**Total Sensors: ~$18**

---

## 4. Power Management

| Component | Specification | Quantity | Unit Price | Supplier | Notes |
|-----------|--------------|----------|------------|----------|-------|
| Power Supply | 24V, 3A (72W), desktop-style | 1 | $18 | Amazon | Powers stepper motor + buck converter |
| Power Switch | Rocker switch, 10A, 125V AC | 1 | $2 | Amazon | Main power cutoff |
| Fuse Holder + Fuse | Inline, 5A fast-blow | 1 | $3 | Amazon | Overcurrent protection |
| Wire (Power) | 18 AWG stranded, red/black | 10 ft | $5 | Amazon | High-current paths |
| Wire (Signal) | 22 AWG solid core, multi-color | 50 ft | $8 | Amazon | Control signals |
| Heat Shrink Tubing | Assorted sizes | 1 pack | $5 | Amazon | Insulation |

**Total Power Management: ~$41**

---

## 5. Hardware & Fasteners

### 5.1 Aluminum Extrusions

| Component | Specification | Quantity | Unit Price | Supplier | Notes |
|-----------|--------------|----------|------------|----------|-------|
| 2020 V-Slot Extrusion | 1000mm length, black anodized | 2 | $24 (pair) | Amazon/OpenBuilds | Gantry rails |
| 2020 Extrusion (standard) | 450mm length | 4 | $12 (set) | Amazon/OpenBuilds | Base frame crossbars |
| 2020 Extrusion (standard) | 150mm length | 4 | $6 (set) | Amazon/OpenBuilds | Vertical corner supports |

**Extrusion Subtotal: ~$42**

### 5.2 Linear Motion Components

| Component | Specification | Quantity | Unit Price | Supplier | Notes |
|-----------|--------------|----------|------------|----------|-------|
| V-Wheels | Delrin, 608ZZ bearings, 24mm OD | 4 | $12 (set) | Amazon/OpenBuilds | Carriage wheels for V-slot |
| Eccentric Spacers | 6mm bore, 0.5mm offset | 2 | $4 (pair) | Amazon/OpenBuilds | Adjustable wheel tension |
| GT2 Belt (6mm) | 2mm pitch, 1200mm length (or 5m roll) | 1 | $8 | Amazon | Closed-loop or cut-to-length |
| M5 Shaft (wheel axles) | 40mm length, precision ground | 4 | $6 (set) | Amazon/McMaster | Wheel mounting |

**Linear Motion Subtotal: ~$30**

### 5.3 Fasteners & Mounting Hardware

| Component | Specification | Quantity | Unit Price | Supplier | Notes |
|-----------|--------------|----------|------------|----------|-------|
| M5 T-Nuts | Drop-in style for 2020 extrusion | 50 | $8 | Amazon/OpenBuilds | Extrusion mounting |
| M5 Socket Head Bolts | 10mm, 15mm, 20mm lengths, assorted | 50 | $10 | Amazon | Frame assembly |
| M3 Socket Head Bolts | 8mm, 12mm, 16mm lengths, assorted | 50 | $6 | Amazon | 3D-printed part mounting |
| M3 Threaded Inserts | Heat-set, brass, 4mm OD × 5mm length | 30 | $8 | Amazon | Embedded in printed parts |
| Corner Brackets (90°) | Aluminum, 2020 profile | 8 | $12 | Amazon/OpenBuilds | Frame corners |
| Nylon Spacers | M3, 5mm length | 20 | $3 | Amazon | Clearance spacing |

**Fasteners Subtotal: ~$47**

---

## 6. Structural & Sealing Materials

| Component | Specification | Quantity | Unit Price | Supplier | Notes |
|-----------|--------------|----------|------------|----------|-------|
| Silicone Sheet | Food-grade, 3mm thick, 300×300mm | 1 | $12 | Amazon | Flap gate material |
| Plastic Piano Hinge | Continuous, 300mm length | 1 | $5 | Amazon | Flap hinge |
| Aluminum Bar Stock | 10×20mm cross-section, 300mm length | 1 | $6 | Amazon/McMaster | Flap weight bar |
| Silicone Adhesive | RTV silicone, 85g tube | 1 | $8 | Amazon | Bonding weight bar to flap |
| PETG Filament | 1kg spool | 1 | $22 | Amazon | 3D printing (primary) |
| PLA Filament (optional) | 1kg spool | 1 | $18 | Amazon | Alternative for non-structural parts |

**Structural/Sealing Subtotal: ~$71**

---

## 7. Custom 3D-Printed Parts

*All STL files generated from parametric OpenSCAD source (see `cad/` directory).*

**Print Time Estimates** (0.2mm layers, 40% infill, PETG):

| Part Name | Quantity | Print Time | Filament Used | Notes |
|-----------|----------|------------|---------------|-------|
| Gantry Carriage Block | 1 | 8 hours | 120g | Main carriage riding on V-wheels |
| Motor Mount Plate | 1 | 4 hours | 60g | NEMA 17 mounting with belt tensioning slots |
| Idler Pulley Mount | 1 | 2 hours | 30g | Bearing mount for belt idler |
| Rake Cartridge (Type A - Clay) | 1 | 6 hours | 90g | Fine-toothed clumping clay rake |
| Rake Cartridge (Type B - Pellet) | 1 | 5 hours | 75g | Wide-scoop pellet rake |
| Limit Switch Brackets | 2 | 1 hour each | 15g each | Mount switches to extrusion |
| Load Cell Platform Mounts | 4 | 1.5 hours each | 20g each | Corner mounts under litter bed |
| Drop Zone Guide Funnel | 1 | 3 hours | 45g | Directs waste to flap opening |
| Electronics Enclosure | 1 | 5 hours | 70g | Houses ESP32, drivers, buck converter |

**Total Print Time: ~40 hours** (can be batched)
**Total Filament: ~525g** (~$11 in PETG at $22/kg)

---

## 8. Litter Bed & Waste Collection

| Component | Specification | Quantity | Unit Price | Supplier | Notes |
|-----------|--------------|----------|------------|----------|-------|
| Litter Tray/Pan | Large plastic tray, 800×400×100mm | 1 | $15 | Amazon/Pet Store | Holds litter, sits on load cells |
| Waste Bin | 13-gallon (~50L) plastic bin with lid | 1 | $8 | Hardware Store | Waste collection under drop zone |
| Trash Bags | 13-gallon, drawstring | 1 box (50) | $10 | Grocery Store | Bin liners |
| Cat Litter (user-supplied) | Clumping clay, pellets, etc. | N/A | N/A | Pet Store | Not included in BoM |

**Litter/Waste Subtotal: ~$33**

---

## Total Bill of Materials Summary

| Category | Subtotal |
|----------|----------|
| Microcontroller & Electronics | $37 |
| Motors & Actuators | $24 |
| Sensors | $18 |
| Power Management | $41 |
| Hardware & Fasteners (extrusions, motion, fasteners) | $119 |
| Structural & Sealing Materials | $71 |
| Litter Bed & Waste Collection | $33 |
| **Grand Total** | **$343** |

*Note: 3D-printed parts are ~$11 in material but require ~40 hours print time.*

---

## Sourcing Notes

### Recommended Suppliers by Region

**United States:**
- **Amazon**: Most electronics, sensors, motors, hardware
- **OpenBuilds Store** (openbuildspartstore.com): V-slot extrusions, wheels, specialized hardware
- **StepperOnline** (omc-stepperonline.com): High-quality NEMA 17 motors
- **McMaster-Carr**: Precision fasteners, aluminum bar stock

**Europe:**
- **AliExpress**: Electronics, sensors (2-4 week shipping)
- **Ooznest** (ooznest.co.uk): V-slot extrusions and motion components
- **3DJake**: PETG filament, 3D printing supplies

**Asia-Pacific:**
- **AliExpress**: All electronics and motion components
- **Local hardware stores**: Extrusions (check for 2020 profile compatibility)

### Alternative Component Substitutions

**If NEMA 17 unavailable:** NEMA 14 can work with reduced torque (may require slower cleaning cycles)

**If TMC2209 unavailable:** A4988 stepper drivers work but are noisier (cats may be more startled)

**If V-slot unavailable:** Standard 2020 extrusion + linear rail MGN12 (increases cost by ~$30)

**If ESP32 unavailable:** Arduino Mega 2560 works (loses WiFi capability for future app integration)

**If load cells unavailable:** Pressure mat sensors (less precise, harder to calibrate)

### Cost Reduction Strategies

**Budget Version (~$180):**
- Omit WiFi features → use Arduino Nano ($5) instead of ESP32
- Use A4988 drivers ($4) instead of TMC2209
- Single rake cartridge (Type A only) → saves $3 in filament
- Simplified electronics enclosure (open-air mounting) → saves $2 filament

**Premium Version (~$280):**
- Add OLED display ($8) for status readout
- Add RGB LED strip ($12) for status indication
- Upgrade to NEMA 17 pancake stepper ($18) for quieter operation
- Acrylic or polycarbonate panels ($25) for full enclosure

---

## Tools Required (Not Included in BoM)

**Essential:**
- 3D printer (256×256×256mm minimum build volume)
- Hex key (Allen wrench) set (metric, 2mm-5mm)
- Wire strippers
- Soldering iron + solder
- Multimeter
- Adjustable wrench
- Screwdriver set (Phillips, flathead)

**Optional:**
- Heat gun (for threaded inserts)
- Digital calipers (QC of printed parts)
- Crimping tool (cleaner wire terminations)
- Label maker (wire identification)

---

## Consumables & Ongoing Costs

**Annual Maintenance Parts:**
- V-wheels (if worn): $12
- GT2 belt (if worn): $8
- Silicone flap (if torn): $12
- Trash bags (50 bags, ~1 per week): $20

**Annual Cost: ~$52** (~$4/month for materials, plus electricity ~$2/month)

---

## Safety Equipment (Recommended)

- Safety glasses (during assembly)
- Wire management clips (prevent pinch hazards)
- Cable strain relief (prevent wire fatigue failures)
- Thermal paste (for TMC2209 heatsink, usually included)

---

**Next**: See `phase4-electronics-wiring.md` for wiring diagrams and `phase5-firmware-design.md` for control logic.
