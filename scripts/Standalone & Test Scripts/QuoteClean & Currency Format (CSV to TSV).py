#!/usr/bin/env python3
"""
Quote Clean & Currency Format (TSV output)
- Input: CSV with comma delimiter.
- Output: **TSV** (tab-delimited) to avoid quoting even when we keep comma thousands.
Rules:
- Remove unnecessary surrounding quotes; trim whitespace; strip stray internal double quotes.
- Currency-like values:
    * Always two decimals.
    * Add thousands separators only when abs(value) >= 1000.00.
    * Preserve leading '$' if the original had it.
- Writes sibling *_clean.tsv; does not overwrite original.
"""

import os, csv, re, sys, shutil
from datetime import datetime

CURRENCY_LIKE = re.compile(r'^\s*\$?-?\d{1,3}(?:,\d{3})*(?:\.\d+)?\s*$|^\s*\$?-?\d+(?:\.\d+)?\s*$')

def create_backup(file_path: str) -> str:
    timestamp = datetime.now().strftime("_%Y%m%d-%H%M")
    root, ext = os.path.splitext(file_path)
    backup_path = f"{root}{timestamp}{ext}"
    shutil.copy2(file_path, backup_path)
    return backup_path

def format_currency_preserve_symbol(raw: str):
    if not CURRENCY_LIKE.match(raw):
        return None
    s = raw.strip()
    had_dollar = s.startswith("$")
    s = s.replace("$", "").replace(",", "")
    try:
        val = float(s)
    except ValueError:
        return None
    if abs(val) >= 1000:
        core = f"{val:,.2f}"
    else:
        core = f"{val:.2f}"
    return f"${core}" if had_dollar else core

def clean_field(val: str) -> str:
    if val is None:
        return ""
    if len(val) >= 2 and val[0] == '"' and val[-1] == '"':
        val = val[1:-1]
    val = val.strip().replace('"', '')
    maybe = format_currency_preserve_symbol(val)
    return maybe if maybe is not None else val

def process_to_tsv(file_path: str, encoding="latin1") -> str:
    root, _ = os.path.splitext(file_path)
    out_path = f"{root}_clean.tsv"
    with open(file_path, "r", encoding=encoding, newline="") as f_in, \
         open(out_path,  "w", encoding=encoding, newline="") as f_out:
        reader = csv.reader(f_in)  # input CSV
        writer = csv.writer(f_out, delimiter="\t", quoting=csv.QUOTE_NONE, escapechar="\\")
        for row in reader:
            writer.writerow([clean_field(col) for col in row])
    return out_path

def main():
    while True:
        print("\nENTER PATH TO CSV:")
        p = input().strip().strip('"')
        if not os.path.exists(p):
            print(f"File not found: {p}")
            continue
        try:
            b = create_backup(p)
            print(f"Backup created: {os.path.basename(b)}")
            out = process_to_tsv(p)
            print(f"TSV written: {out}")
            print("\nAnother file? (Y/N)")
            if input().strip().upper() != "Y":
                print("Done. Press Enter to exit..."); input(); sys.exit(0)
        except Exception as e:
            print(f"ERROR: {e}")
            print("Press Enter to continue..."); input()

if __name__ == "__main__":
    main()
