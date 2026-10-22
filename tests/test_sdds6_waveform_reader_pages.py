"""Waveform-list readers validate every categorical page before connecting."""

from __future__ import annotations

import struct
from pathlib import Path

import pytest

from conftest import SoftIOC, _executable, _invoke
from test_rampload import _isolated_ca_environment


@pytest.mark.parametrize("program", ["sddswget", "sddswgetdata"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_waveform_list_rejects_invalid_third_enum_page(
  tmp_path: Path, program: str, encoding: str,
) -> None:
  source = tmp_path / "waveforms.sdds"
  header = (
    "SDDS6\n"
    '&column name=WaveformPV, type=string &end\n'
    '&column name=Metadata, type=enum, enum_values="-7:valid" &end\n'
    f"&data mode={encoding}, endian=little &end\n"
  )

  def page(code: int) -> bytes:
    return (f"1\nunused {code}\n".encode("ascii") if encoding == "ascii"
            else struct.pack("<2i", 1, 6) + b"unused" + struct.pack("<i", code))

  source.write_bytes(header.encode("ascii") + page(-7) * 2 + page(999))
  arguments = [str(source), str(tmp_path / "output.sdds"),
               "-inputStyle=wmonitor", "-pendIOtime=0.01"]
  if program == "sddswget":
    arguments.append("-provider=ca")

  result = _invoke([str(_executable(program)), *arguments],
                   _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0
  assert "enum" in result.stderr.lower(), result.stderr
  assert "problem doing search" not in result.stderr


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_waveform_list_validation_preserves_first_page_metadata(
  soft_ioc: SoftIOC, tmp_path: Path, encoding: str,
) -> None:
  source, output = tmp_path / "waveforms.sdds", tmp_path / "output.sdds"
  pv = soft_ioc.pv("array")
  header = (
    "SDDS6\n"
    '&column name=WaveformPV, type=string &end\n'
    '&column name=Metadata, type=enum, enum_values="-7:first,19:ignored" &end\n'
    f"&data mode={encoding}, endian=little &end\n"
  )

  def page(code: int) -> bytes:
    encoded = pv.encode("ascii")
    return (f'1\n"{pv}" {code}\n'.encode("ascii") if encoding == "ascii"
            else struct.pack("<2i", 1, len(encoded)) + encoded + struct.pack("<i", code))

  source.write_bytes(header.encode("ascii") + page(-7) + page(19) * 3)

  soft_ioc.run("sddswget", str(source), str(output), "-inputStyle=wmonitor",
               "-provider=ca", "-pendIOtime=2")

  result = soft_ioc.run_sdds("sdds2stream", str(output), "-parameters=Metadata", "-enumCodes")
  assert result.stdout.split() == ["-7"]
