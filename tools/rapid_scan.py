#!/usr/bin/env python3
# Copyright (C) 2026, Charles Chiou

"""
Rapid raw book scanner for Brother MFC-L2715DW over network (AirScan/eSCL).
Dumps raw 300 DPI Color PNGs directly into scanned_docs/raw_chapter3/ without
any OCR or PDF processing, minimizing wait time between page flips.
"""

from pathlib import Path
import sys
import os
import time
import subprocess
import argparse

SCANNER_DEVICE = "escl:http://192.168.11.5:80"
OUTPUT_DIR = os.path.join(str(Path(__file__).resolve().parent.parent), "scanned_docs/raw_chapter3")

def scan_spread(start_p, end_p):
    out_file = os.path.join(OUTPUT_DIR, f"page_{start_p:03d}_{end_p:03d}.png")
    t0 = time.time()
    print(f"[*] Scanning pages {start_p}–{end_p} -> {os.path.basename(out_file)}...")
    cmd = [
        "scanimage",
        "-d", SCANNER_DEVICE,
        "--source", "Flatbed",
        "--mode", "Color",
        "--resolution", "300",
        "--format", "png"
    ]
    with open(out_file, "wb") as f:
        res = subprocess.run(cmd, stdout=f, stderr=subprocess.PIPE)
    if res.returncode != 0:
        if os.path.exists(out_file):
            os.remove(out_file)
        err = res.stderr.decode("utf-8", errors="ignore").strip()
        print(f"[!] Scan error: {err}")
        return False
    size_mb = os.path.getsize(out_file) / (1024 * 1024)
    elapsed = time.time() - t0
    print(f"[✓] Pages {start_p}–{end_p} saved ({size_mb:.1f} MB in {elapsed:.1f}s)")
    return True

def main():
    parser = argparse.ArgumentParser(description="Rapid book scanner")
    parser.add_argument("--start", type=int, default=160, help="First page (default: 160)")
    parser.add_argument("--end", type=int, default=204, help="Last page (default: 204)")
    args = parser.parse_args()

    os.makedirs(OUTPUT_DIR, exist_ok=True)

    print("=" * 65)
    print("  ABC-80 Rapid Scanner: Chapter 3 (pp. 160–204)")
    print("  Zero OCR / Zero Post-Processing during scan")
    print("=" * 65)
    print(f"Target directory: {OUTPUT_DIR}\n")

    curr = args.start
    while curr <= args.end:
        next_p = curr + 1 if curr + 1 <= args.end else curr
        page_label = f"pages {curr}–{next_p}" if next_p != curr else f"page {curr}"
        
        # Check if already scanned
        expected_file = os.path.join(OUTPUT_DIR, f"page_{curr:03d}_{next_p:03d}.png")
        status_tag = " [EXISTS - will overwrite if scanned]" if os.path.exists(expected_file) else ""

        print("-" * 65)
        print(f" NEXT: Place {page_label} on the scanner flatbed glass{status_tag}")
        print("-" * 65)
        
        try:
            prompt = f"Press [Enter] to scan {page_label} (or 's' to skip, 'q' to quit): "
            choice = input(prompt).strip().lower()
        except (KeyboardInterrupt, EOFError):
            print("\nExiting.")
            break

        if choice == 'q':
            print("Stopping scanner.")
            break
        elif choice == 's':
            print(f"Skipping {page_label}.")
            curr += 2
            continue

        success = scan_spread(curr, next_p)
        if success:
            curr += 2
        else:
            retry = input("Scan failed. Press [Enter] to retry or 's' to skip: ").strip().lower()
            if retry == 's':
                curr += 2

    print("\n" + "=" * 65)
    scanned_count = len([f for f in os.listdir(OUTPUT_DIR) if f.endswith(".png")])
    print(f"Scan session complete. Total raw scans in {OUTPUT_DIR}: {scanned_count}")
    print("You can now run post-processing in batch (rotation, OCR, PDF merge).")
    print("=" * 65)

if __name__ == "__main__":
    main()
