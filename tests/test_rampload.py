"""Safe coverage for the legacy Booster ramp command-line programs."""

from __future__ import annotations

import os
import subprocess
import tempfile
import time
from pathlib import Path

import pytest

from conftest import _executable, _invoke


def _isolated_ca_environment() -> dict[str, str]:
  environment = os.environ.copy()
  environment.update(
    {
      "EPICS_CA_ADDR_LIST": "127.0.0.1:1",
      "EPICS_CA_AUTO_ADDR_LIST": "NO",
      "XDG_RUNTIME_DIR": "/tmp",
    }
  )
  return environment


@pytest.mark.parametrize("arguments", [(), ("first.sdds", "second.sdds")])
def test_rampload_requires_exactly_one_specification(arguments: tuple[str, ...]) -> None:
  result = _invoke(
    [str(_executable("rampload")), *arguments],
    _isolated_ca_environment(),
  )

  assert result.returncode == 1
  assert "usage:" in result.stderr


def test_rampload_reports_an_invalid_sdds_input(tmp_path: Path) -> None:
  invalid_input = tmp_path / "invalid.sdds"
  invalid_input.write_text("not an SDDS file\n", encoding="ascii")

  result = _invoke(
    [str(_executable("rampload")), str(invalid_input)],
    _isolated_ca_environment(),
  )

  assert result.returncode == 1
  assert "unable to initialize" in result.stderr


@pytest.mark.parametrize("option", ["-h", "-?"])
def test_setbcorr_help_options(option: str) -> None:
  result = _invoke(
    [str(_executable("setbcorr")), option],
    _isolated_ca_environment(),
  )

  assert result.returncode == 0
  assert "Usage is:" in result.stdout


def test_setbcorr_rejects_an_unknown_option() -> None:
  result = _invoke(
    [str(_executable("setbcorr")), "-invalid"],
    _isolated_ca_environment(),
  )

  assert result.returncode == 1
  assert "Invalid option" in result.stderr


def test_setbcorr_rejects_an_oversized_output_filename() -> None:
  result = _invoke(
    [str(_executable("setbcorr")), "-o", "x" * 9000, "B1C0", "1"],
    _isolated_ca_environment(),
  )

  assert result.returncode == 1
  assert "Output filename is too long" in result.stderr


@pytest.mark.parametrize(
  "arguments",
  [
    ("-a",),
    ("-nr",),
    ("-ns",),
    ("-o",),
    ("-o", "/tmp/setbcorr-parser-output.sdds"),
    ("-q",),
  ],
)
def test_setbcorr_safe_validation_paths(arguments: tuple[str, ...]) -> None:
  result = _invoke(
    [str(_executable("setbcorr")), *arguments],
    _isolated_ca_environment(),
  )

  assert result.returncode == 1
  assert "Invalid option" not in result.stderr
  assert "Usage is:" in result.stdout


@pytest.mark.parametrize("option", ["-b", "-fg"])
def test_setbcorr_site_action_options_are_recognized(option: str) -> None:
  """The site-PV actions must enter CA processing instead of rejecting the option."""

  with tempfile.TemporaryFile(mode="w+") as stdout_file, tempfile.TemporaryFile(mode="w+") as stderr_file:
    process = subprocess.Popen(
      [str(_executable("setbcorr")), option],
      env=_isolated_ca_environment(),
      text=True,
      stdout=stdout_file,
      stderr=stderr_file,
    )
    try:
      time.sleep(0.2)
      assert process.poll() is None, f"{option} exited before attempting its site-PV action"
    finally:
      if process.poll() is None:
        process.terminate()
      try:
        process.wait(timeout=3)
      except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=3)

    stderr_file.seek(0)
    assert "Invalid option" not in stderr_file.read()
