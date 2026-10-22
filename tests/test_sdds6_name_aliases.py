"""Categorical PV-name aliases must not silently select a different PV list."""

from pathlib import Path

import pytest

from conftest import SoftIOC
from test_sdds6_optional_fields import _assert_rejected
from test_sdds6_validation import _write_columns


READERS = [
  ("sddsmonitor", "input", ("ControlName", "Device", "DeviceName")),
  ("sddslogger", "input", ("ControlName", "Device", "DeviceName")),
  ("sddssynchlog", "input", ("ControlName", "Device", "DeviceName")),
  ("sddssnapshot", "input", ("ControlName", "Device")),
  ("sddscaramp", "ramp", ("ControlName", "Device", "DeviceName")),
  ("sddsglitchlogger", "glitches", ("ControlName", "Device", "DeviceName")),
  ("sddspvaglitchlogger", "glitches", ("ControlName", "Device", "DeviceName")),
  ("sddsoptimize", "variables", ("controlPV", "ControlName", "Device", "DeviceName")),
  ("sddsoptimize", "measurements", ("ControlName", "Device", "DeviceName")),
  ("sddslogger", "conditions", ("ControlName", "Device", "DeviceName")),
  ("sddspvaglitchlogger", "conditions", ("ControlName", "Device", "DeviceName")),
  ("sddswmonitor", "scalars", ("ControlName", "Device", "DeviceName")),
  ("BPS16monitor", "scalars", ("ControlName", "DeviceName")),
  ("BPS16monitor", "assignments", ("ControlName", "DeviceName")),
]
CASES = [(program, mode, names, field) for program, mode, names in READERS for field in names]


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("program,mode,names,field", CASES)
def test_name_readers_reject_enum_alias_before_fallback(
  soft_ioc: SoftIOC, tmp_path: Path, encoding, program, mode, names, field,
):
  source, controls, output = (tmp_path / name for name in ("aliases.sdds", "controls.sdds", "output.sdds"))
  # Every other supported alias remains a valid primitive fallback, exposing
  # readers that simply skip a categorical field during type-filtered lookup.
  columns = [(name, "enum", -7) if name == field else (name, "string", soft_ioc.pv("double"))
             for name in names]
  columns += [("ReadbackName", "string", "signal"), ("ReadbackUnits", "string", "V"),
              ("LowerLimit", "double", -1), ("UpperLimit", "double", 1),
              ("ValueString", "string", "0")]
  _write_columns(source, columns, encoding)
  _write_columns(controls, [("ControlName", "string", soft_ioc.pv("double")),
                            ("ReadbackUnits", "string", "V"),
                            ("LowerLimit", "double", -1), ("UpperLimit", "double", 1)], "ascii")

  if mode == "ramp":
    arguments = [f"-rampTo={source},steps=1,pause=0"]
  elif mode == "glitches":
    arguments = [str(source), str(tmp_path), "-time=0.01"]
  elif mode == "variables":
    arguments = [f"-varFile={source}", "-varScript=unused", "-dryRun", "-simplex"]
  elif mode == "measurements":
    arguments = [f"-measFile={source}", f"-varFile={controls}", "-dryRun", "-simplex"]
  elif mode == "conditions":
    arguments = [str(controls), str(output) if program == "sddslogger" else str(tmp_path),
                 f"-conditions={source},allMustPass", "-time=0.01"]
  elif mode in ("scalars", "assignments"):
    arguments = [str(output), f"-{mode}={source}", "-steps=1"]
    if program == "sddswmonitor":
      arguments.append(f"-PVnames={soft_ioc.pv('array')}")
  else:
    arguments = [str(source), str(output)]
    if program == "sddssynchlog":
      arguments += ["-samples=1", "-time=0.01"]
    elif program != "sddssnapshot":
      arguments += ["-steps=1"]

  _assert_rejected(program, arguments, field, environment=soft_ioc.environment)


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("alias", ["ControlName", "Device", "DeviceName"])
def test_primitive_name_aliases_still_work_with_enum_metadata(
  soft_ioc: SoftIOC, tmp_path: Path, encoding, alias,
):
  source, output = tmp_path / "controls.sdds", tmp_path / "output.sdds"
  _write_columns(source, [(alias, "string", soft_ioc.pv("double")),
                          ("ReadbackName", "string", "signal"),
                          ("ReadbackUnits", "string", "V"), ("Metadata", "enum", -7)], encoding)
  soft_ioc.put("double", "1.25")

  soft_ioc.run("sddsmonitor", str(source), str(output), "-steps=1", "-pendIOtime=2")

  reading = soft_ioc.run_sdds("sdds2stream", str(output), "-columns=signal", "-noquotes")
  assert float(reading.stdout) == pytest.approx(1.25)


def test_explicit_ramp_name_column_allows_unrelated_enum_alias(soft_ioc: SoftIOC, tmp_path: Path):
  source = tmp_path / "ramp.sdds"
  _write_columns(source, [("ChosenPV", "string", soft_ioc.pv("double")),
                          ("ControlName", "enum", -7), ("Value", "double", 1.25)], "ascii")
  soft_ioc.put("double", "0")

  soft_ioc.run("sddscaramp", f"-rampTo={source},steps=1,pause=0",
               "-controlNameColumn=ChosenPV", "-pendIOtime=2")

  assert float(soft_ioc.get("double")) == pytest.approx(1.25)
