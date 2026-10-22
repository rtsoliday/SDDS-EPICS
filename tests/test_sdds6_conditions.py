"""ENUM validation in shared waveform and acquisition-condition readers."""

from pathlib import Path

import pytest

from conftest import SoftIOC, _executable, _invoke
from test_rampload import _isolated_ca_environment
from test_sdds6_page_errors import _write_invalid_metadata_page
from test_sdds6_validation import _write_columns


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("with_offset", [False, True])
def test_waveform_scale_rejects_enum_even_without_offset(tmp_path: Path, encoding, with_offset):
  source = tmp_path / "waveforms.sdds"
  columns = [("WaveformPV", "string", "unused"), ("WaveformName", "string", "signal"),
             ("ReadbackScale", "enum", -7)]
  if with_offset:
    columns.append(("ReadbackOffset", "double", 0))
  _write_columns(source, columns, encoding)

  result = _invoke([str(_executable("sddswmonitor")), str(source), str(tmp_path / "output.sdds"),
                    "-steps=1", "-pendIOtime=0.01"], _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0
  assert "Column ReadbackScale has enum type" in result.stderr, result.stderr
  assert "problem doing search" not in result.stderr


@pytest.mark.parametrize("program", ["sddslogger", "sddsmonitor", "sddsstatmon"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("valid_first_page", [False, True], ids=["first-page", "later-page"])
def test_conditions_report_invalid_enum_pages(tmp_path: Path, program, encoding, valid_first_page):
  controls, conditions = tmp_path / "controls.sdds", tmp_path / "conditions.sdds"
  _write_columns(controls, [("ControlName", "string", "unused"),
                            ("ReadbackUnits", "string", "V")], encoding)
  _write_invalid_metadata_page(conditions, [("ControlName", "string", "unused"),
                                           ("LowerLimit", "double", 0),
                                           ("UpperLimit", "double", 1)], encoding, valid_first_page)

  result = _invoke([str(_executable(program)), str(controls), str(tmp_path / "output.sdds"),
                    f"-conditions={conditions},allMustPass", "-steps=1", "-pendIOtime=0.01"],
                   _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0
  assert "enum" in result.stderr.lower(), result.stderr
  assert "problem doing search" not in result.stderr


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_conditions_accept_enum_metadata(soft_ioc: SoftIOC, tmp_path: Path, encoding):
  controls, conditions = tmp_path / "controls.sdds", tmp_path / "conditions.sdds"
  soft_ioc.put("double", "0")
  _write_columns(controls, [("ControlName", "string", soft_ioc.pv("double")),
                            ("ReadbackUnits", "string", "V")], encoding)
  _write_columns(conditions, [("ControlName", "string", soft_ioc.pv("double")),
                              ("LowerLimit", "double", -1), ("UpperLimit", "double", 1),
                              ("Metadata", "enum", -7)], encoding)
  header, page = conditions.read_bytes().rsplit(b"&end\n", 1)
  conditions.write_bytes(header + b"&end\n" + page * 2)

  soft_ioc.run("sddslogger", str(controls), str(tmp_path / "output.sdds"),
               f"-conditions={conditions},allMustPass", "-steps=1", "-pendIOtime=2")
