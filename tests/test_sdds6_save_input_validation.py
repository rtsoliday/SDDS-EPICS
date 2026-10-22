"""Save operations validate enum input before opening their output files."""

from __future__ import annotations

import struct
from pathlib import Path

import pytest

from conftest import SoftIOC, _executable, _invoke
from test_rampload import _isolated_ca_environment
from test_sdds6_validation import _write_columns


@pytest.mark.parametrize("program", ["sddscasr", "sddspvasaverestore"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("field", ["CAError", "Count", "Lineage", "TimeStamp", "StartTime",
                                    "Time", "LoginID", "EffectiveUID", "GroupID", "SnapType",
                                    "SnapshotDescription", "SnapshotFilename", "RequestFile",
                                    "ElapsedTimeToCAConnect", "ElapsedTimeToSave", "PendIOTime"])
def test_save_rejects_owned_enum_fields_before_opening_output(
  tmp_path: Path, program: str, encoding: str, field: str,
) -> None:
  source, output = tmp_path / "request.sdds", tmp_path / "saved.sdds"
  columns = [("ControlName", "string", "unused")]
  parameters = ""
  kind = "Column" if field in ("CAError", "Count", "Lineage") else "Parameter"
  if kind == "Column":
    columns.append((field, "enum", -7))
  else:
    parameters = f'&parameter name={field}, type=enum, enum_values="-7:categorical", fixed_value=-7 &end\n'
  _write_columns(source, columns, encoding, parameters=parameters)
  original = b"existing output must survive a rejected request\n"
  output.write_bytes(original)

  result = _invoke([str(_executable(program)), str(source), str(output), "-save", "-pendIOtime=0.01"],
                   _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0
  assert f"{kind} {field} has enum type" in result.stderr
  assert output.read_bytes() == original


@pytest.mark.parametrize("program", ["sddscasr", "sddspvasaverestore"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_save_rejects_invalid_later_enum_page_before_opening_output(
  tmp_path: Path, program: str, encoding: str,
) -> None:
  source, output = tmp_path / "request.sdds", tmp_path / "saved.sdds"
  _write_columns(source, [("ControlName", "string", "unused"),
                          ("Metadata", "enum", -7)], encoding)
  header, page = source.read_bytes().rsplit(b"&end\n", 1)
  invalid_page = (page.replace(b"-7", b"999") if encoding == "ascii" else
                  page[:-4] + struct.pack("<i", 999))
  source.write_bytes(header + b"&end\n" + page * 2 + invalid_page)
  original = b"existing output must survive a rejected request\n"
  output.write_bytes(original)

  result = _invoke([str(_executable(program)), str(source), str(output), "-save", "-pendIOtime=0.01"],
                   _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0
  assert "enum" in result.stderr.lower(), result.stderr
  assert output.read_bytes() == original


@pytest.mark.parametrize("program", ["sddscasr", "sddspvasaverestore"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_save_validation_preserves_first_page_enum_metadata(
  soft_ioc: SoftIOC, tmp_path: Path, program: str, encoding: str,
) -> None:
  source, output = tmp_path / "request.sdds", tmp_path / "saved.sdds"
  _write_columns(source, [("ControlName", "string", soft_ioc.pv("double")),
                          ("Metadata", "enum", -7)], encoding)
  header, page = source.read_bytes().rsplit(b"&end\n", 1)
  ignored_page = (page.replace(b"-7", b"19") if encoding == "ascii" else
                  page[:-4] + struct.pack("<i", 19))
  header = header.replace(b'-7:categorical', b'-7:first,19:ignored')
  source.write_bytes(header + b"&end\n" + page + ignored_page * 2)

  soft_ioc.run(program, str(source), str(output), "-save", "-pendIOtime=2")

  result = soft_ioc.run_sdds("sdds2stream", str(output), "-columns=Metadata", "-enumCodes")
  assert result.stdout.split() == ["-7"]
