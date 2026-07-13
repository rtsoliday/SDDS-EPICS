"""Static guards for memory and indexing defects that require facility data to trigger."""

from __future__ import annotations

from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[1]


def _source(relative_path: str) -> str:
  return (REPOSITORY_ROOT / relative_path).read_text(encoding="utf-8")


def test_vector_page_growth_uses_the_structure_size() -> None:
  for relative_path in (
    "SDDSepics/srinitbpmvector.c",
    "SDDSepics/srinitvector.c",
  ):
    source = _source(relative_path)
    assert "sizeof(*vector) * (vectors + 1)" in source
    assert "sizeof(vector) * (vectors + 1)" not in source


def test_multi_page_assignments_index_new_entries() -> None:
  source = _source("SDDSepics/SDDSepics.c")

  assert "assignment[i + *assignments].PVname = PV[i];" in source
  assert "assignment[i + *assignments].PVchid = NULL;" in source
  assert "assignment[i].PVname = PV[i];" not in source
  assert "assignment[i].PVchid = NULL;" not in source


def test_character_waveform_is_terminated_without_scanning_past_input() -> None:
  source = _source("SDDSepics/sddscasr.c")
  function = source.rsplit("char *read_char_waveform", 1)[1].split(
    "void setup_other_caconnections", 1
  )[0]

  assert "(*stringval)[length] = '\\0';" in function
  assert "strlen(*stringval)" not in function


def test_control_bit_sample_modes_free_the_sample_value() -> None:
  source = _source("SDDSepics/sddsgencontrolbits.c")
  block = source.split('sprintf(tmpBuffer, "SampleMode%ld", i);', 1)[1].split(
    "if (SDDS_CheckParameter(&ramData, \"BunchPattern\"", 1
  )[0]

  assert "free(sample);" in block
  assert "free(plane);" not in block


def test_control_law_waveform_cleanup_frees_sleep_rows_once() -> None:
  for relative_path in (
    "SDDSepics/sddscontrollaw.c",
    "SDDSepics/sddspvacontrollaw.cc",
  ):
    source = _source(relative_path)
    cleanup = source.split("void cleanupTestWaveforms", 1)[-1]
    row_cleanup = cleanup.split("if (waveform_tests->ignore) {", 1)[0]
    assert row_cleanup.count("free(waveform_tests->sleep[i]);") == 1


def test_srinitvector_honors_setsource_independently_of_corrtype() -> None:
  source = _source("SDDSepics/srinitvector.c")
  parser = source.split("case CLO_CORR_TYPE:", 1)[1].split(
    "case CLO_UNIFIED:", 1
  )[0]
  initializer = source.rsplit("void init_corr_vector", 1)[1]

  assert "sourceModeInput = sourceMode[1];\n        break;" in parser
  assert "case CLO_SETSOURCE:" in parser
  assert "if (setSourceMode)\n    set_source_mode" in initializer


def test_booster_bump_lookahead_is_bounded() -> None:
  source = _source("SDDSepics/sddsboosterbump.c")

  assert "if (abs(corrector_ramp[i].time" not in source
  assert source.count("j + 1 < corrector_ramp[i].nord") >= 5
  assert "dummyFlags = pipeFlags = 0;" in source
