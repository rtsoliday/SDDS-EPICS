"""PV limit checks must not hide SDDS6 enum read errors."""

from pathlib import Path

import pytest

from conftest import SoftIOC, _executable, _invoke
from test_rampload import _isolated_ca_environment
from test_sdds6_page_errors import _write_invalid_metadata_page
from test_sdds6_validation import _write_columns


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("valid_first_page", [False, True], ids=["first-page", "extra-page"])
def test_pvtest_reports_invalid_enum_pages(tmp_path: Path, encoding: str, valid_first_page: bool):
  source = tmp_path / "limits.sdds"
  _write_invalid_metadata_page(source, [("ControlName", "string", "unused"),
                                       ("MinimumValue", "double", 0),
                                       ("MaximumValue", "double", 1)], encoding, valid_first_page)

  result = _invoke([str(_executable("sddspvtest")), str(source), "-time=0,seconds", "-pendIOtime=0.01"],
                   _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0
  assert "enum" in result.stderr.lower(), result.stderr
  assert "problem doing search" not in result.stderr


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("field", ["ControlName", "MinimumValue", "MaximumValue", "Monitor", "ChangeLimit"])
def test_pvtest_rejects_enum_names_limits_and_flags(tmp_path: Path, encoding: str, field: str):
  source = tmp_path / "limits.sdds"
  columns = [("ControlName", "string", "unused"), ("MinimumValue", "double", 0),
             ("MaximumValue", "double", 1)]
  columns = [(name, "enum", -7) if name == field else (name, kind, value)
             for name, kind, value in columns]
  if field in ("Monitor", "ChangeLimit"):
    columns.append((field, "enum", -7))
  _write_columns(source, columns, encoding)

  result = _invoke([str(_executable("sddspvtest")), str(source), "-time=0,seconds", "-pendIOtime=0.01"],
                   _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0
  assert f'Something wrong with column "{field}"' in result.stderr, result.stderr
  assert "problem doing search" not in result.stderr


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_pvtest_accepts_unrelated_enum_metadata(soft_ioc: SoftIOC, tmp_path: Path, encoding: str):
  source = tmp_path / "limits.sdds"
  _write_columns(source, [("ControlName", "string", soft_ioc.pv("double")),
                          ("MinimumValue", "double", -10), ("MaximumValue", "double", 10),
                          ("Metadata", "enum", -7)], encoding)
  soft_ioc.put("double", "5")

  result = soft_ioc.run("sddspvtest", str(source), "-time=0,seconds", "-pendIOtime=2")

  assert result.returncode == 0
  assert result.stderr.strip() == "0"
