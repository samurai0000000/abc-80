#!/usr/bin/env python3
# Copyright (C) 2026, Charles Chiou

"""
Parser for ABC-80 ROM assembly listings.
Extracts fields: (page, address, opcodes, label, instruction, comment_zh)
"""

from pathlib import Path
import os
import re
import glob

PAGES_DIR = os.path.join(str(Path(__file__).resolve().parent.parent), "scanned_docs/rom_pages")

def parse_page_file(fpath):
    pname = os.path.basename(fpath)
    pnum = int(re.search(r'\d+', pname).group(0))
    with open(fpath, "r", encoding="utf-8") as f:
        lines = f.readlines()
        
    records = []
    for line in lines:
        raw = line.strip()
        if not raw or any(hdr in raw for hdr in ["第三篇", "第三章", "地址", "工作碼", "標記", "組合語言", "說 明", "監督程式"]):
            continue
            
        # Match address at start or after some garbage characters
        # e.g. "0000 06 00 RSTO LD B,O0H 當電源開啟..."
        # or "! 0038 22 F8 13 BREAK LD (TEMP),HL ..."
        m = re.search(r'(?:^|[^\da-fA-F])([0-7][0-9A-Fa-f]{3})\b\s+(.*)', raw)
        if m:
            addr = m.group(1).upper()
            rest = m.group(2).strip()
            records.append({
                "page": pnum,
                "addr": addr,
                "raw": rest,
                "orig_line": raw
            })
    return records

def main():
    files = sorted(glob.glob(os.path.join(PAGES_DIR, "page_*.txt")))
    all_recs = []
    for f in files:
        recs = parse_page_file(f)
        all_recs.extend(recs)
    print(f"Parsed {len(all_recs)} address entries from {len(files)} pages.")
    
    # Check address continuity
    print(f"First address: {all_recs[0]['addr']}, Last address: {all_recs[-1]['addr']}")

if __name__ == "__main__":
    main()
