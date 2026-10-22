"""Feedback matrices must not silently lose categorical readback columns."""

from pathlib import Path

import pytest

from conftest import SoftIOC, _executable, _invoke
from test_rampload import _isolated_ca_environment
from test_sdds6_validation import _write_columns


@pytest.mark.parametrize("program", ["sddscontrollaw", "sddspvacontrollaw"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("auxiliary", [False, True], ids=["primary", "auxiliary"])
def test_feedback_matrices_reject_enum_readbacks(
  tmp_path: Path, program: str, encoding: str, auxiliary: bool,
) -> None:
  primary, categorical = tmp_path / "primary.sdds", tmp_path / "categorical.sdds"
  columns = [("Actuator", "string", "unused"), ("Readback", "double", 1)]
  _write_columns(primary, columns, encoding)
  _write_columns(categorical, columns + [("CategoricalReadback", "enum", -7)], encoding)
  arguments = ([str(primary), f"-auxiliaryOutput=matrixFile={categorical}"] if auxiliary
               else [str(categorical)])

  result = _invoke([str(_executable(program)), *arguments, "-dryRun", "-steps=1",
                    "-interval=0.001", "-pendIOtime=0.01"],
                   _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0, result.stdout + result.stderr
  assert "Matrix column CategoricalReadback has enum type" in result.stderr, result.stderr
  assert "problem doing search" not in result.stderr


@pytest.mark.parametrize("program", ["sddscontrollaw", "sddspvacontrollaw"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("auxiliary", [False, True], ids=["primary", "auxiliary"])
def test_feedback_matrices_accept_enum_parameter_metadata(
  soft_ioc: SoftIOC, tmp_path: Path, program: str, encoding: str, auxiliary: bool,
) -> None:
  matrix = tmp_path / "matrix.sdds"
  _write_columns(matrix, [("Actuator", "string", soft_ioc.pv("double")),
                          (soft_ioc.pv("range1"), "double", 1)], encoding,
                 parameters='&parameter name=Metadata, type=enum, enum_values="-7:retained", fixed_value=-7 &end\n')
  options = [f"-auxiliaryOutput=matrixFile={matrix}"] if auxiliary else []

  soft_ioc.run(program, str(matrix), *options, "-dryRun", "-steps=1",
               "-interval=0.001", "-pendIOtime=2")
