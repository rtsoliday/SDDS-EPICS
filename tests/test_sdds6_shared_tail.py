"""Validate ignored enum pages in the shared acquisition input readers."""

from pathlib import Path

import pytest

from conftest import SoftIOC
from test_sdds6_validation import _write_columns


def _write_pages(path, columns, encoding, valid_pages, invalid_tail):
  _write_columns(path, columns + [("Metadata", "enum", -7)], encoding)
  header, page = path.read_bytes().rsplit(b"&end\n", 1)
  bad_page = page.replace(b"-7", b"999") if encoding == "ascii" else page[:-4] + b"\xe7\x03\x00\x00"
  path.write_bytes(header + b"&end\n" + page * valid_pages + (bad_page if invalid_tail else b""))


@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("kind", ["scalar", "conditions", "waveform", "modified-scalar"])
@pytest.mark.parametrize("invalid_tail", [False, True], ids=["valid", "bad-fourth-page"])
def test_shared_readers_validate_entire_enum_tail(
  soft_ioc: SoftIOC, tmp_path: Path, encoding: str, kind: str, invalid_tail: bool,
):
  source, output = tmp_path / "input.sdds", tmp_path / "output.sdds"
  controls = [("ControlName", "string", soft_ioc.pv("double")),
              ("ReadbackName", "string", "signal"), ("ReadbackUnits", "string", "V")]
  program = "sddsmonitor"
  if kind == "conditions":
    main = tmp_path / "controls.sdds"
    _write_columns(main, controls, encoding)
    columns = [("ControlName", "string", soft_ioc.pv("double")),
               ("LowerLimit", "double", -1), ("UpperLimit", "double", 1)]
    args = [str(main), str(output), f"-conditions={source},allMustPass"]
    soft_ioc.put("double", "0")
  elif kind == "waveform":
    program = "sddswmonitor"
    columns = [("WaveformPV", "string", soft_ioc.pv("array")),
               ("WaveformName", "string", "signal")]
    args = [str(source), str(output)]
  elif kind == "modified-scalar":
    program = "sddswmonitor"
    columns = controls
    args = [str(output), f"-PVnames={soft_ioc.pv('array')}", f"-scalars={source}"]
  else:
    columns = controls
    args = [str(source), str(output)]
  _write_pages(source, columns, encoding, 3, invalid_tail)

  result = soft_ioc.invoke(program, *args, "-steps=1", "-pendIOtime=2")

  if invalid_tail:
    assert result.returncode > 0, result.stdout + result.stderr
    assert "enum" in result.stderr.lower(), result.stderr
    assert not output.exists()
  else:
    assert result.returncode == 0, result.stdout + result.stderr
    assert output.exists()
