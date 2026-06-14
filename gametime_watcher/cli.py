"""Command-line interface for the Gametime ticket watcher."""

from __future__ import annotations

import argparse
import json
import shlex
import subprocess
import sys
import time
import urllib.request
from typing import List, Optional, Union

from .api import GametimeError, extract_event_id, fetch_event_html, parse_event, parse_listings, search_events
from .filters import SectionMatcher, filter_listings
from .models import Event, Listing


def _format_listing(l: Listing) -> str:
    seats = ",".join(l.seats) if l.seats else "?"
    group = f" ({l.section_group})" if l.section_group else ""
    lots = "/".join(str(x) for x in l.available_lots) or "?"
    face = f", face ${l.face_value_dollars:.2f}" if l.face_value is not None else ""
    return (
        f"${l.price_total_dollars:>7.2f}/tkt  "
        f"sec {l.section:>5}{group}  row {l.row or '?':>3}  "
        f"seats[{seats}]  lots:{lots}{face}"
    )


def _scan(url_or_id: str, args) -> "tuple[Optional[Event], List[Listing], List[Listing]]":
    event_id = extract_event_id(url_or_id, user_agent=args.user_agent, timeout=args.timeout)
    html = fetch_event_html(event_id, user_agent=args.user_agent, timeout=args.timeout)
    event = parse_event(html)
    if event is not None and not event.id:
        event.id = event_id
    all_listings = parse_listings(html)
    matches = filter_listings(
        all_listings,
        sections=_sections_spec(args),
        max_price_dollars=args.max_price,
        quantity=args.quantity,
        allow_larger=args.allow_larger,
    )
    return event, all_listings, matches


def _emit_json(event: Optional[Event], matches: List[Listing]) -> str:
    return json.dumps(
        {
            "event": event.to_dict() if event else None,
            "match_count": len(matches),
            "matches": [m.to_dict() for m in matches],
        },
        indent=2,
    )


def _sections_spec(args) -> Optional[str]:
    """Merge repeated --sections values into one comma-separated spec (or None)."""
    if not args.sections:
        return None
    return ",".join(args.sections)


def _criteria_text(args) -> str:
    sec = _sections_spec(args) or "any section"
    price = f"<= ${args.max_price:.2f}/ticket" if args.max_price is not None else "any price"
    qty = f"{args.quantity} seat(s)" if args.quantity else "any quantity"
    extra = " (or larger lots)" if args.allow_larger and args.quantity else ""
    return f"{qty}{extra} in [{sec}] {price}"


def _post_webhook(url: str, payload: dict, timeout: float) -> None:
    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(
        url, data=data, headers={"Content-Type": "application/json"}, method="POST"
    )
    try:
        urllib.request.urlopen(req, timeout=timeout)
    except Exception as exc:  # pragma: no cover - best-effort notification
        print(f"[warn] webhook POST failed: {exc}", file=sys.stderr)


def _run_command(command: str, payload: dict) -> None:
    try:
        subprocess.run(
            shlex.split(command),
            input=json.dumps(payload),
            text=True,
            check=False,
        )
    except Exception as exc:  # pragma: no cover - best-effort notification
        print(f"[warn] notify command failed: {exc}", file=sys.stderr)


def _notify(event: Optional[Event], matches: List[Listing], args) -> None:
    payload = {
        "event": event.to_dict() if event else None,
        "matches": [m.to_dict() for m in matches],
    }
    if args.webhook:
        _post_webhook(args.webhook, payload, args.timeout)
    if args.command:
        _run_command(args.command, payload)


def _build_search_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="gametime_watcher search",
        description="Search Gametime for events by team or performer name.",
    )
    p.add_argument("query", help="Team or performer to search for (e.g. 'Athletics').")
    p.add_argument("--json", action="store_true", help="Output JSON instead of text.")
    p.add_argument("--user-agent", default=None, help="Override the HTTP User-Agent.")
    p.add_argument("--timeout", type=float, default=30.0, help="HTTP timeout in seconds.")
    return p


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="gametime_watcher",
        description=(
            "List current Gametime ticket prices for an event and alert when "
            "seats in chosen sections drop below a per-ticket price.\n\n"
            "Use 'gametime_watcher search <query>' to find event links by "
            "team or performer name."
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    p.add_argument("event", help="Gametime event/listing URL, short link, or 24-char event id")
    p.add_argument(
        "-s", "--sections", action="append",
        help="Section filter: ranges (200-299), exact (119), or group names "
             "(\"Solon Club\"); comma-separated. May be given multiple times "
             "to combine filters. Default: all sections.",
    )
    p.add_argument(
        "-p", "--max-price", type=float,
        help="Maximum all-in price PER TICKET, in dollars.",
    )
    p.add_argument(
        "-q", "--quantity", type=int, default=1,
        help="Number of seats wanted together (default: 1). Only listings "
             "offering this lot size are kept.",
    )
    p.add_argument(
        "--allow-larger", action="store_true",
        help="Also keep listings that only offer a larger lot than --quantity.",
    )
    p.add_argument("--json", action="store_true", help="Output JSON instead of text.")
    p.add_argument(
        "--watch", action="store_true",
        help="Poll repeatedly and alert only on newly-seen matching listings.",
    )
    p.add_argument(
        "--interval", type=float, default=300.0,
        help="Polling interval in seconds when --watch is set (default: 300).",
    )
    p.add_argument("--webhook", help="POST a JSON payload to this URL on new matches.")
    p.add_argument(
        "--command",
        help="Run this command on new matches; the JSON payload is sent on stdin.",
    )
    p.add_argument("--user-agent", default=None, help="Override the HTTP User-Agent.")
    p.add_argument("--timeout", type=float, default=30.0, help="HTTP timeout in seconds.")
    return p


def _run_search(argv: List[str]) -> int:
    """Handle the ``search`` subcommand."""
    parser = _build_search_parser()
    args = parser.parse_args(argv)
    if args.user_agent is None:
        from .api import DEFAULT_USER_AGENT
        args.user_agent = DEFAULT_USER_AGENT

    try:
        events = search_events(args.query, user_agent=args.user_agent, timeout=args.timeout)
    except GametimeError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    if args.json:
        payload = {
            "query": args.query,
            "event_count": len(events),
            "events": [
                {**e.to_dict(), "url": e.extra.get("url"), "min_price_total_cents": e.extra.get("min_price_total")}
                for e in events
            ],
        }
        print(json.dumps(payload, indent=2))
    else:
        print(f"Found {len(events)} upcoming event(s) for {args.query!r}:\n")
        for e in sorted(events, key=lambda x: x.datetime_local or ""):
            min_p = e.extra.get("min_price_total")
            price_str = f"  from ${min_p / 100:.0f}" if min_p else ""
            print(f"  {e.datetime_local or '???':>19}  {e.name or '?'}{price_str}")
            print(f"    {e.extra.get('url', '')}")
    return 0 if events else 1


def main(argv: Optional[List[str]] = None) -> int:
    raw = argv if argv is not None else sys.argv[1:]
    # Dispatch to subcommand if first arg is 'search'.
    if raw and raw[0] == "search":
        return _run_search(raw[1:])

    parser = build_parser()
    args = parser.parse_args(argv)
    if args.user_agent is None:
        from .api import DEFAULT_USER_AGENT
        args.user_agent = DEFAULT_USER_AGENT
    if args.quantity is not None and args.quantity < 1:
        parser.error("--quantity must be >= 1")

    if not args.watch:
        try:
            event, all_listings, matches = _scan(args.event, args)
        except GametimeError as exc:
            print(f"error: {exc}", file=sys.stderr)
            return 2

        if args.json:
            print(_emit_json(event, matches))
        else:
            title = event.name if event and event.name else args.event
            when = f" @ {event.datetime_local}" if event and event.datetime_local else ""
            print(f"{title}{when}")
            print(f"Criteria: {_criteria_text(args)}")
            print(f"Found {len(matches)} of {len(all_listings)} listings:")
            for l in matches:
                print("  " + _format_listing(l))
        # Exit 0 when matches found, 1 when none (useful for cron/scripts).
        return 0 if matches else 1

    # --watch mode: poll and alert on new matches.
    seen: set = set()
    print(
        f"Watching {args.event} every {args.interval:g}s for {_criteria_text(args)} "
        "(Ctrl-C to stop)...",
        file=sys.stderr,
    )
    try:
        while True:
            try:
                event, all_listings, matches = _scan(args.event, args)
            except GametimeError as exc:
                print(f"[warn] scan failed: {exc}", file=sys.stderr)
            else:
                new = [m for m in matches if (m.id, m.price_total) not in seen]
                for m in matches:
                    seen.add((m.id, m.price_total))
                if new:
                    stamp = time.strftime("%Y-%m-%d %H:%M:%S")
                    print(f"[{stamp}] {len(new)} new matching listing(s):")
                    for l in new:
                        print("  " + _format_listing(l))
                    _notify(event, new, args)
            time.sleep(max(1.0, args.interval))
    except KeyboardInterrupt:
        print("\nStopped.", file=sys.stderr)
        return 0


if __name__ == "__main__":  # pragma: no cover
    raise SystemExit(main())
