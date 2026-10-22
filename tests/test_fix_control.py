"""Regression tests for the SDDS6 audit fixes in control-bit, feedback, optimizer, and experiment tools."""

from __future__ import annotations

import struct
from pathlib import Path

import pytest

from conftest import SoftIOC, _executable, _invoke, _sdds_executable
from test_rampload import _isolated_ca_environment
from test_sdds6_validation import BUNCH_COLUMNS, PRESET_COLUMNS, _write_columns

# One complete turn with a turn marker and wrap marker.
WAVEFORM = [0x40000000] + [0] * 322 + [0x80000000]


def _run(program: str, *arguments: str):
  return _invoke([str(_executable(program)), *arguments], _isolated_ca_environment(), timeout=3)


def _first_parameter(source: Path, name: str) -> str:
  """Return the first-page value of a parameter, preserving empty strings."""
  result = _invoke([str(_sdds_executable("sdds2stream")), str(source), f"-parameters={name}", "-noquotes"],
                   _isolated_ca_environment())
  assert result.returncode == 0, result.stderr
  return result.stdout.split("\n")[0]


def _columns(source: Path, names: str) -> str:
  result = _invoke([str(_sdds_executable("sdds2stream")), str(source), f"-columns={names}"],
                   _isolated_ca_environment())
  assert result.returncode == 0, result.stderr
  return result.stdout


def _write_raw_ram(source: Path, waveform_type: str, encoding: str,
                   parameters: list[tuple[str, str, str | int | float]] = ()) -> None:
  """Write an SDDS1 RAM file with non-fixed parameters, as legacy tools produce."""
  header = "SDDS1\n"
  for name, kind, _ in parameters:
    header += f"&parameter name={name}, type={kind} &end\n"
  header += f"&column name=Waveform, type={waveform_type} &end\n&data mode={encoding}, endian=little &end\n"
  if waveform_type == "long":
    values = [value - (1 << 32) if value >= 1 << 31 else value for value in WAVEFORM]
  else:
    values = WAVEFORM
  if encoding == "ascii":
    data = "".join(f'"{value}"\n' if kind == "string" else f"{value}\n" for _, kind, value in parameters)
    data += f"{len(values)}\n" + "\n".join(map(str, values)) + "\n"
    source.write_bytes(header.encode("ascii") + data.encode("ascii"))
    return
  formats = {"long": "i", "ulong": "I", "short": "h", "double": "d"}
  data = struct.pack("<i", len(values))
  for _, kind, value in parameters:
    if kind == "string":
      text = str(value).encode("ascii")
      data += struct.pack("<i", len(text)) + text
    else:
      data += struct.pack("<" + formats[kind], value)
  data += struct.pack(f"<{len(values)}" + formats[waveform_type], *values)
  source.write_bytes(header.encode("ascii") + data)


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_raw_ram_accepts_signed_long_waveform(tmp_path: Path, encoding: str) -> None:
  # sddswget stores a DBF_LONG RAM waveform as SDDS long; the 32 bits are the same bit masks.
  outputs = []
  for waveform_type in ("ulong", "long"):
    source, output = tmp_path / f"{waveform_type}.sdds", tmp_path / f"{waveform_type}-bits.sdds"
    _write_raw_ram(source, waveform_type, encoding)
    result = _run("sddsgencontrolbits", str(source), str(output), "-scopeArrayLength=324")
    assert result.returncode == 0, result.stderr
    outputs.append(_columns(output, "TurnMarker,WrapMarker,CommutationSwitch,PlaneSwitch0"))
  assert outputs[0] == outputs[1]
  assert "1" in outputs[1]


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_raw_ram_rejects_non_mask_waveform_types(tmp_path: Path, encoding: str) -> None:
  source = tmp_path / "ram.sdds"
  source.write_text(
    "SDDS1\n&column name=Waveform, type=double &end\n&data mode=ascii &end\n2\n1\n2\n", encoding="ascii")

  result = _run("sddsgencontrolbits", str(source), str(tmp_path / "bits.sdds"))

  assert result.returncode > 0
  assert "long or unsigned long bit masks" in result.stderr


def test_raw_ram_keeps_long_string_comment(tmp_path: Path) -> None:
  # Strings are copied directly; formatting through a fixed 1024-byte buffer would overflow.
  source, output = tmp_path / "ram.sdds", tmp_path / "bits.sdds"
  comment = "c" * 3000
  _write_raw_ram(source, "ulong", "binary", [("Comment", "string", comment)])

  result = _run("sddsgencontrolbits", str(source), str(output), "-scopeArrayLength=324")

  assert result.returncode == 0, result.stderr
  assert _first_parameter(output, "Comment") == comment


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_raw_ram_ignores_optional_parameters_of_other_types(tmp_path: Path, encoding: str) -> None:
  # Legacy readers only used text parameters of string type and counts of long type.
  source, output = tmp_path / "ram.sdds", tmp_path / "bits.sdds"
  _write_raw_ram(source, "ulong", encoding, [
    ("Comment", "short", 3), ("PlaneMode", "long", 1), ("SampleMode0", "double", 1.5),
    ("TurnMarkerInterval", "double", 100.0), ("TransitionDeadTime", "short", 3),
    ("SamplesPerBunch", "double", 2.0),
  ])

  result = _run("sddsgencontrolbits", str(source), str(output), "-scopeArrayLength=324")

  assert result.returncode == 0, result.stderr
  assert _first_parameter(output, "Comment") == "Control RAM was obtained from input RAM file."
  assert _first_parameter(output, "PlaneMode0") == ""
  assert _first_parameter(output, "TurnMarkerInterval") == "324"
  assert _first_parameter(output, "TransitionDeadTime") == "10"
  assert _first_parameter(output, "SamplesPerBunch0") == "5"


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_raw_ram_plane_mode_without_other_modes(tmp_path: Path, encoding: str) -> None:
  # The accumulator defaults to the RAM plane mode; an absent sample mode is written as blank
  # instead of indexing the mode tables with -1.
  source, output = tmp_path / "ram.sdds", tmp_path / "bits.sdds"
  _write_raw_ram(source, "ulong", encoding, [("PlaneMode", "string", "y")])

  result = _run("sddsgencontrolbits", str(source), str(output), "-scopeArrayLength=324")

  assert result.returncode == 0, result.stderr
  for receiver in range(4):
    assert _first_parameter(output, f"PlaneMode{receiver}") == "y"
    assert _first_parameter(output, f"AccumulatorMode{receiver}") == "y"
    assert _first_parameter(output, f"SampleMode{receiver}") == ""


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_preset_non_string_accumulator_mode_falls_back_to_plane_mode(tmp_path: Path, encoding: str) -> None:
  preset, bunch, output = tmp_path / "preset.sdds", tmp_path / "bunch.sdds", tmp_path / "bits.sdds"
  columns = [("AccumulatorMode", "long", 1) if name == "AccumulatorMode" else (name, kind, value)
             for name, kind, value in PRESET_COLUMNS]
  _write_columns(preset, columns, encoding, version=1)
  _write_columns(bunch, BUNCH_COLUMNS, encoding, version=1)

  result = _run("sddsgencontrolbits", str(output), f"-presetRAMconfiguration=chosen,file={preset}",
                f"-bunchPatternFile={bunch}")

  assert result.returncode == 0, result.stderr
  assert _first_parameter(output, "AccumulatorMode0") == "x"


@pytest.mark.parametrize("program", ["sddscontrollaw", "sddspvacontrollaw"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_feedback_waveform_tests_reject_enum_waveform_pv(tmp_path: Path, program: str, encoding: str) -> None:
  matrix, tests = tmp_path / "matrix.sdds", tmp_path / "tests.sdds"
  _write_columns(matrix, [("Actuator", "string", "unused"), ("Readback", "double", 1)], "ascii")
  _write_columns(tests, [("DeviceName", "string", "Readback"), ("Index", "long", 0),
                         ("MinimumValue", "double", -1), ("MaximumValue", "double", 1)], encoding,
                 parameters='&parameter name=WaveformPV, type=enum, enum_values="-7:unused", fixed_value=-7 &end\n')

  result = _invoke([str(_executable(program)), str(matrix), f"-waveforms={tests},test",
                    "-dryRun", "-steps=1", "-interval=0.001", "-pendIOtime=0.01"],
                   _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0
  assert 'Something wrong with parameter "WaveformPV"' in result.stderr


@pytest.mark.parametrize("field", ["Tolerance", "ReadbackUnits"])
def test_optimizer_ignores_unconsulted_enum_fields(tmp_path: Path, field: str) -> None:
  # Without a ReadbackName column, tolerances are never read; ReadbackUnits is never read from a variables file.
  source = tmp_path / "variables.sdds"
  _write_columns(source, [("ControlName", "string", "unused"), ("LowerLimit", "double", -1),
                          ("UpperLimit", "double", 1), ("InitialValue", "double", 0),
                          ("InitialChange", "double", 0.1), (field, "enum", -7)], "ascii")

  # The missing knob file stops the run right after the variables file has been checked.
  result = _invoke([str(_executable("sddsoptimize")), f"-varFile={source}", "-varScript=unused",
                    f"-knobFiles={tmp_path / 'missing.sdds'}", "-dryRun", "-simplex", "-pendIOtime=0.01"],
                   _isolated_ca_environment(), timeout=5)

  assert result.returncode > 0
  assert "has enum type" not in result.stderr, result.stderr


@pytest.mark.parametrize("field", ["Rootname", "Suffix"])
def test_vector_lists_accept_enum_free_trailing_blank_lines(soft_ioc: SoftIOC, tmp_path: Path, field: str) -> None:
  # Enum-free inputs keep their historical behavior: a failed extra-page read simply ends the input.
  roots, suffixes = tmp_path / "roots.sdds", tmp_path / "suffixes.sdds"
  for path, name, value in ((roots, "Rootname", soft_ioc.prefix), (suffixes, "Suffix", "double")):
    _write_columns(path, [(name, "string", value)], "ascii", version=1)
    if name == field:
      path.write_bytes(path.read_bytes() + b"\n\n")
  experiment = tmp_path / "experiment.namelist"
  experiment.write_text(
    f'&variable PV_name="{soft_ioc.pv("double")}", index_number=0, index_limit=1 &end\n'
    '&execute &end\n', encoding="ascii",
  )

  result = soft_ioc.invoke("sddsvexperiment", str(experiment), str(tmp_path / "output.sdds"),
                           f"-rootnameFile={roots}", f"-suffixFile={suffixes}", "-dryRun", "-pendIOtime=2")

  # The extra-page probe of the enum-free file must not be reported as an error.
  assert "Unable to read page" not in result.stderr, result.stderr
