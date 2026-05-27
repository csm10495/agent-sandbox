# Open-Source Auto-Emptying Cat Litter Box

A fully open-source, DIY auto-emptying cat litter box designed for home manufacturing on standard CoreXY 3D printers (256×256×256mm build volume).

## Overview

This project implements a **Linear Gantry / Reciprocating Rake** mechanism using standard 2020 V-slot aluminum extrusions for the frame and 3D-printed components for joints, carriage blocks, motor mounts, and cleaning rake assemblies.

### Key Features

- ✅ **Universal Litter Compatibility**: Modular rake system supports clumping clay, silica gel, wood/pine pellets, and tofu litter
- ✅ **Auto-Detection**: Load cell-based cat presence detection with configurable cleaning delay (5-15 minutes)
- ✅ **Home Manufacturing**: All custom parts fit within 256×256×256mm build volume
- ✅ **Serviceable Design**: Standard grocery/trash bag disposal with mechanical odor seal
- ✅ **Fully Parametric**: OpenSCAD designs with adjustable parameters
- ✅ **Production Firmware**: ESP32-based with state machine and safety interrupts

## Project Structure

```
.
├── docs/               # Documentation and design specifications
│   ├── phase1-mechanical-architecture.md
│   ├── phase2-bill-of-materials.md
│   ├── phase4-electronics-wiring.md
│   └── phase5-firmware-design.md
├── cad/                # OpenSCAD parametric designs
│   ├── rake-cartridge.scad
│   └── gantry-carriage.scad
├── firmware/           # ESP32/Arduino production code
│   └── litter-box-controller/
│       └── litter-box-controller.ino
└── hardware/           # Hardware documentation
    └── wiring-diagram.md
```

## Quick Start

1. **Review Documentation**: Start with `docs/phase1-mechanical-architecture.md` for system overview
2. **Source Components**: Use `docs/phase2-bill-of-materials.md` to acquire all parts
3. **Print Components**: Open `.scad` files in OpenSCAD, adjust parameters if needed, export to STL
4. **Assemble Hardware**: Follow mechanical architecture guide and wiring diagrams
5. **Flash Firmware**: Upload `firmware/litter-box-controller/litter-box-controller.ino` to ESP32
6. **Configure**: Adjust cleaning delay and sensitivity via firmware constants

## Design Philosophy

This design prioritizes:
- **Accessibility**: Standard, readily available components
- **Manufacturability**: No specialized tools or large-format printers required
- **Serviceability**: Easy maintenance and bag replacement
- **Reliability**: Mechanical simplicity with proven linear motion systems
- **Safety**: Hardware interrupts prevent cat injury during cleaning cycles

## Manufacturing Constraints

- **Build Volume**: 256×256×256mm (optimized for Bambu Lab P2S and similar)
- **Frame**: Standard 2020 V-slot aluminum extrusion
- **Motion**: Belt-driven linear gantry with NEMA 17 stepper
- **Control**: ESP32 microcontroller with TMC2209 stepper drivers

## License

This project is fully open-source. See LICENSE file for details.

## Contributing

This is a complete design specification. Contributions for improvements, alternative rake designs, or enclosure variations are welcome.

## Safety Notice

⚠️ **Important**: This device contains moving parts. Ensure proper installation of safety features including:
- Weight-based emergency stop (cat re-entry detection)
- End-stop limit switches for gantry travel limits
- Proper motor current limiting to prevent mechanical damage

Never operate without safety systems enabled.
