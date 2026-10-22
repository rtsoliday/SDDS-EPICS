"""SDDS6 append validation for the log daemon's raw ASCII message writer."""

from __future__ import annotations

import os
import subprocess
import time
from pathlib import Path

import pytest

from conftest import _binary_directory, _run, _sdds_executable
from test_logdaemon import _process_ids_for_executable, _stop_processes, _unused_logdaemon_port


@pytest.mark.parametrize("field", ["Time", "message", "Metadata"])
def test_logdaemon_validates_existing_enum_log(tmp_path: Path, field: str) -> None:
  daemon = _binary_directory() / "logDaemon"
  if not daemon.is_file():
    pytest.skip("logDaemon is not built on this platform")
  config = tmp_path / "log.config"
  config.write_text("", encoding="ascii")
  (tmp_path / "log.sourceId").write_text("enumtest~message\n", encoding="ascii")
  directory = tmp_path / "enumtest"
  directory.mkdir()
  log = directory / time.strftime("%Y-%j-%m%d.log")
  header = ('SDDS6\n'
            '&parameter name=sourceId, type=string, fixed_value=enumtest &end\n'
            '&parameter name=Metadata, type=enum, enum_values="-7:retained", '
            'fixed_value=-7 &end\n')
  header += '&array name=History, type=enum, enum_values="-7:retained", dimensions=1 &end\n'
  for name, kind in (("Time", "double"), ("message", "string")):
    definition = 'type=enum, enum_values="-7:original"' if name == field else f'type={kind}'
    header += f'&column name={name}, {definition} &end\n'
  row = f'{"-7" if field == "Time" else "0"} {"-7" if field == "message" else "original"}\n'
  log.write_text(header + '&data mode=ascii, no_row_counts=1 &end\n' + '1\n-7\n' + row,
                 encoding="ascii")
  original = log.read_bytes()

  port = _unused_logdaemon_port()
  service = f"pytest-enum-{os.getpid()}"
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
      diagnostic = f"column {field} has enum type"
      while time.monotonic() < deadline:
        processes |= _process_ids_for_executable(daemon) - before
        if field == "Metadata":
          if processes:
            break
        elif diagnostic in errors.read_text():
          break
        time.sleep(0.05)

      if field == "Metadata":
        assert processes, errors.read_text()
        _run([str(_binary_directory() / "logMessage"), f"-serviceId={service}",
              "-sourceId=enumtest", "-tag=message", "appended"], environment)
        values = _run([str(_sdds_executable("sdds2stream")), str(log),
                       "-columns=message", "-noquotes"], environment)
        assert values.stdout.split() == ["original", "appended"]
        metadata = _run([str(_sdds_executable("sdds2stream")), str(log),
                         "-parameters=Metadata", "-noquotes"], environment)
        assert metadata.stdout.split() == ["retained"]
        history = _run([str(_sdds_executable("sdds2stream")), str(log),
                        "-arrays=History", "-noquotes"], environment)
        assert history.stdout.split() == ["retained"]
      else:
        assert diagnostic in errors.read_text(), errors.read_text()
        assert log.read_bytes() == original
    finally:
      if parent.poll() is None:
        parent.terminate()
        parent.wait(timeout=5)
      processes |= _process_ids_for_executable(daemon) - before
      _stop_processes(processes)
