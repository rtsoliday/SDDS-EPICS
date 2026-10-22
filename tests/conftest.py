"""Shared fixtures for SDDS EPICS command-line integration tests."""

from __future__ import annotations

import os
import platform
import socket
import subprocess
import tempfile
import time
from dataclasses import dataclass
from pathlib import Path

import pytest


REPOSITORY_ROOT = Path(__file__).resolve().parents[1]


def _binary_directory() -> Path:
  configured_directory = os.environ.get("SDDS_EPICS_BIN_DIR")
  if configured_directory:
    return Path(configured_directory)
  operating_system = "Windows" if platform.system().startswith("CYGWIN") else platform.system()
  return REPOSITORY_ROOT / "bin" / f"{operating_system}-{platform.machine()}"


# Legacy programs compiled only for the tests. ``make test-binaries`` leaves them
# in their component build directories instead of installing them in bin/.
TEST_ONLY_PROGRAMS = {
  "SDDSepics": {
    "getinjectiontrajectory", "makeboosterbump", "sddsSubtractDispersionOrbit",
    "sddstransferFFtoWaveform", "sddstransferscalartowaveform", "sddswgetdata",
    "srinitbpmvector", "srinitcorrvector", "srinitvector",
  },
  "rampload": {"ramploadnew"},
}
# MSVC cannot compile these sources (<pthread.h> or <sys/time.h>).
WINDOWS_UNBUILT_TEST_PROGRAMS = {"getinjectiontrajectory", "makeboosterbump", "ramploadnew"}


def _is_windows() -> bool:
  return platform.system() == "Windows" or platform.system().startswith("CYGWIN")


def _test_binary_directories(component: str) -> list[Path]:
  """Return candidate build directories for test-only programs."""
  configured = os.environ.get("SDDS_EPICS_TEST_BIN_DIRS")
  if configured:
    return [Path(directory) for directory in configured.split(os.pathsep) if directory]
  # Without ``make test``, use the component's normal and DEV_BUILD directories.
  object_directory = f"O.{_binary_directory().name}"
  return [REPOSITORY_ROOT / component / object_directory,
          REPOSITORY_ROOT / component / f"{object_directory}-dev"]


def _test_only_executable(name: str, component: str, suffix: str) -> Path:
  candidates = [directory / f"{name}{suffix}" for directory in _test_binary_directories(component)]
  existing = [candidate for candidate in candidates if candidate.is_file()]
  if existing:
    # Prefer the most recent build when both normal and -dev outputs exist.
    return max(existing, key=lambda candidate: candidate.stat().st_mtime)
  if _is_windows() and name in WINDOWS_UNBUILT_TEST_PROGRAMS:
    pytest.skip(f"{name} is not built with MSVC")
  pytest.fail("required test executable was not built: " + ", ".join(str(path) for path in candidates))


def _executable(name: str) -> Path:
  suffix = ".exe" if platform.system() == "Windows" else ""
  for component, programs in TEST_ONLY_PROGRAMS.items():
    if name in programs:
      return _test_only_executable(name, component, suffix)
  executable = _binary_directory() / f"{name}{suffix}"
  if not executable.is_file():
    pytest.fail(f"required test executable was not built: {executable}")
  return executable


def _sdds_executable(name: str) -> Path:
  configured_directory = os.environ.get("SDDS_BIN_DIR")
  directory = Path(configured_directory) if configured_directory else REPOSITORY_ROOT.parent / "SDDS" / "bin" / _binary_directory().name
  suffix = ".exe" if platform.system() == "Windows" else ""
  executable = directory / f"{name}{suffix}"
  if not executable.is_file():
    pytest.fail(f"required SDDS test executable was not built: {executable}")
  return executable


def _unused_udp_port() -> int:
  with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as udp_socket:
    udp_socket.bind(("127.0.0.1", 0))
    return udp_socket.getsockname()[1]


def _command_arguments(command: list[str]) -> list[str]:
  if os.name != "nt":
    return command
  # scanargs() interprets backslash escapes, so native Windows paths containing
  # components such as ``test_*`` would reach the program with ``\t`` changed
  # into a tab. Windows APIs accept forward slashes for these absolute paths.
  return [argument.replace("\\", "/") for argument in command]


def _invoke(command: list[str], environment: dict[str, str], timeout: float = 10.0) -> subprocess.CompletedProcess[str]:
  # caRepeater may outlive the command and inherit its output descriptors. Regular
  # files let subprocess.run wait for only the requested command, rather than for
  # EOF from every process that inherited a capture pipe.
  with tempfile.TemporaryFile(mode="w+") as stdout_file, tempfile.TemporaryFile(mode="w+") as stderr_file:
    completed = subprocess.run(
      _command_arguments(command),
      env=environment,
      text=True,
      stdout=stdout_file,
      stderr=stderr_file,
      timeout=timeout,
    )
    stdout_file.seek(0)
    stderr_file.seek(0)
    return subprocess.CompletedProcess(
      completed.args,
      completed.returncode,
      stdout_file.read(),
      stderr_file.read(),
    )


def _run(command: list[str], environment: dict[str, str], timeout: float = 10.0) -> subprocess.CompletedProcess[str]:
  result = _invoke(command, environment, timeout)
  if result.returncode != 0:
    pytest.fail(
      f"command failed with exit code {result.returncode}: {' '.join(command)}\n"
      f"stdout:\n{result.stdout}\n"
      f"stderr:\n{result.stderr}"
    )
  return result


@dataclass(frozen=True)
class SoftIOC:
  """Commands and names for the isolated IOC used by a test session."""

  environment: dict[str, str]
  prefix: str
  cavget: Path
  cavput: Path
  temporary_directory: Path

  def pv(self, name: str) -> str:
    return f"{self.prefix}{name}"

  def get(self, name: str) -> str:
    result = _run([str(self.cavget), f"-list={self.pv(name)}", "-pendIoTime=2"], self.environment)
    return result.stdout.strip()

  def put(self, name: str, value: str) -> None:
    _run(
      [str(self.cavput), f"-list={self.pv(name)}={value}", "-pendIoTime=2"],
      self.environment,
    )

  def run(self, executable: str | Path, *arguments: str, timeout: float = 10.0) -> subprocess.CompletedProcess[str]:
    path = _executable(executable) if isinstance(executable, str) else executable
    return _run([str(path), *arguments], self.environment, timeout)

  def invoke(self, executable: str | Path, *arguments: str, timeout: float = 10.0) -> subprocess.CompletedProcess[str]:
    path = _executable(executable) if isinstance(executable, str) else executable
    return _invoke([str(path), *arguments], self.environment, timeout)

  def run_sdds(self, executable: str, *arguments: str, timeout: float = 10.0) -> subprocess.CompletedProcess[str]:
    return _run([str(_sdds_executable(executable)), *arguments], self.environment, timeout)


@pytest.fixture(scope="session")
def soft_ioc(tmp_path_factory: pytest.TempPathFactory) -> SoftIOC:
  """Start sddsSoftIOC with loopback-only CA discovery and a test PV."""

  temporary_directory = tmp_path_factory.mktemp("soft_ioc")
  definition_file = temporary_directory / "pvs.sdds"
  definition_file.write_text(
    "SDDS1\n"
    "&column name=ControlName, type=string, &end\n"
    "&column name=Type, type=string, &end\n"
    "&column name=ElementCount, type=long, &end\n"
    "&column name=ReadbackUnits, type=string, &end\n"
    "&column name=EnumStrings, type=string, &end\n"
    "&data mode=ascii, no_row_counts=1, &end\n"
    "double double 1 volts \"\"\n"
    "integer int 1 counts \"\"\n"
    "text string 1 text \"\"\n"
    "enum enum 1 state \"OFF,ON,AUTO\"\n"
    "charArray char 16 text \"\"\n"
    "array double 4 volts \"\"\n"
    "range1 double 1 volts \"\"\n"
    "range2 double 1 volts \"\"\n"
    "skip double 1 volts \"\"\n",
    encoding="ascii",
  )

  server_port = _unused_udp_port()

  environment = os.environ.copy()
  environment.update(
    {
      "EPICS_CA_ADDR_LIST": f"127.0.0.1:{server_port}",
      "EPICS_CA_AUTO_ADDR_LIST": "NO",
      "EPICS_CA_SERVER_PORT": str(server_port),
      "EPICS_CAS_INTF_ADDR_LIST": "127.0.0.1",
      "XDG_RUNTIME_DIR": str(temporary_directory),
    }
  )

  prefix = f"pytest:{os.getpid()}:"
  cavget = _executable("cavget")
  cavput = _executable("cavput")
  command = [
    str(_executable("sddsSoftIOC")),
    str(definition_file),
    "-standalone",
    "-executionTime=0",
    f"-pvPrefix={prefix}",
  ]
  stdout_file = (temporary_directory / "sddsSoftIOC.stdout").open("w+")
  stderr_file = (temporary_directory / "sddsSoftIOC.stderr").open("w+")
  process = subprocess.Popen(
    _command_arguments(command),
    env=environment,
    text=True,
    stdout=stdout_file,
    stderr=stderr_file,
  )

  def ioc_output() -> tuple[str, str]:
    stdout_file.flush()
    stderr_file.flush()
    stdout_file.seek(0)
    stderr_file.seek(0)
    return stdout_file.read(), stderr_file.read()

  deadline = time.monotonic() + 10.0
  expected_initial_value = "0"
  while time.monotonic() < deadline:
    if process.poll() is not None:
      stdout, stderr = ioc_output()
      pytest.fail(
        f"sddsSoftIOC exited before its PV became available (exit {process.returncode})\n"
        f"stdout:\n{stdout}\nstderr:\n{stderr}"
      )
    result = _invoke(
      [str(cavget), f"-list={prefix}double", "-pendIoTime=0.2"],
      environment,
      timeout=2,
    )
    if result.returncode == 0 and result.stdout.strip() == expected_initial_value:
      break
    time.sleep(0.1)
  else:
    process.terminate()
    process.wait(timeout=5)
    stdout, stderr = ioc_output()
    pytest.fail(
      f"timed out waiting for sddsSoftIOC PV {prefix}double\n"
      f"stdout:\n{stdout}\nstderr:\n{stderr}"
    )

  yield SoftIOC(
    environment=environment,
    prefix=prefix,
    cavget=cavget,
    cavput=cavput,
    temporary_directory=temporary_directory,
  )

  process.terminate()
  try:
    process.wait(timeout=5)
  except subprocess.TimeoutExpired:
    process.kill()
    process.wait(timeout=5)
  stdout_file.close()
  stderr_file.close()
