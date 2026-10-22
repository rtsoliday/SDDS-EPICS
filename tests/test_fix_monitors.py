"""Regression tests for the monitor and query fixes after the SDDS6 audit.

Enum-free (legacy) inputs must behave as they did before SDDS6 support was
added, while files that define enums still report invalid later pages.
"""

from __future__ import annotations

from pathlib import Path

import pytest

from conftest import SoftIOC, _executable, _invoke
from test_rampload import _isolated_ca_environment
from test_sdds6_append import _append_case
from test_sdds6_page_errors import _write_invalid_metadata_page
from test_sdds6_validation import _write_columns


def _write_legacy(path: Path, columns, parameters: str = "", pages: int = 1, blank_lines: int = 1) -> None:
  """Write an SDDS1 ASCII file, optionally repeating its page, with trailing blank lines."""
  _write_columns(path, columns, "ascii", parameters=parameters, version=1)
  header, page = path.read_bytes().rsplit(b"&end\n", 1)
  path.write_bytes(header + b"&end\n" + page * pages + b"\n" * blank_lines)


def _column_type(soft_ioc: SoftIOC, path: Path, column: str, tmp_path: Path) -> str:
  query = tmp_path / "query.sdds"
  soft_ioc.run_sdds("sddsquery", str(path), f"-sddsOutput={query}")
  listing = soft_ioc.run_sdds("sdds2stream", str(query), "-columns=Name,Type", "-noquotes")
  types = dict(line.split()[:2] for line in listing.stdout.splitlines() if line.strip())
  return types[column]


# --- Enum-free inputs ending in a blank line are accepted, as before SDDS6 ---------------------

def test_monitor_accepts_legacy_inputs_with_trailing_blank_line(soft_ioc: SoftIOC, tmp_path: Path) -> None:
  controls, conditions, output = tmp_path / "controls.sdds", tmp_path / "conditions.sdds", tmp_path / "output.sdds"
  soft_ioc.put("double", "0")
  _write_legacy(controls, [("ControlName", "string", soft_ioc.pv("double")),
                           ("ReadbackName", "string", "signal"), ("ReadbackUnits", "string", "V")])
  _write_legacy(conditions, [("ControlName", "string", soft_ioc.pv("double")),
                             ("LowerLimit", "double", -1), ("UpperLimit", "double", 1)])

  result = soft_ioc.run("sddsmonitor", str(controls), str(output), f"-conditions={conditions},allMustPass",
                        "-steps=1", "-pendIOtime=2")

  assert "number-of-rows" not in result.stderr, result.stderr
  values = soft_ioc.run_sdds("sdds2stream", str(output), "-columns=signal")
  assert len(values.stdout.split()) == 1


@pytest.mark.parametrize("blank_lines", [1, 2])
def test_pvtest_accepts_legacy_limits_with_trailing_blank_line(
  soft_ioc: SoftIOC, tmp_path: Path, blank_lines: int,
) -> None:
  source = tmp_path / "limits.sdds"
  _write_legacy(source, [("ControlName", "string", soft_ioc.pv("double")),
                         ("MinimumValue", "double", -10), ("MaximumValue", "double", 10)],
                blank_lines=blank_lines)
  soft_ioc.put("double", "5")

  result = soft_ioc.run("sddspvtest", str(source), "-time=0,seconds", "-pendIOtime=2")

  assert result.stderr.strip() == "0"


def test_waveform_monitor_accepts_legacy_input_with_trailing_blank_line(soft_ioc: SoftIOC, tmp_path: Path) -> None:
  source, output = tmp_path / "waveforms.sdds", tmp_path / "output.sdds"
  _write_legacy(source, [("WaveformPV", "string", soft_ioc.pv("array")), ("WaveformName", "string", "signal")],
                parameters="&parameter name=WaveformLength, type=long, fixed_value=4 &end\n")

  soft_ioc.run("sddswmonitor", str(source), str(output), "-steps=1", "-pendIOtime=2")

  values = soft_ioc.run_sdds("sdds2stream", str(output), "-columns=signal")
  assert len(values.stdout.split()) == 4


def test_vector_monitor_accepts_legacy_lists_with_trailing_blank_line(soft_ioc: SoftIOC, tmp_path: Path) -> None:
  roots, suffixes, output = tmp_path / "roots.sdds", tmp_path / "suffixes.sdds", tmp_path / "output.sdds"
  _write_legacy(roots, [("Rootname", "string", soft_ioc.prefix)])
  _write_legacy(suffixes, [("Suffix", "string", "double")])

  soft_ioc.run("sddsvmonitor", str(output), f"-rootname={roots}", f"-suffix={suffixes}",
               "-steps=1", "-pendIOtime=2")

  assert output.exists()


@pytest.mark.parametrize("program", ["sddssnapshot", "sddscainfo"])
def test_readback_programs_accept_legacy_input_with_trailing_blank_line(
  soft_ioc: SoftIOC, tmp_path: Path, program: str,
) -> None:
  source, output = tmp_path / "input.sdds", tmp_path / "output.sdds"
  _write_legacy(source, [("ControlName", "string", soft_ioc.pv("double"))])
  soft_ioc.put("double", "1.25")

  soft_ioc.run(program, str(source), str(output), "-pendIOtime=2")

  names = soft_ioc.run_sdds("sdds2stream", str(output), "-columns=ControlName", "-noquotes")
  assert names.stdout.split() == [soft_ioc.pv("double")]


# --- Enum files with an undefined key on a later page are still rejected ------------------------

@pytest.mark.parametrize("program", ["sddspvtest", "sddsmonitor", "sddssnapshot", "sddscainfo"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_enum_files_with_invalid_later_page_are_still_rejected(
  soft_ioc: SoftIOC, tmp_path: Path, program: str, encoding: str,
) -> None:
  source, output = tmp_path / "input.sdds", tmp_path / "output.sdds"
  columns = [("ControlName", "string", soft_ioc.pv("double"))]
  if program == "sddspvtest":
    columns += [("MinimumValue", "double", -10), ("MaximumValue", "double", 10)]
  elif program == "sddsmonitor":
    columns += [("ReadbackName", "string", "signal"), ("ReadbackUnits", "string", "V")]
  _write_invalid_metadata_page(source, columns, encoding, True)

  if program == "sddspvtest":
    result = soft_ioc.invoke(program, str(source), "-time=0,seconds", "-pendIOtime=2")
  elif program == "sddsmonitor":
    result = soft_ioc.invoke(program, str(source), str(output), "-steps=1", "-pendIOtime=2")
  else:
    result = soft_ioc.invoke(program, str(source), str(output), "-pendIOtime=2")

  assert result.returncode > 0
  assert "enum" in result.stderr.lower(), result.stderr


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_enum_conditions_with_invalid_later_page_are_still_rejected(tmp_path: Path, encoding: str) -> None:
  controls, conditions = tmp_path / "controls.sdds", tmp_path / "conditions.sdds"
  _write_columns(controls, [("ControlName", "string", "unused"), ("ReadbackUnits", "string", "V")], encoding)
  _write_invalid_metadata_page(conditions, [("ControlName", "string", "unused"),
                                           ("LowerLimit", "double", 0), ("UpperLimit", "double", 1)],
                               encoding, True)

  result = _invoke([str(_executable("sddsmonitor")), str(controls), str(tmp_path / "output.sdds"),
                    f"-conditions={conditions},allMustPass", "-steps=1", "-pendIOtime=0.01"],
                   _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0
  assert "enum" in result.stderr.lower(), result.stderr


# --- No new "extra pages" warnings for multi-page enum-free inputs -------------------------------

def test_pvtest_multipage_legacy_limits_do_not_change_stderr(soft_ioc: SoftIOC, tmp_path: Path) -> None:
  source = tmp_path / "limits.sdds"
  _write_legacy(source, [("ControlName", "string", soft_ioc.pv("double")),
                         ("MinimumValue", "double", -10), ("MaximumValue", "double", 10)],
                pages=2, blank_lines=0)
  soft_ioc.put("double", "5")

  result = soft_ioc.run("sddspvtest", str(source), "-time=0,seconds", "-pendIOtime=2")

  assert result.stderr.strip() == "0"


def test_multipage_legacy_conditions_and_waveforms_do_not_warn(soft_ioc: SoftIOC, tmp_path: Path) -> None:
  waveforms, conditions, output = tmp_path / "waveforms.sdds", tmp_path / "conditions.sdds", tmp_path / "output.sdds"
  soft_ioc.put("double", "0")
  _write_legacy(waveforms, [("WaveformPV", "string", soft_ioc.pv("array")), ("WaveformName", "string", "signal")],
                parameters="&parameter name=WaveformLength, type=long, fixed_value=4 &end\n",
                pages=2, blank_lines=0)
  _write_legacy(conditions, [("ControlName", "string", soft_ioc.pv("double")),
                             ("LowerLimit", "double", -1), ("UpperLimit", "double", 1)],
                pages=2, blank_lines=0)

  result = soft_ioc.run("sddswmonitor", str(waveforms), str(output), f"-conditions={conditions},allMustPass",
                        "-steps=1", "-pendIOtime=2")

  assert "extra pages" not in result.stderr, result.stderr


# --- sddswmonitor -dataType matches type names case-insensitively --------------------------------

@pytest.mark.parametrize("name,expected", [("FLOAT", "float"), ("Long", "long"), ("short", "short")])
def test_waveform_monitor_datatype_ignores_case(
  soft_ioc: SoftIOC, tmp_path: Path, name: str, expected: str,
) -> None:
  output = tmp_path / "output.sdds"

  soft_ioc.run("sddswmonitor", str(output), f"-PVnames={soft_ioc.pv('array')}", f"-dataType={name}",
               "-steps=1", "-pendIOtime=2")

  assert _column_type(soft_ioc, output, soft_ioc.pv("array"), tmp_path) == expected


@pytest.mark.parametrize("name,message", [("bogus", "invalid -dataType value"),
                                          ("ENUM", "enum output requires a dictionary")])
def test_waveform_monitor_datatype_still_rejects_unknown_and_enum(tmp_path: Path, name: str, message: str) -> None:
  result = _invoke([str(_executable("sddswmonitor")), str(tmp_path / "output.sdds"), "-PVnames=unused",
                    f"-dataType={name}", "-steps=1", "-pendIOtime=0.01"], _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0
  assert message in result.stdout + result.stderr


# --- Enum fields that a program never reads are accepted -----------------------------------------

@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_waveform_monitor_scalars_accept_unused_enum_deadband(soft_ioc: SoftIOC, tmp_path: Path, encoding: str) -> None:
  scalars, output = tmp_path / "scalars.sdds", tmp_path / "output.sdds"
  _write_columns(scalars, [("ControlName", "string", soft_ioc.pv("double")),
                           ("ReadbackName", "string", "scalar"), ("DeadBand", "enum", -7)], encoding)

  soft_ioc.run("sddswmonitor", str(output), f"-PVnames={soft_ioc.pv('array')}", f"-scalars={scalars}",
               "-steps=1", "-pendIOtime=2")

  assert output.exists()


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("field", ["rmsPVPrefix", "ReadbackOffset", "ReadbackScale"])
def test_synchlog_waveforms_accept_unused_enum_fields(
  soft_ioc: SoftIOC, tmp_path: Path, encoding: str, field: str,
) -> None:
  scalars, waveforms, output = tmp_path / "scalars.sdds", tmp_path / "waveforms.sdds", tmp_path / "output.sdds"
  _write_columns(scalars, [("ControlName", "string", soft_ioc.pv("double")),
                           ("ReadbackName", "string", "scalar"), ("ReadbackUnits", "string", "V")], encoding)
  _write_columns(waveforms, [("WaveformPV", "string", soft_ioc.pv("array")),
                             ("WaveformName", "string", "waveform"), (field, "enum", -7)], encoding,
                 parameters="&parameter name=WaveformLength, type=long, fixed_value=4 &end\n")

  result = soft_ioc.invoke("sddssynchlog", str(scalars), str(output), f"-waveformData={waveforms}",
                           "-samples=1", "-time=0.01", "-pendIOtime=2")

  assert f"Column {field} has enum type" not in result.stderr, result.stderr


@pytest.mark.parametrize("program,field", [
  ("sddsmonitor", "PageTimeStamp"), ("sddsstatmon", "PageTimeStamp"), ("sddsstatmon", "Samples"),
])
def test_append_accepts_enum_parameters_that_are_never_set(
  soft_ioc: SoftIOC, tmp_path: Path, program: str, field: str,
) -> None:
  output, arguments = _append_case(soft_ioc, tmp_path, program, "binary", field, parameter=True)

  soft_ioc.run(program, *arguments, "-append", "-steps=1", "-pendIOtime=2")

  labels = soft_ioc.run_sdds("sdds2stream", str(output), f"-parameters={field}", "-noquotes")
  assert labels.stdout.split() == ["categorical", "categorical"]


@pytest.mark.parametrize("program", ["sddsmonitor", "sddsstatmon", "sddsvmonitor", "sddswmonitor"])
def test_append_retains_nonfixed_enum_parameter_values(soft_ioc: SoftIOC, tmp_path: Path, program: str) -> None:
  output, arguments = _append_case(soft_ioc, tmp_path, program, "ascii", None)
  # Add a non-fixed enum parameter whose dictionary does not map code 0, so an
  # appended page that left it zero-initialized would fail validation.
  header, data = output.read_bytes().split(b"&data ", 1)
  output.write_bytes(header + b'&parameter name=Mode, type=enum, enum_values="-7:retained,5:other" &end\n&data '
                     + data.replace(b"&end\n", b"&end\n-7\n", 1))

  soft_ioc.run(program, *arguments, "-append", "-steps=1", "-pendIOtime=2")

  labels = soft_ioc.run_sdds("sdds2stream", str(output), "-parameters=Mode", "-noquotes")
  assert labels.stdout.split() == ["retained", "retained"]
