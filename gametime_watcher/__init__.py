"""Gametime ticket-price watcher.

A small, dependency-free toolkit for listing current Gametime ticket prices for
an event and alerting when a desired number of seats in chosen sections drops
below a target per-ticket price.

Public API:
    extract_event_id(url_or_id)      -> str
    fetch_event_html(event_id, ...)  -> str
    parse_event(html)                -> Event
    parse_listings(html)             -> list[Listing]
    SectionMatcher.parse(spec)       -> SectionMatcher
    filter_listings(...)             -> list[Listing]
"""

from .models import Event, Listing
from .api import extract_event_id, fetch_event_html, parse_event, parse_listings, search_events
from .filters import SectionMatcher, filter_listings

__all__ = [
    "Event",
    "Listing",
    "extract_event_id",
    "fetch_event_html",
    "parse_event",
    "parse_listings",
    "search_events",
    "SectionMatcher",
    "filter_listings",
]
