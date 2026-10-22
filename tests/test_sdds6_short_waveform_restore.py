"""Restore categorical waveforms independently of the destination's capacity."""

from __future__ import annotations

import os
import struct
from pathlib import Path

import pytest

from conftest import SoftIOC
from test_sdds6_restore import _read_waveform, _set_zero_waveform, _write_snapshot


@pytest.mark.parametrize("program", ["sddscasr", "sddspvasaverestore"])
@pytest.mark.parametrize("one_file", [False, True], ids=["per-pv", "one-file"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("kind", ["enum", "string"])
@pytest.mark.parametrize("target,labels", [("enum", ["ON"]), ("array", ["11"]),
                                         ("array", ["11", "22"])])
def test_short_categorical_waveform_restore(
  soft_ioc: SoftIOC, tmp_path: Path, program: str, one_file: bool,
  encoding: str, kind: str, target: str, labels: list[str],
) -> None:
  if os.name == "nt" and not one_file:
    pytest.skip("per-PV snapshot filenames contain colons from the test IOC PV names")
  pv = soft_ioc.pv(target)
  snapshot, root = tmp_path / "snapshot.sdds", tmp_path / "waveform"
  waveform = Path(f"{root}.waveform" if one_file else f"{root}.{pv}")
  _write_snapshot(snapshot, pv)
  codes = [-7, 101][:len(labels)]
  mapping = ",".join(f"{code}:{label}" for code, label in zip(codes, labels))
  definition = f'type=enum, enum_values="{mapping}"' if kind == "enum" else "type=string"
  header = (
    "SDDS6\n"
    f'&parameter name=WaveformPV, type=string, fixed_value="{pv}" &end\n'
    f"&column name=Waveform, {definition} &end\n"
    f"&data mode={encoding}, endian=little &end\n"
  )
  if encoding == "ascii":
    values = [str(code) for code in codes] if kind == "enum" else labels
    data = (str(len(values)) + "\n" + "\n".join(values) + "\n").encode("ascii")
  else:
    data = struct.pack("<i", len(labels))
    if kind == "enum":
      data += struct.pack(f"<{len(codes)}i", *codes)
    else:
      for label in labels:
        value = label.encode("ascii")
        data += struct.pack("<i", len(value)) + value
  waveform.write_bytes(header.encode("ascii") + data)
  if target == "enum":
    soft_ioc.put("enum", "OFF")
  else:
    _set_zero_waveform(soft_ioc, tmp_path / "reset.sdds")
  option = f"-waveform=rootname={root}" + (",onefile" if one_file else "")

  soft_ioc.run(program, str(snapshot), "-restore", option, "-pendIOtime=2")

  if target == "enum":
    assert soft_ioc.get("enum") == "ON"
  else:
    values = _read_waveform(soft_ioc, tmp_path / "readback.sdds")
    assert values[:len(labels)] == pytest.approx([float(label) for label in labels])
