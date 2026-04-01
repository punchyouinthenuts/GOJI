
#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Combine selected CSVs by header name (case-insensitive), union all columns,
and insert an ID column (6-char uppercase alphanumeric, unique) as the first column.

Key changes (v2):
- Robust encoding handling: attempts multiple encodings by actually parsing the CSV.
  This avoids "sample looks fine but full read fails" issues (e.g., cp1252 smart quotes 0x92).
- Keeps everything else the same as v1.

Encodings tried (in order): cp1252 (ANSI), utf-8-sig, utf-8, latin-1, utf-16, utf-16le, utf-16be.
"""
import os
import sys
import csv
import re
import random
import string
from pathlib import Path
from typing import List, Dict, Tuple, Set, Iterable

RANDOM_ALPHABET = string.ascii_uppercase + string.digits

PREFERRED_ENCODINGS = ("cp1252", "utf-8-sig", "utf-8", "latin-1", "utf-16", "utf-16le", "utf-16be")

def input_nonempty(prompt: str) -> str:
    while True:
        try:
            s = input(prompt).strip()
        except EOFError:
            print("\nNo input received. Exiting.")
            sys.exit(1)
        if s:
            return s

def clean_dir_path(p: str) -> str:
    # Accept paths with or without quotes, trailing slashes, etc.
    p = p.strip().strip('"').strip("'")
    return p

def list_csv_files(dir_path: str) -> List[Path]:
    p = Path(dir_path)
    if not p.exists() or not p.is_dir():
        print(f"ERROR: '{dir_path}' is not a valid directory.")
        return []
    csvs = [f for f in sorted(p.iterdir()) if f.is_file() and f.suffix.lower() == ".csv"]
    return csvs

def parse_selection(max_index: int) -> List[int]:
    while True:
        raw = input_nonempty("Enter the numbers of the files to combine (e.g., 1,2,3): ")
        # allow spaces
        parts = re.split(r"[,\s]+", raw.strip().strip(','))
        nums = []
        ok = True
        for part in parts:
            if not part:
                continue
            if not part.isdigit():
                ok = False
                break
            n = int(part)
            if n < 1 or n > max_index:
                ok = False
                break
            nums.append(n)
        if not ok or not nums:
            print("Invalid input. Please enter numbers from the list, separated by commas.")
            continue
        # Confirm
        unique_nums = sorted(set(nums), key=lambda x: nums.index(x))  # keep original order of first appearance
        print("You selected:", ", ".join(map(str, unique_nums)))
        yn = input_nonempty("Proceed? (Y/N): ").strip().lower()
        if yn in ("y", "yes"):
            return unique_nums
        else:
            print("\nOkay, let's try again.\n")

def try_read_csv_with_encodings(path: Path, encodings: Iterable[str] = None) -> Tuple[str, List[str], List[Dict[str, str]]]:
    """
    Attempt to fully parse the CSV with several encodings; return the first that succeeds.
    Returns: (encoding_used, fieldnames, rows)
    """
    if encodings is None:
        encodings = PREFERRED_ENCODINGS
    last_err = None
    for enc in encodings:
        try:
            with open(path, "r", encoding=enc, newline="") as f:
                reader = csv.DictReader(f)
                fieldnames = list(reader.fieldnames) if reader.fieldnames else []
                rows = [row for row in reader]  # fully parse to ensure decode works
            return enc, fieldnames, rows
        except Exception as e:
            last_err = e
            continue
    # If we get here, re-raise the last error with context
    raise RuntimeError(f"Failed to decode/parse '{path.name}' with tried encodings: {list(encodings)}") from last_err

def normalize_header(h: str) -> str:
    # Case-insensitive key; wording is paramount, so only lower/strip; do not merge near-duplicates.
    return (h or "").strip().lower()

def build_canonical_headers(selected_files: List[Path]) -> Tuple[List[str], List[Tuple[Path, str, List[str], List[Dict[str, str]]]]]:
    """
    Returns:
      headers_ordered: union of headers with order rule:
        - headers from the FIRST selected file in their original order
        - then any new headers discovered in subsequent files appended as discovered
      file_info: list of tuples (path, encoding, fieldnames, rows)
    """
    headers_ordered: List[str] = []
    seen_keys: Set[str] = set()
    file_info = []
    for i, path in enumerate(selected_files):
        enc, fields, rows = try_read_csv_with_encodings(path)
        if not fields:
            print(f"WARNING: '{path.name}' has no header row; skipping.")
            continue
        file_info.append((path, enc, fields, rows))
        if i == 0:
            for h in fields:
                key = normalize_header(h)
                if key not in seen_keys:
                    headers_ordered.append(h)  # keep original casing & spacing from first occurrence
                    seen_keys.add(key)
        else:
            for h in fields:
                key = normalize_header(h)
                if key not in seen_keys:
                    headers_ordered.append(h)
                    seen_keys.add(key)
    return headers_ordered, file_info

def generate_unique_ids(n: int) -> List[str]:
    ids: Set[str] = set()
    out: List[str] = []
    while len(out) < n:
        token = "".join(random.choices(RANDOM_ALPHABET, k=6))
        if token not in ids:
            ids.add(token)
            out.append(token)
    return out

def align_and_write(output_path: Path, headers: List[str], file_info: List[Tuple[Path, str, List[str], List[Dict[str, str]]]]):
    # Insert ID as the first column
    headers_with_id = ["ID"] + headers

    # Build normalization map from key -> canonical header (as in headers list)
    key_to_canonical: Dict[str, str] = {}
    for h in headers:
        key_to_canonical[normalize_header(h)] = h

    # Count total rows to pre-generate unique IDs
    total_rows = sum(len(rows) for _, _, _, rows in file_info)
    ids = generate_unique_ids(total_rows)
    id_iter = iter(ids)

    with open(output_path, "w", encoding="utf-8-sig", newline="") as f_out:
        writer = csv.DictWriter(f_out, fieldnames=headers_with_id, quoting=csv.QUOTE_MINIMAL)
        writer.writeheader()

        written = 0
        for path, enc, fieldnames, rows in file_info:
            # Map this file's fields to canonical headers (case-insensitive equality only)
            file_field_to_canonical: Dict[str, str] = {}
            for fh in fieldnames:
                key = normalize_header(fh)
                if key in key_to_canonical:
                    file_field_to_canonical[fh] = key_to_canonical[key]
                else:
                    # Shouldn't happen because headers union was already built
                    file_field_to_canonical[fh] = fh

            for row in rows:
                out_row = {h: "" for h in headers_with_id}
                out_row["ID"] = next(id_iter)
                # Place values according to canonical mapping
                for fh, val in row.items():
                    if fh is None:
                        continue
                    canon = file_field_to_canonical.get(fh)
                    if canon is None:
                        continue
                    out_row[canon] = val if val is not None else ""
                writer.writerow(out_row)
                written += 1

    print(f"Wrote {written} rows to: {output_path}")

def main():
    print("=== Combine CSVs by Header (Case-Insensitive) & Add Unique 6-Char ID (v2) ===")
    while True:
        dir_input = input_nonempty('Enter directory path containing CSV files (e.g., C:\\Users\\JCox\\Downloads\\): ')
        dir_path = clean_dir_path(dir_input)
        csv_files = list_csv_files(dir_path)
        if not csv_files:
            print("No CSV files found in that directory. Try again.\n")
            continue

        # Show numbered list
        print("\nCSV files found:")
        for idx, p in enumerate(csv_files, start=1):
            print(f"{idx}. {p.name}")
        print("")

        selection = parse_selection(len(csv_files))
        selected_files = [csv_files[i-1] for i in selection]

        # Confirm list of selected names before proceeding
        print("\nYou chose to combine these files (in this order):")
        for p in selected_files:
            print(f"- {p.name}")
        yn = input_nonempty("Final confirmation? (Y/N): ").strip().lower()
        if yn not in ("y", "yes"):
            print("\nOkay, starting over.\n")
            continue

        # Build headers union (order: first file's headers, then new ones)
        headers, file_info = build_canonical_headers(selected_files)
        if not file_info:
            print("No readable CSVs were selected. Exiting.")
            sys.exit(1)

        # Output path: COMBINED.csv in same directory
        out_path = Path(dir_path) / "COMBINED.csv"
        align_and_write(out_path, headers, file_info)
        print("\nDone!")
        break

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nOperation canceled by user.")
        sys.exit(1)
