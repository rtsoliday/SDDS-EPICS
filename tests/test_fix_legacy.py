"""Regression tests for the legacy-program and test-build fixes after the SDDS6 audit.

Enum-free (legacy) inputs must behave as they did before SDDS6 support was
added. Test-only legacy programs stay in their component build directories.
"""

from __future__ import annotations

import os
import struct
from pathlib import Path

import pytest

from conftest import (REPOSITORY_ROOT, TEST_ONLY_PROGRAMS, WINDOWS_UNBUILT_TEST_PROGRAMS,
                      _executable, _invoke, _is_windows)
from test_rampload import _isolated_ca_environment
from test_sdds6_validation import MAGNET_COLUMNS, _write_columns


def _magnet_result(tmp_path: Path, mode: str, columns, encoding: str = "binary", version: int = 6):
  source = tmp_path / "magnet.sdds"
  contents = "degaussing specification" if mode == "degauss-legacy" else "tri-degaussing specification"
  _write_columns(source, columns, encoding, contents=contents, version=version)
  program = "degauss" if mode.startswith("degauss") else "standardize"
  return _invoke([str(_executable(program)), str(source)], _isolated_ca_environment(), timeout=3)


def _assert_reaches_control_type(result) -> None:
  # The deliberately invalid ControlType stops before CA searches, after reading all fields.
  assert result.returncode == 1, result.stderr
  assert "illegal ControlType" in result.stderr
  assert "Invalid column type" not in result.stderr


DWELL_COLUMNS = ("RampRate", "RampTransitionPercentage", "DwellTime")


def _replace(columns, field: str, kind: str):
  # _write_columns maps the enum code -7 to a label.
  value = -7 if kind == "enum" else 0
  return [(name, kind, value) if name == field else (name, data_type, data)
          for name, data_type, data in columns]


@pytest.mark.parametrize("field", ["Minutes", "Seconds"])
@pytest.mark.parametrize("kind", ["double", "enum"])
def test_standardize_dwell_mode_ignores_unused_time_columns(tmp_path: Path, field: str, kind: str):
  columns = _replace(MAGNET_COLUMNS["standardize"], field, kind)

  _assert_reaches_control_type(_magnet_result(tmp_path, "standardize", columns))


@pytest.mark.parametrize("field", ["OUTB", "OUTC", "OUTD", "RampRate"])
@pytest.mark.parametrize("kind", ["double", "enum"])
def test_standardize_time_mode_ignores_unused_dwell_columns(tmp_path: Path, field: str, kind: str):
  # Without DwellTime the specification uses Minutes and Seconds.
  columns = _replace([column for column in MAGNET_COLUMNS["standardize"] if column[0] != "DwellTime"],
                     field, kind)

  _assert_reaches_control_type(_magnet_result(tmp_path, "standardize", columns))


@pytest.mark.parametrize("field", ["Minutes", "Seconds"])
@pytest.mark.parametrize("kind", ["double", "enum"])
def test_standardize_time_mode_checks_time_columns(tmp_path: Path, field: str, kind: str):
  columns = _replace([column for column in MAGNET_COLUMNS["standardize"] if column[0] not in DWELL_COLUMNS],
                     field, kind)

  result = _magnet_result(tmp_path, "standardize", columns)

  assert result.returncode > 0
  assert field in result.stderr
  assert "Invalid column type" in result.stderr


@pytest.mark.parametrize("mode", MAGNET_COLUMNS)
@pytest.mark.parametrize("version", [1, 6])
def test_magnet_specifications_ignore_non_string_suffix_names(tmp_path: Path, mode: str, version: int):
  columns = [(name, data_type, value) for name, data_type, value in MAGNET_COLUMNS[mode]
             if name != "SuffixName"] + [("SuffixName", "long", 3)]

  _assert_reaches_control_type(_magnet_result(tmp_path, mode, columns, version=version))


def _transfer_input(path: Path, encoding: str) -> None:
  """Write an enum-free waveform specification whose final page cannot be read."""
  header = (
    'SDDS1\n&parameter name=WaveformPV, type=string, fixed_value="unused" &end\n'
    "&column name=DeviceName, type=string &end\n"
    f"&data mode={encoding}, endian=little &end\n"
  ).encode("ascii")
  if encoding == "ascii":
    # A trailing blank line after a page with row counts.
    data = b"1\nS01A:P1\n\n"
  else:
    # A final page whose row count is present but whose rows are truncated.
    text = b"S01A:P1"
    data = struct.pack("<2i", 1, len(text)) + text + struct.pack("<i", 1)
  path.write_bytes(header + data)


@pytest.mark.parametrize("program", ["sddstransferFFtoWaveform", "sddstransferscalartowaveform"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_transfer_programs_accept_legacy_unreadable_final_page(tmp_path: Path, program: str, encoding: str):
  source = tmp_path / "input.sdds"
  _transfer_input(source, encoding)

  result = _invoke([str(_executable(program)), str(source), "-steps=1", "-pendIOtime=0.01"],
                   _isolated_ca_environment(), timeout=5)

  # The input is accepted as before; the isolated CA environment then has no
  # server for the listed channels.
  assert "Unable to read page" not in result.stderr
  assert "problem doing search" in result.stderr


def test_watch_input_reload_keeps_running_configuration() -> None:
  source = (REPOSITORY_ROOT / "SDDSepics" / "sddstransferscalartowaveform.c").read_text(encoding="utf-8")
  loop = source.split("if (file_is_modified(loopParam.inputfile", 1)[1].split("/*read scalar pvs at each step */", 1)[0]

  # A reload reads into new buffers and keeps the current ones when rejected.
  assert "&newWaveforms, &newWaveformData" in loop
  assert "!= 2" in loop
  reader = source.split("long readinputfile(char *inputfile", 2)[2]
  assert "laterPageReadFailed(&SDDSin, pageCode)" in reader
  assert "if (!reload)" in reader


def test_test_binaries_do_not_override_production_lists() -> None:
  top = (REPOSITORY_ROOT / "Makefile").read_text(encoding="utf-8")
  recipe = top.split("test-binaries: all", 1)[1].split("\ntest:", 1)[0]

  assert "PROD=" not in recipe
  assert "$(MAKE) -C rampload test-programs" in recipe
  # DEV_BUILD=1 uses the -dev object directory selected by Makefile.rules.
  assert "O.$(OS)-$(ARCH)" not in top
  assert "$(OBJ_DIR) install-programs" in recipe
  sdds_epics = (REPOSITORY_ROOT / "SDDSepics" / "Makefile").read_text(encoding="utf-8")
  test_programs = sdds_epics.split("test-programs:", 1)[1]
  assert "LEGACY_TEST_TARGETS" not in test_programs


@pytest.mark.parametrize("program", sorted(set().union(*TEST_ONLY_PROGRAMS.values())))
def test_test_only_programs_run_from_build_directories(program: str) -> None:
  if _is_windows() and program in WINDOWS_UNBUILT_TEST_PROGRAMS:
    pytest.skip(f"{program} is not built with MSVC")
  executable = _executable(program)

  assert executable.parent.name.startswith("O."), executable
  assert os.access(executable, os.X_OK)
