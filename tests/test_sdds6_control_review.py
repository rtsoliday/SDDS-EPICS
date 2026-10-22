"""Regressions found while reviewing the SDDS6 control-input changes."""

import struct
import subprocess
import time
from pathlib import Path

import pytest

from conftest import SoftIOC, _command_arguments, _executable, _invoke
from test_rampload import _isolated_ca_environment
from test_sdds6_validation import BUNCH_COLUMNS, MAGNET_COLUMNS, PRESET_COLUMNS, _write_columns


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_rampload_rejects_long_string_pv_name_without_crashing(tmp_path: Path, encoding: str) -> None:
  source = tmp_path / "ramp.sdds"
  control_name = "x" * 3000
  header = (
    "SDDS1\n&parameter name=ControlName, type=string &end\n"
    '&parameter name=ControlType, type=string, fixed_value="pv" &end\n'
    "&parameter name=RampBias, type=double, fixed_value=0 &end\n"
    "&column name=RampTime, type=double &end\n"
    "&column name=RampSetpoint, type=double &end\n"
    f"&data mode={encoding}, endian=little &end\n"
  ).encode("ascii")
  if encoding == "ascii":
    page = f'"{control_name}"\n2\n0 0\n1 1\n'.encode("ascii")
  else:
    page = struct.pack("<2i", 2, len(control_name)) + control_name.encode("ascii")
    page += struct.pack("<4d", 0, 0, 1, 1)
  source.write_bytes(header + page)

  result = _invoke([str(_executable("rampload")), str(source)], _isolated_ca_environment())

  assert result.returncode == 1, result.stderr
  assert f"process variable name {control_name} is too long" in result.stderr


def _invalid_third_page(source: Path, columns, encoding: str, *, contents: str = "") -> None:
  _write_columns(source, columns + [("Metadata", "enum", -7)], encoding, contents=contents)
  header, page = source.read_bytes().rsplit(b"&end\n", 1)
  invalid = page.replace(b"-7", b"999") if encoding == "ascii" else page[:-4] + struct.pack("<i", 999)
  source.write_bytes(header + b"&end\n" + page * 2 + invalid)


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("table", ["preset", "bunch"])
def test_control_bits_reject_invalid_third_enum_page(tmp_path: Path, encoding: str, table: str) -> None:
  preset, bunch, output = (tmp_path / name for name in ("preset.sdds", "bunch.sdds", "bits.sdds"))
  for name, source, columns in (("preset", preset, PRESET_COLUMNS), ("bunch", bunch, BUNCH_COLUMNS)):
    if name == table:
      _invalid_third_page(source, columns, encoding)
    else:
      _write_columns(source, columns, encoding)

  result = _invoke([str(_executable("sddsgencontrolbits")), str(output),
                    f"-presetRAMconfiguration=chosen,file={preset}", f"-bunchPatternFile={bunch}"],
                   _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0, result.stdout + result.stderr
  assert "Unmapped enum code" in result.stderr
  assert not output.exists()


@pytest.mark.parametrize("program", ["sddscontrollaw", "sddspvacontrollaw"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("auxiliary", [False, True])
def test_feedback_filter_rejects_invalid_third_enum_page(
  tmp_path: Path, program: str, encoding: str, auxiliary: bool,
) -> None:
  matrix, coefficients = tmp_path / "matrix.sdds", tmp_path / "filter.sdds"
  _write_columns(matrix, [("Actuator", "string", "unused"), ("Readback", "double", 1)], "ascii")
  _invalid_third_page(coefficients, [("DeviceName", "string", "unused"), ("a0", "double", 1),
                                    ("b0", "double", 1)], encoding)
  option = (f"-auxiliaryOutput=matrixFile={matrix},coefFile={coefficients}" if auxiliary
            else f"-filterFile={coefficients}")

  result = _invoke([str(_executable(program)), str(matrix), option, "-dryRun", "-steps=1",
                    "-interval=0.001", "-pendIOtime=0.01"], _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0, result.stdout + result.stderr
  assert "Unmapped enum code" in result.stderr


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("mode", MAGNET_COLUMNS)
def test_magnet_specification_rejects_invalid_third_enum_page(tmp_path: Path, encoding: str, mode: str) -> None:
  source = tmp_path / "magnet.sdds"
  contents = "degaussing specification" if mode == "degauss-legacy" else "tri-degaussing specification"
  _invalid_third_page(source, MAGNET_COLUMNS[mode], encoding, contents=contents)
  program = "degauss" if mode.startswith("degauss") else "standardize"

  result = _invoke([str(_executable(program)), str(source)], _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0
  assert "Unmapped enum code" in result.stderr
  assert "illegal ControlType" not in result.stderr


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_optimizer_variables_reject_invalid_third_enum_page(tmp_path: Path, encoding: str) -> None:
  source = tmp_path / "variables.sdds"
  _invalid_third_page(source, [("ControlName", "string", "unused"), ("LowerLimit", "double", -1),
                               ("UpperLimit", "double", 1), ("InitialValue", "double", 0),
                               ("InitialChange", "double", 0.1)], encoding)

  result = _invoke([str(_executable("sddsoptimize")), f"-varFile={source}", "-varScript=unused",
                    f"-knobFiles={tmp_path / 'missing.sdds'}", "-dryRun", "-simplex", "-pendIOtime=0.01"],
                   _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0
  assert "Unmapped enum code" in result.stderr


@pytest.mark.parametrize("field", ["DeviceName", "WaveformPV"])
def test_watched_transfer_keeps_configuration_after_required_enum_field(
  soft_ioc: SoftIOC, tmp_path: Path, field: str,
) -> None:
  source, replacement = tmp_path / "transfer.sdds", tmp_path / "replacement.sdds"
  output, errors = tmp_path / "transfer.stdout", tmp_path / "transfer.stderr"
  parameter = f'&parameter name=WaveformPV, type=string, fixed_value="{soft_ioc.pv("array")}" &end\n'
  _write_columns(source, [("DeviceName", "string", "unused")], "ascii", parameters=parameter)
  command = [str(_executable("sddstransferscalartowaveform")), str(source), "-watchInput", "-dryRun",
             "-steps=100", "-interval=0.03", "-verbose", "-pendIOtime=2"]
  with output.open("w") as stdout, errors.open("w") as stderr:
    process = subprocess.Popen(_command_arguments(command), env=soft_ioc.environment,
                               stdout=stdout, stderr=stderr)
    try:
      deadline = time.monotonic() + 5
      while "Starting loop" not in output.read_text() and time.monotonic() < deadline:
        assert process.poll() is None, errors.read_text()
        time.sleep(0.02)
      assert "Starting loop" in output.read_text(), errors.read_text()
      # The legacy file watcher compares ctime in whole seconds.
      while time.time() < int(source.stat().st_ctime) + 1.05:
        time.sleep(0.02)
      columns = [("DeviceName", "enum", -7)] if field == "DeviceName" else [("DeviceName", "string", "unused")]
      if field == "WaveformPV":
        parameter = '&parameter name=WaveformPV, type=enum, enum_values="-7:unused", fixed_value=-7 &end\n'
      _write_columns(replacement, columns, "ascii", parameters=parameter)
      replacement.replace(source)
      process.wait(timeout=6)
    finally:
      if process.poll() is None:
        process.terminate()
        process.wait(timeout=5)

  assert process.returncode == 0, errors.read_text()
  assert "continuing with the previous configuration" in errors.read_text()
  after_reload = output.read_text().split("input file modified, reload input file", 1)[1]
  assert f"working on {soft_ioc.pv('array')}, reading scalar pvs" in after_reload
