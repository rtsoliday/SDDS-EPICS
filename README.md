# SDDS-EPICS C/C++ ToolKit

## Overview
SDDS-EPICS is a collection of EPICS Channel Access applications and utilities built on the Self-Describing Data Set (SDDS) protocol.  It provides tools for data acquisition, monitoring, logging, and analysis in accelerator and beamline control systems.

## Prerequisites
This repository relies on the following external projects:

- SDDS: https://github.com/rtsoliday/SDDS
- EPICS Base: https://github.com/epics-base/epics-base

## Repository Structure:
```
📦 SDDS-EPICS Repository
├── AGENTS.md        # Agent instructions and guidelines
├── Makefile         # Top-level build configuration
├── Makefile.build   # Build helpers
├── Makefile.rules   # Common make rules
├── doc/             # Documentation sources
├── logDaemon/       # Distributed logging daemon and client library
├── oagca/           # CA and PVA generic tools (cavget, cavput, cawait)
├── rampload/        # Ramp-loading utilities
├── runcontrol/      # EPICS run control integration
├── SDDSepics/       # SDDS-based EPICS applications (snapshot, monitor, logger, etc.)
├── LICENSE          # Licensing information
└── README.md        # Project overview
```

## Compilation
1. Clone the repository:
   ```sh
   git clone https://github.com/rtsoliday/SDDS-EPICS.git
   cd SDDS-EPICS
   ```
2. Build using `make`:
   ```sh
   make -j
   ```

## Notable Tools

- **sddspcas**: Portable Channel Access Server (PCAS) configured by SDDS input files. It creates and serves PVs defined by a `ControlName` column (with optional limits, units, element count, type, and equation support).
- **sddsSoftIOC**: Temporary EPICS Base `softIoc` wrapper compatible with the same SDDS input format as `sddspcas`. It generates an IOC `.db` at runtime, launches `softIoc`, and removes generated files on exit.

Detailed manual pages for both tools are included in the LaTeX documentation sources under `doc/` (see `doc/EPICStoolkit.tex`).

## License
This library is distributed under the **Software License Agreement** found in the `LICENSE` file.

## Authors
- M. Borland
- L. Emery
- C. Saunders
- R. Soliday
- H. Shang

## Acknowledgments
This project is developed and maintained by **[Accelerator Operations & Physics](https://www.aps.anl.gov/Accelerator-Operations-Physics)** at the **Advanced Photon Source** at **Argonne National Laboratory**.

For more details, visit the official **[SDDS documentation](https://www.aps.anl.gov/Accelerator-Operations-Physics/Documentation)**.
