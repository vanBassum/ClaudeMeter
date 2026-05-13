#!/usr/bin/env python3
"""Scrape local Claude Code transcripts and POST today's totals to ClaudeMeter.

Reads every ~/.claude/projects/**/*.jsonl, sums the usage block on each
assistant turn, computes today's totals (input/output/cache tokens + an
estimated USD cost in cents), and POSTs the aggregate as JSON to the
ClaudeMeter device's /api/usage endpoint.

Pricing is approximate and per-million-tokens — see PRICES below. Update it
when Anthropic publishes new rates.

Run once per minute via cron / Task Scheduler / launchd, or wire it to a
Claude Code Stop hook so it fires the moment a turn completes.

Examples:
    scrape_claude_usage.py --device 192.168.1.42
    scrape_claude_usage.py --device claudemeter.local --token s3cr3t
    scrape_claude_usage.py --device 192.168.1.42 --dry-run
"""

from __future__ import annotations

import argparse
import datetime as dt
import json
import os
import pathlib
import sys
import urllib.request
from typing import Iterable

# Rough Anthropic prices per million tokens (USD). cache_read is much cheaper
# than fresh input. Update when prices change.
PRICES = {
    # model_id_substring : (input, output, cache_read, cache_write)
    "claude-opus-4":      (15.00, 75.00, 1.50, 18.75),
    "claude-sonnet-4":    ( 3.00, 15.00, 0.30,  3.75),
    "claude-haiku-4":     ( 1.00,  5.00, 0.10,  1.25),
    "claude-3-7-sonnet":  ( 3.00, 15.00, 0.30,  3.75),
    "claude-3-5-haiku":   ( 0.80,  4.00, 0.08,  1.00),
}
FALLBACK_PRICE = (3.00, 15.00, 0.30, 3.75)  # treat unknown as sonnet-4


def price_for_model(model: str) -> tuple[float, float, float, float]:
    for needle, p in PRICES.items():
        if needle in model:
            return p
    return FALLBACK_PRICE


def iter_jsonl(path: pathlib.Path) -> Iterable[dict]:
    try:
        with path.open("r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                try:
                    yield json.loads(line)
                except json.JSONDecodeError:
                    continue
    except OSError:
        return


def turn_timestamp(record: dict) -> dt.datetime | None:
    """Best-effort: pull a UTC timestamp out of a transcript record."""
    ts = record.get("timestamp")
    if not ts:
        return None
    try:
        # Claude Code transcripts use ISO-8601 with 'Z' or '+00:00'.
        return dt.datetime.fromisoformat(ts.replace("Z", "+00:00"))
    except (ValueError, AttributeError):
        return None


def turn_usage_and_model(record: dict) -> tuple[dict, str] | None:
    """Return (usage_dict, model_id) for assistant turns, else None."""
    if record.get("type") != "assistant":
        return None
    msg = record.get("message") or {}
    usage = msg.get("usage")
    if not isinstance(usage, dict):
        return None
    model = msg.get("model") or record.get("model") or ""
    return usage, model


def aggregate(root: pathlib.Path) -> dict:
    today_local = dt.datetime.now().astimezone()
    midnight = today_local.replace(hour=0, minute=0, second=0, microsecond=0)
    midnight_utc = midnight.astimezone(dt.timezone.utc)

    totals = {
        "input_tokens": 0,
        "output_tokens": 0,
        "cache_creation_tokens": 0,
        "cache_read_tokens": 0,
        "cost_cents": 0,
    }
    last_activity_unix = 0
    cost_usd = 0.0

    if not root.exists():
        return {
            "today": totals,
            "last_activity_unix": last_activity_unix,
            "schema_version": 1,
        }

    for jsonl in root.rglob("*.jsonl"):
        for rec in iter_jsonl(jsonl):
            ts = turn_timestamp(rec)
            if ts is not None:
                ts_unix = int(ts.timestamp())
                if ts_unix > last_activity_unix:
                    last_activity_unix = ts_unix
            usage_model = turn_usage_and_model(rec)
            if usage_model is None or ts is None:
                continue
            if ts < midnight_utc:
                continue

            usage, model = usage_model
            in_tok    = int(usage.get("input_tokens", 0) or 0)
            out_tok   = int(usage.get("output_tokens", 0) or 0)
            cache_in  = int(usage.get("cache_creation_input_tokens", 0) or 0)
            cache_rd  = int(usage.get("cache_read_input_tokens",     0) or 0)

            totals["input_tokens"]          += in_tok
            totals["output_tokens"]         += out_tok
            totals["cache_creation_tokens"] += cache_in
            totals["cache_read_tokens"]     += cache_rd

            pi, po, pcr, pcw = price_for_model(model)
            cost_usd += (in_tok   * pi  / 1_000_000)
            cost_usd += (out_tok  * po  / 1_000_000)
            cost_usd += (cache_rd * pcr / 1_000_000)
            cost_usd += (cache_in * pcw / 1_000_000)

    totals["cost_cents"] = int(round(cost_usd * 100))
    return {
        "today": totals,
        "last_activity_unix": last_activity_unix,
        "schema_version": 1,
    }


def post(device: str, payload: dict, token: str | None) -> None:
    url = device if "://" in device else f"http://{device}/api/usage"
    body = json.dumps(payload).encode("utf-8")
    headers = {"Content-Type": "application/json"}
    if token:
        headers["Authorization"] = f"Bearer {token}"
    req = urllib.request.Request(url, data=body, headers=headers, method="POST")
    with urllib.request.urlopen(req, timeout=10) as resp:
        if resp.status >= 400:
            raise SystemExit(f"Device returned HTTP {resp.status}: {resp.read()!r}")
        print(f"-> {url}: {resp.status} {resp.read().decode('utf-8', 'replace')}")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument(
        "--device",
        required=True,
        help="ClaudeMeter device hostname/IP (or full URL). e.g. 192.168.1.42",
    )
    parser.add_argument(
        "--token",
        default=os.environ.get("CLAUDEMETER_TOKEN"),
        help="Ingest auth token matching the device's usage.ingest_token setting.",
    )
    parser.add_argument(
        "--root",
        default=pathlib.Path.home() / ".claude" / "projects",
        type=pathlib.Path,
        help="Path to ~/.claude/projects (default: %(default)s)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print the JSON payload instead of POSTing.",
    )
    args = parser.parse_args(argv)

    payload = aggregate(args.root)

    if args.dry_run:
        print(json.dumps(payload, indent=2))
        return 0

    post(args.device, payload, args.token)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
