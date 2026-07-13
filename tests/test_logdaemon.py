"""End-to-end tests for logDaemon, its client library, and CLI client."""

from __future__ import annotations

import os
import signal
import socket
import subprocess
import time
from pathlib import Path

import pytest

from conftest import _binary_directory, _run


def _process_ids_for_executable(executable: Path) -> set[int]:
  """Return process IDs whose executable is the requested binary."""

  proc = Path("/proc")
  expected = executable.resolve()
  process_ids = set()
  if proc.is_dir():
    for entry in proc.iterdir():
      if not entry.name.isdigit():
        continue
      try:
        if (entry / "exe").resolve() == expected:
          process_ids.add(int(entry.name))
      except (FileNotFoundError, PermissionError):
        continue
  else:
    result = subprocess.run(
      ["ps", "-axo", "pid=,command="],
      text=True,
      capture_output=True,
      check=True,
    )
    for line in result.stdout.splitlines():
      fields = line.strip().split(maxsplit=1)
      if len(fields) != 2 or not fields[0].isdigit():
        continue
      command = fields[1].split(maxsplit=1)[0]
      try:
        if Path(command).resolve() == expected:
          process_ids.add(int(fields[0]))
      except (OSError, RuntimeError):
        continue
  return process_ids


def _process_exists(process_id: int) -> bool:
  try:
    os.kill(process_id, 0)
  except ProcessLookupError:
    return False
  except PermissionError:
    return True
  return True


def _stop_processes(process_ids: set[int]) -> None:
  for process_id in process_ids:
    try:
      os.kill(process_id, signal.SIGTERM)
    except ProcessLookupError:
      pass

  deadline = time.monotonic() + 3
  while time.monotonic() < deadline:
    remaining = {process_id for process_id in process_ids if _process_exists(process_id)}
    if not remaining:
      return
    time.sleep(0.05)

  for process_id in process_ids:
    try:
      os.kill(process_id, signal.SIGKILL)
    except ProcessLookupError:
      pass


def _unused_logdaemon_port() -> int:
  """Find a free high UDP port, including values above signed-short range."""

  first_port = 32768 + os.getpid() % (65536 - 32768)
  ports = list(range(first_port, 65536)) + list(range(32768, first_port))
  for port in ports:
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as udp_socket:
      try:
        udp_socket.bind(("127.0.0.1", port))
      except OSError:
        continue
      return port
  raise AssertionError("unable to find an unused logDaemon UDP port")


@pytest.mark.parametrize("mode", ["text", "SDDS"])
def test_logdaemon_client_library_and_logmessage(tmp_path: Path, mode: str) -> None:
  """Run the bundled library self-test and submit a real CLI log message."""

  log_daemon = _binary_directory() / "logDaemon"
  test_logger = _binary_directory() / "testLogger"
  log_message = _binary_directory() / "logMessage"
  if not log_daemon.is_file():
    pytest.skip("logDaemon is not built on this platform")
  for executable in (log_daemon, test_logger, log_message):
    assert executable.is_file(), f"required test executable was not built: {executable}"

  port = _unused_logdaemon_port()
  service_id = f"pytest-{mode}-{os.getpid()}"
  config_file = tmp_path / "log.config"
  saved_directory = tmp_path / "saved"
  config_file.write_text("", encoding="ascii")
  environment = os.environ.copy()
  environment.update(
    {
      "LOG_PORT": str(port),
      "LOG_SERVER_ID": service_id,
    }
  )

  before = _process_ids_for_executable(log_daemon)
  stdout_file = (tmp_path / "logDaemon.stdout").open("w+")
  stderr_file = (tmp_path / "logDaemon.stderr").open("w+")
  parent = subprocess.Popen(
    [
      str(log_daemon),
      "-m",
      mode,
      "-i",
      service_id,
      "-f",
      str(config_file),
      "-p",
      str(port),
      "-h",
      str(tmp_path),
      "-o",
      str(saved_directory),
    ],
    env=environment,
    text=True,
    stdout=stdout_file,
    stderr=stderr_file,
  )
  daemon_processes: set[int] = set()

  try:
    assert parent.wait(timeout=5) == 0
    deadline = time.monotonic() + 5
    while time.monotonic() < deadline:
      daemon_processes = _process_ids_for_executable(log_daemon) - before
      if daemon_processes:
        break
      time.sleep(0.05)
    assert len(daemon_processes) == 1, "logDaemon did not leave exactly one server process"

    self_test = _run(
      [str(test_logger), f"-serviceId={service_id}"],
      environment,
      timeout=15,
    )
    assert self_test.stdout.count("passed") == 9

    message = _run(
      [
        str(log_message),
        f"-serviceId={service_id}",
        "-sourceId=cli",
        "-tag=message",
        "hello world",
      ],
      environment,
      timeout=10,
    )
    assert message.returncode == 0

    deadline = time.monotonic() + 3
    log_files: list[Path] = []
    while time.monotonic() < deadline:
      log_files = list((tmp_path / "cli").glob("*.log"))
      if log_files and "hello world" in log_files[0].read_text(encoding="utf-8"):
        break
      time.sleep(0.05)
    assert log_files
    assert "hello world" in log_files[0].read_text(encoding="utf-8")
  finally:
    if parent.poll() is None:
      parent.terminate()
      parent.wait(timeout=5)
    _stop_processes(daemon_processes)
    stdout_file.close()
    stderr_file.close()
