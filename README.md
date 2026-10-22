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
and test-only program targets before running the suite. These include the legacy
readers `sddswgetdata`, `sddstransferFFtoWaveform`,
`sddstransferscalartowaveform`, `getinjectiontrajectory`, `makeboosterbump`,
`sddsSubtractDispersionOrbit`, `srinitbpmvector`, `srinitcorrvector`, and
`srinitvector`, plus the legacy `ramploadnew` utility. The integration tests
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

## SDDS6 and categorical data

Build SDDS-EPICS against the updated SDDS headers and libraries together. SDDS6
changes the SDDS layout structures, so existing executables must be rebuilt.
SDDS input parsing and enum dictionary preservation are provided by the SDDS
library; programs that copy input metadata retain enum columns, parameters,
and arrays with their dictionaries.

An SDDS enum stores signed 32-bit dictionary keys. These keys need not match
EPICS enum state indices. The following inputs resolve enum values to their
dictionary labels before sending them to EPICS:

- `sddswput`: enum `Waveform` columns.
- `sddscasr` and `sddspvasaverestore`: enum `ValueString` columns for scalar
  restores and enum `Waveform` columns. Quotes in enum labels are preserved
  literally. Save requests still require primitive string `ValueString` storage.
- `sddscaramp`: enum final-value columns. Categorical values are set as labels
  and are not interpolated during a numeric ramp.
- `rampload`: enum `ControlName` and `ControlType` parameters.
- `sddsgencontrolbits`: categorical labels and modes in preset and bunch-pattern
  tables, and textual parameters in raw RAM files (including per-receiver modes).
  Timing, count, and raw RAM bit-mask inputs retain their required numeric types.

CA string values must fit in the protocol's 40-byte string buffer, including
the terminating null byte. Longer labels are rejected by the CA restore and
ramp readers above. `sddspvasaverestore` also checks enum labels for CA
destinations before sending any restored values; PVA destinations retain their
existing string length support.

Numeric inputs, including ramp times, ramp setpoints, and optimization
matrices, remain numeric. Both feedback controllers reject enum columns in primary
and auxiliary response matrices instead of silently omitting their readbacks;
unrelated enum parameter metadata is accepted. Feedback filter `DeviceName` columns require strings;
filter coefficient columns reject enums instead of silently using default values.
Convert enums explicitly with `sddscast` when numeric codes are intended.
Other configuration fields continue to require the types
documented for each program. PV-name aliases (`ControlName`, `Device`,
`DeviceName`, and the optimizer's `controlPV`) reject enum storage before
selecting a fallback name column. This prevents a categorical name field from
silently selecting a different PV list. An explicit `sddscaramp`
`-controlNameColumn` selection still permits unrelated enum metadata.
The `sddslogger` units aliases `ReadbackUnits`,
`ReadbackUnit`, and `Units` require strings and reject enums before selecting
units from a fallback source. Optional fields also reject unsupported enums,
including scaling and dead bands, waveform lengths and name prefixes,
save/restore providers and validation settings, logger trigger thresholds and
array indices, optimization limits and orbit modes, permissive masks, and
feedforward exit policies. A supplied enum is reported as
a type error instead of selecting the default for an absent field.
Waveform scaling columns reject enums even when the companion offset column
is absent. Condition-file readers also check for failed extra-page reads,
including undefined enum keys, before using the collected conditions.
Raw magnet, feedback-offset, orbit-bump, vector,
measurement, and knob readers validate their expected storage types before using input
buffers, so enums in those fields are reported as type errors. Append-mode
timestamps use numeric conversions and reject categorical data. The scalar,
statistical, vector, and waveform monitors also reject enums in existing sample
columns and runtime parameters before opening the file for append, while
retaining unrelated enum parameter metadata. This includes the numeric
`Samples` parameter in statistical monitor files. Event and alarm loggers apply the
same checks to their primitive output fields. Waveform
readers report failed page reads separately from end-of-file. Waveform
restores validate the entire waveform file, including per-PV files, before
restoring any of the collected pages. Scalar restores also validate all later
snapshot pages before writing PVs, while using values from the first page.
Save/restore clients retain the waveform
file's element count and storage type even when the destination PV has a larger
capacity. One-element enum waveforms also restore their dictionary labels.
Control specification readers also reject
failed page reads before interpreting the page's buffers.
Both CA and PVA glitch loggers report invalid pages before starting logging.
Multi-page values files, control and signal lists, knob files, and waveform
specifications also distinguish invalid pages from end-of-file, so enum
validation errors cannot silently truncate the input.
Readers that use only the first page also report invalid data encountered while
checking for extra pages. This includes the `wmonitor` input style in `sddswget`
and `sddswgetdata`; `sddswget` preserves the first page's enum metadata while
checking the extra page, including when reading from standard input.
`sddspvtest` checks for these errors before connecting to the listed PVs;
its names, limits, and monitor flags retain their documented primitive types,
and unrelated enum metadata is accepted.
`sddssnapshot` and `sddscainfo` also report failed page reads, including undefined
enum keys, instead of silently producing successful partial results.
Collected EPICS values retain their existing output types; copying enum
metadata causes SDDS to select SDDS6 automatically. Waveform and image monitors
reject `enum` as a requested output storage type: they have no input dictionary
for assigning labels to the collected values.
`MV20image2sdds` requires a character pixel column and checks the parsed type,
including when an enum label contains text resembling a type declaration.

The distributed `logDaemon` also rejects enum columns in an existing SDDS log
before appending raw ASCII messages. Unrelated enum parameter and array metadata
is allowed; existing pages are validated before the file is opened for append.
`setbcorr` also validates an existing ramp file before appending raw ASCII pages.
It requires its own primitive ASCII page layout and rejects enum columns,
nonfixed enum parameters, and enum arrays before opening the file for append.
Additional fixed parameters, including enum metadata, are retained; existing
pages are checked before any ramp data is appended.

PV server registries require string `rec_name` columns; `-pcasPVFile` also
requires a string `host_name` column. Both servers validate these types before
reading name buffers, checking dynamic PV additions, appending entries, or
cleaning the registry on exit. An enum in a
name column is a type error; unrelated enum metadata remains valid for lookup.
A failed registry page read, including an undefined enum key, also prevents
dynamic PV additions.

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
These configuration columns retain their documented primitive types. An SDDS
enum in one of them is rejected instead of silently applying the default for an
absent optional column. Unrelated enum metadata is accepted.

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
