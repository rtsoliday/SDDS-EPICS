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
- **Indentation:** Two spaces per level; tabs are avoided.
- **Brace style:** Opening braces on the same line as function or control statements (K&R style).
- **Comments:** Each source file begins with a Doxygen-style block comment (`/** ... */`) documenting the file, authorship, and license. Functions also include Doxygen headers.
- **Naming:**
  - Public API functions use the `SDDS_` prefix followed by PascalCase (e.g., `SDDS_WriteAsciiPage`).
  - Internal helper functions and variables use camelCase (e.g., `bigBuffer`, `compute_average`).
  - Macros and constants are uppercase with underscores (e.g., `SDDS_SHORT`, `INITIAL_BIG_BUFFER_SIZE`).
- **File organization:**
  - C source files use `.c` extension; C++ utilities use `.cc`.
- **Header inclusion order:** Project headers (`"..."`) first, followed by standard library headers (`<...>`), then third-party headers.
- **Line length:** No strict maximum; some lines exceed 200 characters.

These guidelines should be followed when contributing new code or documentation to maintain consistency with the existing project.
