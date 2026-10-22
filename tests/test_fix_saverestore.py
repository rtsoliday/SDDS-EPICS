"""Regression tests for the save/restore and waveform put/get fixes after the SDDS6 audit.

Enum-free (legacy) inputs must behave as they did before SDDS6 support was
added, while files that define enums keep their dictionary validation.
"""

from __future__ import annotations

import os
from pathlib import Path

import pytest

from conftest import SoftIOC
from test_sdds6_restore import _read_waveform


RESTORE_PROGRAMS = ["sddscasr", "sddspvasaverestore"]


def _write_text(path: Path, text: str) -> None:
  path.write_bytes(text.encode("utf-8"))


def _scalar_snapshot(path: Path, rows: list[tuple[str, str]], trailing: str = "") -> None:
  """Write an enum-free ASCII snapshot without IndirectName (all rows are scalar)."""
  _write_text(path,
              "SDDS1\n"
              "&column name=ControlName, type=string &end\n"
              "&column name=ValueString, type=string &end\n"
              "&data mode=ascii &end\n"
              f"{len(rows)}\n" + "".join(f'"{pv}" "{value}"\n' for pv, value in rows) + trailing)


def _waveform_snapshot(path: Path, pv: str, value_column: str = "&column name=ValueString, type=string &end\n",
                       value: str = "WaveformPV") -> None:
  version = 6 if "type=enum" in value_column else 1
  _write_text(path,
              f"SDDS{version}\n"
              "&column name=ControlName, type=string &end\n"
              "&column name=IndirectName, type=string &end\n"
              + value_column +
              "&data mode=ascii &end\n"
              f'1\n"{pv}" "{pv}" {value}\n')


def _waveform_file(path: Path, pv: str, values: list[str], column_type: str = "double", trailing: str = "") -> None:
  _write_text(path,
              "SDDS1\n"
              f'&parameter name=WaveformPV, type=string, fixed_value="{pv}" &end\n'
              f"&column name=Waveform, type={column_type} &end\n"
              "&data mode=ascii &end\n"
              f"{len(values)}\n" + "".join(f'"{value}"\n' if column_type == "string" else f"{value}\n"
                                           for value in values) + trailing)


def _set_array(soft_ioc: SoftIOC, tmp_path: Path, values: list[float]) -> None:
  source = tmp_path / "set-array.sdds"
  _waveform_file(source, soft_ioc.pv("array"), [str(value) for value in values])
  soft_ioc.run("sddswput", str(source), "-pendIOtime=2")


def _waveform_paths(tmp_path: Path, pv: str, one_file: bool) -> tuple[Path, str]:
  root = tmp_path / "waveform"
  path = Path(f"{root}.waveform" if one_file else f"{root}.{pv}")
  return path, f"-waveform=rootname={root}" + (",onefile" if one_file else "")


def _skip_per_pv_on_windows(one_file: bool) -> None:
  if os.name == "nt" and not one_file:
    pytest.skip("per-PV snapshot filenames contain colons from the test IOC PV names")


# --- Enum-free inputs ending in a blank line are accepted, as before SDDS6 ---------------------

@pytest.mark.parametrize("program", RESTORE_PROGRAMS)
def test_restore_accepts_legacy_snapshot_with_trailing_blank_line(
  soft_ioc: SoftIOC, tmp_path: Path, program: str,
) -> None:
  snapshot = tmp_path / "snapshot.sdds"
  _scalar_snapshot(snapshot, [(soft_ioc.pv("double"), "3.5")], trailing="\n")
  soft_ioc.put("double", "0")

  soft_ioc.run(program, str(snapshot), "-restore", "-pendIOtime=2")

  assert float(soft_ioc.get("double")) == pytest.approx(3.5)


@pytest.mark.parametrize("program", RESTORE_PROGRAMS)
@pytest.mark.parametrize("one_file", [False, True], ids=["per-pv", "one-file"])
def test_restore_accepts_legacy_waveform_file_with_trailing_blank_line(
  soft_ioc: SoftIOC, tmp_path: Path, program: str, one_file: bool,
) -> None:
  _skip_per_pv_on_windows(one_file)
  pv = soft_ioc.pv("array")
  snapshot = tmp_path / "snapshot.sdds"
  waveform, option = _waveform_paths(tmp_path, pv, one_file)
  _waveform_snapshot(snapshot, pv)
  _waveform_file(waveform, pv, ["1", "2", "3", "4"], trailing="\n")
  _set_array(soft_ioc, tmp_path, [0, 0, 0, 0])

  soft_ioc.run(program, str(snapshot), "-restore", option, "-pendIOtime=2")

  assert _read_waveform(soft_ioc, tmp_path / "readback.sdds") == pytest.approx([1, 2, 3, 4])


def test_sddswput_accepts_legacy_file_with_trailing_blank_line(soft_ioc: SoftIOC, tmp_path: Path) -> None:
  source = tmp_path / "waveform.sdds"
  _waveform_file(source, soft_ioc.pv("array"), ["4", "3", "2", "1"], trailing="\n")
  _set_array(soft_ioc, tmp_path, [0, 0, 0, 0])

  soft_ioc.run("sddswput", str(source), "-pendIOtime=2")

  assert _read_waveform(soft_ioc, tmp_path / "readback.sdds") == pytest.approx([4, 3, 2, 1])


@pytest.mark.parametrize("program", ["sddswget", "sddswgetdata"])
@pytest.mark.parametrize("style", ["wmonitor", "wget"])
def test_waveform_readers_accept_legacy_lists_with_trailing_blank_line(
  soft_ioc: SoftIOC, tmp_path: Path, program: str, style: str,
) -> None:
  source, output = tmp_path / "list.sdds", tmp_path / "output.sdds"
  pv = soft_ioc.pv("array")
  if style == "wmonitor":
    text = ("SDDS1\n&column name=WaveformPV, type=string &end\n&data mode=ascii &end\n"
            f'1\n"{pv}"\n\n')
  else:
    text = ("SDDS1\n"
            f'&parameter name=WaveformPV, type=string, fixed_value="{pv}" &end\n'
            "&column name=Dummy, type=double &end\n&data mode=ascii &end\n4\n0\n0\n0\n0\n\n")
  _write_text(source, text)
  _set_array(soft_ioc, tmp_path, [1, 2, 3, 4])
  arguments = [str(source), str(output), f"-inputStyle={style}", "-pendIOtime=2"]
  if program == "sddswget":
    arguments.append("-provider=ca")

  result = soft_ioc.run(program, *arguments)

  assert "extra pages" not in result.stderr, result.stderr
  assert output.exists()


@pytest.mark.parametrize("program", ["sddswget", "sddswgetdata"])
def test_waveform_readers_do_not_warn_about_legacy_extra_pages(
  soft_ioc: SoftIOC, tmp_path: Path, program: str,
) -> None:
  source, output = tmp_path / "list.sdds", tmp_path / "output.sdds"
  pv = soft_ioc.pv("array")
  page = f'1\n"{pv}"\n'
  _write_text(source, "SDDS1\n&column name=WaveformPV, type=string &end\n&data mode=ascii &end\n" + page * 2)
  arguments = [str(source), str(output), "-inputStyle=wmonitor", "-pendIOtime=2"]
  if program == "sddswget":
    arguments.append("-provider=ca")

  result = soft_ioc.run(program, *arguments)

  assert "extra pages" not in result.stderr, result.stderr


# --- A scalar row for an array PV is reported and not written ----------------------------------

@pytest.mark.parametrize("program", RESTORE_PROGRAMS)
def test_restore_reports_scalar_row_for_array_pv(soft_ioc: SoftIOC, tmp_path: Path, program: str) -> None:
  snapshot = tmp_path / "snapshot.sdds"
  _scalar_snapshot(snapshot, [(soft_ioc.pv("double"), "7"), (soft_ioc.pv("array"), "9")])
  soft_ioc.put("double", "0")
  _set_array(soft_ioc, tmp_path, [1, 2, 3, 4])

  result = soft_ioc.invoke(program, str(snapshot), "-restore", "-pendIOtime=2")

  assert "scalar value" in result.stderr and "4 elements" in result.stderr, result.stderr
  assert float(soft_ioc.get("double")) == pytest.approx(7)
  assert _read_waveform(soft_ioc, tmp_path / "readback.sdds") == pytest.approx([1, 2, 3, 4])


# --- A waveform file longer than the PV is truncated with a warning ----------------------------

@pytest.mark.parametrize("program", RESTORE_PROGRAMS)
@pytest.mark.parametrize("one_file", [False, True], ids=["per-pv", "one-file"])
def test_restore_truncates_waveform_file_longer_than_pv(
  soft_ioc: SoftIOC, tmp_path: Path, program: str, one_file: bool,
) -> None:
  _skip_per_pv_on_windows(one_file)
  pv = soft_ioc.pv("array")
  snapshot = tmp_path / "snapshot.sdds"
  waveform, option = _waveform_paths(tmp_path, pv, one_file)
  _waveform_snapshot(snapshot, pv)
  _waveform_file(waveform, pv, ["5", "6", "7", "8", "9", "10"])
  _set_array(soft_ioc, tmp_path, [0, 0, 0, 0])

  result = soft_ioc.run(program, str(snapshot), "-restore", option, "-pendIOtime=2")

  assert "only the first 4" in result.stderr, result.stderr
  assert _read_waveform(soft_ioc, tmp_path / "readback.sdds") == pytest.approx([5, 6, 7, 8])


# --- Plain string waveform elements longer than the CA limit are truncated ---------------------

def test_sddswput_truncates_long_plain_strings(soft_ioc: SoftIOC, tmp_path: Path) -> None:
  source = tmp_path / "text.sdds"
  _waveform_file(source, soft_ioc.pv("text"), ["x" * 45], column_type="string")
  soft_ioc.put("text", "unchanged")

  soft_ioc.run("sddswput", str(source), "-pendIOtime=2")

  assert soft_ioc.get("text") == "x" * 39


@pytest.mark.parametrize("one_file", [False, True], ids=["per-pv", "one-file"])
def test_casr_truncates_long_plain_waveform_strings(soft_ioc: SoftIOC, tmp_path: Path, one_file: bool) -> None:
  _skip_per_pv_on_windows(one_file)
  pv = soft_ioc.pv("text")
  snapshot = tmp_path / "snapshot.sdds"
  waveform, option = _waveform_paths(tmp_path, pv, one_file)
  _waveform_snapshot(snapshot, pv)
  _waveform_file(waveform, pv, ["y" * 45], column_type="string")
  soft_ioc.put("text", "unchanged")

  soft_ioc.run("sddscasr", str(snapshot), "-restore", option, "-pendIOtime=2")

  assert soft_ioc.get("text") == "y" * 39


# --- Long enum ValueString labels on waveform rows are not checked -----------------------------

@pytest.mark.parametrize("program", RESTORE_PROGRAMS)
def test_restore_ignores_long_enum_value_on_waveform_rows(soft_ioc: SoftIOC, tmp_path: Path, program: str) -> None:
  if os.name == "nt":
    pytest.skip("per-PV snapshot filenames contain colons from the test IOC PV names")
  pv = soft_ioc.pv("array")
  snapshot = tmp_path / "snapshot.sdds"
  waveform, option = _waveform_paths(tmp_path, pv, False)
  label = "/long/path/to/the/saved/waveform/file." + "z" * 20
  _waveform_snapshot(snapshot, pv, f'&column name=ValueString, type=enum, enum_values="-7:{label}" &end\n', "-7")
  _waveform_file(waveform, pv, ["1", "2", "3", "4"])
  _set_array(soft_ioc, tmp_path, [0, 0, 0, 0])

  soft_ioc.run(program, str(snapshot), "-restore", option, "-pendIOtime=2")

  assert _read_waveform(soft_ioc, tmp_path / "readback.sdds") == pytest.approx([1, 2, 3, 4])


# --- sddscaramp checks every file's labels before ramping --------------------------------------

def test_caramp_rejects_long_label_in_later_file_before_ramping(soft_ioc: SoftIOC, tmp_path: Path) -> None:
  first, second = tmp_path / "first.sdds", tmp_path / "second.sdds"
  _write_text(first,
              "SDDS1\n&column name=ControlName, type=string &end\n&column name=Value, type=double &end\n"
              f'&data mode=ascii &end\n1\n"{soft_ioc.pv("double")}" 5\n')
  _write_text(second,
              "SDDS6\n&column name=ControlName, type=string &end\n"
              f'&column name=Value, type=enum, enum_values="-7:{"x" * 40}" &end\n'
              f'&data mode=ascii &end\n1\n"{soft_ioc.pv("text")}" -7\n')
  soft_ioc.put("double", "0")
  soft_ioc.put("text", "unchanged")

  result = soft_ioc.invoke("sddscaramp", f"-rampTo={first},steps=1,pause=0",
                           f"-rampTo={second},steps=1,pause=0", "-pendIOtime=2")

  assert result.returncode > 0
  assert "Channel Access string length limit" in result.stderr
  assert float(soft_ioc.get("double")) == pytest.approx(0)
  assert soft_ioc.get("text") == "unchanged"


# --- -add and -restore=verify reformat values without overflowing their buffers ----------------

def test_casr_add_restores_sum(soft_ioc: SoftIOC, tmp_path: Path) -> None:
  snapshot = tmp_path / "snapshot.sdds"
  _scalar_snapshot(snapshot, [(soft_ioc.pv("double"), "1.25")])
  soft_ioc.put("double", "2")

  soft_ioc.run("sddscasr", str(snapshot), "-restore", "-add", "-pendIOtime=2")

  assert float(soft_ioc.get("double")) == pytest.approx(3.25)


def test_casr_verify_restores_short_values(soft_ioc: SoftIOC, tmp_path: Path) -> None:
  snapshot = tmp_path / "snapshot.sdds"
  _scalar_snapshot(snapshot, [(soft_ioc.pv("double"), "4"), (soft_ioc.pv("integer"), "3")])
  soft_ioc.put("double", "0")
  soft_ioc.put("integer", "0")

  result = soft_ioc.run("sddscasr", str(snapshot), "-restore=verify", "-pendIOtime=2")

  assert "did not accept" not in result.stderr, result.stderr
  assert float(soft_ioc.get("double")) == pytest.approx(4)
  assert float(soft_ioc.get("integer")) == pytest.approx(3)
