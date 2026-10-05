#!/usr/bin/env python3
# Copyright (C) 2026, Charles Chiou

"""
Parallel split-page OCR extractor for ROM listing pages (pp. 166-204).
Uses ProcessPoolExecutor across multiple cores.
"""

from pathlib import Path
import os
from concurrent.futures import ProcessPoolExecutor, as_completed
from PIL import Image
import pytesseract

RAW_DIR = os.path.join(str(Path(__file__).resolve().parent.parent), "scanned_docs/raw_chapter3")
OUT_DIR = os.path.join(str(Path(__file__).resolve().parent.parent), "scanned_docs/rom_pages")

SPREADS = [
    ("page_166_167.png", 166, 167),
    ("page_168_169.png", 168, 169),
    ("page_170_171.png", 170, 171),
    ("page_172_173.png", 172, 173),
    ("page_174_175.png", 174, 175),
    ("page_176_177.png", 176, 177),
    ("page_178_179.png", 178, 179),
    ("page_180_181.png", 180, 181),
    ("page_182_183.png", 182, 183),
    ("page_184_185.png", 184, 185),
    ("page_186_187.png", 186, 187),
    ("page_188_189.png", 188, 189),
    ("page_190_191.png", 190, 191),
    ("page_192_193.png", 192, 193),
    ("page_194_195.png", 194, 195),
    ("page_196_197.png", 196, 197),
    ("page_198_199.png", 198, 199),
    ("page_200_201.png", 200, 201),
    ("page_202_203.png", 202, 203),
    ("page_204.png", 204, None),
]

def process_spread(spread_info):
    fname, left_num, right_num = spread_info
    fpath = os.path.join(RAW_DIR, fname)
    if not os.path.exists(fpath):
        return fname, False
        
    img = Image.open(fpath).rotate(270, expand=True)
    w, h = img.size
    
    # Left page
    left_crop = img.crop((0, 0, int(w * 0.49), h))
    left_txt_path = os.path.join(OUT_DIR, f"page_{left_num:03d}.txt")
    txt_l = pytesseract.image_to_string(left_crop, lang='chi_tra+eng', config='--psm 6')
    with open(left_txt_path, "w", encoding="utf-8") as f:
        f.write(txt_l)
        
    if right_num:
        right_crop = img.crop((int(w * 0.51), 0, w, h))
        right_txt_path = os.path.join(OUT_DIR, f"page_{right_num:03d}.txt")
        txt_r = pytesseract.image_to_string(right_crop, lang='chi_tra+eng', config='--psm 6')
        with open(right_txt_path, "w", encoding="utf-8") as f:
            f.write(txt_r)
            
    print(f"[✓] Completed {fname} (p.{left_num}" + (f", p.{right_num}" if right_num else "") + ")")
    return fname, True

def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    print("=" * 65)
    print(f"  Parallel Split-Page ROM Extractor: {len(SPREADS)} spreads")
    print("  Workers: 16 parallel processes")
    print("=" * 65)
    
    with ProcessPoolExecutor(max_workers=16) as executor:
        futures = [executor.submit(process_spread, s) for s in SPREADS]
        for f in as_completed(futures):
            f.result()
            
    pages_count = len(os.listdir(OUT_DIR))
    print("=" * 65)
    print(f"[✓] Extracted {pages_count} individual page text files into {OUT_DIR}")

if __name__ == "__main__":
    main()
