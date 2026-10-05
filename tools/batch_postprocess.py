#!/usr/bin/env python3
# Copyright (C) 2026, Charles Chiou

"""
Parallel batch post-processor for ABC-80 scanned pages.
Uses multiprocessing to rotate 270 degrees and run Tesseract OCR (chi_tra+eng)
across all available CPU cores, then merges into the master PDF.
"""

from pathlib import Path
import sys
import os
import glob
import time
import shutil
from concurrent.futures import ProcessPoolExecutor, as_completed
from PIL import Image
import pytesseract
import pypdf

RAW_DIR = os.path.join(str(Path(__file__).resolve().parent.parent), "scanned_docs/raw_chapter3")
MASTER_PDF = os.path.join(str(Path(__file__).resolve().parent.parent), "scanned_docs/Z80_Micro_Computer_Building.pdf")

def process_single_image(png_path):
    t0 = time.time()
    base = os.path.splitext(os.path.basename(png_path))[0]
    out_pdf = os.path.join(RAW_DIR, f"{base}_ocr.pdf")
    
    # Open and rotate 270 deg clockwise (right-side up)
    img = Image.open(png_path)
    img_rot = img.rotate(270, expand=True)
    
    # Run Tesseract OCR to PDF
    pdf_bytes = pytesseract.image_to_pdf_or_hocr(img_rot, extension='pdf', lang='chi_tra+eng')
    with open(out_pdf, "wb") as f:
        f.write(pdf_bytes)
        
    elapsed = time.time() - t0
    size_mb = os.path.getsize(out_pdf) / (1024 * 1024)
    print(f"[+] [{elapsed:5.1f}s] {base} -> {os.path.basename(out_pdf)} ({size_mb:.2f} MB)")
    return png_path, out_pdf

def sort_key(filename):
    # Sort by the first page number in filename
    # e.g. page_160_161.png -> 160, page_204.png -> 204
    base = os.path.basename(filename)
    parts = base.replace(".png").replace(".pdf").split("_")
    for part in parts:
        if part.isdigit():
            return int(part)
    return 9999

def main():
    raw_files = sorted(glob.glob(os.path.join(RAW_DIR, "page_*.png")), key=sort_key)
    if not raw_files:
        print("[!] No raw PNG files found in", RAW_DIR)
        sys.exit(1)
        
    print("=" * 65)
    print(f"  Parallel Post-Processing: {len(raw_files)} pages")
    print("  Workers: 16 parallel processes")
    print("  Steps: Rotate 270° -> Tesseract OCR (chi_tra+eng) -> Merge PDF")
    print("=" * 65)

    t_start = time.time()
    results = {}
    
    with ProcessPoolExecutor(max_workers=16) as executor:
        futures = {executor.submit(process_single_image, f): f for f in raw_files}
        for future in as_completed(futures):
            f_orig = futures[future]
            try:
                orig, pdf_out = future.result()
                results[orig] = pdf_out
            except Exception as e:
                print(f"[!] Error processing {f_orig}: {e}")
                
    total_ocr_time = time.time() - t_start
    print("=" * 65)
    print(f"[✓] Parallel OCR finished in {total_ocr_time:.1f}s ({len(results)}/{len(raw_files)} completed)")
    
    # Merge into master PDF
    print("\n[*] Merging into master PDF:", MASTER_PDF)
    backup_pdf = MASTER_PDF + ".bak"
    if os.path.exists(MASTER_PDF) and not os.path.exists(backup_pdf):
        shutil.copy2(MASTER_PDF, backup_pdf)
        print(f"[*] Backup created: {backup_pdf}")
        
    writer = pypdf.PdfWriter()
    
    # Keep the first 26 pages from existing master PDF (Pages 1 to 121 of book)
    if os.path.exists(MASTER_PDF):
        existing_reader = pypdf.PdfReader(MASTER_PDF)
        keep_count = min(26, len(existing_reader.pages))
        print(f"[*] Keeping first {keep_count} pages from existing master PDF...")
        for i in range(keep_count):
            writer.add_page(existing_reader.pages[i])
            
    # Append the new sorted OCR pages
    print(f"[*] Appending {len(raw_files)} newly processed pages...")
    for raw_f in raw_files:
        ocr_pdf = results.get(raw_f)
        if ocr_pdf and os.path.exists(ocr_pdf):
            r = pypdf.PdfReader(ocr_pdf)
            for p in r.pages:
                writer.add_page(p)
        else:
            print(f"[!] Warning: missing OCR PDF for {raw_f}")
            
    with open(MASTER_PDF, "wb") as f:
        writer.write(f)
        
    final_size_mb = os.path.getsize(MASTER_PDF) / (1024 * 1024)
    print(f"[✓] Successfully wrote master PDF: {MASTER_PDF}")
    print(f"[✓] Final total pages: {len(writer.pages)} ({final_size_mb:.1f} MB)")
    print("=" * 65)

if __name__ == "__main__":
    main()
