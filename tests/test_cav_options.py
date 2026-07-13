"""Command-line option coverage for cavget and cavput."""

from __future__ import annotations

import time
from pathlib import Path

import pytest

from conftest import SoftIOC


def _list(soft_ioc: SoftIOC, *entries: str) -> str:
  return "-list=" + ",".join(soft_ioc.pv(entry) for entry in entries)


def test_cavget_formatting_options(soft_ioc: SoftIOC) -> None:
  soft_ioc.put("double", "12.3456")

  formatted = soft_ioc.run(
    soft_ioc.cavget,
    _list(soft_ioc, "double"),
    "-labeled",
    "-floatFormat=%.2f",
    "-embrace=start=[,end=]",
  )

  assert formatted.stdout.strip() == f"[{soft_ioc.pv('double')} 12.35]"


def test_cavget_delimiter_and_multiple_values(soft_ioc: SoftIOC) -> None:
  soft_ioc.put("range1", "1.5")
  soft_ioc.put("range2", "2.5")

  result = soft_ioc.run(
    soft_ioc.cavget,
    _list(soft_ioc, "range1", "range2"),
    "-delimiter=|",
  )

  assert result.stdout.strip() == "1.5|2.5"


def test_cavget_cavput_form(soft_ioc: SoftIOC) -> None:
  soft_ioc.put("double", "7.25")

  result = soft_ioc.run(soft_ioc.cavget, _list(soft_ioc, "double"), "-cavputForm")

  assert result.stdout.strip() == f"{soft_ioc.pv('double')}=7.25"


def test_cavget_string_quoting_options(soft_ioc: SoftIOC) -> None:
  soft_ioc.put("text", "hello world")

  quoted = soft_ioc.run(soft_ioc.cavget, _list(soft_ioc, "text"))
  unquoted = soft_ioc.run(soft_ioc.cavget, _list(soft_ioc, "text"), "-noQuotes")

  assert quoted.stdout.strip() == '"hello world"'
  assert unquoted.stdout.strip() == "hello world"


def test_cavget_enum_text_and_numerical_options(soft_ioc: SoftIOC) -> None:
  soft_ioc.put("enum", "AUTO")

  text = soft_ioc.run(soft_ioc.cavget, _list(soft_ioc, "enum"))
  numerical = soft_ioc.run(soft_ioc.cavget, _list(soft_ioc, "enum"), "-numerical")

  assert text.stdout.strip() == "AUTO"
  assert numerical.stdout.strip() == "2"


def test_cavget_repeat_average_and_sigma_options(soft_ioc: SoftIOC) -> None:
  soft_ioc.put("double", "4.5")

  samples = soft_ioc.run(
    soft_ioc.cavget,
    _list(soft_ioc, "double"),
    "-repeat=number=3,pause=0",
  )
  average = soft_ioc.run(
    soft_ioc.cavget,
    _list(soft_ioc, "double"),
    "-repeat=number=3,pause=0,average,sigma",
  )

  assert [float(value) for value in samples.stdout.split()] == pytest.approx([4.5, 4.5, 4.5])
  assert [float(value) for value in average.stdout.split()] == pytest.approx([4.5, 0.0])


@pytest.mark.parametrize("statistics_format", ["tagvalue", "pretty"])
def test_cavget_statistics_formats(soft_ioc: SoftIOC, statistics_format: str) -> None:
  soft_ioc.put("double", "3.5")

  result = soft_ioc.run(
    soft_ioc.cavget,
    _list(soft_ioc, "double"),
    f"-statistics=number=3,pause=0,format={statistics_format}",
  )

  assert "mean" in result.stdout
  assert "3.5" in result.stdout


def test_cavget_statistics_sdds_and_despike_options(soft_ioc: SoftIOC, tmp_path: Path) -> None:
  output = tmp_path / "statistics.sdds"
  soft_ioc.put("double", "8")

  result = soft_ioc.run(
    soft_ioc.cavget,
    _list(soft_ioc, "double"),
    f"-statistics=number=4,pause=0,format=SDDS,file={output}",
    "-despike=neighbors=2,passes=1,averageOf=2,threshold=1",
  )

  assert result.returncode == 0
  assert output.read_bytes().startswith(b"SDDS1")


def test_cavget_missing_pv_error_options(soft_ioc: SoftIOC) -> None:
  missing = soft_ioc.pv("missing")

  substituted = soft_ioc.run(
    soft_ioc.cavget,
    f"-list={missing}",
    "-pendIoTime=0.1",
    "-errorValue=MISSING",
  )
  excluded = soft_ioc.run(
    soft_ioc.cavget,
    f"-list={missing}",
    "-pendIoTime=0.1",
    "-excludeErrors",
  )
  printed = soft_ioc.run(
    soft_ioc.cavget,
    f"-list={missing}",
    "-pendIoTime=0.1",
    "-printErrors",
  )

  assert substituted.stdout.strip() == "MISSING"
  assert excluded.stdout == ""
  assert missing in printed.stderr


def test_cavget_dry_run_range_and_delist_options(soft_ioc: SoftIOC) -> None:
  result = soft_ioc.run(
    soft_ioc.cavget,
    f"-list={soft_ioc.prefix}range",
    "-range=begin=1,end=2,format=%ld",
    f"-list=,{soft_ioc.pv('skip')}",
    f"-delist=*skip",
    "-dryRun",
  )

  assert result.stdout.split() == [soft_ioc.pv("range1"), soft_ioc.pv("range2")]


def test_cavget_provider_and_info_options(soft_ioc: SoftIOC) -> None:
  result = soft_ioc.run(
    soft_ioc.cavget,
    _list(soft_ioc, "double"),
    "-provider=ca",
    "-info",
  )

  assert "Element Count:" in result.stdout
  assert "Native Data Type:" in result.stdout


def test_cavget_and_cavput_char_array_options(soft_ioc: SoftIOC) -> None:
  soft_ioc.run(
    soft_ioc.cavput,
    f"-list={soft_ioc.pv('charArray')}=hello",
    "-charArray",
  )

  result = soft_ioc.run(
    soft_ioc.cavget,
    _list(soft_ioc, "charArray"),
    "-charArray",
    "-noQuotes",
  )

  assert result.stdout.rstrip("\n\x00") == "hello"


def test_cavput_numerical_and_delta_options(soft_ioc: SoftIOC) -> None:
  soft_ioc.run(soft_ioc.cavput, f"-list={soft_ioc.pv('double')}=10", "-numerical")
  soft_ioc.run(
    soft_ioc.cavput,
    f"-list={soft_ioc.pv('double')}=2",
    "-deltaMode=factor=1.5",
  )

  assert float(soft_ioc.get("double")) == pytest.approx(13.0)


def test_cavput_ramp_option(soft_ioc: SoftIOC) -> None:
  soft_ioc.put("double", "0")

  soft_ioc.run(
    soft_ioc.cavput,
    f"-list={soft_ioc.pv('double')}=9",
    "-ramp=steps=3,pause=0",
  )

  assert float(soft_ioc.get("double")) == pytest.approx(9.0)


def test_cavput_range_delist_and_dry_run_options(soft_ioc: SoftIOC) -> None:
  result = soft_ioc.run(
    soft_ioc.cavput,
    f"-list={soft_ioc.prefix}range",
    "-range=begin=1,end=2,format=%ld",
    "-list==6",
    f"-list=,{soft_ioc.pv('skip')}=99",
    "-delist=*skip",
    "-dryRun",
  )

  assert soft_ioc.pv("range1") in result.stdout
  assert soft_ioc.pv("range2") in result.stdout
  assert soft_ioc.pv("skip") not in result.stdout


def test_cavput_blunder_ahead_option(soft_ioc: SoftIOC) -> None:
  result = soft_ioc.run(
    soft_ioc.cavput,
    f"-list={soft_ioc.pv('integer')}=17,{soft_ioc.pv('missing')}=1",
    "-pendIoTime=0.1",
    "-blunderAhead=silently",
  )

  assert result.returncode == 0
  assert float(soft_ioc.get("integer")) == pytest.approx(17)


def test_cavput_obsolete_options_remain_accepted(soft_ioc: SoftIOC) -> None:
  result = soft_ioc.run(
    soft_ioc.cavput,
    f"-list={soft_ioc.pv('double')}=1",
    "-ezcaTiming",
    "-noGroups",
    "-dryRun",
  )

  assert "obsolete" in result.stderr


def test_cawait_timing_repeat_emit_and_provider_options(soft_ioc: SoftIOC) -> None:
  soft_ioc.put("double", "5")

  result = soft_ioc.run(
    "cawait",
    f"-waitFor={soft_ioc.pv('double')},lowerLimit=4,upperLimit=6,above=4,below=6,monitor",
    "-interval=0.01",
    "-timeLimit=1",
    "-totalTimeLimit=2",
    "-repeat=1",
    "-emit=event=EVENT,timeout=TIMEOUT,end=END",
    "-pendIoTime=2",
    "-noWarnings",
    "-provider=ca",
  )

  assert result.stdout.splitlines() == ["EVENT", "END"]


def test_cawait_logical_operator_options(soft_ioc: SoftIOC) -> None:
  soft_ioc.put("double", "5")
  soft_ioc.put("integer", "4")

  result = soft_ioc.run(
    "cawait",
    f"-waitFor={soft_ioc.pv('double')},equal=5",
    f"-waitFor={soft_ioc.pv('integer')},equal=4",
    "-and",
    f"-waitFor={soft_ioc.pv('double')},equal=6",
    "-not",
    "-and",
    f"-waitFor={soft_ioc.pv('integer')},equal=999",
    "-or",
    "-timeLimit=1",
    "-emit=event=LOGIC_OK",
  )

  assert result.stdout.strip() == "LOGIC_OK"


def test_cawait_command_and_subprocess_options(soft_ioc: SoftIOC, tmp_path: Path) -> None:
  pre_event = tmp_path / "pre-event"
  on_event = tmp_path / "on-event"
  on_end = tmp_path / "on-end"
  subprocess_output = tmp_path / "subprocess-output"
  soft_ioc.put("double", "5")

  soft_ioc.run(
    "cawait",
    f"-waitFor={soft_ioc.pv('double')},equal=5",
    f"-preEvent=touch {pre_event}",
    f"-onEvent=touch {on_event}",
    "-postEvent=true",
    f"-onEnd=touch {on_end}",
    f"-subprocess=cat > {subprocess_output},event=SUB_EVENT,timeout=SUB_TIMEOUT,end=SUB_END",
    "-timeLimit=1",
    "-repeat=1",
  )

  subprocess_lines: list[str] = []
  deadline = time.monotonic() + 2
  while time.monotonic() < deadline:
    if subprocess_output.exists():
      subprocess_lines = subprocess_output.read_text(encoding="utf-8").splitlines()
      if subprocess_lines == ["SUB_EVENT", "SUB_END"]:
        break
    time.sleep(0.01)

  assert pre_event.exists()
  assert on_event.exists()
  assert on_end.exists()
  assert subprocess_lines == ["SUB_EVENT", "SUB_END"]


@pytest.mark.parametrize(
  ("executable", "arguments"),
  [
    ("cavget", ("-list=test", "-provider=invalid")),
    ("cavput", ("-list=test=1", "-provider=invalid")),
    ("cavget", ("-list=test", "-repeat=number=2", "-statistics=number=2,format=tagvalue")),
  ],
)
def test_invalid_option_combinations_fail(
  soft_ioc: SoftIOC,
  executable: str,
  arguments: tuple[str, ...],
) -> None:
  result = soft_ioc.invoke(executable, *arguments)

  assert result.returncode != 0
