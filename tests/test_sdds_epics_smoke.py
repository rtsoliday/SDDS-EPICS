"""Safe command-line parser coverage for specialized SDDS-EPICS programs."""

from __future__ import annotations

import os
from pathlib import Path

import pytest

from conftest import _binary_directory, _invoke


SPECIALIZED_SDDS_EPICS_PROGRAMS = [
  "MV20image2sdds",
  "sddscontrollaw",
  "sddscaramp",
  "sddsgencontrolbits",
  "sddslogTurnByTurnDAQ",
  "degauss",
  "standardize",
  "sddssynchlog",
  "sddsmonitor",
  "sddsstatmon",
  "sddsvmonitor",
  "sddswmonitor",
  "BPS16monitor",
  "squishPVs",
  "sddsalarmlog",
  "toggle",
  "sddslogger",
  "sddsfeedforward",
  "sdds2dfeedforward",
  "sddsglitchlogger",
  "sddsoptimize",
  "sddslogonchange",
  "sddscasr",
  "sddsimagemonitor",
  "makesrbump",
  "sddsexperiment",
  "sddsvexperiment",
  "sddspvalogger",
  "sddspvasaverestore",
  "sddspermissive",
  "sddspvaglitchlogger",
  "sddspvacontrollaw",
  "sddspcas",
]


def _program_path(name: str) -> Path:
  suffix = ".exe" if os.name == "nt" else ""
  return _binary_directory() / f"{name}{suffix}"


@pytest.mark.parametrize("program", SPECIALIZED_SDDS_EPICS_PROGRAMS)
def test_program_rejects_unknown_option(program: str, tmp_path: Path) -> None:
  """Verify each program starts, parses arguments, and rejects unknown input safely."""

  executable = _program_path(program)
  if not executable.is_file():
    if program == "sddspcas":
      pytest.skip("sddspcas is disabled because the optional PCAS module is unavailable")
    pytest.fail(f"required test executable was not built: {executable}")

  environment = os.environ.copy()
  environment.update(
    {
      "EPICS_CA_ADDR_LIST": "127.0.0.1:1",
      "EPICS_CA_AUTO_ADDR_LIST": "NO",
      "EPICS_PVA_ADDR_LIST": "127.0.0.1:1",
      "EPICS_PVA_AUTO_ADDR_LIST": "NO",
      "XDG_RUNTIME_DIR": str(tmp_path),
    }
  )
  result = _invoke(
    [str(executable), "-definitelyInvalidOption"],
    environment,
    timeout=3,
  )

  assert result.returncode > 0, (
    f"{program} accepted an unknown option\n"
    f"stdout:\n{result.stdout}\n"
    f"stderr:\n{result.stderr}"
  )


def test_sddsbcontrol_is_built_but_not_started() -> None:
  """sddsbcontrol has no parser and immediately connects to hard-coded machine PVs."""

  executable = _program_path("sddsbcontrol")
  assert executable.is_file()
  assert os.access(executable, os.X_OK)
