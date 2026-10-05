#!/usr/bin/env python3
# Copyright (C) 2026, Charles Chiou

"""
Interactive scanner script for Brother MFC-L2715DW over network (AirScan/eSCL).
Scans flatbed directly, applies OCR (chi_tra+eng), auto-orients, and appends to Z80_Micro_Computer_Building.pdf.
"""

from pathlib import Path
import sys
import subprocess
import os
import io
import pypdf
from PIL import Image
import pytesseract

SCANNER_DEVICE = "escl:http://192.168.11.5:80"
OUTPUT_PDF = os.path.join(str(Path(__file__).resolve().parent.parent), "scanned_docs/Z80_Micro_Computer_Building.pdf")

def scan_single_page(resolution=300):
    print(f"[*] Triggering scan on {SCANNER_DEVICE} ({resolution} DPI Color)...")
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
        print(f"[!] Scan failed: {res.stderr.decode('utf-8', errors='ignore')}")
        return None
    print(f"[+] Scan received ({len(res.stdout)} bytes).")
    return res.stdout

def process_and_append(image_bytes, rotate_left=True):
    print("[*] Running OCR and generating PDF page...")
    img = Image.open(io.BytesIO(image_bytes))
    
    # Generate searchable PDF with OCR
    pdf_bytes = pytesseract.image_to_pdf_or_hocr(img, extension='pdf', lang='chi_tra+eng')
    
    # Read generated single page
    new_reader = pypdf.PdfReader(io.BytesIO(pdf_bytes))
    new_page = new_reader.pages[0]
    if rotate_left:
        new_page.rotate(90)
    
    # Append to consolidated PDF
    writer = pypdf.PdfWriter()
    if os.path.exists(OUTPUT_PDF):
        existing_reader = pypdf.PdfReader(OUTPUT_PDF)
        for p in existing_reader.pages:
            writer.add_page(p)
    
    writer.add_page(new_page)
    
    with open(OUTPUT_PDF, 'wb') as f:
        writer.write(f)
    
    print(f"[+] Appended to {OUTPUT_PDF}. Total pages: {len(writer.pages)}")

def main():
    print("=" * 60)
    print("  ABC-80 Brother MFC-L2715DW Network Scanner Assistant")
    print("=" * 60)
    print("Place book spread on the scanner flatbed, close cover, and press [Enter].")
    print("Type 'q' and press [Enter] to quit.\n")
    
    while True:
        try:
            inp = input("Press [Enter] to scan next page (or 'q' to quit): ").strip().lower()
        except (EOFError, KeyboardInterrupt):
            break
        if inp == 'q':
            break
        
        raw_png = scan_single_page()
        if raw_png:
            process_and_append(raw_png, rotate_left=True)
            print("[✓] Done!\n")

if __name__ == "__main__":
    main()
