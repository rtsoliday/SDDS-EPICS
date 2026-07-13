"""Integration tests for cavget and cavput over Channel Access."""

from __future__ import annotations

import pytest

from conftest import SoftIOC


def test_cavget_reads_initial_value(soft_ioc: SoftIOC) -> None:
  soft_ioc.put("double", "0")

  assert float(soft_ioc.get("double")) == pytest.approx(0.0)


def test_cavput_updates_value(soft_ioc: SoftIOC) -> None:
  soft_ioc.put("double", "12.5")

  assert float(soft_ioc.get("double")) == pytest.approx(12.5)
