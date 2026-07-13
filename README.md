# SDDS-EPICS C/C++ Toolkit

SDDS-EPICS is a collection of command-line applications and libraries for
working with EPICS process variables and Self-Describing Data Set (SDDS)
files. The toolkit includes Channel Access (CA) and PVAccess (PVA) clients,
data loggers and monitors, save/restore and control applications, run-control
support, a distributed log daemon, and SDDS-configured PV servers.

The software is maintained by Accelerator Operations and Physics at the
Advanced Photon Source.

## Repository layout

- `SDDSepics/` — SDDS-based EPICS applications, including monitors, loggers,
  save/restore tools, control programs, and PV servers.
- `oagca/` — general CA/PVA command-line clients: `cavget`, `cavput`, and
  `cawait`.
- `rampload/` — ramp-loading utilities.
- `runcontrol/` — the run-control library and test client.
- `logDaemon/` — the distributed logging daemon, client library, and clients.
- `tests/` — pytest unit, command-line, regression, and local integration
  tests.
- `doc/` — LaTeX sources for the EPICS Toolkit, APS Run Control, and APS Log
  Daemon manuals.

Build products are placed in `bin/<OS>-<architecture>/` and
`lib/<OS>-<architecture>/`. For example, an Apple silicon build uses
`bin/Darwin-arm64/` and `lib/Darwin-arm64/`.

## Prerequisites

The build expects these source trees to be checked out next to one another:

```text
parent-directory/
├── SDDS/
├── SDDS-EPICS/
└── epics-base/
```

Clone and build [EPICS Base 7](https://github.com/epics-base/epics-base) for
the host architecture. Clone [SDDS](https://github.com/rtsoliday/SDDS) next to
this repository; the top-level SDDS-EPICS build builds the SDDS libraries it
requires.

The subdirectory makefiles search for EPICS Base in the following order:
`/usr/local/oag/base`, `../../epics-base`, `../../../../base`, and
`$HOME/epics/base`, with paths evaluated from each source subdirectory. The
adjacent layout above satisfies the `../../epics-base` search.

Additional platform requirements are:

- A C and C++ compiler, GNU Make, and the libraries required by SDDS and EPICS
  Base.
- On macOS, install LLVM `libomp` with MacPorts (`port install libomp`) or
  Homebrew (`brew install libomp`) when the SDDS libraries use OpenMP. The
  build detects the standard MacPorts, Apple silicon Homebrew, and Intel
  Homebrew locations.
- On Windows/Cygwin, clone [gsl](https://github.com/rtsoliday/gsl) next to this
  repository. The top-level makefile builds that copy.
- `sddspcas` is optional with EPICS Base 7. To build it, install and build the
  EPICS `pcas` module so that `casdef.h` and the PCAS libraries are either in
  EPICS Base or in a `pcas` directory next to `epics-base`. If PCAS is absent,
  the rest of the toolkit still builds and the makefile reports that
  `sddspcas` is disabled.

## Building

From the repository root:

```sh
make -j4
```

Use a suitable job count for the machine. A parallel build is supported. The
top-level build processes SDDS dependencies first, then builds `oagca`,
`rampload`, `runcontrol`, `logDaemon`, and `SDDSepics`.

To remove intermediate object directories while retaining installed products:

```sh
make clean
```

To also remove this platform's `bin/` and `lib/` output directories:

```sh
make distclean
```

## Testing

The test suite requires Python 3 and pytest. Install pytest in the selected
Python environment, then run:

```sh
python3 -m pip install pytest
make test
```

`make test` builds the repository, the SDDS helper programs used by the tests,
and test-only program targets before running the suite. The integration tests
start temporary local CA servers and log-daemon processes, use dynamically
selected loopback ports, and clean them up afterward. The host must therefore
permit local UDP and TCP socket use.

To rerun pytest without rebuilding first:

```sh
python3 -m pytest tests
```

An unavailable optional `sddspcas` executable is reported as a skipped test,
not a test failure.

## CA and PVA selection

`cavget`, `cavput`, and `cawait` accept `-provider=ca` or `-provider=pva`; CA
is the default. Several programs in `SDDSepics/` select a provider per PV from
an SDDS `Provider` column or from provider qualifiers in their command-line
options. See the program's generated usage text or the
[EPICS Toolkit manual sources](doc/EPICStoolkit.tex) for its exact input
schema and options.

## SDDS-configured PV servers

- `sddsSoftIOC` reads one or more SDDS files, generates a temporary EPICS
  database, starts the EPICS Base `softIoc`, and removes generated files on
  exit. It is always built.
- `sddspcas` is an optional Portable Channel Access Server configured with the
  same basic SDDS input schema. It can also evaluate equations involving other
  PVs and can merge definitions submitted by later instances on the same host.

Both require a string `ControlName` column. Optional columns describe the
record type, limits, units, and array length; `sddsSoftIOC` also supports enum
definitions through `EnumStrings`. See
[`doc/sddsSoftIOC.tex`](doc/sddsSoftIOC.tex) and
[`doc/sddspcas.tex`](doc/sddspcas.tex) for details.

## Portable macOS deployment

On macOS, the build copies the detected `libomp.dylib` into
`lib/Darwin-<architecture>/`, changes its install name to
`@rpath/libomp.dylib`, ad-hoc signs it, and links executables with a path
relative to their own location. When available, the LLVM runtime license is
copied beside it as `LICENSE.libomp.txt`.

Deploy the matching `bin` and `lib` directories together, preserving their
relative layout:

```text
deployment-root/
├── bin/Darwin-arm64/<programs>
└── lib/Darwin-arm64/
    ├── libomp.dylib
    └── LICENSE.libomp.txt
```

The destination Mac does not need MacPorts or Homebrew for `libomp`. Binaries
remain architecture-specific and may still depend on operating-system
compatibility and any other non-system dynamic libraries shown by `otool -L`.

## Building the manuals

The manuals have a separate toolchain. From the repository root:

```sh
make -C doc check-deps
make -C doc
```

This produces PostScript, PDF, and HTML versions of three manuals. See
[`doc/README.md`](doc/README.md) for package requirements and output names.

## Maintaining `pvaSDDS`

`pvaSDDS.cc` and `pvaSDDS.h` are intentionally present in `SDDSepics/`,
`oagca/`, and `runcontrol/` so each directory can be built independently. A
change to this shared implementation must be copied to all three locations.
Verify the copies before committing:

```sh
cmp SDDSepics/pvaSDDS.cc oagca/pvaSDDS.cc
cmp SDDSepics/pvaSDDS.cc runcontrol/pvaSDDS.cc
cmp SDDSepics/pvaSDDS.h oagca/pvaSDDS.h
cmp SDDSepics/pvaSDDS.h runcontrol/pvaSDDS.h
```

## License and authors

This software is distributed under the [Software License Agreement](LICENSE).
The principal authors are M. Borland, L. Emery, C. Saunders, R. Soliday, and
H. Shang.

Additional SDDS documentation is available from the
[Advanced Photon Source](https://www.aps.anl.gov/Accelerator-Operations-Physics/Documentation).
