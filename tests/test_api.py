"""Tests for parsing event pages into Event/Listing objects."""

import os

import pytest

from gametime_watcher.api import GametimeError, extract_event_id, parse_event, parse_listings

FIXTURE = os.path.join(os.path.dirname(__file__), "fixtures", "event_page.html")
EVENT_ID = "68af5b72c95bdeed8553f07f"


@pytest.fixture(scope="module")
def html():
    with open(FIXTURE, encoding="utf-8") as fh:
        return fh.read()


# --- extract_event_id (offline, no network) ---------------------------------

def test_extract_bare_id():
    assert extract_event_id(EVENT_ID) == EVENT_ID


def test_extract_from_event_url():
    url = f"https://gametime.co/events/{EVENT_ID}"
    assert extract_event_id(url) == EVENT_ID


def test_extract_from_listing_url():
    url = (
        "https://gametime.co/mlb-baseball/pirates-at-as-tickets/"
        f"6-17-2026-west-sacramento-ca-sutter-health-park/events/{EVENT_ID}/listings/abc"
    )
    assert extract_event_id(url) == EVENT_ID


def test_extract_rejects_unresolvable_non_url():
    with pytest.raises(GametimeError):
        extract_event_id("not-an-id")


# --- parse_event ------------------------------------------------------------

def test_parse_event_name_and_datetime(html):
    event = parse_event(html)
    assert event is not None
    assert event.name == "Test Team A at Test Team B"
    assert event.datetime_local == "2026-06-17T18:40:00"
    assert event.venue_id == "55313d1878fea568e6000001"


def test_parse_event_returns_none_when_absent():
    assert parse_event("<html><body>nothing here</body></html>") is None


# --- parse_listings ---------------------------------------------------------

def test_parse_listings_count_and_dedup(html):
    listings = parse_listings(html)
    # Fixture has 6 listing objects but one is a duplicate id -> 5 unique.
    assert len(listings) == 5
    assert len({l.id for l in listings}) == 5


def test_parse_listing_fields(html):
    by_id = {l.id: l for l in parse_listings(html)}
    cheap = by_id["aaaaaaaaaaaaaaaaaaaaaaa1"]
    assert cheap.section == "117"
    assert cheap.section_group == "Field Level"
    assert cheap.row == "12"
    assert cheap.seats == ["9", "10"]
    assert cheap.available_lots == [2]
    assert cheap.price_total == 3800
    assert cheap.price_total_dollars == 38.0
    assert cheap.face_value == 12500
    assert cheap.event_id == EVENT_ID


def test_parse_listing_unescapes_seo_url(html):
    by_id = {l.id: l for l in parse_listings(html)}
    url = by_id["aaaaaaaaaaaaaaaaaaaaaaa1"].url
    assert url.startswith("https://gametime.co/events/")
    assert "\\u002F" not in url and "/listings/" in url


def test_parse_listing_handles_non_numeric_section(html):
    by_id = {l.id: l for l in parse_listings(html)}
    lawn = by_id["aaaaaaaaaaaaaaaaaaaaaaa5"]
    assert lawn.section == "Lawn"
    assert lawn.section_is_numeric is False
    assert lawn.section_number is None


def test_parse_empty_html_returns_no_listings():
    assert parse_listings("<html></html>") == []
