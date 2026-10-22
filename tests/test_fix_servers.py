"""Regression tests: legacy (enum-free) inputs keep their pre-SDDS6 behavior in
logDaemon, setbcorr, sddspermissive and the SDDS-configured PV servers."""

from __future__ import annotations

import gzip
import os
import subprocess
import time
from pathlib import Path

import pytest

from conftest import _binary_directory, _command_arguments, _executable, _invoke, _run
from test_logdaemon import _process_ids_for_executable, _stop_processes, _unused_logdaemon_port
from test_rampload import _isolated_ca_environment
from test_sdds6_registry import _server_environment
from test_sdds6_setbcorr import _write_ramp
from test_sdds6_validation import _pv_server_executable, _write_columns


LOG_HEADER = ('SDDS1\n'
              '&parameter name=sourceId, type=string, fixed_value=fixtest &end\n'
              '&column name=Time, units=s, type=double &end\n'
              '&column name=severity, type=string &end\n'
              '&column name=message, type=string &end\n'
              '&data mode=ascii, no_row_counts=1 &end\n')


@pytest.mark.parametrize("contents", [
  # A message with fewer values than tags, followed by a complete row.
  LOG_HEADER + '1.000000 "Minor" "first"\n2.000000 "Major"\n3.000000 "Minor" "third"\n',
  # A zero-length file (e.g. left by a failed header write).
  "",
], ids=["short-row", "empty"])
def test_logdaemon_appends_to_existing_legacy_log_without_reading_rows(tmp_path: Path, contents: str) -> None:
  daemon = _binary_directory() / "logDaemon"
  if not daemon.is_file():
    pytest.skip("logDaemon is not built on this platform")
  config = tmp_path / "log.config"
  config.write_text("", encoding="ascii")
  (tmp_path / "log.sourceId").write_text("fixtest~severity~message\n", encoding="ascii")
  directory = tmp_path / "fixtest"
  directory.mkdir()
  log = directory / time.strftime("%Y-%j-%m%d.log")
  log.write_text(contents, encoding="ascii")
  original = log.read_bytes()

  port = _unused_logdaemon_port()
  service = f"pytest-fix-{os.getpid()}"
  environment = os.environ.copy()
  environment.update({"LOG_PORT": str(port), "LOG_SERVER_ID": service})
  before = _process_ids_for_executable(daemon)
  processes: set[int] = set()
  errors = tmp_path / "logDaemon.stderr"
  with (tmp_path / "logDaemon.stdout").open("w+") as stdout, errors.open("w+") as stderr:
    parent = subprocess.Popen(
      [str(daemon), "-m", "SDDS", "-i", service, "-f", str(config), "-p", str(port),
       "-h", str(tmp_path), "-o", str(tmp_path / "saved")],
      env=environment, text=True, stdout=stdout, stderr=stderr,
    )
    try:
      assert parent.wait(timeout=5) == 0
      deadline = time.monotonic() + 5
      while time.monotonic() < deadline and not processes:
        processes |= _process_ids_for_executable(daemon) - before
        time.sleep(0.05)
      assert processes, errors.read_text()
      # The daemon must still be running after opening the existing log.
      time.sleep(0.2)
      assert processes & _process_ids_for_executable(daemon), errors.read_text()

      _run([str(_binary_directory() / "logMessage"), f"-serviceId={service}",
            "-sourceId=fixtest", "-tag=severity", "Minor", "-tag=message", "appended"], environment)
      deadline = time.monotonic() + 5
      while time.monotonic() < deadline and log.read_bytes() == original:
        time.sleep(0.05)
      updated = log.read_bytes()
      assert updated.startswith(original)
      assert updated[len(original):].endswith(b' "Minor" "appended"\n'), updated
    finally:
      if parent.poll() is None:
        parent.terminate()
        parent.wait(timeout=5)
      processes |= _process_ids_for_executable(daemon) - before
      _stop_processes(processes)


def _setbcorr_append(source: Path) -> list[str]:
  return [str(_executable("setbcorr")), "-a", "-o", str(source), "-nr", "-ns", "unused", "1"]


def test_setbcorr_refuses_to_append_to_compressed_ramp(tmp_path: Path) -> None:
  plain = tmp_path / "plain.sdds"
  _write_ramp(plain, "ascii", "")
  source = tmp_path / "ramp.sdds.gz"
  source.write_bytes(gzip.compress(plain.read_bytes().replace(b"SDDS6\n", b"SDDS1\n")))
  original = source.read_bytes()

  result = _invoke(_setbcorr_append(source), _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0
  assert "compressed" in result.stderr, result.stderr
  assert source.read_bytes() == original


def test_setbcorr_still_refuses_ramp_with_trailing_blank_line(tmp_path: Path) -> None:
  # Appending after a blank line would create a page that rampload cannot read.
  source = tmp_path / "ramp.sdds"
  _write_ramp(source, "ascii", "")
  source.write_bytes(source.read_bytes().replace(b"SDDS6\n", b"SDDS1\n") + b"\n")
  original = source.read_bytes()

  result = _invoke(_setbcorr_append(source), _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0
  assert source.read_bytes() == original


@pytest.mark.parametrize("state", ["missing", "empty", "no-final-newline"])
def test_setbcorr_append_accepts_legacy_targets(tmp_path: Path, state: str) -> None:
  source = tmp_path / "ramp.sdds"
  if state == "empty":
    source.write_bytes(b"")
  elif state == "no-final-newline":
    _write_ramp(source, "ascii", "")
    source.write_bytes(source.read_bytes().replace(b"SDDS6\n", b"SDDS1\n").rstrip(b"\n"))

  # A compatible target proceeds to timing PV discovery. Stop it there: these
  # tests use no site IOCs and must never exercise the real Booster PV actions.
  with (tmp_path / "stdout").open("w+") as stdout, (tmp_path / "stderr").open("w+") as stderr:
    process = subprocess.Popen(_command_arguments(_setbcorr_append(source)),
                               env=_isolated_ca_environment(), stdout=stdout, stderr=stderr)
    try:
      try:
        process.wait(timeout=0.5)
      except subprocess.TimeoutExpired:
        pass
    finally:
      if process.poll() is None:
        process.terminate()
      process.wait(timeout=3)
    stderr.seek(0)
    text = stderr.read()
  assert "setbcorr:" not in text, text
  assert "Unable to read" not in text, text
  # The file is opened for append (and created if missing) before timing PVs are read.
  assert source.exists()


def test_permissive_accepts_legacy_input_with_trailing_blank_line(tmp_path: Path) -> None:
  source = tmp_path / "permissive.sdds"
  _write_columns(source, [("ControlName", "string", "unused"), ("Description", "string", "test"),
                          ("MinimumValue", "double", 0), ("MaximumValue", "double", 1)],
                 "ascii", version=1)
  source.write_bytes(source.read_bytes() + b"\n  \n")

  with (tmp_path / "stdout").open("w+") as stdout, (tmp_path / "stderr").open("w+") as stderr:
    process = subprocess.Popen(
      _command_arguments([str(_executable("sddspermissive")), str(source),
                          "-runControlPV=string=unused", "-pendIOtime=0.01"]),
      env=_isolated_ca_environment(), stdout=stdout, stderr=stderr)
    try:
      try:
        process.wait(timeout=1)
      except subprocess.TimeoutExpired:
        pass
    finally:
      if process.poll() is None:
        process.terminate()
      process.wait(timeout=3)
    stdout.seek(0)
    stderr.seek(0)
    text = stdout.read() + stderr.read()
  # Input reading succeeded; any later failure comes from the absent run control PV.
  assert "Unable to read page" not in text, text
  assert "number-of-rows" not in text, text


@pytest.mark.parametrize("program", ["sddsSoftIOC", "sddspcas"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_master_pv_registry_accepts_empty_page_without_rec_name(
  tmp_path: Path, program: str, encoding: str,
) -> None:
  executable = _pv_server_executable(program)
  definitions, registry = tmp_path / "pvs.sdds", tmp_path / "registry.sdds"
  _write_columns(definitions, [("ControlName", "string", "test")], "ascii")
  header = ("SDDS1\n&column name=Other, type=string &end\n"
            f"&data mode={encoding}" + (", endian=little" if encoding == "binary" else "") + " &end\n")
  registry.write_bytes(header.encode("ascii") + (b"0\n" if encoding == "ascii" else b"\0\0\0\0"))
  original = registry.read_bytes()

  result = _invoke(
    [str(executable), str(definitions), "-standalone", "-executionTime=0.5",
     f"-masterPVFile={registry}", f"-pvPrefix=pytest:fixregistry:{os.getpid()}:"],
    _server_environment(tmp_path), timeout=8,
  )

  assert result.returncode == 0, result.stderr
  assert "must have string type" not in result.stderr
  assert registry.read_bytes() == original


@pytest.mark.skipif(os.name == "nt", reason="dynamic PV additions use Unix sockets")
def test_softioc_dynamic_add_skips_unreadable_legacy_master_registry(tmp_path: Path) -> None:
  executable = _pv_server_executable("sddsSoftIOC")
  definitions, additions = tmp_path / "pvs.sdds", tmp_path / "additions.sdds"
  registry, errors = tmp_path / "registry.sdds", tmp_path / "server.stderr"
  _write_columns(definitions, [("ControlName", "string", "initial")], "ascii")
  _write_columns(additions, [("ControlName", "string", "additional")], "ascii")
  _write_columns(registry, [("rec_name", "string", "unrelated")], "binary", version=1)
  environment = _server_environment(tmp_path)
  prefix = f"pytest:fixregistry:{os.getpid()}:"
  command = [str(executable), str(definitions), "-standalone", "-executionTime=0",
             f"-masterPVFile={registry}", f"-pvPrefix={prefix}"]

  with (tmp_path / "server.stdout").open("w") as stdout, errors.open("w") as stderr:
    process = subprocess.Popen(command, env=environment, stdout=stdout, stderr=stderr)
    try:
      deadline = time.monotonic() + 8
      while time.monotonic() < deadline:
        assert process.poll() is None, errors.read_text()
        result = _invoke([str(_executable("cavget")), f"-list={prefix}initial",
                          "-pendIoTime=0.2"], environment, timeout=2)
        if result.returncode == 0 and result.stdout.strip() == "0":
          break
        time.sleep(0.05)
      else:
        pytest.fail(f"PV server did not start: {errors.read_text()}")

      # An enum-free registry whose page cannot be read (one row declared, none
      # present) is skipped with a warning, as for a registry that cannot be opened.
      registry.write_bytes(b"SDDS1\n&column name=rec_name, type=string &end\n"
                           b"&data mode=ascii &end\n1\n")
      forwarded = _invoke([str(executable), str(additions), "-standalone",
                            f"-pvPrefix={prefix}"], environment, timeout=3)
      assert forwarded.returncode == 0, forwarded.stderr
      assert "forwarded" in forwarded.stderr

      deadline = time.monotonic() + 10
      while time.monotonic() < deadline:
        assert process.poll() is None, errors.read_text()
        added = _invoke([str(_executable("cavget")), f"-list={prefix}additional",
                         "-pendIoTime=0.2"], environment, timeout=2)
        if added.returncode == 0 and added.stdout.strip() == "0":
          break
        time.sleep(0.1)
      else:
        pytest.fail(f"dynamic PV was not added: {errors.read_text()}")
      assert "warning: Unable to read SDDS file" in errors.read_text()
    finally:
      if process.poll() is None:
        process.terminate()
      try:
        process.wait(timeout=5)
      except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=5)
