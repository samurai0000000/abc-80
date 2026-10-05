#!/usr/bin/env python3
# Copyright (C) 2026, Charles Chiou

"""
take_snapshot.py - Takes a headless browser screenshot of the ABC-80 web UI.

Usage:
    python3 tools/take_snapshot.py [--out doc/assets/web_snapshot.png]
"""

import argparse
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser(description="Take headless screenshot of ABC-80 web UI")
    parser.add_argument("--out", default=str(ROOT / "doc" / "assets" / "web_snapshot.png"),
                        help="Output PNG path")
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=1024)
    args = parser.parse_args()

    html_path = ROOT / "web" / "index.html"
    url = f"file://{html_path}"

    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    try:
        from playwright.sync_api import sync_playwright
    except ImportError:
        print("ERROR: playwright not installed. Run: pip3 install playwright && playwright install chromium",
              file=sys.stderr)
        sys.exit(1)

    print(f"[snapshot] Launching chromium, viewport={args.width}x{args.height}")
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        ctx = browser.new_context(viewport={"width": args.width, "height": args.height})
        page = ctx.new_page()
        page.goto(url, wait_until="networkidle")
        page.wait_for_timeout(1000)
        page.screenshot(path=str(out_path), full_page=False)
        browser.close()

    print(f"[snapshot] Saved: {out_path}")


if __name__ == "__main__":
    main()
