"""SDDS6 waveform restores use dictionary labels with the isolated local IOC."""

from __future__ import annotations

import os
import struct
from pathlib import Path

import pytest

from conftest import SoftIOC


MAPPING = "-7:11,19:22,101:33,-42:44"
CODES = (-7, 19, 101, -42)


def _write_waveform(path: Path, pv: str, encoding: str, mapping: str = MAPPING) -> None:
  header = (
    "SDDS6\n"
    f'&parameter name=WaveformPV, type=string, fixed_value="{pv}" &end\n'
    f'&column name=Waveform, type=enum, enum_values="{mapping}" &end\n'
    f"&data mode={encoding}, endian=little &end\n"
  )
  if encoding == "ascii":
    data = ("4\n" + "\n".join(map(str, CODES)) + "\n").encode("ascii")
  else:
    data = struct.pack("<5i", 4, *CODES)
  path.write_bytes(header.encode("ascii") + data)


def _write_snapshot(path: Path, pv: str) -> None:
  path.write_text(
    "SDDS1\n"
    "&column name=ControlName, type=string &end\n"
    "&column name=IndirectName, type=string &end\n"
    "&column name=ValueString, type=string &end\n"
    "&data mode=ascii &end\n"
    f'1\n"{pv}" "{pv}" WaveformPV\n',
    encoding="ascii",
  )


def _set_zero_waveform(soft_ioc: SoftIOC, path: Path) -> None:
  path.write_text(
    "SDDS1\n"
    f'&parameter name=WaveformPV, type=string, fixed_value="{soft_ioc.pv("array")}" &end\n'
    "&column name=Waveform, type=double &end\n"
    "&data mode=ascii &end\n"
    "4\n0\n0\n0\n0\n",
    encoding="ascii",
  )
  soft_ioc.run("sddswput", str(path), "-pendIOtime=2")


def _read_waveform(soft_ioc: SoftIOC, path: Path) -> list[float]:
  soft_ioc.run(
    "sddswget",
    f"-PVnames={soft_ioc.pv('array')}",
    str(path),
    "-provider=ca",
    "-pendIOtime=2",
  )
  result = soft_ioc.run_sdds("sdds2stream", str(path), "-columns=Waveform")
  return [float(value) for value in result.stdout.split()]


@pytest.mark.parametrize(
  ("program", "encoding"),
  [("sddscasr", "binary"), ("sddspvasaverestore", "ascii"), ("sddspvasaverestore", "binary")],
)
@pytest.mark.parametrize("one_file", [False, True], ids=["per-pv", "one-file"])
def test_enum_waveform_restore_uses_labels(
  soft_ioc: SoftIOC,
  tmp_path: Path,
  program: str,
  one_file: bool,
  encoding: str,
) -> None:
  if os.name == "nt" and not one_file:
    pytest.skip("per-PV snapshot filenames contain colons from the test IOC PV names")
  snapshot = tmp_path / "snapshot.sdds"
  waveform_root = tmp_path / "waveform"
  pv = soft_ioc.pv("array")
  waveform = Path(f"{waveform_root}.waveform" if one_file else f"{waveform_root}.{pv}")
  _write_snapshot(snapshot, pv)
  _write_waveform(waveform, pv, encoding)
  _set_zero_waveform(soft_ioc, tmp_path / "reset.sdds")
  option = f"-waveform=rootname={waveform_root}" + (",onefile" if one_file else "")

  soft_ioc.run(program, str(snapshot), "-restore", option, "-pendIOtime=2")

  assert _read_waveform(soft_ioc, tmp_path / "readback.sdds") == pytest.approx([11, 22, 33, 44])


@pytest.mark.parametrize("one_file", [False, True], ids=["per-pv", "one-file"])
def test_casr_rejects_long_enum_labels_before_writing(
  soft_ioc: SoftIOC,
  tmp_path: Path,
  one_file: bool,
) -> None:
  if os.name == "nt" and not one_file:
    pytest.skip("per-PV snapshot filenames contain colons from the test IOC PV names")
  snapshot = tmp_path / "snapshot.sdds"
  waveform_root = tmp_path / "waveform"
  pv = soft_ioc.pv("array")
  waveform = Path(f"{waveform_root}.waveform" if one_file else f"{waveform_root}.{pv}")
  _write_snapshot(snapshot, pv)
  _write_waveform(waveform, pv, "ascii", f"-7:{'x' * 40},19:22,101:33,-42:44")
  _set_zero_waveform(soft_ioc, tmp_path / "reset.sdds")
  option = f"-waveform=rootname={waveform_root}" + (",onefile" if one_file else "")

  result = soft_ioc.invoke("sddscasr", str(snapshot), "-restore", option, "-pendIOtime=2")

  assert result.returncode > 0
  assert "Channel Access string length limit" in result.stderr
  assert _read_waveform(soft_ioc, tmp_path / "readback.sdds") == pytest.approx([0, 0, 0, 0])


@pytest.mark.parametrize("program", ["sddswput", "sddscaramp"])
def test_scalar_enum_label_cannot_overflow_ca_string(
  soft_ioc: SoftIOC,
  tmp_path: Path,
  program: str,
) -> None:
  source = tmp_path / "long-label.sdds"
  pv = soft_ioc.pv("text")
  if program == "sddswput":
    fields = f'&parameter name=WaveformPV, type=string, fixed_value="{pv}" &end\n'
    column = "Waveform"
    row = "-7"
    arguments = [str(source)]
  else:
    fields = "&column name=ControlName, type=string &end\n"
    column = "Value"
    row = f'"{pv}" -7'
    arguments = [f"-rampTo={source},steps=1,pause=0"]
  source.write_text(
    "SDDS6\n"
    + fields
    + f'&column name={column}, type=enum, enum_values="-7:{"x" * 40}" &end\n'
    + f"&data mode=ascii &end\n1\n{row}\n",
    encoding="ascii",
  )
  soft_ioc.put("text", "unchanged")

  result = soft_ioc.invoke(program, *arguments, "-pendIOtime=2")

  assert result.returncode > 0
  assert "Channel Access string length limit" in result.stderr
  assert soft_ioc.get("text") == "unchanged"


@pytest.mark.parametrize("one_file", [False, True], ids=["per-pv", "one-file"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("label", ["x" * 39, "x" * 40, "é" * 20],
                         ids=["fits", "too-long", "too-long-utf8"])
def test_pvasaverestore_validates_ca_enum_labels_before_writing(
  soft_ioc: SoftIOC, tmp_path: Path, one_file: bool, encoding: str, label: str
) -> None:
  if os.name == "nt" and not one_file:
    pytest.skip("per-PV snapshot filenames contain colons from the test IOC PV names")
  snapshot, root = tmp_path / "snapshot.sdds", tmp_path / "waveform"
  pvs = [soft_ioc.pv("array"), soft_ioc.pv("text")]
  snapshot.write_text(
    "SDDS1\n"
    "&column name=ControlName, type=string &end\n"
    "&column name=IndirectName, type=string &end\n"
    "&column name=ValueString, type=string &end\n"
    "&data mode=ascii &end\n2\n"
    + "".join(f'"{pv}" "{pv}" WaveformPV\n' for pv in pvs),
    encoding="ascii",
  )
  header = (
    "SDDS6\n"
    '&parameter name=WaveformPV, type=string &end\n'
    f'&column name=Waveform, type=enum, enum_values="-7:11,19:{label}" &end\n'
    f"&data mode={encoding}, endian=little &end\n"
  )
  pages = []
  for pv, code in zip(pvs, [-7, 19]):
    name = pv.encode("ascii")
    data = (f'"{pv}"\n1\n{code}\n'.encode("ascii") if encoding == "ascii"
            else struct.pack("<2i", 1, len(name)) + name + struct.pack("<i", code))
    pages.append(data)
    if not one_file:
      Path(f"{root}.{pv}").write_bytes(header.encode("utf-8") + data)
  if one_file:
    Path(f"{root}.waveform").write_bytes(header.encode("utf-8") + b"".join(pages))
  _set_zero_waveform(soft_ioc, tmp_path / "reset.sdds")
  soft_ioc.put("text", "unchanged")
  option = f"-waveform=rootname={root}" + (",onefile" if one_file else "")

  result = soft_ioc.invoke("sddspvasaverestore", str(snapshot), "-restore", option, "-pendIOtime=2")

  if len(label.encode("utf-8")) < 40:
    assert result.returncode == 0, result.stderr
    assert _read_waveform(soft_ioc, tmp_path / "readback.sdds") == pytest.approx([11, 0, 0, 0])
    assert soft_ioc.get("text") == label
  else:
    assert result.returncode > 0
    assert _read_waveform(soft_ioc, tmp_path / "readback.sdds") == pytest.approx([0, 0, 0, 0])
    assert soft_ioc.get("text") == "unchanged"
    assert "Channel Access string length limit" in result.stderr


@pytest.mark.parametrize("program", ["sddswput", "sddscasr", "sddspvasaverestore"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("one_file", [False, True], ids=["per-pv", "one-file"])
def test_invalid_later_enum_page_prevents_waveform_writes(
  soft_ioc: SoftIOC, tmp_path: Path, program: str, encoding: str, one_file: bool
) -> None:
  if program == "sddswput" and not one_file:
    pytest.skip("sddswput always reads a single waveform file")
  if os.name == "nt" and not one_file:
    pytest.skip("per-PV snapshot filenames contain colons from the test IOC PV names")
  root = tmp_path / "waveform"
  pv = soft_ioc.pv("array")
  source = Path(f"{root}.waveform" if one_file else f"{root}.{pv}")
  _write_waveform(source, pv, encoding)
  # The valid first page must not be restored when a later page has an
  # undefined dictionary key. EOF and a failed page read are different results.
  invalid_page = b"4\n-7\n19\n101\n999\n" if encoding == "ascii" else struct.pack("<5i", 4, -7, 19, 101, 999)
  source.write_bytes(source.read_bytes() + invalid_page)
  _set_zero_waveform(soft_ioc, tmp_path / "reset.sdds")
  if program == "sddswput":
    arguments = [str(source)]
  else:
    snapshot = tmp_path / "snapshot.sdds"
    _write_snapshot(snapshot, pv)
    option = f"-waveform=rootname={root}" + (",onefile" if one_file else "")
    arguments = [str(snapshot), "-restore", option]

  result = soft_ioc.invoke(program, *arguments, "-pendIOtime=2")

  assert result.returncode > 0
  assert "enum" in result.stderr.lower()
  assert _read_waveform(soft_ioc, tmp_path / "readback.sdds") == pytest.approx([0, 0, 0, 0])
