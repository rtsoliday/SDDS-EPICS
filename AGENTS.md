# Agent Instructions

This repository contains the SDDS EPICS (Self Describing Data Sets) C/C++ tools.

## Project Layout

This repository is organized into the following top-level files and directories:

- `AGENTS.md`: agent instructions and guidelines.
- `README.md`: project overview, build prerequisites, and usage examples.
- `LICENSE`: licensing information.
- `Makefile`, `Makefile.build`, `Makefile.rules`: top-level build configuration.
- `doc/`: LaTeX sources and documentation files.
- `logDaemon/`: source code and libraries for the logging daemon.
- `oagca/`: source files for CA and PVA generic tools (e.g., cavget, cavput, cawait).
- `runcontrol/`: libraries and examples for run control via CA and PVA.
- `SDDSepics/`: SDDS EPICS tools and applications (C/C++ sources).

## Coding Conventions
- **Indentation:** Two spaces are used for each indentation level. Tabs are generally avoided.
- **Braces:** Opening braces are placed on the same line as function or control statements.
- **Comments:** Source files begin with Doxygen‑style block comments documenting the file, authorship, and licensing.
- **Naming:** Functions use camelCase with a lowercase initial letter (e.g., `moveToStringArray`), while macros and constants are uppercase with underscores (e.g., `COLUMN_BASED`).
- **Headers:** Standard C headers appear after project headers in `#include` lists.
- **Line Length:** No strict maximum line length is enforced; some lines exceed 200 characters.

These guidelines should be followed when contributing new code or documentation to maintain consistency with the existing project.
