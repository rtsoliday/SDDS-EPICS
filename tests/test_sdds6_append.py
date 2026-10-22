"""Append readers must validate types before modifying an existing SDDS6 file."""

from __future__ import annotations

from pathlib import Path

import pytest

from conftest import SoftIOC
from test_sdds6_validation import _write_columns


def _append_case(soft_ioc: SoftIOC, tmp_path: Path, program: str, encoding: str,
                 field: str | None, parameter: bool = False):
  controls, output = tmp_path / "controls.sdds", tmp_path / "output.sdds"
  _write_columns(controls, [("ControlName", "string", soft_ioc.pv("double")),
                            ("ReadbackName", "string", "signal"),
                            ("ReadbackUnits", "string", "V")], encoding)
  parameters = ('&parameter name=StartHour, type=float, fixed_value=0 &end\n'
                '&parameter name=StartDayOfMonth, type=short, fixed_value=1 &end\n'
                '&parameter name=StartTime, type=double, fixed_value=0 &end\n'
                '&parameter name=Metadata, type=enum, enum_values="-7:retained", fixed_value=-7 &end\n')
  runtime = [("PageTimeStamp", "string", "original")]
  if program in ("sddsmonitor", "sddslogger", "sddsstatmon"):
    columns = [("Step", "long", 0), ("Time", "double", 0),
               ("TimeOfDay", "double", 0), ("DayOfMonth", "double", 1),
               ("CAerrors", "long", 0), ("signal", "double", 0)]
    arguments = [str(controls), str(output)]
    if program == "sddsstatmon":
      columns[-1] = ("signalMean", "double", 0)
      runtime.append(("Samples", "long", 2))
      arguments += ["-includeStatistics=mean", "-samplesPerStatistic=2", "-interval=0.01"]
  else:
    runtime += [("Step", "long", 0), ("Time", "double", 0),
                ("TimeOfDay", "float", 0), ("DayOfMonth", "float", 1),
                ("CAerrors", "long", 0)]
    if program == "sddsvmonitor":
      roots, suffixes = tmp_path / "roots.sdds", tmp_path / "suffixes.sdds"
      _write_columns(roots, [("Rootname", "string", soft_ioc.prefix)], encoding)
      _write_columns(suffixes, [("Suffix", "string", "double")], encoding)
      columns = [("Index", "long", 0), ("Rootname", "string", soft_ioc.prefix),
                 ("double", "double", 0)]
      arguments = [str(output), f"-rootname={roots}", f"-suffix={suffixes}"]
    else:
      _write_columns(controls, [("WaveformPV", "string", soft_ioc.pv("array")),
                                ("WaveformName", "string", "signal")], encoding)
      columns = [("Index", "long", 0), ("signal", "double", 0)]
      arguments = [str(controls), str(output), "-datatype=double"]
      if parameter and field == "scalar":
        scalars = tmp_path / "scalars.sdds"
        _write_columns(scalars, [("ControlName", "string", soft_ioc.pv("double")),
                                ("ReadbackName", "string", "scalar"),
                                ("ReadbackUnits", "string", "V")], encoding)
        arguments.append(f"-scalars={scalars}")
        runtime.append(("scalar", "double", 0))
      elif parameter and field == "signal:RMS":
        arguments.append("-rms")
        runtime.append(("signal:RMS", "double", 0))
  for name, kind, value in runtime:
    if parameter and name == field:
      definition = 'type=enum, enum_values="-7:categorical", fixed_value=-7'
    else:
      definition = f'type={kind}, fixed_value="{value}"'
    parameters += f'&parameter name={name}, {definition} &end\n'
  if field and not parameter:
    columns = [(name, "enum", -7) if name == field else (name, kind, value)
               for name, kind, value in columns]
  _write_columns(output, columns, encoding, parameters=parameters)
  return output, arguments


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("program,field", [
  ("sddsmonitor", "signal"), ("sddsmonitor", "Time"),
  ("sddslogger", "signal"), ("sddslogger", "Step"),
  ("sddsstatmon", "signalMean"), ("sddsstatmon", "Time"),
  ("sddsvmonitor", "double"), ("sddsvmonitor", "Rootname"),
  ("sddswmonitor", "signal"), ("sddswmonitor", "Index"),
])
def test_append_rejects_enum_sample_columns_without_changing_file(
  soft_ioc: SoftIOC, tmp_path: Path, encoding: str, program: str, field: str,
) -> None:
  output, arguments = _append_case(soft_ioc, tmp_path, program, encoding, field)
  original = output.read_bytes()

  result = soft_ioc.invoke(program, *arguments, "-append", "-steps=1", "-pendIOtime=2")

  assert result.returncode > 0, result.stdout + result.stderr
  assert f"Column {field} has enum type" in result.stderr, result.stderr
  assert output.read_bytes() == original


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("program,field", [
  ("sddsmonitor", "PageTimeStamp"), ("sddslogger", "PageTimeStamp"),
  ("sddsstatmon", "PageTimeStamp"), ("sddsstatmon", "Samples"),
  ("sddsvmonitor", "Step"), ("sddsvmonitor", "PageTimeStamp"),
  ("sddswmonitor", "scalar"), ("sddswmonitor", "signal:RMS"),
])
def test_append_rejects_enum_runtime_parameters_without_changing_file(
  soft_ioc: SoftIOC, tmp_path: Path, encoding: str, program: str, field: str,
) -> None:
  output, arguments = _append_case(soft_ioc, tmp_path, program, encoding, field, parameter=True)
  original = output.read_bytes()

  result = soft_ioc.invoke(program, *arguments, "-append", "-steps=1", "-pendIOtime=2")

  assert result.returncode > 0, result.stdout + result.stderr
  assert f"Parameter {field} has enum type" in result.stderr, result.stderr
  assert output.read_bytes() == original


@pytest.mark.parametrize("program", ["sddsmonitor", "sddslogger", "sddsstatmon", "sddsvmonitor", "sddswmonitor"])
def test_append_preserves_unrelated_enum_metadata(soft_ioc: SoftIOC, tmp_path: Path, program: str) -> None:
  output, arguments = _append_case(soft_ioc, tmp_path, program, "binary", None)

  soft_ioc.run(program, *arguments, "-append", "-steps=1", "-pendIOtime=2")

  assert output.read_bytes().startswith(b"SDDS6\n")
  labels = soft_ioc.run_sdds("sdds2stream", str(output), "-parameters=Metadata", "-noquotes")
  assert labels.stdout.split() == ["retained", "retained"]


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("program,logger_format", [
  ("sddsalarmlog", False), ("sddslogonchange", False), ("sddslogonchange", True),
])
@pytest.mark.parametrize("field,parameter", [("Value", False), ("PageTimeStamp", True),
                                             ("TimeStamp", True), ("YearStartTime", True)])
def test_event_log_append_rejects_enum_storage_before_modifying_file(
  soft_ioc: SoftIOC, tmp_path: Path, encoding: str, program: str,
  logger_format: bool, field: str, parameter: bool,
) -> None:
  controls, output = tmp_path / "controls.sdds", tmp_path / "output.sdds"
  _write_columns(controls, [("ControlName", "string", soft_ioc.pv("double")),
                            ("ReadbackName", "string", "signal")], encoding)
  if logger_format and not parameter:
    field = "signal"
  parameters = (f'&parameter name={field}, type=enum, enum_values="-7:categorical", fixed_value=-7 &end\n'
                if parameter else "")
  columns = [("signal" if logger_format else "Value", "double", 0),
             ("Time", "double", 0), ("TimeWS", "double", 0)]
  if not parameter:
    columns[0] = (field, "enum", -7)
  _write_columns(output, columns, encoding, parameters=parameters)
  original = output.read_bytes()
  options = ["-sddslogger"] if logger_format else []

  result = soft_ioc.invoke(program, str(controls), str(output), "-append",
                           "-timeDuration=0.02", "-pendEventTime=0.01", "-connectTimeout=2", *options)

  assert result.returncode > 0, result.stdout + result.stderr
  assert f"{'Parameter' if parameter else 'Column'} {field} has enum type" in result.stderr, result.stderr
  assert output.read_bytes() == original


def test_logonchange_logger_format_appends_with_enum_parameter_metadata(
  soft_ioc: SoftIOC, tmp_path: Path,
) -> None:
  controls, output = tmp_path / "controls.sdds", tmp_path / "output.sdds"
  _write_columns(controls, [("ControlName", "string", soft_ioc.pv("double")),
                            ("ReadbackName", "string", "signal")], "binary")
  parameters = '&parameter name=Metadata, type=enum, enum_values="-7:retained", fixed_value=-7 &end\n'
  for name, kind, value in [("StartTime", "double", 0), ("YearStartTime", "double", 0),
                            ("StartHour", "float", 0), ("StartDayOfMonth", "short", 1),
                            ("StartYear", "short", 2026), ("StartJulianDay", "short", 294),
                            ("StartMonth", "short", 10), ("TimeStamp", "string", "original"),
                            ("PageTimeStamp", "string", "original")]:
    parameters += f'&parameter name={name}, type={kind}, fixed_value="{value}" &end\n'
  _write_columns(output, [("signal", "double", 0), ("Time", "double", 0), ("TimeWS", "double", 0)],
                 "binary", parameters=parameters)

  soft_ioc.run("sddslogonchange", str(controls), str(output), "-append", "-sddslogger",
               "-timeDuration=0.05", "-pendEventTime=0.01", "-connectTimeout=2", "-logInitialValues")

  labels = soft_ioc.run_sdds("sdds2stream", str(output), "-parameters=Metadata", "-noquotes")
  assert labels.stdout.split() == ["retained", "retained"]
