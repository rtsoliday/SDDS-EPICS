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

## Operational Security for Automated Tests and Diagnostics

Development and testing occur on monitored laboratory systems. Commands that
combine temporary executable content, runtime injection, wrapper scripts, or
headless process automation may resemble malicious activity and trigger
security alerts even when used for legitimate debugging.

### Default requirements

- Prefer existing repository build and test targets over ad hoc shell workflows.
- Run commands as separate, directly attributable steps. Do not combine
  compilation, permission changes, execution, log collection, and cleanup into
  one `bash -lc` command.
- Place generated executables and shared libraries in the repository's
  documented build or test-artifact directory, not in `/tmp`.
- Do not create executable wrapper scripts dynamically. Prefer checked-in test
  helpers, direct argument-list process execution, or purpose-built test
  binaries.
- Do not apply `chmod +x` to generated content unless the user has explicitly
  approved it. Checked-in scripts should carry their executable bit in Git.
- Prefer supported headless facilities, such as `QT_QPA_PLATFORM=offscreen`,
  over additional display or process wrappers when they satisfy the test.
- Preserve commands, source paths, output paths, and test names in test logs so
  activity can be readily attributed to this repository.

### Security-sensitive techniques

Codex must not introduce or execute any of the following without explicit user
approval in the current conversation:

- `LD_PRELOAD`, `DYLD_INSERT_LIBRARIES`, or other runtime library injection
- `ptrace`, debugger attachment, syscall/API hooking, or symbol interposition
- generated executable content in `/tmp`, `/var/tmp`, or another shared
  temporary directory
- dynamically generated wrapper scripts that execute or replace another program
- privilege changes, capability changes, setuid/setgid behavior, or namespace
  manipulation
- disabling, bypassing, or testing endpoint security controls

Before requesting approval, Codex must explain:

1. why the technique is necessary;
2. which process and files it affects;
3. where generated artifacts will be stored;
4. what safer alternatives were considered;
5. the exact command or repository test target that will run; and
6. how artifacts will be retained or removed.

Approval for one command or test does not authorize unrelated uses of the same
technique.

### Design and test guidance

When a regression test appears to require runtime injection or executable
wrappers, first prefer one of these approaches:

1. Extract the relevant behavior into a directly testable function or component.
2. Add a narrow, documented test interface to the application.
3. Use a checked-in helper program built through the normal build system.
4. Run the security-sensitive integration test only through a named, opt-in
   target on an approved test host or CI runner.

Security-sensitive tests must be clearly named and excluded from routine local
test execution by default. Their documentation should state why the technique
is used and identify the expected processes and artifacts.

If a requested diagnostic cannot be completed without a security-sensitive
technique, stop and ask the user rather than constructing an ad hoc workaround.

## Machine-managed coding quickstart
This supplement is generated from repository evidence and leaves the handwritten guidance above unchanged.

<!-- BEGIN MACHINE:summary -->
## Quick start
- Repository-local guidance is sufficient: start with `AGENTS.md`, `README.md`, `docs/`, build/test/config files, and the source tree.
- SDDS-EPICS is a collection of EPICS Channel Access applications and utilities built on the Self-Describing Data Set (SDDS) protocol. It provides tools for data acquisition, monitoring, logging, and analysis in accelerator and beamline control systems.
- Primary work areas: `doc`, `logDaemon`, `oagca`, `rampload`, `runcontrol`, `SDDSepics`.

## Read first
- `README.md`: Primary project overview and workflow notes
- `doc/README.md`: Supporting documentation with operational details
- `doc/Makefile`: Build system entry point or dependency manifest
- `logDaemon/Makefile`: Build system entry point or dependency manifest
- `oagca/Makefile`: Build system entry point or dependency manifest

## Build and test
- Documented setup/build commands: `make -j`, `make`.
- Detected build systems: GNU Make.
- Unknown: no test workflow evidence was found in the inspected files.
- Likely run commands or operator entry points: `./SDDS-EPICS`.

## Operational warnings
- Local checkout layout appears significant; avoid casual changes to sibling-repo assumptions or relative paths.
- Legacy compatibility paths are still present; confirm which mode is actually in use before changing defaults.
- Platform-specific dependency setup matters; do not assume one platform's build recipe carries over unchanged.

## Compatibility constraints
- Legacy components are still present; prefer the documented default path before switching to older modes.
- Cross-platform support exists, but platform-specific dependency setup matters.
- Build and runtime behavior likely depends on neighboring core toolkit checkouts.

## Related knowledge
- Repository-local documentation should be treated as authoritative.
- If a shared `llm-wiki/` directory is present in this workspace or parent folder, consult [the matching repo page](../llm-wiki/repos/SDDS-EPICS.md) for additional architectural context.
- If no shared wiki is present, continue using repository-local evidence only.
- If available, [the SDDS concept page](../llm-wiki/concepts/sdds.md) adds broader cross-repo context.
- If available, [the EPICS concept page](../llm-wiki/concepts/epics.md) adds broader cross-repo context.
- If present in this workspace, [the cross-repo map](../llm-wiki/insights/cross-repo-map.md) helps explain related repositories.
<!-- END MACHINE:summary -->
