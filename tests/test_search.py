"""Tests for the search subcommand and search_events API."""

import json

import pytest

from gametime_watcher import cli
from gametime_watcher.api import search_events
from gametime_watcher.models import Event


FAKE_SEARCH_RESULTS = [
    Event(
        id="aaa111aaa111aaa111aaa111",
        name="Pirates at Athletics",
        datetime_local="2026-06-17T18:40:00",
        venue_id="55313d1878fea568e6000001",
        extra={"url": "https://gametime.co/events/aaa111aaa111aaa111aaa111", "min_price_total": 2000},
    ),
    Event(
        id="bbb222bbb222bbb222bbb222",
        name="Athletics at Giants",
        datetime_local="2026-06-20T19:05:00",
        venue_id="663344556677889900aabb01",
        extra={"url": "https://gametime.co/events/bbb222bbb222bbb222bbb222", "min_price_total": 3500},
    ),
]


@pytest.fixture(autouse=True)
def stub_search(monkeypatch):
    monkeypatch.setattr(cli, "search_events", lambda *a, **k: FAKE_SEARCH_RESULTS)


def test_search_text_output(capsys):
    rc = cli.main(["search", "Athletics"])
    out = capsys.readouterr().out
    assert rc == 0
    assert "Found 2 upcoming event(s)" in out
    assert "Pirates at Athletics" in out
    assert "Athletics at Giants" in out
    assert "https://gametime.co/events/aaa111aaa111aaa111aaa111" in out
    assert "from $20" in out


def test_search_json_output(capsys):
    rc = cli.main(["search", "Athletics", "--json"])
    data = json.loads(capsys.readouterr().out)
    assert rc == 0
    assert data["query"] == "Athletics"
    assert data["event_count"] == 2
    assert data["events"][0]["id"] == "aaa111aaa111aaa111aaa111"
    assert data["events"][0]["url"] == "https://gametime.co/events/aaa111aaa111aaa111aaa111"
    assert data["events"][0]["min_price_total_cents"] == 2000


def test_search_exit_1_when_no_results(monkeypatch, capsys):
    monkeypatch.setattr(cli, "search_events", lambda *a, **k: [])
    rc = cli.main(["search", "Nonexistent Team"])
    assert rc == 1
    assert "Found 0 upcoming event(s)" in capsys.readouterr().out


def test_search_sorted_by_datetime(capsys):
    """Results are sorted chronologically."""
    rc = cli.main(["search", "Athletics"])
    out = capsys.readouterr().out
    # Pirates game is on 6/17, Giants game on 6/20
    assert out.index("Pirates") < out.index("Giants")
