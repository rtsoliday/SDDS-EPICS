"""Check setbcorr's raw ASCII append preflight with loopback-only CA discovery."""

from __future__ import annotations

import struct
import subprocess
from pathlib import Path

import pytest

from conftest import _command_arguments, _executable, _invoke
from test_rampload import _isolated_ca_environment


PARAMETERS = [
  ("TimeStamp", "string", "original"),
  ("BPMTime", "double", 1.0),
  ("BPMDuration", "double", 2.0),
  ("ControlName", "string", "unused:rampRAMPTABLE"),
  ("ControlType", "string", "pv"),
  ("RampBias", "double", 0.0),
  ("CorrName", "string", "unused"),
  ("CorrCurrentAO", "string", "unused:CurrentAO"),
]


def _write_ramp(path: Path, encoding: str, field: str) -> None:
  header = "SDDS6\n"
  parameters = [(name, "enum" if name == field else kind, -7 if name == field else value)
                for name, kind, value in PARAMETERS]
  columns = [(name, "enum" if name == field else "double", -7 if name == field else 0.0)
             for name in ("RampTime", "RampSetpoint")]
  for keyword, definitions in (("parameter", parameters), ("column", columns)):
    for name, kind, _ in definitions:
      mapping = ', enum_values="-7:categorical"' if kind == "enum" else ""
      header += f"&{keyword} name={name}, type={kind}{mapping} &end\n"
  if field == "History":
    header += '&array name=History, type=enum, enum_values="-7:categorical", dimensions=1 &end\n'
  header += f"&data mode={encoding}, endian=little &end\n"
  if encoding == "ascii":
    values = [str(value) for _, _, value in parameters]
    if field == "History":
      values += ["1", "-7"]
    values += ["1", " ".join(str(value) for _, _, value in columns)]
    data = ("\n".join(values) + "\n").encode("ascii")
  else:
    data = struct.pack("<i", 1)
    for _, kind, value in parameters:
      if kind == "string":
        text = value.encode("ascii")
        data += struct.pack("<i", len(text)) + text
      else:
        data += struct.pack("<i" if kind == "enum" else "<d", value)
    if field == "History":
      data += struct.pack("<2i", 1, -7)
    for _, kind, value in columns:
      data += struct.pack("<i" if kind == "enum" else "<d", value)
  path.write_bytes(header.encode("ascii") + data)


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("field", ["ControlName", "ControlType", "RampBias", "RampTime", "RampSetpoint", "History"])
def test_setbcorr_rejects_enum_append_fields_without_modifying_file(tmp_path: Path, encoding: str, field: str) -> None:
  source = tmp_path / "ramp.sdds"
  _write_ramp(source, encoding, field)
  original = source.read_bytes()

  result = _invoke([str(_executable("setbcorr")), "-a", "-o", str(source), "-nr", "-ns", "unused", "1"],
                   _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0
  assert "enum" in result.stderr.lower(), result.stderr
  assert field in result.stderr
  assert source.read_bytes() == original


@pytest.mark.parametrize("version", [1, 6])
def test_setbcorr_accepts_primitive_layout_and_fixed_enum_metadata(tmp_path: Path, version: int) -> None:
  source = tmp_path / "ramp.sdds"
  _write_ramp(source, "ascii", "")
  contents = source.read_bytes()
  if version == 6:
    contents = contents.replace(b"SDDS6\n", b'SDDS6\n&parameter name=Metadata, type=enum, '
                                b'enum_values="-7:retained", fixed_value=-7 &end\n')
  else:
    contents = contents.replace(b"SDDS6\n", b"SDDS1\n")
  source.write_bytes(contents)
  command = [str(_executable("setbcorr")), "-a", "-o", str(source), "-nr", "-ns", "unused", "1"]

  # A compatible layout proceeds to timing PV discovery. Stop it there: these
  # tests use no site IOCs and must never exercise the real Booster PV actions.
  with (tmp_path / "stdout").open("w+") as stdout, (tmp_path / "stderr").open("w+") as stderr:
    process = subprocess.Popen(_command_arguments(command), env=_isolated_ca_environment(),
                               stdout=stdout, stderr=stderr)
    try:
      with pytest.raises(subprocess.TimeoutExpired):
        process.wait(timeout=0.3)
    finally:
      if process.poll() is None:
        process.terminate()
      process.wait(timeout=3)
    stderr.seek(0)
    assert "setbcorr:" not in stderr.read()
  assert source.read_bytes() == contents


@pytest.mark.parametrize("invalid", ["binary", "no-row-counts", "column-order", "invalid-fixed-enum"])
def test_setbcorr_rejects_incompatible_or_invalid_pages(tmp_path: Path, invalid: str) -> None:
  source = tmp_path / "ramp.sdds"
  _write_ramp(source, "binary" if invalid == "binary" else "ascii", "")
  contents = source.read_bytes()
  if invalid == "no-row-counts":
    contents = contents.replace(b"mode=ascii", b"mode=ascii, no_row_counts=1")
  elif invalid == "column-order":
    contents = contents.replace(b"name=RampTime", b"name=Temporary")
    contents = contents.replace(b"name=RampSetpoint", b"name=RampTime")
    contents = contents.replace(b"name=Temporary", b"name=RampSetpoint")
  elif invalid == "invalid-fixed-enum":
    contents = contents.replace(b"SDDS6\n", b'SDDS6\n&parameter name=Metadata, type=enum, '
                                b'enum_values="-7:retained", fixed_value=999 &end\n')
  source.write_bytes(contents)

  result = _invoke([str(_executable("setbcorr")), "-a", "-o", str(source), "-nr", "-ns", "unused", "1"],
                   _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0
  if invalid == "invalid-fixed-enum":
    assert "enum" in result.stderr.lower(), result.stderr
  else:
    assert "primitive ASCII ramp layout" in result.stderr
  assert source.read_bytes() == contents
