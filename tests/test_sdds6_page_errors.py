"""Waveform input readers must distinguish invalid enum data from EOF."""

from __future__ import annotations

import struct
from pathlib import Path

import pytest

from conftest import SoftIOC, _executable, _invoke
from test_rampload import _isolated_ca_environment
from test_sdds6_validation import PRESET_COLUMNS, BUNCH_COLUMNS, _write_columns


def _write_invalid_metadata_page(
  source: Path, columns: list[tuple[str, str, str | int | float]], encoding: str,
  valid_first_page: bool, parameters: str = "",
) -> None:
  _write_columns(source, columns + [("Metadata", "enum", -7)], encoding, parameters=parameters)
  header, page = source.read_bytes().rsplit(b"&end\n", 1)
  invalid_page = page.replace(b"-7", b"999") if encoding == "ascii" else page[:-4] + struct.pack("<i", 999)
  source.write_bytes(header + b"&end\n" + (page if valid_first_page else b"") + invalid_page)


@pytest.mark.parametrize("program", ["sddsexperiment", "sddsvexperiment"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("valid_first_page", [False, True], ids=["first-page", "later-page"])
def test_experiment_values_reject_invalid_enum_pages(
  tmp_path: Path, program: str, encoding: str, valid_first_page: bool,
) -> None:
  source, experiment = tmp_path / "values.sdds", tmp_path / "experiment.namelist"
  _write_invalid_metadata_page(source, [("Value", "double", 1)], encoding, valid_first_page)
  name_field = "PV_name" if program == "sddsvexperiment" else "control_name"
  experiment.write_text(
    f'&variable {name_field}="unused", units="V", values_file="{source}", '
    'values_file_column="Value", index_number=0 &end\n', encoding="ascii",
  )
  arguments = [str(experiment)]
  if program == "sddsvexperiment":
    arguments.append(str(tmp_path / "output.sdds"))
  result = _invoke([str(_executable(program)), *arguments, "-dryRun", "-pendIOtime=0.01"],
                   _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0
  assert "enum" in result.stderr.lower(), result.stderr
  assert "problem reading column Value" in result.stderr


@pytest.mark.parametrize("program", ["sddsexperiment", "sddsvexperiment"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_experiment_values_accept_complete_enum_metadata(
  soft_ioc: SoftIOC, tmp_path: Path, program: str, encoding: str,
) -> None:
  source, experiment = tmp_path / "values.sdds", tmp_path / "experiment.namelist"
  _write_columns(source, [("Value", "double", 1), ("Metadata", "enum", -7)], encoding)
  header, page = source.read_bytes().rsplit(b"&end\n", 1)
  source.write_bytes(header + b"&end\n" + page * 2)
  name_field = "PV_name" if program == "sddsvexperiment" else "control_name"
  experiment.write_text(
    f'&variable {name_field}="{soft_ioc.pv("double")}", units="V", values_file="{source}", '
    'values_file_column="Value", index_number=0 &end\n', encoding="ascii",
  )
  arguments = [str(experiment)]
  if program == "sddsvexperiment":
    arguments.append(str(tmp_path / "output.sdds"))

  soft_ioc.run(program, *arguments, "-dryRun", "-pendIOtime=2")


@pytest.mark.parametrize("program", ["sddsvmonitor", "sddspermissive", "squishPVs"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("valid_first_page", [False, True], ids=["first-page", "later-page"])
def test_control_lists_reject_invalid_enum_pages(
  tmp_path: Path, program: str, encoding: str, valid_first_page: bool,
) -> None:
  source = tmp_path / "controls.sdds"
  if program == "sddsvmonitor":
    columns = [("ListData", "string", "unused")]
    parameters = '&parameter name=ListType, type=string, fixed_value="rootname" &end\n'
    arguments = [str(source), str(tmp_path / "output.sdds"), "-steps=1"]
  elif program == "sddspermissive":
    columns = [("ControlName", "string", "unused"), ("Description", "string", "test"),
               ("MinimumValue", "double", 0), ("MaximumValue", "double", 1)]
    parameters = ""
    arguments = [str(source), "-runControlPV=string=unused"]
  else:
    columns = [("BpmPV", "string", "unused")]
    parameters = ('&parameter name=Gain, type=double, fixed_value=1 &end\n'
                  '&parameter name=CorrectorPV, type=string, fixed_value="unused" &end\n')
    arguments = [str(source)]
  _write_invalid_metadata_page(source, columns, encoding, valid_first_page, parameters)

  result = _invoke([str(_executable(program)), *arguments, "-pendIOtime=0.01"],
                   _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0
  assert "enum" in (result.stderr + result.stdout).lower(), result.stderr + result.stdout


@pytest.mark.parametrize("program", ["sddsglitchlogger", "sddspvaglitchlogger"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("valid_first_page", [False, True], ids=["first-page", "later-page"])
def test_glitch_loggers_reject_invalid_enum_pages(
  tmp_path: Path, program: str, encoding: str, valid_first_page: bool,
) -> None:
  source = tmp_path / "glitches.sdds"
  parameters = ('&parameter name=TriggerControlName, type=string, fixed_value="unused" &end\n'
                '&parameter name=OutputRootname, type=string, fixed_value="glitch" &end\n'
                '&parameter name=GlitchThreshold, type=double, fixed_value=1 &end\n')
  _write_invalid_metadata_page(source, [("ControlName", "string", "unused")],
                               encoding, valid_first_page, parameters)

  result = _invoke([str(_executable(program)), str(source), str(tmp_path),
                    "-time=0.01", "-pendIOtime=0.01"],
                   _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0
  assert "enum" in result.stderr.lower(), result.stderr
  assert not list(tmp_path.glob("glitch-*"))


@pytest.mark.parametrize("program", ["sddsglitchlogger", "sddspvaglitchlogger"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_glitch_loggers_accept_complete_enum_metadata(
  soft_ioc: SoftIOC, tmp_path: Path, program: str, encoding: str, monkeypatch: pytest.MonkeyPatch,
) -> None:
  source = tmp_path / "glitches.sdds"
  pv = soft_ioc.pv("double")
  parameters = (f'&parameter name=TriggerControlName, type=string, fixed_value="{pv}" &end\n'
                '&parameter name=OutputRootname, type=string, fixed_value="glitch" &end\n'
                '&parameter name=GlitchThreshold, type=double, fixed_value=1 &end\n')
  _write_columns(source, [("ControlName", "string", pv), ("Metadata", "enum", -7)],
                 encoding, parameters=parameters)
  header, page = source.read_bytes().rsplit(b"&end\n", 1)
  source.write_bytes(header + b"&end\n" + page * 2)

  # Keep the root passed to SDDS's legacy generation-filename helper short,
  # even when pytest's artifact directory has a long absolute path.
  monkeypatch.chdir(tmp_path)
  soft_ioc.run(program, str(source), ".", "-time=0.02",
               "-sampleInterval=0.01", "-circularBuffer=before=0,after=1", "-pendIOtime=2")

  outputs = list(tmp_path.glob("glitch-*"))
  assert len(outputs) == 1
  assert outputs[0].read_bytes().startswith(b"SDDS")


@pytest.mark.parametrize("program", ["sddsmonitor", "sddsstatmon", "sddslogger", "sddssynchlog"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_scalar_lists_report_invalid_extra_enum_page(tmp_path: Path, program: str, encoding: str) -> None:
  source = tmp_path / "controls.sdds"
  _write_invalid_metadata_page(source, [("ControlName", "string", "unused"),
                                       ("ReadbackName", "string", "signal"),
                                       ("ReadbackUnits", "string", "V")], encoding, True)
  if program == "sddssynchlog":
    arguments = [str(source), str(tmp_path / "output.sdds"), "-samples=1", "-time=0.01"]
  else:
    arguments = [str(source), str(tmp_path / "output.sdds"), "-steps=1"]
  result = _invoke([str(_executable(program)), *arguments, "-pendIOtime=0.01"],
                   _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0
  assert "enum" in result.stderr.lower(), result.stderr


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_measurement_list_reports_invalid_extra_enum_page(soft_ioc: SoftIOC, tmp_path: Path, encoding: str) -> None:
  source, experiment = tmp_path / "measurements.sdds", tmp_path / "experiment.namelist"
  _write_invalid_metadata_page(source, [("ControlName", "string", soft_ioc.pv("double"))], encoding, True)
  experiment.write_text(f'&measurement_file filename="{source}" &end\n', encoding="ascii")

  result = soft_ioc.invoke("sddsexperiment", str(experiment), "-dryRun", "-pendIOtime=2")

  assert result.returncode > 0
  assert "enum" in result.stderr.lower(), result.stderr


@pytest.mark.parametrize("program", ["sddsvmonitor", "sddsvexperiment"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("field", ["Rootname", "Suffix"])
def test_vector_lists_report_invalid_extra_enum_page(
  soft_ioc: SoftIOC, tmp_path: Path, program: str, encoding: str, field: str,
) -> None:
  roots, suffixes = tmp_path / "roots.sdds", tmp_path / "suffixes.sdds"
  for path, name, value in ((roots, "Rootname", soft_ioc.prefix), (suffixes, "Suffix", "double")):
    columns = [(name, "string", value)]
    if name == field:
      _write_invalid_metadata_page(path, columns, encoding, True)
    else:
      _write_columns(path, columns, encoding)
  if program == "sddsvmonitor":
    result = soft_ioc.invoke(program, str(tmp_path / "output.sdds"), f"-rootname={roots}",
                             f"-suffix={suffixes}", "-steps=1", "-pendIOtime=2")
  else:
    experiment = tmp_path / "experiment.namelist"
    experiment.write_text(
      f'&variable PV_name="{soft_ioc.pv("double")}", index_number=0, index_limit=1 &end\n'
      '&execute &end\n', encoding="ascii",
    )
    result = soft_ioc.invoke(program, str(experiment), str(tmp_path / "output.sdds"),
                             f"-rootnameFile={roots}", f"-suffixFile={suffixes}", "-dryRun", "-pendIOtime=2")

  assert result.returncode > 0
  assert "enum" in result.stderr.lower(), result.stderr


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_waveform_monitor_scalar_list_reports_invalid_extra_enum_page(
  soft_ioc: SoftIOC, tmp_path: Path, encoding: str,
) -> None:
  source = tmp_path / "scalars.sdds"
  _write_invalid_metadata_page(source, [("ControlName", "string", soft_ioc.pv("double")),
                                       ("ReadbackName", "string", "signal"),
                                       ("ReadbackUnits", "string", "V")], encoding, True)

  result = soft_ioc.invoke("sddswmonitor", str(tmp_path / "output.sdds"),
                           f"-PVnames={soft_ioc.pv('array')}", f"-scalars={source}", "-steps=1", "-pendIOtime=2")

  assert result.returncode > 0
  assert "enum" in result.stderr.lower(), result.stderr


@pytest.mark.parametrize("program", ["sddswmonitor", "sddssynchlog", "sddsimagemonitor"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("valid_first_page", [False, True], ids=["first-page", "extra-page"])
def test_waveform_monitor_lists_report_invalid_enum_pages(
  soft_ioc: SoftIOC, tmp_path: Path, program: str, encoding: str, valid_first_page: bool,
) -> None:
  source, output = tmp_path / "waveforms.sdds", tmp_path / "output.sdds"
  _write_invalid_metadata_page(
    source, [("WaveformPV", "string", soft_ioc.pv("array")),
             ("WaveformName", "string", "waveform"), ("ReadbackUnits", "string", "V")],
    encoding, valid_first_page,
    '&parameter name=WaveformLength, type=long, fixed_value=4 &end\n',
  )
  if program in ("sddswmonitor", "sddsimagemonitor"):
    arguments = [str(source), str(output), "-steps=1"]
  else:
    scalars = tmp_path / "scalars.sdds"
    _write_columns(scalars, [("ControlName", "string", soft_ioc.pv("double")),
                            ("ReadbackName", "string", "scalar"),
                            ("ReadbackUnits", "string", "V")], encoding)
    arguments = [str(scalars), str(output), f"-waveformData={source}", "-samples=1", "-time=0.01"]

  result = soft_ioc.invoke(program, *arguments, "-pendIOtime=2")

  assert result.returncode > 0
  assert "enum" in result.stderr.lower(), result.stderr
  assert not output.exists()


@pytest.mark.parametrize("option", ["scalars", "assignments"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("valid_first_page", [False, True], ids=["first-page", "later-page"])
def test_bps_monitor_lists_reject_invalid_enum_pages(
  tmp_path: Path, option: str, encoding: str, valid_first_page: bool,
) -> None:
  source = tmp_path / "signals.sdds"
  _write_invalid_metadata_page(source, [("ControlName", "string", "unused"),
                                       ("ReadbackName", "string", "signal")], encoding, valid_first_page)

  result = _invoke([str(_executable("BPS16monitor")), str(tmp_path / "output.sdds"),
                    f"-{option}={source}", "-steps=1", "-pendIOtime=0.01"],
                   _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0
  assert "enum" in (result.stderr + result.stdout).lower(), result.stderr + result.stdout


@pytest.mark.parametrize("program", ["sddswget", "sddswgetdata",
                                    "sddstransferFFtoWaveform", "sddstransferscalartowaveform"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("valid_first_page", [False, True], ids=["first-page", "later-page"])
def test_waveform_readers_report_invalid_enum_pages(
  tmp_path: Path, program: str, encoding: str, valid_first_page: bool
) -> None:
  source = tmp_path / "input.sdds"
  header = (
    "SDDS6\n"
    '&parameter name=WaveformPV, type=string, fixed_value="unused" &end\n'
    '&column name=DeviceName, type=string &end\n'
    '&column name=Metadata, type=enum, enum_values="-7:valid" &end\n'
    f"&data mode={encoding}, endian=little &end\n"
  )

  def page(code: int) -> bytes:
    return (f"1\nunused {code}\n".encode("ascii") if encoding == "ascii"
            else struct.pack("<2i", 1, 6) + b"unused" + struct.pack("<i", code))

  source.write_bytes(header.encode("ascii") + (page(-7) if valid_first_page else b"") + page(999))
  arguments = [str(source)]
  if program in ("sddswget", "sddswgetdata"):
    arguments += [str(tmp_path / "output.sdds"), "-inputStyle=wget"]
  else:
    arguments += ["-steps=1"]
  result = _invoke([str(_executable(program)), *arguments, "-pendIOtime=0.01"],
                   _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0
  assert "enum" in result.stderr.lower(), result.stderr
  assert "problem doing search" not in result.stderr


@pytest.mark.parametrize("program", ["sddswget", "sddswgetdata"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_waveform_lists_report_invalid_extra_enum_page(tmp_path: Path, program: str, encoding: str) -> None:
  source = tmp_path / "waveforms.sdds"
  header = (
    "SDDS6\n"
    '&column name=WaveformPV, type=string &end\n'
    '&column name=Metadata, type=enum, enum_values="-7:valid" &end\n'
    f"&data mode={encoding}, endian=little &end\n"
  )

  def page(code: int) -> bytes:
    return (f"1\nunused {code}\n".encode("ascii") if encoding == "ascii"
            else struct.pack("<2i", 1, 6) + b"unused" + struct.pack("<i", code))

  source.write_bytes(header.encode("ascii") + page(-7) + page(999))
  arguments = [str(source), str(tmp_path / "output.sdds"), "-inputStyle=wmonitor", "-pendIOtime=0.01"]
  if program == "sddswget":
    arguments.append("-provider=ca")
  result = _invoke([str(_executable(program)), *arguments],
                   _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0
  assert "enum" in result.stderr.lower(), result.stderr
  assert "problem doing search" not in result.stderr


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("table", ["preset", "bunch"])
def test_control_bits_reject_invalid_enum_pages(tmp_path: Path, encoding: str, table: str) -> None:
  preset, bunch, output = tmp_path / "preset.sdds", tmp_path / "bunch.sdds", tmp_path / "bits.sdds"
  for name, path, columns in (("preset", preset, PRESET_COLUMNS), ("bunch", bunch, BUNCH_COLUMNS)):
    # An unused metadata column still invalidates the complete input page.
    metadata = [("Metadata", "enum", 999)] if name == table else []
    _write_columns(path, columns + metadata, encoding)
  result = _invoke([str(_executable("sddsgencontrolbits")), str(output),
                    f"-presetRAMconfiguration=chosen,file={preset}", f"-bunchPatternFile={bunch}"],
                   _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0
  assert f"Unable to read {table}" in result.stderr
  assert not output.exists()
