"""Reject categorical data before interpreting it as control buffers."""

from __future__ import annotations

import os
import struct
from pathlib import Path

import pytest

from conftest import _binary_directory, _executable, _invoke, _sdds_executable, _unused_udp_port
from test_rampload import _isolated_ca_environment


def _write_columns(path, columns, encoding, *, contents="", parameters="", version=6, enum_labels=None):
  header = f'SDDS{version}\n&description contents="{contents}" &end\n' + parameters
  for name, data_type, _ in columns:
    label = (enum_labels or {}).get(name, "categorical")
    mapping = f', enum_values="-7:{label}"' if data_type == "enum" else ""
    header += f"&column name={name}, type={data_type}{mapping} &end\n"
  header += f"&data mode={encoding}, endian=little &end\n"
  if encoding == "ascii":
    data = ("1\n" + " ".join(f'"{value}"' if kind == "string" else str(value)
                             for _, kind, value in columns) + "\n").encode("ascii")
  else:
    data = struct.pack("<i", 1)
    for _, kind, value in columns:
      if kind == "string":
        text = value.encode("ascii")
        data += struct.pack("<i", len(text)) + text
      else:
        data += struct.pack("<" + ("d" if kind == "double" else "i"), value)
  path.write_bytes(header.encode("ascii") + data)


def _pv_server_executable(program: str) -> Path:
  if program == "sddspcas" and not (_binary_directory() / (program + (".exe" if os.name == "nt" else ""))).is_file():
    pytest.skip("sddspcas requires the optional PCAS module")
  return _executable(program)


@pytest.mark.parametrize("program", ["sddsSoftIOC", "sddspcas"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("field", ["ControlName", "Hopr", "Lopr", "ReadbackUnits",
                                  "ElementCount", "Type", "EnumStrings", "Equation"])
def test_pv_definitions_reject_enum_configuration_fields(
  tmp_path: Path, program: str, encoding: str, field: str,
):
  executable = _pv_server_executable(program)
  source = tmp_path / "pvs.sdds"
  columns = [("ControlName", "string", "unused")]
  if field == "ControlName":
    columns = [("ControlName", "enum", -7)]
  else:
    columns.append((field, "enum", -7))
  _write_columns(source, columns, encoding)

  result = _invoke([str(executable), str(source), "-standalone", "-executionTime=0.5"],
                   _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0
  assert f"PV definition column {field} cannot have enum type" in result.stderr


@pytest.mark.parametrize("program", ["sddsSoftIOC", "sddspcas"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_pv_definitions_accept_enum_metadata(tmp_path: Path, program: str, encoding: str):
  executable = _pv_server_executable(program)
  source = tmp_path / "pvs.sdds"
  _write_columns(source, [("ControlName", "string", "state"), ("Type", "string", "enum"),
                          ("EnumStrings", "string", "OFF,ON"), ("Metadata", "enum", -7)], encoding)
  server_port = _unused_udp_port()
  environment = _isolated_ca_environment()
  environment.update({
    "EPICS_CA_ADDR_LIST": f"127.0.0.1:{server_port}",
    "EPICS_CA_SERVER_PORT": str(server_port),
    "EPICS_CAS_INTF_ADDR_LIST": "127.0.0.1",
    "XDG_RUNTIME_DIR": str(tmp_path),
  })

  result = _invoke([str(executable), str(source), "-standalone", "-executionTime=0.5",
                    f"-pvPrefix=pytest:metadata:{os.getpid()}:"], environment, timeout=8)

  assert result.returncode == 0, result.stderr


COMMON_COLUMNS = [("ControlName", "string", "unused"), ("ControlType", "string", "invalid")]
LINK_COLUMNS = [(name, "string", "unused") for name in ("SuffixName", "OUTB", "OUTC", "OUTD")]
MAGNET_COLUMNS = {
  "degauss": COMMON_COLUMNS + [
    ("FirstPeakCurrent", "double", 1), ("NumCycles", "long", 1),
    ("LastPeakCurrent", "double", 0), ("OutputInterval", "double", 1),
    ("MaxStepSize", "double", 1),
  ] + LINK_COLUMNS,
  "standardize": COMMON_COLUMNS + [
    ("From", "double", 0), ("To", "double", 1),
    ("Minutes", "long", 0), ("Seconds", "long", 1),
    ("RampRate", "double", 1), ("RampTransitionPercentage", "double", 1),
    ("DwellTime", "double", 1), ("NumCycles", "long", 1),
    ("Finish", "double", 0), ("Approach", "string", "above"),
  ] + LINK_COLUMNS,
  "degauss-legacy": COMMON_COLUMNS + [
    ("DecayMinutes", "long", 0), ("DecaySeconds", "long", 1),
    ("PeriodSeconds", "long", 1), ("MaxCurrent", "double", 1),
    ("NumDecay", "long", 1), ("LastPeakCurrent", "double", 0),
    ("OutputInterval", "double", 1),
  ],
}


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
# The standardize specification selects dwell mode, which never reads Minutes
# or Seconds; unread optional columns keep their legacy "ignored" behavior.
@pytest.mark.parametrize("mode,field", [(mode, name) for mode, columns in MAGNET_COLUMNS.items()
                                        for name, _, _ in columns
                                        if not (mode == "standardize" and name in ("Minutes", "Seconds"))])
def test_magnet_specifications_reject_enum_buffers(tmp_path: Path, encoding: str, mode: str, field: str):
  source = tmp_path / "magnet.sdds"
  columns = [(name, "enum", -7) if name == field else (name, kind, value)
             for name, kind, value in MAGNET_COLUMNS[mode]]
  contents = "degaussing specification" if mode == "degauss-legacy" else "tri-degaussing specification"
  _write_columns(source, columns, encoding, contents=contents)
  program = "degauss" if mode.startswith("degauss") else "standardize"

  result = _invoke([str(_executable(program)), str(source)], _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0
  assert field in result.stderr
  assert "Invalid column type" in result.stderr


@pytest.mark.parametrize("mode", MAGNET_COLUMNS)
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("version", [1, 6])
def test_magnet_primitive_types_still_parse(tmp_path: Path, mode: str, encoding: str, version: int):
  source = tmp_path / "magnet.sdds"
  contents = "degaussing specification" if mode == "degauss-legacy" else "tri-degaussing specification"
  _write_columns(source, MAGNET_COLUMNS[mode], encoding, contents=contents, version=version)
  program = "degauss" if mode.startswith("degauss") else "standardize"

  result = _invoke([str(_executable(program)), str(source)], _isolated_ca_environment(), timeout=3)

  # The deliberately invalid ControlType stops before CA searches, after reading all fields.
  assert result.returncode == 1
  assert "illegal ControlType" in result.stderr
  assert "Invalid column type" not in result.stderr


@pytest.mark.parametrize("program", ["sddscontrollaw", "sddspvacontrollaw"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("option,field", [
  ("offsets", "ControlName"), ("offsets", "Offset"),
  ("PVOffsets", "ControlName"), ("PVOffsets", "OffsetControlName"),
])
def test_feedback_rejects_enum_offsets(tmp_path: Path, program: str, encoding: str, option: str, field: str):
  matrix, offsets = tmp_path / "matrix.sdds", tmp_path / "offsets.sdds"
  _write_columns(matrix, [("Actuator", "string", "unused"), ("Readback", "double", 1)], "ascii")
  columns = [("ControlName", "string", "Readback")]
  columns += [("Offset", "double", 0)] if option == "offsets" else [("OffsetControlName", "string", "unused")]
  columns = [(name, "enum", -7) if name == field else (name, kind, value) for name, kind, value in columns]
  _write_columns(offsets, columns, encoding)

  result = _invoke([str(_executable(program)), str(matrix), f"-{option}={offsets}",
                    "-dryRun", "-steps=1", "-interval=0.001"], _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0
  assert field in result.stderr
  assert "wrong data type" in result.stderr


@pytest.mark.parametrize("program", ["sddscontrollaw", "sddspvacontrollaw"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_feedback_filter_rejects_enum_device_names(tmp_path: Path, program: str, encoding: str):
  matrix, coefficients = tmp_path / "matrix.sdds", tmp_path / "filter.sdds"
  _write_columns(matrix, [("Actuator", "string", "unused"), ("Readback", "double", 1)], "ascii")
  _write_columns(coefficients, [("DeviceName", "enum", -7), ("a0", "double", 1),
                                ("b0", "double", 1)], encoding)

  result = _invoke([str(_executable(program)), str(matrix), f"-filterFile={coefficients}",
                    "-dryRun", "-steps=1", "-interval=0.001"], _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0
  assert "DeviceName" in result.stderr
  assert "wrong data type" in result.stderr


@pytest.mark.parametrize("program", ["sddscontrollaw", "sddspvacontrollaw"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("field", ["a0", "b0", "a3", "b3"])
@pytest.mark.parametrize("auxiliary", [False, True])
def test_feedback_filters_reject_enum_coefficients(
  tmp_path: Path, program: str, encoding: str, field: str, auxiliary: bool,
):
  matrix, coefficients = tmp_path / "matrix.sdds", tmp_path / "filter.sdds"
  _write_columns(matrix, [("Actuator", "string", "unused"), ("Readback", "double", 1)], "ascii")
  columns = [("DeviceName", "string", "unused"), ("a0", "double", 1), ("b0", "double", 1)]
  columns = [(name, "enum", -7) if name == field else (name, kind, value)
             for name, kind, value in columns]
  if field not in ("a0", "b0"):
    columns.append((field, "enum", -7))
  _write_columns(coefficients, columns, encoding)
  option = (f"-auxiliaryOutput=matrixFile={matrix},coefFile={coefficients}" if auxiliary
            else f"-filterFile={coefficients}")

  result = _invoke([str(_executable(program)), str(matrix), option, "-dryRun", "-steps=1",
                    "-interval=0.001"], _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0
  assert f"Filter coefficient column {field} must contain numeric data" in result.stderr


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_optimizer_extra_log_rejects_enum_control_names(tmp_path: Path, encoding: str):
  variables, extra = tmp_path / "variables.sdds", tmp_path / "extra.sdds"
  _write_columns(variables, [("ControlName", "string", "unused"), ("LowerLimit", "double", -1),
                             ("UpperLimit", "double", 1), ("InitialValue", "double", 0),
                             ("InitialChange", "double", 0.1)], encoding)
  _write_columns(extra, [("ControlName", "enum", -7)], encoding)

  result = _invoke([str(_executable("sddsoptimize")), f"-varFile={variables}",
                    "-varScript=unused", "-dryRun", "-simplex",
                    f"-logFile={tmp_path / 'log.sdds'}", f"-extraLogFile={extra}"],
                   _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0
  assert "ControlName" in result.stderr
  assert "wrong data type" in result.stderr


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("field", ["ControlName", "ValueString", "WaveformPV", "DeviceName"])
def test_bpm_vectors_stop_after_enum_type_errors(tmp_path: Path, encoding: str, field: str):
  reference, vector = tmp_path / "reference.sdds", tmp_path / "vector.sdds"
  columns = [("ControlName", "string", "unused"), ("ValueString", "string", "0")]
  columns = [(name, "enum", -7) if name == field else (name, kind, value)
             for name, kind, value in columns]
  _write_columns(reference, columns, encoding)
  parameter = ('&parameter name=WaveformPV, type=enum, enum_values="-7:unused", fixed_value=-7 &end\n'
               if field == "WaveformPV" else
               '&parameter name=WaveformPV, type=string, fixed_value="unused" &end\n')
  _write_columns(vector, [("DeviceName", "enum", -7) if field == "DeviceName" else
                          ("DeviceName", "string", "unused")], encoding, parameters=parameter)

  result = _invoke([str(_executable("srinitbpmvector")), str(vector), "-plane=h", "-dryRun",
                    f"-setpointFile={reference}", f"-offsetFile={reference}", f"-gainFile={reference}",
                    "-pendIOtime=0.01"], _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0
  assert field in result.stderr
  assert "wrong data type" in result.stderr
  assert "problem doing search" not in result.stderr


PRESET_COLUMNS = [
  ("Label", "string", "chosen"), ("PlaneMode", "string", "x"),
  ("CommutationMode", "string", "a"), ("SampleMode", "string", "bunchpattern"),
  ("BunchPattern", "string", "chosen"), ("SamplesPerBunch", "long", 1),
  ("TurnMarkerOffset", "long", 0), ("TransitionDeadTime", "long", 0),
  ("AccumulatorMode", "string", "y"),
]
BUNCH_COLUMNS = [
  ("Label", "string", "chosen"), ("Multiplicity", "long", 1),
  ("Multiplets", "long", 1), ("MultipletStart", "long", 0),
  ("MultipletSpacing", "long", 1), ("BpmGroupBuckets", "long", 1),
  ("BpmGroupMode", "string", "None"),
]


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_control_bits_categorical_presets_use_labels(tmp_path: Path, encoding: str):
  outputs = []
  for use_enums in (False, True):
    preset = tmp_path / f"preset-{use_enums}.sdds"
    bunch = tmp_path / f"bunch-{use_enums}.sdds"
    for path, columns in ((preset, PRESET_COLUMNS), (bunch, BUNCH_COLUMNS)):
      labels = {name: value for name, kind, value in columns if kind == "string"}
      converted = [(name, "enum", -7) if use_enums and kind == "string" else (name, kind, value)
                   for name, kind, value in columns]
      _write_columns(path, converted, encoding, enum_labels=labels)
    output, masks = tmp_path / f"bits-{use_enums}.sdds", tmp_path / f"masks-{use_enums}.sdds"
    result = _invoke([str(_executable("sddsgencontrolbits")), str(output),
                      f"-presetRAMconfiguration=chosen,file={preset}", f"-bunchPatternFile={bunch}",
                      f"-controlRAMFile={masks}"], _isolated_ca_environment(), timeout=3)
    assert result.returncode == 0, result.stderr
    data = _invoke([str(_sdds_executable("sdds2stream")), str(masks), "-columns=Waveform"],
                   _isolated_ca_environment())
    assert data.returncode == 0, data.stderr
    assert len(data.stdout.split()) > 0
    outputs.append(data.stdout)
  assert outputs[0] == outputs[1]


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("table,field", [(table, name) for table, columns in
                                        (("preset", PRESET_COLUMNS), ("bunch", BUNCH_COLUMNS))
                                        for name, kind, _ in columns if kind == "long"])
def test_control_bits_reject_enum_counts(tmp_path: Path, encoding: str, table: str, field: str):
  preset, bunch, output = tmp_path / "preset.sdds", tmp_path / "bunch.sdds", tmp_path / "bits.sdds"
  for name, path, columns in (("preset", preset, PRESET_COLUMNS), ("bunch", bunch, BUNCH_COLUMNS)):
    converted = [(column, "enum", -7) if table == name and column == field else (column, kind, value)
                 for column, kind, value in columns]
    _write_columns(path, converted, encoding)

  result = _invoke([str(_executable("sddsgencontrolbits")), str(output),
                    f"-presetRAMconfiguration=chosen,file={preset}", f"-bunchPatternFile={bunch}"],
                   _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0
  assert "non-numeric" in result.stderr.lower()


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_control_bits_reject_enum_bit_masks(tmp_path: Path, encoding: str):
  source, output = tmp_path / "masks.sdds", tmp_path / "bits.sdds"
  _write_columns(source, [("Waveform", "enum", -7)], encoding)

  result = _invoke([str(_executable("sddsgencontrolbits")), str(source), str(output)],
                   _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0
  assert "Waveform" in result.stderr
  assert "unsigned long bit masks" in result.stderr


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("field", ["ControlName", "BPMName", "ElementName", "ReferenceControlName"])
def test_orbit_bump_rejects_enum_name_buffers(tmp_path: Path, encoding: str, field: str):
  matrix, response = tmp_path / "matrix.sdds", tmp_path / "response.sdds"
  extra, output = tmp_path / "extra.sdds", tmp_path / "output.sdds"
  matrix_columns = [("ControlName", "enum", -7) if field == "ControlName" else
                    ("ControlName", "string", "Corrector"), ("Readback", "double", 1)]
  _write_columns(matrix, matrix_columns, encoding)
  arguments = [str(_executable("makesrbump")), str(matrix), str(output),
               "-bpmList=Readback", "-deltaList=1", "-review"]
  if field != "ControlName":
    columns = [("BPMName", "enum", -7) if field == "BPMName" else
               ("BPMName", "string", "Readback"), ("Corrector", "double", 1)]
    _write_columns(response, columns, encoding)
    arguments.append(f"-referenceMatrix={response}")
  if field == "ElementName":
    _write_columns(extra, [("ElementName", "enum", -7), ("etax", "double", 1)], encoding)
    arguments += ["-changeRF", f"-twissFile={extra}"]
  if field == "ReferenceControlName":
    _write_columns(extra, [("ControlName", "enum", -7), ("Value", "double", 1)], encoding)
    arguments.append(f"-referenceFile={extra}")

  result = _invoke(arguments, _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0
  assert field.replace("Reference", "") in result.stderr
  assert "wrong data type" in result.stderr


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("field", ["Rootname", "Suffix"])
@pytest.mark.parametrize("program", ["sddsvexperiment", "sddsvmonitor"])
def test_vector_readers_reject_enum_name_buffers(tmp_path: Path, encoding: str, field: str, program: str):
  roots, suffixes = tmp_path / "roots.sdds", tmp_path / "suffixes.sdds"
  for path, name in ((roots, "Rootname"), (suffixes, "Suffix")):
    _write_columns(path, [(name, "enum", -7) if name == field else (name, "string", "unused")], encoding)
  experiment, output = tmp_path / "experiment.namelist", tmp_path / "output.sdds"
  experiment.write_text('&system_call command="unused", index_number=0, index_limit=1 &end\n'
                        '&execute &end\n', encoding="ascii")

  if program == "sddsvexperiment":
    arguments = [str(experiment), str(output), "-dryRun", f"-rootnameFile={roots}", f"-suffixFile={suffixes}"]
  else:
    arguments = [str(output), f"-rootnames={roots}", f"-suffixes={suffixes}", "-steps=1"]
  result = _invoke([str(_executable(program)), *arguments], _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0
  assert field in result.stderr
  assert "wrong data type" in result.stderr


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("field", ["ControlNameParameter", "ControlName", "Gain", "StepSize", "WaitTime"])
def test_optimizer_rejects_enum_knob_fields(tmp_path: Path, encoding: str, field: str):
  variables, knob = tmp_path / "variables.sdds", tmp_path / "knob.sdds"
  _write_columns(variables, [("ControlName", "string", "knob"), ("LowerLimit", "double", -1),
                             ("UpperLimit", "double", 1), ("InitialValue", "double", 0),
                             ("InitialChange", "double", 0.1)], encoding)
  parameters = ('&parameter name=ControlName, type=enum, enum_values="-7:knob", fixed_value=-7 &end\n'
                if field == "ControlNameParameter" else
                '&parameter name=ControlName, type=string, fixed_value="knob" &end\n')
  for name in ("Gain", "StepSize", "WaitTime"):
    definition = 'type=enum, enum_values="-7:categorical", fixed_value=-7' if name == field else 'type=double, fixed_value=1'
    parameters += f'&parameter name={name}, {definition} &end\n'
  columns = [("ControlName", "enum", -7) if field == "ControlName" else ("ControlName", "string", "unused")]
  _write_columns(knob, columns, encoding, parameters=parameters)

  result = _invoke([str(_executable("sddsoptimize")), f"-varFile={variables}", f"-knobFiles={knob}",
                    "-varScript=unused", "-dryRun", "-simplex"], _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0
  assert field.replace("Parameter", "") in result.stderr
  assert "wrong data type" in result.stderr


@pytest.mark.parametrize("program", ["sddscasr", "sddspvasaverestore"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("field", ["SnapshotFilename", "WaveformPV", "WaveformPVOneFile"])
def test_restore_rejects_enum_filename_buffers(tmp_path: Path, program: str, encoding: str, field: str):
  snapshot, root = tmp_path / "snapshot.sdds", tmp_path / "waveform"
  columns = [("ControlName", "string", "unused"), ("ValueString", "string", "WaveformPV")]
  parameters, options = "", []
  if field == "SnapshotFilename":
    parameters = '&parameter name=SnapshotFilename, type=enum, enum_values="-7:unused", fixed_value=-7 &end\n'
  else:
    columns.append(("IndirectName", "string", "WaveformPV"))
    one_file = field == "WaveformPVOneFile"
    waveform = Path(f"{root}.waveform" if one_file else f"{root}.unused")
    _write_columns(waveform, [("Waveform", "double", 1)], encoding,
                   parameters='&parameter name=WaveformPV, type=enum, enum_values="-7:unused", fixed_value=-7 &end\n')
    options = [f"-waveform=rootname={root}" + (",onefile" if one_file else "")]
  _write_columns(snapshot, columns, encoding, parameters=parameters)

  result = _invoke([str(_executable(program)), str(snapshot), "-restore", *options],
                   _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0
  assert field.replace("OneFile", "") in result.stderr
  assert "must have string type" in result.stderr


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_experiment_rejects_enum_measurement_control_names(tmp_path: Path, encoding: str) -> None:
  measurements = tmp_path / "measurements.sdds"
  experiment = tmp_path / "experiment.namelist"
  _write_columns(measurements, [("ControlName", "enum", -7)], encoding)
  experiment.write_text(f'&measurement_file filename="{measurements}" &end\n', encoding="ascii")

  result = _invoke([str(_executable("sddsexperiment")), str(experiment), "-dryRun"],
                   _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0
  assert "ControlName" in result.stderr
  assert "wrong data type" in result.stderr


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("field", ["ControlNameParameter", "ControlName", "Weight", "Gain", "ReadbackName"])
def test_experiment_rejects_enum_knob_fields(tmp_path: Path, encoding: str, field: str):
  knob, experiment = tmp_path / "knob.sdds", tmp_path / "experiment.namelist"
  parameters = '&parameter name=ControlName, type=string, fixed_value="knob" &end\n'
  if field == "ControlNameParameter":
    parameters = '&parameter name=ControlName, type=enum, enum_values="-7:knob", fixed_value=-7 &end\n'
  parameters += ('&parameter name=Gain, type=enum, enum_values="-7:categorical", fixed_value=-7 &end\n'
                 if field == "Gain" else '&parameter name=Gain, type=double, fixed_value=1 &end\n')
  columns = [("ControlName", "string", "unused"), ("Weight", "double", 1), ("ReadbackName", "string", "unused")]
  columns = [(name, "enum", -7) if name == field else (name, kind, value) for name, kind, value in columns]
  _write_columns(knob, columns, encoding, parameters=parameters)
  experiment.write_text(f'&variable control_name="knob", is_knob=1, knob_file_name="{knob}", '
                        'index_number=0, index_limit=2, initial_value=0, final_value=1 &end\n', encoding="ascii")

  result = _invoke([str(_executable("sddsexperiment")), str(experiment), "-dryRun"],
                   _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0
  assert field.replace("Parameter", "").lower() in result.stderr.lower()
  assert "wrong data type" in result.stderr
