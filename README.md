# SDDS-EPICS C/C++ ToolKit

## Overview
SDDS-EPICS is a collection of EPICS Channel Access applications and utilities built on the Self-Describing Data Set (SDDS) protocol.  It provides tools for data acquisition, monitoring, logging, and analysis in accelerator and beamline control systems.

## Prerequisites
This repository incorporates and extends the code from SDDS and EPICS :

- https://github.com/rtsoliday/SDDS
- https://github.com/epics-base

## Repository Structure:
```
📦 SDDS-EPICS Repository
├── logDaemon/       # distributed logging daemon and client library
├── runcontrol/      # EPICS run control integration
├── SDDSepics/       # SDDS-based EPICS applications (snapshot, monitor, logger, etc.)
├── doc/             # Documentation
├── LICENSE          # Licensing information
└── README.md        # This file
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
