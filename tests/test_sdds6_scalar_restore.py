"""Scalar restore values resolve SDDS6 dictionaries before any PV writes."""

from __future__ import annotations

import struct
from pathlib import Path

import pytest

from conftest import SoftIOC, _executable, _invoke
from test_rampload import _isolated_ca_environment


def _write_scalar_snapshot(path: Path, rows: list[tuple[str, int]], labels: list[str], encoding: str) -> None:
  codes = [-7, 101]
  # Enum-mapping escaping and outer namelist escaping are separate layers.
  escaped_labels = [label.replace("\\", "\\\\").replace('"', '\\"') for label in labels]
  mapping = ",".join(f"{code}:{label}" for code, label in zip(codes, escaped_labels))
  mapping = mapping.replace("\\", "\\\\").replace('"', '\\"')
  header = (
    "SDDS6\n"
    "&column name=ControlName, type=string &end\n"
    f'&column name=ValueString, type=enum, enum_values="{mapping}" &end\n'
    f"&data mode={encoding}, endian=little &end\n"
  )
  if encoding == "ascii":
    data = (str(len(rows)) + "\n" + "".join(f'"{pv}" {code}\n' for pv, code in rows)).encode("ascii")
  else:
    data = struct.pack("<i", len(rows))
    for pv, code in rows:
      name = pv.encode("ascii")
      data += struct.pack("<i", len(name)) + name + struct.pack("<i", code)
  path.write_bytes(header.encode("utf-8") + data)


@pytest.mark.parametrize("program", ["sddscasr", "sddspvasaverestore"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("target,label", [("enum", "ON"), ("text", "123"),
                                         ("text", '"quoted"'), ("text", "hello world"),
                                         ("text", "é" * 19)])
def test_scalar_enum_restore_uses_exact_labels(
  soft_ioc: SoftIOC, tmp_path: Path, program: str, encoding: str, target: str, label: str,
) -> None:
  source = tmp_path / "snapshot.sdds"
  _write_scalar_snapshot(source, [(soft_ioc.pv(target), -7)], [label], encoding)
  soft_ioc.put(target, "OFF" if target == "enum" else "unchanged")

  soft_ioc.run(program, str(source), "-restore", "-pendIOtime=2")

  readback = soft_ioc.run("cavget", f"-list={soft_ioc.pv(target)}", "-noQuotes", "-pendIOtime=2")
  assert readback.stdout.rstrip("\n") == label


@pytest.mark.parametrize("program", ["sddscasr", "sddspvasaverestore"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("label", ["x" * 39, "x" * 40, "é" * 20],
                         ids=["fits", "too-long", "too-long-utf8"])
def test_scalar_enum_restore_validates_all_ca_label_lengths(
  soft_ioc: SoftIOC, tmp_path: Path, program: str, encoding: str, label: str,
) -> None:
  source = tmp_path / "snapshot.sdds"
  _write_scalar_snapshot(source, [(soft_ioc.pv("enum"), -7), (soft_ioc.pv("text"), 101)],
                         ["ON", label], encoding)
  soft_ioc.put("enum", "OFF")
  soft_ioc.put("text", "unchanged")

  result = soft_ioc.invoke(program, str(source), "-restore", "-pendIOtime=2")

  if len(label.encode("utf-8")) < 40:
    assert result.returncode == 0, result.stderr
    assert soft_ioc.get("enum") == "ON"
    assert soft_ioc.get("text") == label
  else:
    assert result.returncode > 0
    assert "Channel Access string length limit" in result.stderr
    assert soft_ioc.get("enum") == "OFF"
    assert soft_ioc.get("text") == "unchanged"


@pytest.mark.parametrize("program", ["sddscasr", "sddspvasaverestore"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_scalar_enum_restore_rejects_undefined_keys_before_writing(
  soft_ioc: SoftIOC, tmp_path: Path, program: str, encoding: str,
) -> None:
  source = tmp_path / "snapshot.sdds"
  _write_scalar_snapshot(source, [(soft_ioc.pv("enum"), -7), (soft_ioc.pv("text"), 999)],
                         ["ON", "changed"], encoding)
  soft_ioc.put("enum", "OFF")
  soft_ioc.put("text", "unchanged")

  result = soft_ioc.invoke(program, str(source), "-restore", "-pendIOtime=2")

  assert result.returncode > 0
  assert "enum" in result.stderr.lower()
  assert soft_ioc.get("enum") == "OFF"
  assert soft_ioc.get("text") == "unchanged"


@pytest.mark.parametrize("program", ["sddscasr", "sddspvasaverestore"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("extra_valid_pages", [0, 1])
def test_scalar_restore_validates_ignored_enum_pages_before_writing(
  soft_ioc: SoftIOC, tmp_path: Path, program: str, encoding: str, extra_valid_pages: int,
) -> None:
  source = tmp_path / "snapshot.sdds"
  _write_scalar_snapshot(source, [(soft_ioc.pv("enum"), -7)], ["ON", "OFF"], encoding)
  header, page = source.read_bytes().rsplit(b"&end\n", 1)
  invalid_page = (page.replace(b"-7", b"999") if encoding == "ascii" else
                  page[:-4] + struct.pack("<i", 999))
  source.write_bytes(header + b"&end\n" + page * (1 + extra_valid_pages) + invalid_page)
  soft_ioc.put("enum", "OFF")

  result = soft_ioc.invoke(program, str(source), "-restore", "-pendIOtime=2")

  assert result.returncode > 0, result.stdout + result.stderr
  assert "enum" in result.stderr.lower(), result.stderr
  assert soft_ioc.get("enum") == "OFF"


@pytest.mark.parametrize("program", ["sddscasr", "sddspvasaverestore"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_scalar_restore_keeps_first_page_values_when_validating_extra_pages(
  soft_ioc: SoftIOC, tmp_path: Path, program: str, encoding: str,
) -> None:
  source = tmp_path / "snapshot.sdds"
  _write_scalar_snapshot(source, [(soft_ioc.pv("enum"), -7)], ["ON", "OFF"], encoding)
  header, page = source.read_bytes().rsplit(b"&end\n", 1)
  ignored_page = (page.replace(b"-7", b"101") if encoding == "ascii" else
                  page[:-4] + struct.pack("<i", 101))
  source.write_bytes(header + b"&end\n" + page + ignored_page * 2)
  soft_ioc.put("enum", "OFF")

  soft_ioc.run(program, str(source), "-restore", "-pendIOtime=2")

  assert soft_ioc.get("enum") == "ON"


@pytest.mark.parametrize("program", ["sddscasr", "sddspvasaverestore"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_save_rejects_enum_value_storage(tmp_path: Path, program: str, encoding: str) -> None:
  source = tmp_path / "request.sdds"
  output = tmp_path / "saved.sdds"
  _write_scalar_snapshot(source, [("unused", -7)], ["ON"], encoding)

  result = _invoke([str(_executable(program)), str(source), str(output), "-save", "-pendIOtime=0.01"],
                   _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0
  assert "Column ValueString has enum type" in result.stderr
  assert not output.exists()
