"""Categorical optional fields must not silently select primitive defaults."""

from pathlib import Path

import pytest

from conftest import SoftIOC, _executable, _invoke
from test_rampload import _isolated_ca_environment
from test_sdds6_validation import _write_columns


def _assert_rejected(program, arguments, field, kind="Column", environment=None):
  options = [] if program in ("sddsalarmlog", "sddslogonchange") else [f"-pendIOtime={2 if environment else 0.01}"]
  result = _invoke([str(_executable(program)), *arguments, *options],
                   environment or _isolated_ca_environment(), timeout=5)
  assert result.returncode > 0, result.stdout + result.stderr
  assert f"{kind} {field} has enum type" in result.stderr, result.stdout + result.stderr


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("field", ["ReadbackName", "ReadbackSymbol", "ReadbackUnits",
                                  "ScaleFactor", "UnitsPV"])
def test_scalar_monitor_rejects_enum_optional_fields(soft_ioc: SoftIOC, tmp_path: Path, encoding, field):
  source = tmp_path / "monitor.sdds"
  _write_columns(source, [("ControlName", "string", "unused"), (field, "enum", -7)], encoding)
  _assert_rejected("sddswmonitor", [str(tmp_path / "output.sdds"), "-steps=1",
                                    f"-PVnames={soft_ioc.pv('array')}", f"-scalars={source}"],
                   field, environment=soft_ioc.environment)


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("field", ["WaveformLength", "rmsPVPrefix"])
def test_waveform_monitor_rejects_enum_optional_fields(tmp_path: Path, encoding, field):
  source = tmp_path / "monitor.sdds"
  columns = [("WaveformPV", "string", "unused"), ("WaveformName", "string", "waveform")]
  parameters = ""
  if field == "WaveformLength":
    parameters = '&parameter name=WaveformLength, type=enum, enum_values="-7:four", fixed_value=-7 &end\n'
  else:
    columns.append((field, "enum", -7))
  _write_columns(source, columns, encoding, parameters=parameters)
  _assert_rejected("sddswmonitor", [str(source), str(tmp_path / "output.sdds"), "-steps=1"],
                   field, "Parameter" if parameters else "Column")


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("program,field", [
  (program, field) for program in ("sddscasr", "sddspvasaverestore")
  for field in ("IndirectName",)
] + [("sddspvasaverestore", field) for field in
     ("Provider", "ExpectNumeric", "ExpectFieldType", "ExpectElements")])
def test_restore_rejects_enum_optional_fields(tmp_path: Path, encoding, program, field):
  source = tmp_path / "snapshot.sdds"
  columns = [("ControlName", "string", "unused"), ("ValueString", "string", "1")]
  columns = [(name, "enum", -7) if name == field else (name, kind, value)
             for name, kind, value in columns]
  if field != "ValueString":
    columns.append((field, "enum", -7))
  _write_columns(source, columns, encoding)
  _assert_rejected(program, [str(source), "-restore"], field)


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("field", ["RelativeLimit", "ReadbackName", "Tolerance", "ControlTolerance"])
def test_optimizer_rejects_enum_optional_limits(tmp_path: Path, encoding, field):
  source = tmp_path / "variables.sdds"
  columns = [("ControlName", "string", "unused"), ("LowerLimit", "double", -1),
             ("UpperLimit", "double", 1), ("InitialValue", "double", 0),
             ("InitialChange", "double", 0.1)]
  if field in ("Tolerance", "ControlTolerance"):
    # Tolerances are consulted only when a ReadbackName column is present.
    columns.append(("ReadbackName", "string", "unused"))
  parameters = ""
  if field == "ControlTolerance":
    parameters = '&parameter name=ControlTolerance, type=enum, enum_values="-7:small", fixed_value=-7 &end\n'
  else:
    columns.append((field, "enum", -7))
  _write_columns(source, columns, encoding, parameters=parameters)
  _assert_rejected("sddsoptimize", [f"-varFile={source}", "-varScript=unused", "-dryRun", "-simplex"],
                   field, "Parameter" if parameters else "Column")


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("field", ["MaskControlName", "MaskProvider", "Provider"])
def test_permissive_rejects_enum_optional_mask_and_provider(tmp_path: Path, encoding, field):
  source = tmp_path / "permissive.sdds"
  columns = [("ControlName", "string", "unused"), ("Description", "string", "test"),
             ("MinimumValue", "double", 0), ("MaximumValue", "double", 1)]
  parameters = ""
  if field == "Provider":
    columns.append((field, "enum", -7))
  else:
    parameters = f'&parameter name={field}, type=enum, enum_values="-7:unused", fixed_value=-7 &end\n'
  _write_columns(source, columns, encoding, parameters=parameters)
  _assert_rejected("sddspermissive", [str(source), "-runControlPV=string=unused"],
                   field, "Parameter" if parameters else "Column")


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("program", ["sddsmonitor", "sddsstatmon", "sddslogger", "sddssynchlog"])
def test_scalar_readers_reject_enum_readback_names(tmp_path: Path, encoding, program):
  source = tmp_path / "scalars.sdds"
  _write_columns(source, [("ControlName", "string", "unused"), ("ReadbackName", "enum", -7)], encoding)
  arguments = [str(source), str(tmp_path / "output.sdds")]
  arguments += ["-samples=1", "-time=0.01"] if program == "sddssynchlog" else ["-steps=1"]
  _assert_rejected(program, arguments, "ReadbackName")


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("field", ["ReadbackUnits", "ReadbackUnit", "Units"])
def test_scalar_logger_rejects_enum_units_aliases(tmp_path: Path, encoding, field):
  source = tmp_path / "scalars.sdds"
  _write_columns(source, [("ControlName", "string", "unused"), (field, "enum", -7)], encoding)
  _assert_rejected("sddslogger", [str(source), str(tmp_path / "output.sdds"), "-steps=1"], field)


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("program,field", [("sddsalarmlog", "BitDecoderArray"),
                                          ("sddslogonchange", "Tolerance"),
                                          ("sddslogonchange", "ExpectElements")])
def test_loggers_reject_enum_optional_fields(tmp_path: Path, encoding, program, field):
  source = tmp_path / "controls.sdds"
  _write_columns(source, [("ControlName", "string", "unused"), (field, "enum", -7)], encoding)
  _assert_rejected(program, [str(source), str(tmp_path / "output.sdds"), "-time=0.01"], field)


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("field", ["OffsetPV", "OffsetValue", "Weight", "LowerLimit", "UpperLimit"])
def test_squish_rejects_enum_optional_limits_and_offsets(tmp_path: Path, encoding, field):
  source = tmp_path / "controls.sdds"
  columns = [("BpmPV", "string", "unused")]
  parameters = ('&parameter name=Gain, type=double, fixed_value=1 &end\n'
                '&parameter name=CorrectorPV, type=string, fixed_value="unused" &end\n')
  if field.endswith("Limit"):
    parameters += f'&parameter name={field}, type=enum, enum_values="-7:limit", fixed_value=-7 &end\n'
  else:
    columns.append((field, "enum", -7))
  _write_columns(source, columns, encoding, parameters=parameters)
  _assert_rejected("squishPVs", [str(source)], field, "Parameter" if field.endswith("Limit") else "Column")


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("program,field", [(program, field)
  for program in ("sddstransferFFtoWaveform", "sddstransferscalartowaveform")
  for field in ("Prefix", "Suffix")]
  + [("sddstransferscalartowaveform", field) for field in ("Suffix1", "IsSum")])
def test_waveform_transfers_reject_enum_optional_parameters(tmp_path: Path, encoding, program, field):
  source = tmp_path / "waveforms.sdds"
  parameters = ('&parameter name=WaveformPV, type=string, fixed_value="unused" &end\n'
                f'&parameter name={field}, type=enum, enum_values="-7:categorical", fixed_value=-7 &end\n')
  _write_columns(source, [("DeviceName", "string", "unused")], encoding, parameters=parameters)
  _assert_rejected(program, [str(source), "-steps=1"], field, "Parameter")


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_orbit_bump_rejects_enum_delta_mode(tmp_path: Path, encoding):
  matrix, waveform = tmp_path / "matrix.sdds", tmp_path / "waveform.sdds"
  _write_columns(matrix, [("ControlName", "string", "Corrector"), ("Readback", "double", 1)], encoding)
  parameters = ('&parameter name=WaveformPV, type=string, fixed_value="unused" &end\n'
                '&parameter name=IsDelta, type=enum, enum_values="-7:delta", fixed_value=-7 &end\n')
  _write_columns(waveform, [("DeviceName", "string", "Corrector")], encoding, parameters=parameters)
  _assert_rejected("makesrbump", [str(matrix), str(tmp_path / "output.sdds"), "-bpmList=Readback",
                                  "-deltaList=1", "-review", f"-waveformFile={waveform}"],
                   "IsDelta", "Parameter")


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("program", ["sddsglitchlogger", "sddspvaglitchlogger"])
@pytest.mark.parametrize("field", ["ReadbackName", "GlitchScript"])
def test_glitch_loggers_reject_enum_optional_fields(tmp_path: Path, encoding, program, field):
  source = tmp_path / "glitches.sdds"
  columns = [("ControlName", "string", "unused")]
  parameters = ('&parameter name=TriggerControlName, type=string, fixed_value="unused" &end\n'
                '&parameter name=OutputRootname, type=string, fixed_value="glitch" &end\n'
                '&parameter name=GlitchThreshold, type=double, fixed_value=1 &end\n')
  if field == "GlitchScript":
    parameters += '&parameter name=GlitchScript, type=enum, enum_values="-7:unused", fixed_value=-7 &end\n'
  else:
    columns.append((field, "enum", -7))
  _write_columns(source, columns, encoding, parameters=parameters)
  _assert_rejected(program, [str(source), str(tmp_path), "-time=0.01"],
                   field, "Parameter" if field == "GlitchScript" else "Column")


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("program", ["sddsfeedforward", "sdds2dfeedforward"])
def test_feedforward_rejects_enum_exit_policy(tmp_path: Path, encoding, program):
  feedforward, tests = tmp_path / "feedforward.sdds", tmp_path / "tests.sdds"
  axes = ["Readback"] if program == "sddsfeedforward" else ["Readback1", "Readback2"]
  names = axes + ["Actuator"]
  header = "SDDS1\n"
  for name in names:
    header += f'&parameter name={name}Name, type=string, fixed_value="unused{name}" &end\n'
    header += f'&column name={name}Value, type=double &end\n'
  rows = "2\n0 0\n1 1\n" if len(axes) == 1 else "4\n0 0 0\n0 1 1\n1 0 1\n1 1 2\n"
  feedforward.write_text(header + "&data mode=ascii &end\n" + rows, encoding="ascii")
  _write_columns(tests, [("ControlName", "string", "unused"), ("MinimumValue", "double", 0),
                         ("MaximumValue", "double", 1), ("ExitOnFailure", "enum", -7)], encoding)
  _assert_rejected(program, [str(feedforward), f"-testValues={tests}", "-dryRun", "-steps=1"], "ExitOnFailure")


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("field", ["ConversionFactor", "ConversionOffset", "Analyses"])
def test_bps_assignments_reject_enum_optional_fields(tmp_path: Path, encoding, field):
  source = tmp_path / "assignments.sdds"
  _write_columns(source, [("ControlName", "string", "unused"), ("ReadbackName", "string", "signal"),
                          (field, "enum", -7)], encoding)
  _assert_rejected("BPS16monitor", [str(tmp_path / "output.sdds"), f"-assignments={source}", "-steps=1"], field)


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_ramp_rejects_enum_numerical_flag(tmp_path: Path, encoding):
  source = tmp_path / "ramp.sdds"
  _write_columns(source, [("ControlName", "string", "unused"), ("ValueString", "string", "ON"),
                          ("IsNumerical", "enum", -7)], encoding)
  _assert_rejected("sddscaramp", [f"-rampTo={source},steps=1,pause=0"], "IsNumerical")


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("field", ["ExpectNumeric", "StorageType", "ReadbackName", "ScaleFactor",
                                  "ScalarArrayStartIndex", "ScalarArrayEndIndex", "Average",
                                  "ReadbackUnits", "ReadbackUnit", "Units"])
def test_pva_logger_rejects_enum_optional_input_fields(tmp_path: Path, encoding, field):
  source = tmp_path / "logger.sdds"
  columns = [("ControlName", "string", "unused"), ("Provider", "string", "ca"),
             ("StorageType", "string", "double"), ("ExpectFieldType", "string", "scalar"),
             ("ExpectElements", "long", 1)]
  columns = [(name, "enum", -7) if name == field else (name, kind, value)
             for name, kind, value in columns]
  if field != "StorageType":
    columns.append((field, "enum", -7))
  _write_columns(source, columns, encoding)
  _assert_rejected("sddspvalogger", [str(source), str(tmp_path / "output.sdds"), "-steps=1", "-sampleInterval=0.01"], field)


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("option,field", [("conditions", field) for field in ("ScaleFactor", "HoldoffTime")]
  + [("triggerFile", field) for field in ("MajorAlarm", "MinorAlarm", "NoAlarm", "GlitchThreshold",
                                         "GlitchBaselineSamples", "GlitchBaselineAutoReset", "GlitchScript",
                                         "TransitionThreshold", "TransitionDirection")])
def test_pva_logger_rejects_enum_optional_condition_and_trigger_fields(tmp_path: Path, encoding, option, field):
  source, extra = tmp_path / "logger.sdds", tmp_path / "extra.sdds"
  _write_columns(source, [("ControlName", "string", "unused"), ("Provider", "string", "ca"),
                          ("StorageType", "string", "double"), ("ExpectFieldType", "string", "scalar"),
                          ("ExpectElements", "long", 1)], encoding)
  columns = [("ControlName" if option == "conditions" else "TriggerControlName", "string", "unused"),
             ("Provider", "string", "ca"), (field, "enum", -7)]
  if option == "conditions":
    columns += [("LowerLimit", "double", 0), ("UpperLimit", "double", 1)]
  _write_columns(extra, columns, encoding)
  qualifier = ",allMustPass" if option == "conditions" else ""
  _assert_rejected("sddspvalogger", [str(source), str(tmp_path / "output.sdds"), "-steps=1",
                                     "-sampleInterval=0.01", f"-{option}={extra}{qualifier}"], field)
