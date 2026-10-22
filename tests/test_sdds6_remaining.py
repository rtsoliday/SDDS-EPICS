"""SDDS6 monitor storage validation and raw RAM categorical parameters."""

from __future__ import annotations

import struct
from pathlib import Path

import pytest

from conftest import _executable, _invoke, _sdds_executable
from test_rampload import _isolated_ca_environment


def _run(program: str, *arguments: str):
  return _invoke([str(_executable(program)), *arguments], _isolated_ca_environment(), timeout=3)


def _parameter_values(source: Path, names: list[str]) -> list[str]:
  result = _invoke([str(_sdds_executable("sdds2stream")), str(source),
                    f"-parameters={','.join(names)}", "-noquotes"], _isolated_ca_environment())
  assert result.returncode == 0, result.stderr
  return result.stdout.split()


@pytest.mark.parametrize("categorical", [False, True], ids=["character", "enum"])
def test_mv20_converter_checks_parsed_pixel_type(tmp_path: Path, categorical: bool):
  source, output = tmp_path / "image.mv20", tmp_path / "image.sdds"
  # The native MV20 header describes a single character pixel. An enum label
  # containing "type=character" must not pass the legacy substring check and
  # cause the one-byte pixel buffer to be read as four-byte enum codes.
  header = struct.pack("=128s128s8i8f16i32i", b"image", b"", 0, 1, 1, 1,
                       *([0] * 4), *([0.0] * 8), *([0] * 48))
  source.write_bytes(header + b"x")
  definition = ("-definition=Image,type=enum,enum_values=120:type=character"
                if categorical else "-definition=Image,type=character")

  result = _run("MV20image2sdds", str(source), str(output), definition)

  if categorical:
    assert result.returncode > 0
    assert "data type must be character" in result.stderr
    assert not output.exists() or output.stat().st_size == 0
  else:
    assert result.returncode == 0, result.stderr
    pixel = _invoke([str(_sdds_executable("sdds2stream")), str(output), "-columns=Image"],
                     _isolated_ca_environment())
    assert pixel.returncode == 0, pixel.stderr
    assert pixel.stdout.strip() == "x"


@pytest.mark.parametrize("program", ["sddswmonitor", "sddsimagemonitor"])
@pytest.mark.parametrize("data_type", ["enum", "unknown"])
def test_monitors_reject_output_types_without_storage_definitions(tmp_path: Path, program: str, data_type: str):
  result = _run(program, str(tmp_path / "output.sdds"), "-PVnames=unused",
                f"-dataType={data_type}", "-steps=1", "-pendIOtime=0.01")

  assert result.returncode > 0
  assert "-dataType" in result.stderr
  assert "problem doing search" not in result.stderr


@pytest.mark.parametrize("data_type", ["enum", "unknown"])
def test_waveform_monitor_validates_configured_storage_types(tmp_path: Path, data_type: str):
  source = tmp_path / "monitor.sdds"
  source.write_text(
    "SDDS6\n"
    "&column name=WaveformPV, type=string &end\n"
    "&column name=WaveformName, type=string &end\n"
    "&column name=DataType, type=string &end\n"
    '&column name=Metadata, type=enum, enum_values="-7:unused" &end\n'
    f"&data mode=ascii &end\n1\nunused waveform {data_type} -7\n",
    encoding="ascii",
  )

  result = _run("sddswmonitor", str(source), str(tmp_path / "output.sdds"),
                "-steps=1", "-pendIOtime=0.01")

  assert result.returncode > 0
  assert "Invalid waveform DataType" in result.stderr
  assert "problem doing search" not in result.stderr


def _write_ram(source: Path, parameters: list[tuple[str, str, str | int]], encoding: str) -> None:
  header = "SDDS6\n"
  for name, kind, value in parameters:
    if kind == "enum":
      definition = f'type=enum, enum_values="-7:{value}", fixed_value=-7'
    elif kind == "string":
      definition = f'type=string, fixed_value="{value}"'
    else:
      definition = f"type={kind}, fixed_value={value}"
    header += f"&parameter name={name}, {definition} &end\n"
  header += f"&column name=Waveform, type=ulong &end\n&data mode={encoding}, endian=little &end\n"
  # One complete turn with a turn marker and wrap marker, without any PV names.
  waveform = [0x40000000] + [0] * 322 + [0x80000000]
  data = (("324\n" + "\n".join(map(str, waveform)) + "\n").encode("ascii")
          if encoding == "ascii" else struct.pack("<i324I", 324, *waveform))
  source.write_bytes(header.encode("ascii") + data)


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("per_receiver", [False, True])
@pytest.mark.parametrize("text_type", ["string", "enum", "mixed"])
def test_raw_ram_uses_labels_for_textual_parameters(
  tmp_path: Path, encoding: str, per_receiver: bool, text_type: str
):
  source, output = tmp_path / "ram.sdds", tmp_path / "bits.sdds"
  parameters = [("Comment", "enum" if text_type == "enum" else "string", "ram-comment"),
                ("CommutationMode", "enum" if text_type == "enum" else "string", "a")]
  expected = [("Comment", "ram-comment"), ("CommutationMode", "a")]
  for name, label in (("PlaneMode", "x"), ("AccumulatorMode", "y"),
                      ("SampleMode", "continuous"), ("BunchPattern", "pattern")):
    for receiver in range(4) if per_receiver else [None]:
      kind = "enum" if text_type == "enum" or (text_type == "mixed" and receiver == 1) else "string"
      parameters.append((name if receiver is None else f"{name}{receiver}", kind, label))
    expected += [(f"{name}{receiver}", label) for receiver in range(4)]
  parameters += [("TurnMarkerInterval", "long", 324), ("TransitionDeadTime", "long", 9)]
  for receiver in range(4) if per_receiver else [None]:
    parameters.append(("SamplesPerBunch" if receiver is None else f"SamplesPerBunch{receiver}", "long", 7))
  expected += [("TransitionDeadTime", "9")] + [(f"SamplesPerBunch{i}", "7") for i in range(4)]
  _write_ram(source, parameters, encoding)

  result = _run("sddsgencontrolbits", str(source), str(output), "-scopeArrayLength=324")

  assert result.returncode == 0, result.stderr
  values = _parameter_values(output, [name for name, _ in expected])
  assert values == [value for _, value in expected] * 2


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("field", ["TurnMarkerInterval", "TurnMarkerOffset", "TransitionDeadTime",
                                    "TransistionDeadTime", "SamplesPerBunch", "SamplesPerBunch1"])
def test_raw_ram_rejects_enum_counts(tmp_path: Path, encoding: str, field: str):
  source = tmp_path / "ram.sdds"
  parameters = [(field, "enum", "categorical")]
  if field == "SamplesPerBunch1":
    parameters.insert(0, ("SamplesPerBunch0", "long", 1))
  _write_ram(source, parameters, encoding)

  result = _run("sddsgencontrolbits", str(source), str(tmp_path / "bits.sdds"))

  assert result.returncode > 0
  assert f"RAM parameter {field} must have long type" in result.stderr


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("field", ["PlaneMode1", "AccumulatorMode1", "SampleMode1", "BunchPattern1"])
def test_raw_ram_checks_each_receiver_text_type(tmp_path: Path, encoding: str, field: str):
  source = tmp_path / "ram.sdds"
  prefix = field[:-1]
  label = "continuous" if prefix == "SampleMode" else "x"
  parameters = [(f"{prefix}0", "string", label), (field, "long", 7)]
  _write_ram(source, parameters, encoding)

  result = _run("sddsgencontrolbits", str(source), str(tmp_path / "bits.sdds"))

  assert result.returncode > 0
  assert f"RAM parameter {field} must have string or enum type" in result.stderr


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("field", ["PlaneMode", "PlaneMode1", "AccumulatorMode", "AccumulatorMode1",
                                    "CommutationMode", "SampleMode", "SampleMode1"])
def test_raw_ram_rejects_unrecognized_enum_mode_labels(tmp_path: Path, encoding: str, field: str):
  source = tmp_path / "ram.sdds"
  parameters = [(field, "enum", "unsupported")]
  if field.endswith("1"):
    label = "continuous" if field.startswith("SampleMode") else "x"
    parameters.insert(0, (f"{field[:-1]}0", "string", label))
  _write_ram(source, parameters, encoding)

  result = _run("sddsgencontrolbits", str(source), str(tmp_path / "bits.sdds"))

  assert result.returncode > 0
  assert f"Invalid value for RAM parameter {field}: unsupported" in result.stderr
