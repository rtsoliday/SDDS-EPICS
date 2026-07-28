"""Integration coverage for additional SDDS-EPICS command-line programs."""

from __future__ import annotations

import os
import subprocess
import time
from pathlib import Path

import pytest

from conftest import SoftIOC, _binary_directory, _command_arguments, _executable, _invoke, _unused_udp_port


def _write_control_names(path: Path, *control_names: str) -> None:
  rows = "".join(f'"{name}"\n' for name in control_names)
  path.write_text(
    "SDDS1\n"
    "&column name=ControlName, type=string, &end\n"
    "&data mode=ascii, no_row_counts=1, &end\n"
    f"{rows}",
    encoding="ascii",
  )


def test_sddssnapshot_output_and_sampling_options(soft_ioc: SoftIOC, tmp_path: Path) -> None:
  input_file = tmp_path / "snapshot-input.sdds"
  output_file = tmp_path / "snapshot-output.sdds"
  _write_control_names(input_file, soft_ioc.pv("double"), soft_ioc.pv("integer"))
  soft_ioc.put("double", "1.25")
  soft_ioc.put("integer", "4")

  soft_ioc.run(
    "sddssnapshot",
    str(input_file),
    str(output_file),
    "-pendIOtime=2",
    "-nameOfData=Reading",
    "-unitsOfData=arb",
    "-average=2,0",
  )

  assert output_file.read_bytes().startswith(b"SDDS1")
  query = soft_ioc.run_sdds("sddsquery", str(output_file), "-columnList")
  values = soft_ioc.run_sdds("sdds2stream", str(output_file), "-columns=ControlName,Reading")
  assert "Reading" in query.stdout
  assert soft_ioc.pv("double") in values.stdout
  assert "1.25" in values.stdout
  assert soft_ioc.pv("integer") in values.stdout
  assert "4" in values.stdout


def test_sddssnapshot_obsolete_timing_option(soft_ioc: SoftIOC, tmp_path: Path) -> None:
  input_file = tmp_path / "snapshot-timing-input.sdds"
  output_file = tmp_path / "snapshot-timing-output.sdds"
  _write_control_names(input_file, soft_ioc.pv("double"))

  soft_ioc.run(
    "sddssnapshot",
    str(input_file),
    str(output_file),
    "-ezcaTiming=0.1,20",
  )

  assert output_file.exists()


@pytest.mark.parametrize(
  ("option", "expected_column"),
  [
    ("-makeLoggerInput=withStorageType", "StorageType"),
    ("-makeSCRInput", "ExpectElements"),
  ],
)
def test_sddscainfo_output_modes(
  soft_ioc: SoftIOC,
  tmp_path: Path,
  option: str,
  expected_column: str,
) -> None:
  input_file = tmp_path / f"cainfo-{expected_column}-input.sdds"
  output_file = tmp_path / f"cainfo-{expected_column}-output.sdds"
  _write_control_names(input_file, soft_ioc.pv("double"), soft_ioc.pv("enum"))

  result = soft_ioc.run(
    "sddscainfo",
    str(input_file),
    str(output_file),
    option,
    "-pendIOTime=2",
    "-verbose",
  )

  query = soft_ioc.run_sdds("sddsquery", str(output_file), "-columnList")
  providers = soft_ioc.run_sdds("sdds2stream", str(output_file), "-columns=ControlName,Provider,Active")
  assert result.returncode == 0
  assert expected_column in query.stdout
  assert "Provider" in query.stdout
  assert soft_ioc.pv("double") in providers.stdout
  assert "ca" in providers.stdout


def test_sddswput_and_sddswget_file_workflow(soft_ioc: SoftIOC, tmp_path: Path) -> None:
  put_file = tmp_path / "waveform-put.sdds"
  get_file = tmp_path / "waveform-get.sdds"
  put_file.write_text(
    "SDDS1\n"
    f'&parameter name=WaveformPV, type=string, fixed_value="{soft_ioc.pv("array")}", &end\n'
    "&column name=Waveform, type=double, &end\n"
    "&data mode=ascii, no_row_counts=1, &end\n"
    "1\n2\n3\n4\n",
    encoding="ascii",
  )

  soft_ioc.run("sddswput", str(put_file), "-pendIOtime=2")
  soft_ioc.run(
    "sddswget",
    f"-PVnames={soft_ioc.pv('array')}",
    str(get_file),
    "-pendIOtime=2",
    "-provider=ca",
  )

  waveform = soft_ioc.run_sdds("sdds2stream", str(get_file), "-columns=Waveform")
  assert [float(value) for value in waveform.stdout.split()] == pytest.approx([1, 2, 3, 4])


def test_sddswget_wmonitor_input_style(soft_ioc: SoftIOC, tmp_path: Path) -> None:
  input_file = tmp_path / "wget-input.sdds"
  output_file = tmp_path / "wget-output.sdds"
  input_file.write_text(
    "SDDS1\n"
    "&column name=WaveformPV, type=string, &end\n"
    "&column name=Description, type=string, &end\n"
    "&data mode=ascii, no_row_counts=1, &end\n"
    f'"{soft_ioc.pv("array")}" "test waveform"\n',
    encoding="ascii",
  )

  soft_ioc.run(
    "sddswget",
    str(input_file),
    "-inputStyle=wmonitor",
    str(output_file),
    "-pendIOtime=2",
    "-provider=ca",
  )

  query = soft_ioc.run_sdds("sddsquery", str(output_file), "-parameterList")
  assert "WaveformPV" in query.stdout
  assert "Description" in query.stdout


def test_sddspvtest_pass_output_and_exit_options(soft_ioc: SoftIOC, tmp_path: Path) -> None:
  input_file = tmp_path / "pvtest-pass.sdds"
  input_file.write_text(
    "SDDS1\n"
    "&column name=ControlName, type=string, &end\n"
    "&column name=MinimumValue, type=double, &end\n"
    "&column name=MaximumValue, type=double, &end\n"
    "&column name=Monitor, type=character, &end\n"
    "&data mode=ascii, no_row_counts=1, &end\n"
    f'"{soft_ioc.pv("double")}" 0 10 Y\n',
    encoding="ascii",
  )
  soft_ioc.put("double", "5")

  result = soft_ioc.invoke(
    "sddspvtest",
    str(input_file),
    f"-pvOutput={soft_ioc.pv('integer')},passValue=7,failValue=9",
    "-time=1,seconds",
    "-interval=0.05,seconds",
    "-monitor",
    "-exitcondition=allPass",
    "-pendIOtime=2",
    "-verbose",
    timeout=5,
  )

  assert result.returncode == 0
  assert float(soft_ioc.get("integer")) == pytest.approx(7)


def test_sddspvtest_any_fail_exit_condition(soft_ioc: SoftIOC, tmp_path: Path) -> None:
  input_file = tmp_path / "pvtest-fail.sdds"
  input_file.write_text(
    "SDDS1\n"
    "&column name=ControlName, type=string, &end\n"
    "&column name=MinimumValue, type=double, &end\n"
    "&column name=MaximumValue, type=double, &end\n"
    "&data mode=ascii, no_row_counts=1, &end\n"
    f'"{soft_ioc.pv("double")}" 100 200\n',
    encoding="ascii",
  )
  soft_ioc.put("double", "5")

  result = soft_ioc.invoke(
    "sddspvtest",
    str(input_file),
    f"-pvOutput={soft_ioc.pv('integer')},passValue=7,failValue=9",
    "-time=1,seconds",
    "-interval=0.05,seconds",
    "-exitcondition=anyFail",
    "-pendIOtime=2",
    timeout=5,
  )

  assert result.returncode == 1
  assert float(soft_ioc.get("integer")) == pytest.approx(9)


def test_sddspvalogger_accepts_transition_only_trigger_file(
  soft_ioc: SoftIOC,
  tmp_path: Path,
) -> None:
  input_file = tmp_path / "pvalogger-input.sdds"
  trigger_file = tmp_path / "pvalogger-trigger.sdds"
  output_file = tmp_path / "pvalogger-output.sdds"
  input_file.write_text(
    "SDDS1\n"
    "&column name=ControlName, type=string, &end\n"
    "&column name=Provider, type=string, &end\n"
    "&column name=ExpectNumeric, type=character, &end\n"
    "&column name=ExpectFieldType, type=string, &end\n"
    "&column name=ExpectElements, type=long, &end\n"
    "&data mode=ascii, no_row_counts=1, &end\n"
    f'"{soft_ioc.pv("double")}" ca y scalar 1\n',
    encoding="ascii",
  )
  trigger_file.write_text(
    "SDDS1\n"
    "&column name=TriggerControlName, type=string, &end\n"
    "&column name=Provider, type=string, &end\n"
    "&column name=TransitionThreshold, type=double, &end\n"
    "&column name=TransitionDirection, type=short, &end\n"
    "&data mode=ascii, no_row_counts=1, &end\n"
    f'"{soft_ioc.pv("double")}" ca 0 -1\n',
    encoding="ascii",
  )

  result = soft_ioc.invoke(
    "sddspvalogger",
    str(input_file),
    str(output_file),
    "-sampleInterval=0.05,sec",
    f"-triggerFile={trigger_file}",
    "-time=0.2,sec",
    "-overwrite",
    "-pendIOtime=2",
    timeout=5,
  )

  assert result.returncode == 0
  assert output_file.read_bytes().startswith(b"SDDS1")


def test_sddspvalogger_default_buffer_writes_only_trigger_sample(
  soft_ioc: SoftIOC,
  tmp_path: Path,
) -> None:
  input_file = tmp_path / "pvalogger-single-sample-input.sdds"
  trigger_file = tmp_path / "pvalogger-single-sample-trigger.sdds"
  output_file = tmp_path / "pvalogger-single-sample-output.sdds"
  input_file.write_text(
    "SDDS1\n"
    "&column name=ControlName, type=string, &end\n"
    "&column name=Provider, type=string, &end\n"
    "&column name=ExpectNumeric, type=character, &end\n"
    "&column name=ExpectFieldType, type=string, &end\n"
    "&column name=ExpectElements, type=long, &end\n"
    "&data mode=ascii, no_row_counts=1, &end\n"
    f'"{soft_ioc.pv("integer")}" ca y scalar 1\n',
    encoding="ascii",
  )
  trigger_file.write_text(
    "SDDS1\n"
    "&column name=TriggerControlName, type=string, &end\n"
    "&column name=Provider, type=string, &end\n"
    "&column name=TransitionThreshold, type=double, &end\n"
    "&column name=TransitionDirection, type=short, &end\n"
    "&data mode=ascii, no_row_counts=1, &end\n"
    f'"{soft_ioc.pv("double")}" ca 0.5 -1\n',
    encoding="ascii",
  )
  soft_ioc.put("double", "1")

  stdout_file = (tmp_path / "pvalogger-single-sample.stdout").open("w+")
  stderr_file = (tmp_path / "pvalogger-single-sample.stderr").open("w+")
  process = subprocess.Popen(
    _command_arguments(
      [
        str(_executable("sddspvalogger")),
        str(input_file),
        str(output_file),
        "-sampleInterval=0.05,sec",
        f"-triggerFile={trigger_file}",
        "-time=0.6,sec",
        "-overwrite",
        "-pendIOtime=2",
      ]
    ),
    env=soft_ioc.environment,
    text=True,
    stdout=stdout_file,
    stderr=stderr_file,
  )
  try:
    deadline = time.monotonic() + 3
    while not output_file.exists() and time.monotonic() < deadline:
      if process.poll() is not None:
        break
      time.sleep(0.01)
    time.sleep(0.15)
    soft_ioc.put("double", "0")
    process.wait(timeout=5)
  finally:
    if process.poll() is None:
      process.terminate()
      process.wait(timeout=5)
    stdout_file.flush()
    stderr_file.flush()
    stdout_file.seek(0)
    stderr_file.seek(0)
    stdout = stdout_file.read()
    stderr = stderr_file.read()
    stdout_file.close()
    stderr_file.close()

  assert process.returncode == 0, f"stdout:\n{stdout}\nstderr:\n{stderr}"
  steps = soft_ioc.run_sdds("sdds2stream", str(output_file), "-columns=Step")
  assert [int(value) for value in steps.stdout.split()] == [0]


def test_sddswget_wget_and_automatic_input_styles(soft_ioc: SoftIOC, tmp_path: Path) -> None:
  first_output = tmp_path / "wget-first.sdds"
  automatic_output = tmp_path / "wget-automatic.sdds"
  explicit_output = tmp_path / "wget-explicit.sdds"
  soft_ioc.run(
    "sddswget",
    f"-PVnames={soft_ioc.pv('array')}",
    str(first_output),
    "-provider=ca",
  )

  soft_ioc.run(
    "sddswget",
    str(first_output),
    str(automatic_output),
    "-inputStyle=automatic",
    "-provider=ca",
  )
  soft_ioc.run(
    "sddswget",
    str(first_output),
    str(explicit_output),
    "-inputStyle=wget",
    "-provider=ca",
  )

  assert automatic_output.read_bytes().startswith(b"SDDS1")
  assert explicit_output.read_bytes().startswith(b"SDDS1")


@pytest.mark.parametrize(
  ("executable", "arguments", "error_text"),
  [
    ("sddssnapshot", ("-average=0,-1",), "invalid syntax"),
    ("sddscainfo", ("input", "output", "-makeLoggerInput", "-makeSCRInput"), "can't be used"),
    ("sddswget", ("-PVnames=test", "output", "-provider=invalid"), "invalid -provider"),
    ("sddswget", ("-PVnames=test", "output", "-inputStyle=invalid"), "invalid -inputStyle"),
    ("sddswput", ("input", "-pipe=output"), "invalid -pipe syntax"),
  ],
)
def test_invalid_program_options_fail(
  soft_ioc: SoftIOC,
  tmp_path: Path,
  executable: str,
  arguments: tuple[str, ...],
  error_text: str,
) -> None:
  expanded_arguments = tuple(str(tmp_path / argument) if argument in {"input", "output"} else argument for argument in arguments)
  result = soft_ioc.invoke(executable, *expanded_arguments)

  assert result.returncode != 0
  assert error_text.lower() in result.stderr.lower()


def test_sddspcas_hosts_and_updates_a_pv_without_runcontrol(tmp_path: Path) -> None:
  suffix = ".exe" if os.name == "nt" else ""
  sddspcas = _binary_directory() / f"sddspcas{suffix}"
  if not sddspcas.is_file():
    pytest.skip("sddspcas is disabled because the optional PCAS module is unavailable")

  definition_file = tmp_path / "sddspcas-pvs.sdds"
  definition_file.write_text(
    "SDDS1\n"
    "&column name=ControlName, type=string, &end\n"
    "&column name=Type, type=string, &end\n"
    "&data mode=ascii, no_row_counts=1, &end\n"
    "value double\n",
    encoding="ascii",
  )
  prefix = f"pytest:pcas:{os.getpid()}:"
  server_port = _unused_udp_port()
  environment = os.environ.copy()
  environment.update(
    {
      "EPICS_CA_ADDR_LIST": f"127.0.0.1:{server_port}",
      "EPICS_CA_AUTO_ADDR_LIST": "NO",
      "EPICS_CA_SERVER_PORT": str(server_port),
      "EPICS_CAS_INTF_ADDR_LIST": "127.0.0.1",
      "XDG_RUNTIME_DIR": str(tmp_path),
    }
  )
  stdout_file = (tmp_path / "sddspcas.stdout").open("w+")
  stderr_file = (tmp_path / "sddspcas.stderr").open("w+")
  process = subprocess.Popen(
    [
      str(sddspcas),
      str(definition_file),
      "-standalone",
      "-executionTime=3",
      f"-pvPrefix={prefix}",
    ],
    env=environment,
    text=True,
    stdout=stdout_file,
    stderr=stderr_file,
  )
  cavget = _executable("cavget")
  cavput = _executable("cavput")
  pv_name = f"{prefix}value"

  try:
    deadline = time.monotonic() + 2.5
    while time.monotonic() < deadline:
      assert process.poll() is None, "sddspcas exited before publishing its test PV"
      result = _invoke(
        [str(cavget), f"-list={pv_name}", "-pendIoTime=0.2"],
        environment,
        timeout=2,
      )
      if result.stdout.strip() == "0":
        break
      time.sleep(0.1)
    else:
      pytest.fail(f"timed out waiting for sddspcas PV {pv_name}")

    put_result = _invoke(
      [str(cavput), f"-list={pv_name}=6.5", "-pendIoTime=1"],
      environment,
    )
    get_result = _invoke(
      [str(cavget), f"-list={pv_name}", "-pendIoTime=1"],
      environment,
    )
    assert put_result.returncode == 0
    assert float(get_result.stdout.strip()) == pytest.approx(6.5)
  finally:
    if process.poll() is None:
      process.terminate()
    try:
      process.wait(timeout=5)
    except subprocess.TimeoutExpired:
      process.kill()
      process.wait(timeout=5)
    stdout_file.close()
    stderr_file.close()
