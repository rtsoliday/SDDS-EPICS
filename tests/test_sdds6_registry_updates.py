"""Writable PV registries reject categorical fields before changing files."""

from __future__ import annotations

import os
import struct
from pathlib import Path

import pytest

from conftest import _invoke
from test_sdds6_registry import _server_environment
from test_sdds6_validation import _pv_server_executable, _write_columns


@pytest.mark.parametrize("program", ["sddsSoftIOC", "sddspcas"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("kind", ["column", "parameter", "parameter-fixed", "array"])
def test_writable_pv_registry_rejects_enum_fields_without_rewriting(
  tmp_path: Path, program: str, encoding: str, kind: str,
) -> None:
  executable = _pv_server_executable(program)
  definitions, registry = tmp_path / "pvs.sdds", tmp_path / "registry.sdds"
  _write_columns(definitions, [("ControlName", "string", "test")], "ascii")
  columns = [("rec_name", "string", "unrelated"),
             ("host_name", "string", "unrelated-host")]
  if kind == "column":
    columns.append(("Metadata", "enum", -7))
  _write_columns(registry, columns, encoding)
  if kind != "column":
    header, page = registry.read_bytes().rsplit(b"&end\n", 1)
    field = kind.split("-")[0]
    definition = f'&{field} name=Metadata, type=enum, enum_values="-7:retained"'
    if kind == "parameter-fixed":
      definition += ", fixed_value=-7"
    elif kind == "array":
      definition += ", dimensions=1"
    definition += " &end\n"
    header = header.replace(b"&data", definition.encode("ascii") + b"&data")
    if kind != "parameter-fixed":
      values = [-7] if kind == "parameter" else [1, -7]
      if encoding == "ascii":
        page = ("\n".join(map(str, values)) + "\n").encode("ascii") + page
      else:
        page = page[:4] + struct.pack(f"<{len(values)}i", *values) + page[4:]
    registry.write_bytes(header + b"&end\n" + page)
  original = registry.read_bytes()

  result = _invoke(
    [str(executable), str(definitions), "-standalone", "-executionTime=0.1",
     f"-pcasPVFile={registry}", f"-pvPrefix=pytest:registry-update:{os.getpid()}:"],
    _server_environment(tmp_path), timeout=8,
  )

  assert result.returncode > 0, result.stderr
  assert f"PV registry {kind.split('-')[0]} Metadata cannot have enum type" in result.stderr
  assert registry.read_bytes() == original
