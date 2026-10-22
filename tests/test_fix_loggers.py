"""Regression tests for the event, scalar, and glitch logger fixes."""

from __future__ import annotations

import struct
from pathlib import Path

import pytest

from conftest import SoftIOC, _executable, _invoke
from test_rampload import _isolated_ca_environment
from test_sdds6_page_errors import _write_invalid_metadata_page
from test_sdds6_validation import _write_columns

_TIME_PARAMETERS = [("StartTime", "double", 0), ("YearStartTime", "double", 0),
                    ("StartHour", "float", 0), ("StartDayOfMonth", "short", 1),
                    ("StartYear", "short", 2026), ("StartJulianDay", "short", 294),
                    ("StartMonth", "short", 10), ("TimeStamp", "string", "original"),
                    ("PageTimeStamp", "string", "original")]


def _write_logger_format_log(path: Path, columns: list[tuple[str, str]]) -> None:
  """Write a one-row binary -sddslogger log with a NON-fixed enum parameter.

  Metadata stores code -7 in the page; its dictionary does not map 0, so an
  appended page that leaves the parameter zero-initialized cannot be written.
  """
  header = 'SDDS6\n&description contents="" &end\n'
  header += '&parameter name=Metadata, type=enum, enum_values="-7:retained" &end\n'
  for name, kind, value in _TIME_PARAMETERS:
    header += f'&parameter name={name}, type={kind}, fixed_value="{value}" &end\n'
  for name, kind in columns:
    header += f"&column name={name}, type={kind} &end\n"
  header += "&data mode=binary, endian=little &end\n"
  data = struct.pack("<i", 1) + struct.pack("<i", -7)
  for _, kind in columns:
    data += struct.pack("<i", 0) if kind == "string" else struct.pack("<d", 0)
  path.write_bytes(header.encode("ascii") + data)


def _logonchange_arguments(controls: Path, output: Path, *options: str) -> list[str]:
  return [str(controls), str(output), "-timeDuration=0.05", "-pendEventTime=0.01",
          "-connectTimeout=2", "-logInitialValues", *options]


def test_logonchange_logger_format_append_uses_column_names_and_keeps_enum_parameter(
  soft_ioc: SoftIOC, tmp_path: Path,
) -> None:
  controls, output = tmp_path / "controls.sdds", tmp_path / "output.sdds"
  _write_columns(controls, [("ControlName", "string", soft_ioc.pv("double")),
                            ("ReadbackName", "string", "signal")], "binary")
  # Reverse the column order a new -sddslogger file would have.
  _write_logger_format_log(output, [("TimeWS", "double"), ("signal", "double"), ("Time", "double")])
  soft_ioc.put("double", "1.5")

  soft_ioc.run("sddslogonchange", *_logonchange_arguments(controls, output, "-append", "-sddslogger"))

  labels = soft_ioc.run_sdds("sdds2stream", str(output), "-parameters=Metadata", "-noquotes")
  assert labels.stdout.split() == ["retained", "retained"]
  rows = soft_ioc.run_sdds("sdds2stream", str(output), "-columns=signal,Time,TimeWS", "-page=2")
  values = [[float(value) for value in line.split()] for line in rows.stdout.splitlines() if line.strip()]
  assert values, rows.stdout
  for signal, ioc_time, workstation_time in values:
    assert signal == 1.5, values
    assert ioc_time > 1e8 and workstation_time > 1e8, values


def test_logonchange_logger_format_append_rejects_column_type_mismatch(
  soft_ioc: SoftIOC, tmp_path: Path,
) -> None:
  controls, output = tmp_path / "controls.sdds", tmp_path / "output.sdds"
  _write_columns(controls, [("ControlName", "string", soft_ioc.pv("double")),
                            ("ReadbackName", "string", "signal")], "binary")
  # A numeric PV must not be passed to a string column through SDDS_SetRowValues.
  _write_logger_format_log(output, [("Time", "double"), ("TimeWS", "double"), ("signal", "string")])
  original = output.read_bytes()

  result = soft_ioc.invoke("sddslogonchange", *_logonchange_arguments(controls, output, "-append", "-sddslogger"))

  assert result.returncode > 0, result.stdout + result.stderr
  assert "Column signal in output file" in result.stderr, result.stderr
  assert output.read_bytes() == original


@pytest.mark.parametrize("program", ["sddslogonchange", "sddsalarmlog"])
def test_event_log_append_keeps_nonfixed_enum_parameter(
  soft_ioc: SoftIOC, tmp_path: Path, program: str,
) -> None:
  controls, created, output = tmp_path / "controls.sdds", tmp_path / "created.sdds", tmp_path / "output.sdds"
  _write_columns(controls, [("ControlName", "string", soft_ioc.pv("double"))], "binary")
  options = ["-timeDuration=0.05", "-pendEventTime=0.01", "-connectTimeout=2"]
  if program == "sddslogonchange":
    options.append("-logInitialValues")
  soft_ioc.run(program, str(controls), str(created), *options, "-comment=Metadata,retained")
  # The comment becomes a non-fixed enum parameter whose dictionary does not map 0.
  soft_ioc.run_sdds("sddscast", str(created), str(output), "-cast=parameter,Metadata,string,enum",
                    "-enumValues=parameter,Metadata,-7:retained")

  # sddslogonchange appends in its default mode (indices, no -logAlarms), which
  # requires the AlarmStatusIndex/AlarmSeverityIndex columns only with -logAlarms.
  soft_ioc.run(program, str(controls), str(output), "-append", *options)

  labels = soft_ioc.run_sdds("sdds2stream", str(output), "-parameters=Metadata", "-noquotes")
  assert labels.stdout.split() == ["retained", "retained"]
  codes = soft_ioc.run_sdds("sdds2stream", str(output), "-parameters=Metadata", "-enumCodes")
  assert codes.stdout.split() == ["-7", "-7"]


@pytest.mark.parametrize("program", ["sddspvalogger", "sddspvaglitchlogger"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_pva_conditions_report_invalid_enum_later_page(tmp_path: Path, program: str, encoding: str) -> None:
  source, conditions = tmp_path / "input.sdds", tmp_path / "conditions.sdds"
  # sddspvalogger requires a Provider column; sddspvaglitchlogger would then
  # also require ExpectFieldType, ExpectNumeric, and ExpectElements.
  provider = [("Provider", "string", "ca")] if program == "sddspvalogger" else []
  _write_invalid_metadata_page(conditions, [("ControlName", "string", "unused"), *provider,
                                           ("LowerLimit", "double", 0),
                                           ("UpperLimit", "double", 1)], encoding, True)
  if program == "sddspvalogger":
    _write_columns(source, [("ControlName", "string", "unused"), ("Provider", "string", "ca"),
                            ("StorageType", "string", "double"), ("ExpectFieldType", "string", "scalar"),
                            ("ExpectElements", "long", 1)], encoding)
    arguments = [str(source), str(tmp_path / "output.sdds"), "-steps=1", "-sampleInterval=0.01"]
  else:
    parameters = ('&parameter name=TriggerControlName, type=string, fixed_value="unused" &end\n'
                  '&parameter name=OutputRootname, type=string, fixed_value="glitch" &end\n'
                  '&parameter name=GlitchThreshold, type=double, fixed_value=1 &end\n')
    _write_columns(source, [("ControlName", "string", "unused")], encoding, parameters=parameters)
    arguments = [str(source), str(tmp_path), "-time=0.01"]

  result = _invoke([str(_executable(program)), *arguments, f"-conditions={conditions},allMustPass",
                    "-pendIOtime=0.01"], _isolated_ca_environment(), timeout=5)

  assert result.returncode > 0, result.stdout + result.stderr
  assert "enum" in result.stderr.lower(), result.stdout + result.stderr
  assert not list(tmp_path.glob("glitch-*"))


def _append_truncated_ascii_page(path: Path) -> None:
  # A row count without its rows: the read of the second page fails.
  path.write_bytes(path.read_bytes() + b"2\n")


def test_logger_ignores_failed_later_page_of_enum_free_input(soft_ioc: SoftIOC, tmp_path: Path) -> None:
  controls, output = tmp_path / "controls.sdds", tmp_path / "output.sdds"
  _write_columns(controls, [("ControlName", "string", soft_ioc.pv("double")),
                            ("ReadbackName", "string", "signal"),
                            ("ReadbackUnits", "string", "V")], "ascii", version=1)
  _append_truncated_ascii_page(controls)

  soft_ioc.run("sddslogger", str(controls), str(output), "-steps=1", "-pendIOtime=2")

  assert output.read_bytes().startswith(b"SDDS")


def test_synchlog_ignores_failed_later_page_of_enum_free_input(soft_ioc: SoftIOC, tmp_path: Path) -> None:
  controls = tmp_path / "controls.sdds"
  _write_columns(controls, [("ControlName", "string", soft_ioc.pv("double")),
                            ("ReadbackName", "string", "signal"),
                            ("ReadbackUnits", "string", "V")], "ascii", version=1)
  _append_truncated_ascii_page(controls)

  result = soft_ioc.invoke("sddssynchlog", str(controls), str(tmp_path / "output.sdds"),
                           "-samples=1", "-time=0.01", "-pendIOtime=2")

  assert "Unable to read page" not in result.stderr, result.stderr


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_synchlog_accepts_enum_metadata_in_unread_name_columns(soft_ioc: SoftIOC, tmp_path: Path, encoding: str) -> None:
  controls = tmp_path / "controls.sdds"
  # sddssynchlog never reads messages, so an enum Message column is unrelated metadata.
  _write_columns(controls, [("ControlName", "string", soft_ioc.pv("double")),
                            ("ReadbackName", "string", "signal"),
                            ("Message", "enum", -7)], encoding)

  result = soft_ioc.invoke("sddssynchlog", str(controls), str(tmp_path / "output.sdds"),
                           "-samples=1", "-time=0.01", "-pendIOtime=2")

  assert "has enum type" not in result.stderr, result.stderr


@pytest.mark.parametrize("program", ["sddsglitchlogger", "sddspvaglitchlogger"])
def test_glitch_loggers_ignore_failed_later_page_of_enum_free_input(
  soft_ioc: SoftIOC, tmp_path: Path, program: str, monkeypatch: pytest.MonkeyPatch,
) -> None:
  source = tmp_path / "glitches.sdds"
  pv = soft_ioc.pv("double")
  parameters = (f'&parameter name=TriggerControlName, type=string, fixed_value="{pv}" &end\n'
                '&parameter name=OutputRootname, type=string, fixed_value="glitch" &end\n'
                '&parameter name=GlitchThreshold, type=double, fixed_value=1 &end\n')
  _write_columns(source, [("ControlName", "string", pv)], "ascii", parameters=parameters, version=1)
  _append_truncated_ascii_page(source)

  monkeypatch.chdir(tmp_path)
  soft_ioc.run(program, str(source), ".", "-time=0.02",
               "-sampleInterval=0.01", "-circularBuffer=before=0,after=1", "-pendIOtime=2")

  outputs = list(tmp_path.glob("glitch-*"))
  assert len(outputs) == 1
