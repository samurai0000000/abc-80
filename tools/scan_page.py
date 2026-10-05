#!/usr/bin/env python3
# Copyright (C) 2026, Charles Chiou

"""
Single-spread scanner script for Brother MFC-L2715DW over network (AirScan/eSCL).
Scans flatbed at 300 DPI Color, performs OCR (chi_tra+eng), applies 270-degree rotation
(right-side up for spine-left placement), inserts into Z80_Micro_Computer_Building.pdf,
and outputs a preview thumbnail.
"""

from pathlib import Path
import sys
import os
import io
import argparse
import subprocess
import pypdf
from PIL import Image
import pytesseract

SCANNER_DEVICE = "escl:http://192.168.11.5:80"
PDF_PATH = os.path.join(str(Path(__file__).resolve().parent.parent), "scanned_docs/Z80_Micro_Computer_Building.pdf")
PREVIEW_DIR = os.path.join(str(Path(__file__).resolve().parent.parent), "doc", "assets")

def scan_raw(resolution=300):
    cmd = [
        "scanimage",
        "-d", SCANNER_DEVICE,
        "--source", "Flatbed",
        "--mode", "Color",
        "--resolution", str(resolution),
        "--format", "png"
    ]
    res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if res.returncode != 0:
        raise RuntimeError(f"Scan failed: {res.stderr.decode('utf-8', errors='ignore')}")
    return res.stdout

def process_spread(raw_bytes, start_page, end_page, insert_index=None, rotate_deg=270):
    os.makedirs(PREVIEW_DIR, exist_ok=True)
    img = Image.open(io.BytesIO(raw_bytes))
    
    # 1. Rotate raw image so it is right side up
    rotated_img = img.rotate(rotate_deg, expand=True)
    
    # 2. Save thumbnail preview for quick visual verification
    preview_path = os.path.join(PREVIEW_DIR, f"preview_pp_{start_page}_{end_page}.png")
    thumb = rotated_img.copy()
    thumb.thumbnail((1200, 800))
    thumb.save(preview_path)
    
    # 3. Perform OCR directly on rotated image
    pdf_bytes = pytesseract.image_to_pdf_or_hocr(rotated_img, extension='pdf', lang='chi_tra+eng')
    ocr_reader = pypdf.PdfReader(io.BytesIO(pdf_bytes))
    new_page = ocr_reader.pages[0]
    
    # 4. Insert into master PDF
    reader = pypdf.PdfReader(PDF_PATH)
    writer = pypdf.PdfWriter()
    
    total_existing = len(reader.pages)
    if insert_index is None or insert_index >= total_existing:
        for p in reader.pages:
            writer.add_page(p)
        writer.add_page(new_page)
        actual_pos = len(writer.pages)
    else:
        for i, p in enumerate(reader.pages):
            if i == insert_index:
                writer.add_page(new_page)
            writer.add_page(p)
        actual_pos = insert_index + 1
        
    with open(PDF_PATH, 'wb') as f:
        writer.write(f)
        
    # 5. Extract text summary
    ocr_text = new_page.extract_text() or ""
    first_lines = [l.strip() for l in ocr_text.split('\n') if l.strip()][:5]
    
    return {
        "preview_path": preview_path,
        "pdf_page_num": actual_pos,
        "total_pages": len(writer.pages),
        "text_sample": first_lines
    }

def main():
    parser = argparse.ArgumentParser(description="Scan and insert book spread")
    parser.add_argument("--start", type=int, required=True, help="First book page of spread (e.g. 160)")
    parser.add_argument("--end", type=int, required=True, help="Second book page of spread (e.g. 161)")
    parser.add_argument("--insert-index", type=int, default=None, help="0-based index to insert before (default: append or before appendix)")
    parser.add_argument("--rotate", type=int, default=270, help="Rotation degrees (default 270)")
    args = parser.parse_args()
    
    print(f"[*] Scanning spread pp. {args.start}–{args.end} from {SCANNER_DEVICE}...")
    raw = scan_raw()
    print(f"[+] Scan acquired ({len(raw)} bytes). Processing OCR and PDF insertion...")
    info = process_spread(raw, args.start, args.end, insert_index=args.insert_index, rotate_deg=args.rotate)
    print(f"[✓] Successfully inserted pp. {args.start}–{args.end} as PDF page {info['pdf_page_num']} (Total PDF pages: {info['total_pages']})")
    print(f"[*] Preview saved to: {info['preview_path']}")
    print(f"[*] OCR sample: {info['text_sample']}")

if __name__ == "__main__":
    main()
