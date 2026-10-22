"""First-page monitor configurations still validate enums on every later page."""

from pathlib import Path

import pytest

from conftest import _executable, _invoke
from test_rampload import _isolated_ca_environment
from test_sdds6_page_errors import _write_invalid_metadata_page
from test_sdds6_validation import _write_columns


def _write_invalid_third_page(path: Path, columns, encoding: str, parameters: str = "") -> None:
  _write_columns(path, columns + [("Metadata", "enum", -7)], encoding, parameters=parameters)
  _, valid = path.read_bytes().rsplit(b"&end\n", 1)
  _write_invalid_metadata_page(path, columns, encoding, True, parameters)
  header, pages = path.read_bytes().rsplit(b"&end\n", 1)
  path.write_bytes(header + b"&end\n" + valid + pages)


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("program", ["sddslogger", "sddssynchlog", "sddspvtest", "sddsimagemonitor"])
def test_monitor_inputs_reject_invalid_third_enum_page(tmp_path: Path, encoding: str, program: str) -> None:
  source = tmp_path / "input.sdds"
  if program == "sddspvtest":
    columns = [("ControlName", "string", "unused"), ("MinimumValue", "double", 0),
               ("MaximumValue", "double", 1)]
    arguments = [str(source), "-time=0,seconds"]
  elif program == "sddsimagemonitor":
    columns = [("WaveformPV", "string", "unused")]
    arguments = [str(source), str(tmp_path / "output.sdds"), "-steps=1"]
  else:
    columns = [("ControlName", "string", "unused"), ("ReadbackName", "string", "signal"),
               ("ReadbackUnits", "string", "V")]
    arguments = [str(source), str(tmp_path / "output.sdds")]
    arguments += ["-samples=1", "-time=0.01"] if program == "sddssynchlog" else ["-steps=1"]
  _write_invalid_third_page(source, columns, encoding)

  result = _invoke([str(_executable(program)), *arguments, "-pendIOtime=0.01"],
                   _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0, result.stdout + result.stderr
  assert "enum" in (result.stdout + result.stderr).lower(), result.stdout + result.stderr


_PVA_INPUT = [("ControlName", "string", "unused"), ("Provider", "string", "ca"),
              ("StorageType", "string", "double"), ("ExpectFieldType", "string", "scalar"),
              ("ExpectElements", "long", 1)]


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("role", ["input", "conditions", "triggerFile"])
def test_pva_logger_rejects_invalid_third_enum_page(tmp_path: Path, encoding: str, role: str) -> None:
  source, extra = tmp_path / "input.sdds", tmp_path / "extra.sdds"
  arguments = [str(source), str(tmp_path / "output.sdds"), "-steps=1", "-sampleInterval=0.01"]
  if role == "input":
    _write_invalid_third_page(source, _PVA_INPUT, encoding)
  else:
    _write_columns(source, _PVA_INPUT, encoding)
    columns = [("ControlName" if role == "conditions" else "TriggerControlName", "string", "unused"),
               ("Provider", "string", "ca")]
    columns += ([("LowerLimit", "double", 0), ("UpperLimit", "double", 1)] if role == "conditions"
                else [("GlitchThreshold", "double", 1)])
    _write_invalid_third_page(extra, columns, encoding)
    arguments.append(f"-{role}={extra}" + (",allMustPass" if role == "conditions" else ""))

  result = _invoke([str(_executable("sddspvalogger")), *arguments, "-pendIOtime=0.01"],
                   _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0, result.stdout + result.stderr
  assert "enum" in result.stderr.lower(), result.stdout + result.stderr


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("program", ["sddsglitchlogger", "sddspvaglitchlogger"])
def test_glitch_trigger_file_rejects_invalid_third_enum_page(
  tmp_path: Path, encoding: str, program: str, monkeypatch: pytest.MonkeyPatch,
) -> None:
  source, trigger = tmp_path / "input.sdds", tmp_path / "trigger.sdds"
  _write_columns(source, [("ControlName", "string", "unused")], encoding)
  # These legacy trigger readers require short threshold-control fields; only
  # the double GlitchThreshold is needed to select a trigger.
  _write_invalid_third_page(trigger, [("TriggerControlName", "string", "unused"),
                                    ("GlitchThreshold", "double", 1)], encoding)

  monkeypatch.chdir(tmp_path)
  result = _invoke([str(_executable(program)), str(source), ".",
                    f"-triggerFile={trigger}", "-time=0.01", "-pendIOtime=0.01"],
                   _isolated_ca_environment(), timeout=3)

  assert result.returncode > 0, result.stdout + result.stderr
  assert "enum" in result.stderr.lower(), result.stdout + result.stderr
