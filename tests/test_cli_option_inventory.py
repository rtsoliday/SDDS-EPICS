"""Repository-wide command-line option inventory and parser regression tests."""

from __future__ import annotations

import os
import re
from pathlib import Path

import pytest

from conftest import REPOSITORY_ROOT, _binary_directory, _invoke


OPTION_ARRAY = re.compile(
  r"(?:char\s*\*|const\s+char\s*\*)\s*"
  r"(?:commandline_option|option|options)\s*\[[^]]*\]\s*=\s*\{(.*?)\};",
  re.IGNORECASE | re.DOTALL,
)
STRING_LITERAL = re.compile(r'"([^"\\]*(?:\\.[^"\\]*)*)"')
RUNCONTROL_OPTIONS = {"runcontrolpv", "runcontroldescription"}

# sddsbcontrol is intentionally absent: the whole program is a hard-coded
# run-control client, which is outside the testable scope of this repository.
SDDS_EPICS_PROGRAMS = {
  "BPS16monitor",
  "MV20image2sdds",
  "degauss",
  "makesrbump",
  "sdds2dfeedforward",
  "sddsSoftIOC",
  "sddsalarmlog",
  "sddscainfo",
  "sddscaramp",
  "sddscasr",
  "sddscontrollaw",
  "sddsexperiment",
  "sddsfeedforward",
  "sddsgencontrolbits",
  "sddsglitchlogger",
  "sddsimagemonitor",
  "sddslogger",
  "sddslogonchange",
  "sddslogTurnByTurnDAQ",
  "sddsmonitor",
  "sddsoptimize",
  "sddspcas",
  "sddspermissive",
  "sddspvacontrollaw",
  "sddspvaglitchlogger",
  "sddspvalogger",
  "sddspvasaverestore",
  "sddspvtest",
  "sddssnapshot",
  "sddsstatmon",
  "sddssynchlog",
  "sddsvexperiment",
  "sddsvmonitor",
  "sddswget",
  "sddswmonitor",
  "sddswput",
  "squishPVs",
  "standardize",
  "toggle",
}
OAGCA_PROGRAMS = {"cavget", "cavput", "cawait"}
RAMPLOAD_PROGRAMS = {"rampload", "setbcorr"}

# These files contain standalone programs, but are intentionally absent from
# the production Makefile. Keeping the list explicit prevents a newly added
# main() from being mistaken for covered production code.
LEGACY_UNBUILT_SDDS_EPICS_PROGRAMS = {
  "getinjectiontrajectory",
  "makeboosterbump",
  "sddsSubtractDispersionOrbit",
  "sddsboosterbump",
  "sddscaplayback",
  "sddstransferFFtoWaveform",
  "sddstransferscalartowaveform",
  "sddswgetdata",
  "setsrcorrmode",
  "srinitbpmvector",
  "srinitcorrvector",
  "srinitvector",
}


def _source_for(program: str) -> Path:
  directory = "oagca" if program in OAGCA_PROGRAMS else "SDDSepics"
  for extension in (".c", ".cc"):
    source = REPOSITORY_ROOT / directory / f"{program}{extension}"
    if source.is_file():
      return source
  raise AssertionError(f"no source file found for production program {program}")


def _declared_options(program: str) -> list[str]:
  source = _source_for(program).read_text(encoding="utf-8", errors="replace")
  match = OPTION_ARRAY.search(source)
  if not match:
    return []
  return STRING_LITERAL.findall(match.group(1))


def _makefile_products(path: Path) -> set[str]:
  contents = re.sub(r"\\\n\s*", " ", path.read_text(encoding="utf-8"))
  products: list[str] = []
  for operator, value in re.findall(r"^\s*PROD\s*(\+?=)\s*([^\n#]*)", contents, re.MULTILINE):
    names = re.findall(r"\b[A-Za-z][A-Za-z0-9]*\b", value)
    products = products + names if operator == "+=" else names
  return set(products)


def _program_sources(path: Path) -> set[str]:
  programs = set()
  for source in (*path.glob("*.c"), *path.glob("*.cc")):
    contents = source.read_text(encoding="utf-8", errors="replace")
    if re.search(r"\bmain\s*\(", contents):
      programs.add(source.stem)
  return programs


ARRAY_PARSER_PROGRAMS = sorted(
  (SDDS_EPICS_PROGRAMS - {"degauss", "standardize"}) | OAGCA_PROGRAMS
)
OPTION_CASES = [
  (program, option)
  for program in ARRAY_PARSER_PROGRAMS
  for option in _declared_options(program)
  if option.lower() not in RUNCONTROL_OPTIONS
]

# These options are known to be recognized, but their no-value behavior is
# intentionally identical to the program's unknown-option result. Their valid
# forms have dedicated integration coverage elsewhere in the suite.
INDISTINGUISHABLE_NO_VALUE_OPTIONS = {
  ("cawait", "nowarnings"),
  ("cawait", "repeat"),
  ("sddscainfo", "makeLoggerInput"),
  ("sddscainfo", "makeSCRInput"),
  ("sddscainfo", "pipe"),
  ("sddscainfo", "verbose"),
  ("sddspermissive", "verbose"),
}


def _probe_environment() -> dict[str, str]:
  environment = os.environ.copy()
  environment.update(
    {
      "EPICS_CA_ADDR_LIST": "127.0.0.1:1",
      "EPICS_CA_AUTO_ADDR_LIST": "NO",
      "EPICS_PVA_ADDR_LIST": "127.0.0.1:1",
      "EPICS_PVA_AUTO_ADDR_LIST": "NO",
      "XDG_RUNTIME_DIR": "/tmp",
    }
  )
  return environment


def _program_path(program: str) -> Path:
  suffix = ".exe" if os.name == "nt" else ""
  return _binary_directory() / f"{program}{suffix}"


def _signature(result) -> tuple[int, str, str]:
  return result.returncode, result.stdout, result.stderr


@pytest.fixture(scope="module")
def unknown_option_signatures() -> dict[str, tuple[int, str, str]]:
  arguments = [f"/tmp/sdds-epics-cli-probe-{index}" for index in range(5)]
  signatures = {}
  for program in ARRAY_PARSER_PROGRAMS:
    executable = _program_path(program)
    if not executable.is_file():
      if program == "sddspcas":
        continue
      pytest.fail(f"required test executable was not built: {executable}")
    result = _invoke(
      [str(executable), "-definitelyInvalidOption", *arguments],
      _probe_environment(),
      timeout=2,
    )
    signatures[program] = _signature(result)
  return signatures


def test_option_inventory_is_not_empty() -> None:
  """Guard against a source-parser regression silently collecting no cases."""

  assert len(OPTION_CASES) > 500


def test_every_array_parser_program_exposes_options() -> None:
  """A declaration-style change must not silently remove a whole program."""

  assert {program for program in ARRAY_PARSER_PROGRAMS if not _declared_options(program)} == set()


def test_production_program_inventory_matches_makefiles() -> None:
  """A newly built CLI must be added to this coverage inventory explicitly."""

  sdds_products = _makefile_products(REPOSITORY_ROOT / "SDDSepics" / "Makefile")
  assert SDDS_EPICS_PROGRAMS == sdds_products - {"sddsbcontrol"}
  assert OAGCA_PROGRAMS == _makefile_products(REPOSITORY_ROOT / "oagca" / "Makefile")
  assert {"logDaemon", "logMessage", "testLogger"} == _makefile_products(
    REPOSITORY_ROOT / "logDaemon" / "Makefile"
  )
  assert RAMPLOAD_PROGRAMS == _makefile_products(REPOSITORY_ROOT / "rampload" / "Makefile")


def test_every_standalone_program_is_classified() -> None:
  """Inventory supported and deliberately unbuilt main programs separately."""

  assert _program_sources(REPOSITORY_ROOT / "SDDSepics") == (
    SDDS_EPICS_PROGRAMS | {"sddsbcontrol"} | LEGACY_UNBUILT_SDDS_EPICS_PROGRAMS
  )
  assert _program_sources(REPOSITORY_ROOT / "oagca") == OAGCA_PROGRAMS
  assert _program_sources(REPOSITORY_ROOT / "logDaemon") == {
    "logDaemon",
    "logMessage",
    "testLogger",
  }
  assert _program_sources(REPOSITORY_ROOT / "rampload") == RAMPLOAD_PROGRAMS | {
    "ramploadnew",
  }


def test_runcontrol_options_are_inventoried_but_not_executed() -> None:
  """Make the deliberate run-control exclusion visible and reviewable."""

  excluded = {
    option.lower()
    for program in ARRAY_PARSER_PROGRAMS
    for option in _declared_options(program)
    if option.lower() in RUNCONTROL_OPTIONS
  }
  assert excluded == RUNCONTROL_OPTIONS


@pytest.mark.parametrize(("program", "option"), OPTION_CASES)
def test_every_declared_option_has_a_safe_parser_path(
  program: str,
  option: str,
  unknown_option_signatures: dict[str, tuple[int, str, str]],
) -> None:
  """Every option must reject a missing value safely or continue parsing."""

  executable = _program_path(program)
  if not executable.is_file():
    if program == "sddspcas":
      pytest.skip("sddspcas is disabled because the optional PCAS module is unavailable")
    pytest.fail(f"required test executable was not built: {executable}")

  arguments = [f"/tmp/sdds-epics-cli-probe-{index}" for index in range(5)]
  result = _invoke(
    [str(executable), f"-{option}", *arguments],
    _probe_environment(),
    timeout=2,
  )

  assert result.returncode >= 0, (
    f"{program} -{option} terminated from signal {-result.returncode}\n"
    f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
  )
  if (program, option) not in INDISTINGUISHABLE_NO_VALUE_OPTIONS:
    assert _signature(result) != unknown_option_signatures[program], (
      f"{program} -{option} followed the unknown-option path; "
      "the option inventory or parser dispatch may be stale"
    )


@pytest.mark.parametrize("program", ["degauss", "standardize"])
@pytest.mark.parametrize(
  "arguments",
  [
    ("-q",),
    ("-query",),
    ("-c",),
    ("-configure",),
    ("-s",),
    ("-stop",),
    ("-p", "1"),
    ("-pend", "1"),
    ("-pendIoTime", "1"),
    ("--",),
  ],
)
def test_legacy_magnet_program_option_aliases(program: str, arguments: tuple[str, ...]) -> None:
  """Cover the hand-written parsers and every documented alias."""

  result = _invoke(
    [str(_program_path(program)), *arguments, "/tmp/nonexistent-magnet-specification.sdds"],
    _probe_environment(),
  )

  assert result.returncode > 0
  assert "invalid option" not in result.stderr.lower()


@pytest.mark.parametrize("mode", ["text", "SDDS"])
def test_logdaemon_accepts_every_command_line_option(mode: str, tmp_path: Path) -> None:
  """Use -e as a side-effect-free terminator after parsing all daemon options."""

  if not _program_path("logDaemon").is_file():
    pytest.skip("logDaemon is not built on this platform")
  result = _invoke(
    [
      str(_program_path("logDaemon")),
      "-m",
      mode,
      "-i",
      "pytest",
      "-f",
      str(tmp_path / "log.config"),
      "-p",
      "25001",
      "-r",
      "-s",
      "1024",
      "-o",
      str(tmp_path / "saved"),
      "-h",
      str(tmp_path),
      "-e",
    ],
    _probe_environment(),
  )

  assert result.returncode == 0
  assert "Example log.config file" in result.stdout


@pytest.mark.parametrize(
  ("program", "arguments", "expected"),
  [
    ("logDaemon", ("-m", "invalid"), None),
    ("logMessage", ("-sourceId=test", "-tag=value", "x", "-invalid"), "invalid option"),
    ("logMessage", ("-serviceId=", "-sourceId=test", "-tag=value", "x"), "serviceId string is empty"),
    ("logMessage", ("-serviceId=test", "-sourceId=", "-tag=value", "x"), "sourceId string is empty"),
    ("logMessage", ("-serviceId=test", "-sourceId=test", "-tag=", "x"), "tag name string is empty"),
    ("testLogger", ("-serviceId=",), "Usage:"),
    ("testLogger", ("-invalid",), "Usage:"),
  ],
)
def test_log_client_option_validation(
  program: str,
  arguments: tuple[str, ...],
  expected: str | None,
) -> None:
  if not _program_path(program).is_file():
    pytest.skip(f"{program} is not built on this platform")
  result = _invoke([str(_program_path(program)), *arguments], _probe_environment())

  assert result.returncode != 0
  if expected:
    assert expected in result.stderr


@pytest.mark.parametrize("program", ["sddsfeedforward", "sdds2dfeedforward"])
def test_feedforward_control_log_qualifier_validation(program: str) -> None:
  invalid = _invoke(
    [
      str(_program_path(program)),
      "-controlLog=/tmp/feedforward-control-log,invalid",
      "/tmp/input.sdds",
      "/tmp/output.sdds",
    ],
    _probe_environment(),
  )
  valid_abbreviation = _invoke(
    [
      str(_program_path(program)),
      f"-controlLog=/tmp/{'x' * 100},log",
      "/tmp/input.sdds",
      "/tmp/output.sdds",
    ],
    _probe_environment(),
  )

  assert invalid.returncode != 0
  assert "controlLog syntax" in invalid.stderr
  assert valid_abbreviation.returncode >= 0
  assert "controlLog syntax" not in valid_abbreviation.stderr


@pytest.mark.parametrize("program", ["sddsfeedforward", "sdds2dfeedforward"])
def test_feedforward_rejects_negative_ezca_retries(program: str) -> None:
  result = _invoke(
    [
      str(_program_path(program)),
      "-ezcaTiming=1,-1",
      "/tmp/input.sdds",
      "/tmp/output.sdds",
    ],
    _probe_environment(),
  )

  assert result.returncode != 0
  assert "invalid -ezca values" in result.stderr


@pytest.mark.parametrize(
  "arguments",
  [
    ("-sourceId=test", f"-tag={'t' * 1024}", "value"),
    ("-sourceId=test", "-tag=value", "v" * 1024),
    (
      "-sourceId=test",
      f"-tag={'a' * 700}",
      "one",
      f"-tag={'b' * 700}",
      "two",
      f"-tag={'c' * 700}",
      "three",
    ),
  ],
)
def test_logmessage_rejects_oversized_fields(arguments: tuple[str, ...]) -> None:
  executable = _program_path("logMessage")
  if not executable.is_file():
    pytest.skip("logMessage is not built on this platform")
  result = _invoke([str(executable), *arguments], _probe_environment())

  assert result.returncode == 1
  assert "string too long" in result.stderr


def test_logdaemon_rejects_an_oversized_service_id() -> None:
  executable = _program_path("logDaemon")
  if not executable.is_file():
    pytest.skip("logDaemon is not built on this platform")
  result = _invoke(
    [str(executable), "-i", "x" * 1024, "-e"],
    _probe_environment(),
  )

  assert result.returncode == 1
  assert "service id is too long" in result.stderr


@pytest.mark.parametrize("program", ["degauss", "standardize"])
def test_legacy_magnet_program_rejects_oversized_filename(program: str) -> None:
  result = _invoke(
    [str(_program_path(program)), "x" * 300],
    _probe_environment(),
  )

  assert result.returncode == 1
  assert "filename is too long" in result.stderr
