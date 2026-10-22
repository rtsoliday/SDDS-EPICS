"""PV registry names must never be interpreted as enum-code pointer arrays."""

from __future__ import annotations

import os
import subprocess
import time
from pathlib import Path

import pytest

from conftest import _executable, _invoke, _unused_udp_port
from test_rampload import _isolated_ca_environment
from test_sdds6_validation import _pv_server_executable, _write_columns


def _server_environment(tmp_path: Path) -> dict[str, str]:
  port = _unused_udp_port()
  environment = _isolated_ca_environment()
  environment.update({
    "EPICS_CA_ADDR_LIST": f"127.0.0.1:{port}",
    "EPICS_CA_SERVER_PORT": str(port),
    "EPICS_CAS_INTF_ADDR_LIST": "127.0.0.1",
    "XDG_RUNTIME_DIR": str(tmp_path),
  })
  return environment


@pytest.mark.parametrize("program", ["sddsSoftIOC", "sddspcas"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("option,field", [
  ("masterPVFile", "rec_name"), ("pcasPVFile", "rec_name"),
  ("pcasPVFile", "host_name"),
])
def test_pv_registry_rejects_enum_names(
  tmp_path: Path, program: str, encoding: str, option: str, field: str,
) -> None:
  executable = _pv_server_executable(program)
  definitions, registry = tmp_path / "pvs.sdds", tmp_path / "registry.sdds"
  _write_columns(definitions, [("ControlName", "string", "test")], "ascii")
  columns = [("rec_name", "string", "unrelated")]
  if option == "pcasPVFile":
    columns.append(("host_name", "string", "unrelated-host"))
  columns = [(name, "enum", -7) if name == field else (name, kind, value)
             for name, kind, value in columns]
  _write_columns(registry, columns, encoding)
  original = registry.read_bytes()

  result = _invoke(
    [str(executable), str(definitions), "-standalone", "-executionTime=0.5",
     f"-{option}={registry}", f"-pvPrefix=pytest:registry:{os.getpid()}:"],
    _server_environment(tmp_path), timeout=8,
  )

  assert result.returncode > 0, result.stderr
  assert f"PV registry column {field} must have string type" in result.stderr
  assert registry.read_bytes() == original


@pytest.mark.parametrize("program", ["sddsSoftIOC", "sddspcas"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_master_pv_registry_accepts_unrelated_enum_metadata(
  tmp_path: Path, program: str, encoding: str,
) -> None:
  executable = _pv_server_executable(program)
  definitions, registry = tmp_path / "pvs.sdds", tmp_path / "registry.sdds"
  _write_columns(definitions, [("ControlName", "string", "test")], "ascii")
  _write_columns(registry, [("rec_name", "string", "unrelated"),
                            ("Metadata", "enum", -7)], encoding)
  original = registry.read_bytes()

  result = _invoke(
    [str(executable), str(definitions), "-standalone", "-executionTime=0.5",
     f"-masterPVFile={registry}", f"-pvPrefix=pytest:registry:{os.getpid()}:"],
    _server_environment(tmp_path), timeout=8,
  )

  assert result.returncode == 0, result.stderr
  assert registry.read_bytes() == original


@pytest.mark.skipif(os.name == "nt", reason="dynamic PV additions use Unix sockets")
@pytest.mark.parametrize("program", ["sddsSoftIOC", "sddspcas"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("option,field", [
  ("masterPVFile", "rec_name"), ("pcasPVFile", "rec_name"),
  ("pcasPVFile", "host_name"), ("masterPVFile", "Metadata"),
  ("pcasPVFile", "Metadata"),
])
def test_running_pv_server_rejects_enum_registry_without_crashing_or_rewriting(
  tmp_path: Path, program: str, encoding: str, option: str, field: str,
) -> None:
  executable = _pv_server_executable(program)
  definitions, additions = tmp_path / "pvs.sdds", tmp_path / "additions.sdds"
  registry, errors = tmp_path / "registry.sdds", tmp_path / "server.stderr"
  _write_columns(definitions, [("ControlName", "string", "initial")], "ascii")
  _write_columns(additions, [("ControlName", "string", "additional")], "ascii")
  columns = [("rec_name", "string", "unrelated")]
  if option == "pcasPVFile":
    columns.append(("host_name", "string", "unrelated-host"))
  # The live server appends to this initial registry. Binary pages support
  # append without the padded row-count line required for ASCII append.
  _write_columns(registry, columns, "binary")
  environment = _server_environment(tmp_path)
  prefix = f"pytest:registry:{os.getpid()}:"
  command = [str(executable), str(definitions), "-standalone", "-executionTime=0",
             f"-{option}={registry}", f"-pvPrefix={prefix}"]

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

      # Simulate a registry replacement while the server is running. Both the
      # next conflict check and exit-time cleanup must validate the new layout.
      columns = [(name, "enum", -7) if name == field else (name, kind, value)
                 for name, kind, value in columns]
      if field == "Metadata":
        columns.append(("Metadata", "enum", 999))
      _write_columns(registry, columns, encoding)
      original = registry.read_bytes()
      forwarded = _invoke([str(executable), str(additions), "-standalone",
                            f"-pvPrefix={prefix}"], environment, timeout=3)
      assert forwarded.returncode == 0, forwarded.stderr
      assert "forwarded" in forwarded.stderr

      deadline = time.monotonic() + 5
      diagnostic = ("Unmapped enum code" if field == "Metadata" else
                    f"PV registry column {field} must have string type")
      while diagnostic not in errors.read_text() and time.monotonic() < deadline:
        assert process.poll() is None, errors.read_text()
        time.sleep(0.05)
      assert diagnostic in errors.read_text()
      assert process.poll() is None, errors.read_text()
      missing = _invoke([str(_executable("cavget")), f"-list={prefix}additional",
                         "-pendIoTime=0.2"], environment, timeout=2)
      assert missing.stdout.strip() == "?"
    finally:
      if process.poll() is None:
        process.terminate()
      try:
        process.wait(timeout=5)
      except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=5)

  # sddspcas's SIGTERM handler explicitly exits with status 1.
  assert process.returncode == (1 if program == "sddspcas" else 0), errors.read_text()
  assert registry.read_bytes() == original
