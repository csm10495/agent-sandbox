# AGENTS.md

Guidance for AI agents and contributors working in this repository.

## Project

`gametime_watcher` — a dependency-free (standard-library only) Python tool that
lists current gametime.co ticket prices for an event and alerts when N seats in
chosen sections drop below a per-ticket price. See `README.md` for usage.

## Layout

- `gametime_watcher/models.py` — `Listing` and `Event` dataclasses. Prices are
  in **cents**; `Listing.price_total` is the all-in price **per ticket**.
- `gametime_watcher/api.py` — resolve event id, fetch the event page, parse
  embedded listing/event JSON, and `search_events` (team/performer search via
  Gametime's mobile API).
- `gametime_watcher/filters.py` — `SectionMatcher` (ranges / exact / group
  names) and `filter_listings`.
- `gametime_watcher/cli.py` — argparse CLI with `search` subcommand, one-shot +
  `--watch` polling, and webhook/command notifications.
- `tests/` — pytest suite; offline fixture at `tests/fixtures/event_page.html`.

## How data is obtained

Gametime server-renders every listing into `https://gametime.co/events/<id>`.
The parser splits the HTML on the stable `{"availableLots":` anchor and reads
each listing's fields (`price.total`, `spot.section`, `spot.row`, `seats`,
`availableLots`, `seoUrl`). This is intentionally resilient to key reordering.
If Gametime changes its page structure, update `parse_listings` / `parse_event`
**and** regenerate `tests/fixtures/event_page.html` to match the new format.

## Conventions

- No third-party runtime dependencies; use the standard library only.
- All code must have useful, functional unit tests that assert real behavior and
  edge cases (not coverage filler). Run `python -m pytest` and add tests as you
  go.
- Keep network access out of unit tests (stub `fetch_event_html` /
  `extract_event_id`, parse the fixture).
- When you change behavior, update `README.md` and this file as needed.

## Commands

```bash
pip install pytest
python -m pytest                 # run the test suite
python -m gametime_watcher --help
```
