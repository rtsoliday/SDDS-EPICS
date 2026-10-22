"""SDDS6 categorical input and metadata preservation with the isolated test IOC."""

from __future__ import annotations

import os
import struct
import subprocess
from pathlib import Path

import pytest

from conftest import SoftIOC, _command_arguments, _executable, _invoke


MAPPING = "-7:ON,19:OFF,101:AUTO"


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize(("input_style", "extra_page", "pipe_input"), [
  ("wmonitor", False, False), ("wmonitor", True, False),
  ("wmonitor", True, True), ("wget", False, False),
])
def test_sddswget_preserves_enum_metadata(
  soft_ioc: SoftIOC, tmp_path: Path, encoding: str, input_style: str, extra_page: bool, pipe_input: bool
) -> None:
  source, output = tmp_path / "input.sdds", tmp_path / "output.sdds"
  pv = soft_ioc.pv("array")
  header = (
    "SDDS6\n"
    f'&parameter name=FixedState, type=enum, enum_values="{MAPPING}", fixed_value=19 &end\n'
    f'&parameter name=PageState, type=enum, enum_values="{MAPPING}" &end\n'
  )
  if input_style == "wmonitor":
    header += '&column name=WaveformPV, type=string &end\n'
    # Multiple rows exercise the four-byte enum stride when columns become
    # parameters on successive output pages, including on LP64 platforms.
    codes = [101, 19, -7]
  else:
    header += f'&parameter name=WaveformPV, type=string, fixed_value="{pv}" &end\n'
    codes = [-7, 19, 101, -7]
  header += (f'&column name=State, type=enum, enum_values="{MAPPING}" &end\n'
             f'&data mode={encoding}, endian=little &end\n')
  if encoding == "ascii":
    rows = [f'"{pv}" {code}' if input_style == "wmonitor" else str(code) for code in codes]
    data = ("-7\n" + str(len(codes)) + "\n" + "\n".join(rows) + "\n").encode("ascii")
  else:
    data = struct.pack("<2i", len(codes), -7)
    if input_style == "wmonitor":
      text = pv.encode("ascii")
      for code in codes:
        data += struct.pack("<i", len(text)) + text + struct.pack("<i", code)
    else:
      data += struct.pack(f"<{len(codes)}i", *codes)
  if extra_page:
    # The extra page is ignored, including its distinct parameter/column codes.
    extra = (data.replace(b"-7", b"19").replace(b"101", b"19") if encoding == "ascii" else
             data.replace(struct.pack("<i", -7), struct.pack("<i", 19))
                 .replace(struct.pack("<i", 101), struct.pack("<i", 19)))
  else:
    extra = b""
  source.write_bytes(header.encode("ascii") + data + extra)

  if pipe_input:
    stdout_file, stderr_file = tmp_path / "pipe.stdout", tmp_path / "pipe.stderr"
    with stdout_file.open("wb") as stdout, stderr_file.open("wb") as stderr:
      result = subprocess.run(
        _command_arguments([str(_executable("sddswget")), str(output), "-pipe=input",
                            f"-inputStyle={input_style}", "-provider=ca", "-pendIOtime=2"]),
        env=soft_ioc.environment, input=source.read_bytes(), stdout=stdout, stderr=stderr, timeout=10,
      )
    assert result.returncode == 0, stdout_file.read_text() + stderr_file.read_text()
  else:
    soft_ioc.run("sddswget", str(source), str(output), f"-inputStyle={input_style}",
                 "-provider=ca", "-pendIOtime=2")

  assert output.read_bytes().startswith(b"SDDS6\n")
  query = soft_ioc.run_sdds("sddsquery", str(output))
  assert query.stdout.count(f'enum_values="{MAPPING}"') == 3
  states = soft_ioc.run_sdds("sdds2stream", str(output), "-parameters=FixedState,PageState", "-noquotes")
  output_pages = len(codes) if input_style == "wmonitor" else 1
  assert states.stdout.split() == ["OFF", "ON"] * output_pages
  field = "-parameters=State" if input_style == "wmonitor" else "-columns=State"
  raw = soft_ioc.run_sdds("sdds2stream", str(output), field, "-enumCodes", "-noquotes")
  assert [int(value) for value in raw.stdout.split()] == codes


def _write_metadata_input(path: Path, control_name: str, encoding: str, overwrite_existing: bool) -> None:
  header = (
    "SDDS6\n"
    f'&parameter name=State, type=enum, enum_values="{MAPPING}" &end\n'
    f'&parameter name=FixedState, type=enum, enum_values="{MAPPING}", fixed_value="19" &end\n'
    f'&array name=History, type=enum, enum_values="{MAPPING}", dimensions=1 &end\n'
    "&column name=ControlName, type=string &end\n"
    f'&column name=MetadataState, type=enum, enum_values="{MAPPING}" &end\n'
  )
  if overwrite_existing:
    header += (
      f'&parameter name=TimeStamp, type=enum, enum_values="{MAPPING}", fixed_value="19" &end\n'
      f'&column name=Value, type=enum, enum_values="{MAPPING}" &end\n'
      f'&column name=CAError, type=enum, enum_values="{MAPPING}" &end\n'
    )
  header += f"&data mode={encoding}, endian=little &end\n"
  if encoding == "ascii":
    extra_values = " 19 -7" if overwrite_existing else ""
    data = f'-7\n2\n19 -7\n1\n"{control_name}" 101{extra_values}\n'.encode("utf-8")
  else:
    name = control_name.encode("utf-8")
    # Binary pages begin with the row count, then parameters, arrays and rows.
    data = struct.pack("<6i", 1, -7, 2, 19, -7, len(name)) + name + struct.pack("<i", 101)
    if overwrite_existing:
      data += struct.pack("<2i", 19, -7)
  path.write_bytes(header.encode("utf-8") + data)


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize(
  ("program", "overwrite_existing"),
  [("sddssnapshot", False), ("sddssnapshot", True), ("sddscainfo", False)],
)
def test_sdds6_metadata_survives_readback_programs(
  soft_ioc: SoftIOC,
  tmp_path: Path,
  encoding: str,
  program: str,
  overwrite_existing: bool,
) -> None:
  input_file = tmp_path / f"{program}-{encoding}-input.sdds"
  output_file = tmp_path / f"{program}-{encoding}-output.sdds"
  _write_metadata_input(input_file, soft_ioc.pv("double"), encoding, overwrite_existing)
  soft_ioc.put("double", "1.25")

  soft_ioc.run(program, str(input_file), str(output_file), "-pendIOtime=2")

  assert output_file.read_bytes().startswith(b"SDDS6\n")
  query = soft_ioc.run_sdds("sddsquery", str(output_file))
  assert query.stdout.count(f'enum_values="{MAPPING}"') == 4
  for option, labels, codes in (
    ("-columns=MetadataState", ["AUTO"], ["101"]),
    ("-parameters=State", ["ON"], ["-7"]),
    ("-parameters=FixedState", ["OFF"], ["19"]),
    ("-arrays=History", ["OFF", "ON"], ["19", "-7"]),
  ):
    displayed = soft_ioc.run_sdds("sdds2stream", str(output_file), option, "-noquotes")
    raw = soft_ioc.run_sdds("sdds2stream", str(output_file), option, "-enumCodes", "-noquotes")
    assert displayed.stdout.split() == labels
    assert raw.stdout.split() == codes
  if program == "sddssnapshot":
    values = soft_ioc.run_sdds("sdds2stream", str(output_file), "-columns=Value,CAError", "-noquotes")
    reading, error = values.stdout.split()
    assert float(reading) == pytest.approx(1.25)
    assert error == "n"


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("program", ["sddssnapshot", "sddscainfo"])
@pytest.mark.parametrize("valid_first_page", [False, True], ids=["first-page", "later-page"])
def test_readback_programs_report_invalid_enum_pages(
  soft_ioc: SoftIOC,
  tmp_path: Path,
  encoding: str,
  program: str,
  valid_first_page: bool,
) -> None:
  source = tmp_path / "invalid-metadata.sdds"
  output = tmp_path / "output.sdds"
  _write_metadata_input(source, soft_ioc.pv("double"), encoding, False)
  header, page = source.read_bytes().rsplit(b"&end\n", 1)
  invalid_page = page.replace(b"101", b"999") if encoding == "ascii" else page[:-4] + struct.pack("<i", 999)
  source.write_bytes(header + b"&end\n" + (page if valid_first_page else b"") + invalid_page)

  result = soft_ioc.invoke(program, str(source), str(output), "-pendIOtime=2")

  assert result.returncode > 0
  assert "enum" in result.stderr.lower()


def test_sddswput_uses_enum_labels_instead_of_file_codes(soft_ioc: SoftIOC, tmp_path: Path) -> None:
  input_file = tmp_path / "enum-waveform.sdds"
  input_file.write_text(
    "SDDS6\n"
    f'&parameter name=WaveformPV, type=string, fixed_value="{soft_ioc.pv("enum")}" &end\n'
    f'&column name=Waveform, type=enum, enum_values="{MAPPING}" &end\n'
    "&data mode=ascii &end\n"
    "1\n-7\n",
    encoding="utf-8",
  )
  soft_ioc.put("enum", "OFF")

  soft_ioc.run("sddswput", str(input_file), "-pendIOtime=2")

  assert soft_ioc.get("enum") == "ON"


def test_sddscaramp_uses_enum_labels_without_numeric_ramping(soft_ioc: SoftIOC, tmp_path: Path) -> None:
  input_file = tmp_path / "enum-ramp.sdds"
  input_file.write_text(
    "SDDS6\n"
    "&column name=ControlName, type=string &end\n"
    f'&column name=Value, type=enum, enum_values="{MAPPING}" &end\n'
    "&data mode=ascii &end\n"
    f'1\n"{soft_ioc.pv("enum")}" 101\n',
    encoding="utf-8",
  )
  soft_ioc.put("enum", "OFF")

  soft_ioc.run("sddscaramp", f"-rampTo={input_file},steps=2,pause=0", "-pendIOtime=2")

  assert soft_ioc.get("enum") == "AUTO"


@pytest.mark.parametrize("one_file", [False, True])
def test_sddscasr_restores_enum_waveform_labels(
  soft_ioc: SoftIOC,
  tmp_path: Path,
  one_file: bool,
) -> None:
  if os.name == "nt" and not one_file:
    pytest.skip("per-PV waveform filenames contain colons from the test PV names")
  input_file = tmp_path / "restore-input.sdds"
  root = tmp_path / "restore"
  waveform_file = Path(f"{root}.waveform" if one_file else f"{root}.{soft_ioc.pv('array')}")
  readback_file = tmp_path / "restored-waveform.sdds"
  input_file.write_text(
    "SDDS1\n"
    "&column name=ControlName, type=string &end\n"
    "&column name=IndirectName, type=string &end\n"
    "&column name=ValueString, type=string &end\n"
    "&data mode=ascii &end\n"
    f'1\n"{soft_ioc.pv("array")}" WaveformPV WaveformPV\n',
    encoding="ascii",
  )
  waveform_file.write_text(
    "SDDS6\n"
    f'&parameter name=WaveformPV, type=string, fixed_value="{soft_ioc.pv("array")}" &end\n'
    '&column name=Waveform, type=enum, enum_values="-7:1,19:2,101:3,-101:4" &end\n'
    "&data mode=ascii &end\n"
    "4\n-7\n19\n101\n-101\n",
    encoding="ascii",
  )
  waveform_option = f"-waveform=rootname={root}" + (",onefile" if one_file else "")

  soft_ioc.run("sddscasr", str(input_file), "-restore", waveform_option, "-pendIOtime=2")
  soft_ioc.run(
    "sddswget",
    f"-PVnames={soft_ioc.pv('array')}",
    str(readback_file),
    "-pendIOtime=2",
    "-provider=ca",
  )

  values = soft_ioc.run_sdds("sdds2stream", str(readback_file), "-columns=Waveform")
  assert [float(value) for value in values.stdout.split()] == pytest.approx([1, 2, 3, 4])


def test_sddsoptimize_rejects_enum_direction_matrix(tmp_path: Path) -> None:
  variables = tmp_path / "variables.sdds"
  matrix = tmp_path / "directions.sdds"
  variables.write_text(
    "SDDS1\n"
    "&column name=ControlName, type=string &end\n"
    "&column name=LowerLimit, type=double &end\n"
    "&column name=UpperLimit, type=double &end\n"
    "&column name=InitialValue, type=double &end\n"
    "&column name=InitialChange, type=double &end\n"
    "&data mode=ascii &end\n"
    "1\nunused -1 1 0 0.1\n",
    encoding="ascii",
  )
  matrix.write_text(
    "SDDS6\n"
    f'&column name=Direction, type=enum, enum_values="{MAPPING}" &end\n'
    "&data mode=ascii &end\n"
    "1\n-7\n",
    encoding="ascii",
  )
  environment = os.environ.copy()
  environment.update(
    {
      "EPICS_CA_ADDR_LIST": "127.0.0.1:1",
      "EPICS_CA_AUTO_ADDR_LIST": "NO",
      "EPICS_PVA_ADDR_LIST": "127.0.0.1:1",
      "EPICS_PVA_AUTO_ADDR_LIST": "NO",
    }
  )
  # A supplied initial value and variable script avoid CA searches. Dry run
  # disables execution of the script; invalid matrix data must fail first.
  result = _invoke(
    [
      str(_executable("sddsoptimize")),
      f"-varFile={variables}",
      "-varScript=unused",
      "-dryRun",
      f"-rcds=dmatFile={matrix}",
    ],
    environment,
    timeout=3,
  )

  assert result.returncode > 0, result.stderr
  assert "non-numeric" in result.stderr.lower()
