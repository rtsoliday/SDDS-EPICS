"""SDDS6 ramp input validation that stops before any PV search or write."""

from __future__ import annotations

import struct
from pathlib import Path

import pytest

from conftest import _executable, _invoke
from test_rampload import _isolated_ca_environment


def _write_ramp(
  path: Path,
  encoding: str,
  *,
  enum_field: str | None = None,
  control_name: str = "pytestUnused",
  control_type: str = "dev",
  version: int = 6,
) -> None:
  """Write two ramp points with fixed control parameters and optional enums."""
  enum_text = version == 6
  lines = [f"SDDS{version}"]
  for name, label, code in (
    ("ControlName", control_name, 17),
    ("ControlType", control_type, -4),
  ):
    definition = (
      f'type=enum, enum_values="{code}:{label}", fixed_value={code}'
      if enum_text
      else f'type=string, fixed_value="{label}"'
    )
    lines.append(f"&parameter name={name}, {definition} &end")
  bias_definition = (
    'type=enum, enum_values="0:zero", fixed_value=0'
    if enum_field == "RampBias"
    else "type=short, fixed_value=0"
  )
  lines.append(f"&parameter name=RampBias, {bias_definition} &end")
  for name in ("RampTime", "RampSetpoint"):
    definition = (
      'type=enum, enum_values="0:zero,1:one"'
      if enum_field == name
      else "type=float"
    )
    lines.append(f"&column name={name}, {definition} &end")
  lines.append(f"&data mode={encoding}, endian=little &end")
  header = ("\n".join(lines) + "\n").encode("ascii")
  if encoding == "ascii":
    page = b"2\n0 0\n1 1\n"
  else:
    row_format = "".join("i" if enum_field == name else "f" for name in ("RampTime", "RampSetpoint"))
    page = struct.pack("<i", 2)
    page += struct.pack("<" + row_format, 0, 0)
    page += struct.pack("<" + row_format, 1, 1)
  path.write_bytes(header + page)


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("version", [1, 6])
def test_rampload_reads_control_labels_and_numeric_conversions(
  tmp_path: Path, encoding: str, version: int
) -> None:
  source = tmp_path / "ramp.sdds"
  _write_ramp(source, encoding, version=version)

  result = _invoke([str(_executable("rampload")), str(source)], _isolated_ca_environment())

  assert result.returncode == 1
  assert "dev ControlType no longer supported" in result.stderr
  assert "unable to read" not in result.stderr


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_rampload_uses_enum_control_name_label(tmp_path: Path, encoding: str) -> None:
  source = tmp_path / "ramp.sdds"
  oversized_name = "x" * 300
  _write_ramp(source, encoding, control_name=oversized_name, control_type="pv")

  result = _invoke([str(_executable("rampload")), str(source)], _isolated_ca_environment())

  assert result.returncode == 1
  assert f"process variable name {oversized_name} is too long" in result.stderr


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("field", ["RampBias", "RampTime", "RampSetpoint"])
def test_rampload_rejects_enum_numeric_fields(tmp_path: Path, encoding: str, field: str) -> None:
  source = tmp_path / "ramp.sdds"
  _write_ramp(source, encoding, enum_field=field)

  result = _invoke([str(_executable("rampload")), str(source)], _isolated_ca_environment())

  assert result.returncode == 1
  assert f"unable to read {field}" in result.stderr
  assert "dev ControlType" not in result.stderr
