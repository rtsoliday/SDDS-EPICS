"""Disconnected destinations must not resize buffers loaded from snapshots."""

from __future__ import annotations

import os
from pathlib import Path

import pytest

from conftest import _executable, _invoke
from test_rampload import _isolated_ca_environment


@pytest.mark.parametrize("one_file", [False, True], ids=["per-pv", "one-file"])
@pytest.mark.parametrize("kind", ["string", "enum"])
def test_disconnected_waveform_keeps_file_element_count(
  tmp_path: Path, one_file: bool, kind: str,
) -> None:
  pv = "pytest:missing:waveform"
  if os.name == "nt" and not one_file:
    pytest.skip("per-PV snapshot filenames contain colons")
  snapshot, root = tmp_path / "snapshot.sdds", tmp_path / "waveform"
  snapshot.write_text(
    "SDDS1\n"
    "&column name=ControlName, type=string &end\n"
    "&column name=IndirectName, type=string &end\n"
    "&column name=ValueString, type=string &end\n"
    "&column name=Provider, type=string &end\n"
    "&column name=ExpectNumeric, type=character &end\n"
    "&column name=ExpectFieldType, type=string &end\n"
    "&column name=ExpectElements, type=long &end\n"
    "&data mode=ascii &end\n"
    f'1\n"{pv}" "{pv}" WaveformPV ca y scalarArray 8192\n',
    encoding="ascii",
  )
  waveform = Path(f"{root}.waveform" if one_file else f"{root}.{pv}")
  definition = ('type=enum, enum_values="-7:11"' if kind == "enum" else "type=string")
  waveform.write_text(
    "SDDS6\n"
    f'&parameter name=WaveformPV, type=string, fixed_value="{pv}" &end\n'
    f"&column name=Waveform, {definition} &end\n"
    "&data mode=ascii &end\n1\n" + ("-7\n" if kind == "enum" else "11\n"),
    encoding="ascii",
  )
  option = f"-waveform=rootname={root}" + (",onefile" if one_file else "")

  result = _invoke([str(_executable("sddspvasaverestore")), str(snapshot),
                    "-restore", option, "-pendIOtime=0.01"],
                   _isolated_ca_environment(), timeout=8)

  assert result.returncode == 1, result.stderr
  assert "NULL string passed to PrepPut" not in result.stderr
  assert f"Can't put value to {pv}. Not connected." in result.stderr
