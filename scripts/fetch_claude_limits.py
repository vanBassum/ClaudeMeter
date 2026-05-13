#!/usr/bin/env python3
"""Probe Anthropic with the local Claude Code OAuth token, parse the unified
rate-limit headers (the same data Claude Code's /usage panel shows), and POST
a digest to the ClaudeMeter device.

This intentionally reproduces the request shape Claude Code itself uses
(`max_tokens:1`, content "quota", `anthropic-beta: oauth-2025-04-20`). The
OAuth access token is read from ~/.claude/.credentials.json — this is a
host-local file; it never leaves your machine.

Caveats:
- Uses an undocumented response-header surface. If Anthropic renames the
  `anthropic-ratelimit-unified-*` headers, this will silently degrade to
  "missing" rows on the device.
- The OAuth token expires; if you see 401 here, run `claude` once to
  refresh it (Claude Code refreshes on startup), then re-run this script.
- Each probe costs a few output tokens (negligible). Run it on a Stop hook
  or via cron every 1-5 minutes.

Examples:
    fetch_claude_limits.py --device 192.168.11.33
    fetch_claude_limits.py --device 192.168.11.33 --token s3cr3t
    fetch_claude_limits.py --device 192.168.11.33 --dry-run
"""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import sys
import time
import urllib.error
import urllib.request


CREDENTIALS_PATH = pathlib.Path.home() / ".claude" / ".credentials.json"

# Match the values used by Claude Code's own probe (cli.js, SI9 / unifiedRateLimit).
ANTHROPIC_BETA = "oauth-2025-04-20"
ANTHROPIC_VERSION = "2023-06-01"
PROBE_MODEL = "claude-haiku-4-5"
PROBE_URL = "https://api.anthropic.com/v1/messages"


def load_oauth_token() -> str:
    if not CREDENTIALS_PATH.exists():
        raise SystemExit(
            f"No credentials file at {CREDENTIALS_PATH} — log in with "
            "`claude` first."
        )
    try:
        creds = json.loads(CREDENTIALS_PATH.read_text(encoding="utf-8"))
    except json.JSONDecodeError as e:
        raise SystemExit(f"Could not parse {CREDENTIALS_PATH}: {e}") from e

    oauth = creds.get("claudeAiOauth") or {}
    token = oauth.get("accessToken")
    if not token:
        raise SystemExit(
            "No claudeAiOauth.accessToken in credentials. Re-login with "
            "`claude` and try again."
        )

    expires_at = oauth.get("expiresAt")
    if isinstance(expires_at, (int, float)) and expires_at < time.time() * 1000:
        print("warning: OAuth token appears expired; probe may 401.", file=sys.stderr)

    return token


def probe_anthropic(token: str) -> dict[str, str]:
    """Send a minimal probe and return rate-limit response headers."""
    body = json.dumps({
        "model": PROBE_MODEL,
        "max_tokens": 1,
        "messages": [{"role": "user", "content": "quota"}],
    }).encode("utf-8")
    req = urllib.request.Request(
        PROBE_URL,
        data=body,
        headers={
            "Authorization":    f"Bearer {token}",
            "anthropic-version": ANTHROPIC_VERSION,
            "anthropic-beta":    ANTHROPIC_BETA,
            "content-type":      "application/json",
        },
        method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=15) as resp:
            return {k.lower(): v for k, v in resp.getheaders()}
    except urllib.error.HTTPError as e:
        if e.code == 401:
            raise SystemExit(
                "Anthropic returned 401 — OAuth token expired or revoked. "
                "Run `claude` to refresh."
            ) from e
        if e.code == 429:
            # Headers usually omitted on 429. Surface what we got anyway.
            return {k.lower(): v for k, v in e.headers.items()}
        raise SystemExit(f"Anthropic returned HTTP {e.code}: {e.read()!r}") from e


def reshape(headers: dict[str, str]) -> dict:
    def util_pct(key: str) -> int:
        v = headers.get(key)
        if v is None:
            return -1
        try:
            return int(round(float(v) * 100))
        except ValueError:
            return -1

    def unix_secs(key: str) -> int:
        v = headers.get(key)
        if v is None:
            return 0
        try:
            return int(v)
        except ValueError:
            return 0

    return {
        "schema_version": 2,
        "five_hour_util":  util_pct("anthropic-ratelimit-unified-5h-utilization"),
        "five_hour_reset": unix_secs("anthropic-ratelimit-unified-5h-reset"),
        "seven_day_util":  util_pct("anthropic-ratelimit-unified-7d-utilization"),
        "seven_day_reset": unix_secs("anthropic-ratelimit-unified-7d-reset"),
        "representative":  headers.get("anthropic-ratelimit-unified-representative-claim", ""),
        "fetched_at_unix": int(time.time()),
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
        help="ClaudeMeter device hostname/IP (or full URL). e.g. 192.168.11.33",
    )
    parser.add_argument(
        "--token",
        default=os.environ.get("CLAUDEMETER_TOKEN"),
        help="Ingest auth token matching the device's usage.auth_tok setting.",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print the JSON payload instead of POSTing.",
    )
    args = parser.parse_args(argv)

    oauth = load_oauth_token()
    headers = probe_anthropic(oauth)
    payload = reshape(headers)

    if args.dry_run:
        print(json.dumps(payload, indent=2))
        return 0

    if payload["five_hour_util"] < 0 and payload["seven_day_util"] < 0:
        print(
            "warning: no rate-limit headers in response — probably 429ed; "
            "device snapshot will be left untouched.",
            file=sys.stderr,
        )

    post(args.device, payload, args.token)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
