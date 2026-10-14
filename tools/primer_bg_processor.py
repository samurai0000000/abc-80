#!/usr/bin/env python3
# Copyright (C) 2026, Charles Chiou

"""
Continuous background processor for ABC-80 Primer Part 2 (pp. 94-195).
Monitors scanned_docs/raw_primer_part2/ for new raw scans, rotates them 270 deg,
runs Tesseract OCR (chi_tra+eng), and maintains a merged PDF.
"""

from pathlib import Path
import sys
import os
import glob
import time
from PIL import Image
import pytesseract
import pypdf

REPO_ROOT = str(Path(__file__).resolve().parent.parent)
RAW_DIR = os.path.join(REPO_ROOT, "scanned_docs/raw_primer_part2")
CONSOLIDATED_PDF = os.path.join(REPO_ROOT, "scanned_docs/Z80_Micro_Computer_Primer_Part2.pdf")
LOG_FILE = os.path.join(RAW_DIR, "processor.log")

def log(msg):
    ts = time.strftime("%Y-%m-%d %H:%M:%S")
    line = f"[{ts}] {msg}"
    print(line, flush=True)
    try:
        with open(LOG_FILE, "a", encoding="utf-8") as f:
            f.write(line + "\n")
    except Exception:
        pass

def process_image(png_path):
    base = os.path.splitext(os.path.basename(png_path))[0]
    out_upright = os.path.join(RAW_DIR, f"{base}_upright.png")
    out_pdf = os.path.join(RAW_DIR, f"{base}_ocr.pdf")
    
    if os.path.exists(out_pdf):
        return  # already processed
    
    log(f"Processing {base}...")
    t0 = time.time()
    try:
        img = Image.open(png_path)
        rot = img.rotate(270, expand=True)
        rot.save(out_upright)
        
        pdf_bytes = pytesseract.image_to_pdf_or_hocr(rot, extension='pdf', lang='chi_tra+eng')
        with open(out_pdf, "wb") as f:
            f.write(pdf_bytes)
            
        elapsed = time.time() - t0
        size_mb = os.path.getsize(out_pdf) / (1024 * 1024)
        log(f"Completed {base} in {elapsed:.1f}s ({size_mb:.2f} MB)")
    except Exception as e:
        log(f"Error processing {png_path}: {e}")

def update_consolidated_pdf():
    pdf_files = sorted(glob.glob(os.path.join(RAW_DIR, "page_*_ocr.pdf")))
    if not pdf_files:
        return
    writer = pypdf.PdfWriter()
    for pf in pdf_files:
        try:
            reader = pypdf.PdfReader(pf)
            for page in reader.pages:
                writer.add_page(page)
        except Exception as e:
            log(f"Error reading {pf}: {e}")
    temp_pdf = CONSOLIDATED_PDF + ".tmp"
    with open(temp_pdf, "wb") as f:
        writer.write(f)
    os.replace(temp_pdf, CONSOLIDATED_PDF)
    log(f"Updated {os.path.basename(CONSOLIDATED_PDF)} ({len(writer.pages)} pages)")

def main():
    os.makedirs(RAW_DIR, exist_ok=True)
    log("Primer Part 2 background processor started.")
    
    processed_count = 0
    while True:
        raw_files = sorted(glob.glob(os.path.join(RAW_DIR, "page_*.png")))
        new_work = False
        for rf in raw_files:
            if "_upright" in rf:
                continue
            base = os.path.splitext(os.path.basename(rf))[0]
            out_pdf = os.path.join(RAW_DIR, f"{base}_ocr.pdf")
            if not os.path.exists(out_pdf):
                process_image(rf)
                new_work = True
                processed_count += 1
                
        if new_work:
            update_consolidated_pdf()
            
        time.sleep(2)

if __name__ == "__main__":
    main()
