#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""
Combine selected CSVs or Excel (XLS/XLSX) by header name (case-insensitive),
union all columns, and insert a unique 6-char uppercase alphanumeric ID column
as the first column.

NEW (v3):
- Default directory prompt/loop:
    DEFAULT DIRECTORY: C:\Users\JCox\Downloads\
    USE DEFAULT? Y/N
  If N, repeatedly prompt for ENTER DIRECTORY: until a valid local directory is provided.
  Accepts quoted or unquoted Windows paths.

- File type chooser:
    1) XLS/XLSX
    2) CSV
  If 1: list .xls and .xlsx together, combine to a single XLSX output.
  If 2: list .csv and combine to CSV (same behavior as previous version).

CSV behavior remains robust to multiple encodings by full-parse attempts.

Excel reading requires pandas + engines:
  - .xlsx -> openpyxl
  - .xls  -> xlrd (1.2.0 is recommended for legacy .xls)

If a needed dependency is missing, a clear error is shown.

Based on earlier CSV combiner (v2).
"""
import os
import sys
import csv
import re
import random
import string
from pathlib import Path
from typing import List, Dict, Tuple, Set, Iterable

# Optional import for Excel; we import lazily where needed
try:
    import pandas as pd  # type: ignore
except Exception:
    pd = None  # We'll check when Excel path is chosen

RANDOM_ALPHABET = string.ascii_uppercase + string.digits
PREFERRED_ENCODINGS = ("cp1252", "utf-8-sig", "utf-8", "latin-1", "utf-16", "utf-16le", "utf-16be")

# Use raw string for Windows path to avoid Unicode escape issues
DEFAULT_DIR = r"C:\Users\JCox\Downloads"

# ------------------------ Small helpers ------------------------

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

def dir_exists(path_str: str) -> bool:
    p = Path(path_str)
    return p.exists() and p.is_dir()

def normalize_header(h: str) -> str:
    # Case-insensitive key; keep only lower/strip to avoid merging near-duplicates.
    return (h or "").strip().lower()

def generate_unique_ids(n: int) -> List[str]:
    ids: Set[str] = set()
    out: List[str] = []
    while len(out) < n:
        token = "".join(random.choices(RANDOM_ALPHABET, k=6))
        if token not in ids:
            ids.add(token)
            out.append(token)
    return out

# ------------------------ File discovery ------------------------

def list_files_by_ext(dir_path: str, exts: Tuple[str, ...]) -> List[Path]:
    p = Path(dir_path)
    if not p.exists() or not p.is_dir():
        print(f"ERROR: '{dir_path}' is not a valid directory.")
        return []
    exts_lc = tuple(e.lower() for e in exts)
    files = [f for f in sorted(p.iterdir()) if f.is_file() and f.suffix.lower() in exts_lc]
    return files

def list_csv_files(dir_path: str) -> List[Path]:
    return list_files_by_ext(dir_path, (".csv",))

def list_excel_files(dir_path: str) -> List[Path]:
    return list_files_by_ext(dir_path, (".xls", ".xlsx"))

# ------------------------ Selection UI ------------------------

def parse_selection(max_index: int) -> List[int]:
    while True:
        raw = input_nonempty("Enter the numbers of the files to combine (e.g., 1,2,3): ")
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
        unique_nums = sorted(set(nums), key=lambda x: nums.index(x))  # preserve first-seen order
        print("You selected:", ", ".join(map(str, unique_nums)))
        yn = input_nonempty("Proceed? (Y/N): ").strip().lower()
        if yn in ("y", "yes"):
            return unique_nums
        else:
            print("\nOkay, let's try again.\n")

# ------------------------ CSV parsing ------------------------

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
    raise RuntimeError(f"Failed to decode/parse '{path.name}' with tried encodings: {list(encodings)}") from last_err

def build_canonical_headers_from_csv(selected_files: List[Path]) -> Tuple[List[str], List[Tuple[Path, str, List[str], List[Dict[str, str]]]]]:
    """
    Returns:
      headers_ordered: union of headers (first file's order, then new ones appended)
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
                    headers_ordered.append(h)  # keep original casing from first occurrence
                    seen_keys.add(key)
        else:
            for h in fields:
                key = normalize_header(h)
                if key not in seen_keys:
                    headers_ordered.append(h)
                    seen_keys.add(key)
    return headers_ordered, file_info

def align_and_write_csv(output_path: Path, headers: List[str], file_info: List[Tuple[Path, str, List[str], List[Dict[str, str]]]]):
    headers_with_id = ["ID"] + headers
    key_to_canonical: Dict[str, str] = {normalize_header(h): h for h in headers}

    total_rows = sum(len(rows) for _, _, _, rows in file_info)
    ids = generate_unique_ids(total_rows)
    id_iter = iter(ids)

    with open(output_path, "w", encoding="utf-8-sig", newline="") as f_out:
        writer = csv.DictWriter(f_out, fieldnames=headers_with_id, quoting=csv.QUOTE_MINIMAL)
        writer.writeheader()
        written = 0
        for path, enc, fieldnames, rows in file_info:
            # Map this file's fields to canonical headers
            file_field_to_canonical: Dict[str, str] = {}
            for fh in fieldnames:
                key = normalize_header(fh)
                file_field_to_canonical[fh] = key_to_canonical.get(key, fh)

            for row in rows:
                out_row = {h: "" for h in headers_with_id}
                out_row["ID"] = next(id_iter)
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

# ------------------------ Excel parsing ------------------------

def ensure_pandas_for_excel():
    if pd is None:
        raise RuntimeError(
            "Excel support requires 'pandas'. Please install with:\n"
            "  pip install pandas openpyxl xlrd==1.2.0"
        )
    # engine availability is checked at read time

def try_read_excel_first_sheet(path: Path) -> Tuple[List[str], List[Dict[str, str]]]:
    """
    Read the FIRST sheet of an Excel file using pandas, returning (fieldnames, rows_as_dicts).
    Converts NaN to empty strings and coerces everything to string for uniformity.
    """
    ensure_pandas_for_excel()
    try:
        # dtype=str will best-effort coerce columns to string; some engines ignore,
        # so we coerce after read as well.
        df = pd.read_excel(path, sheet_name=0, dtype=str)
    except ImportError as e:
        # Missing engine details
        raise RuntimeError(
            f"Failed to read '{path.name}'. For .xlsx, install 'openpyxl'. "
            f"For .xls, install 'xlrd==1.2.0'.\nOriginal error: {e}"
        ) from e
    except Exception as e:
        raise RuntimeError(f"Failed to read '{path.name}': {e}") from e

    # Normalize: ensure string type and fill NaNs with ""
    df = df.astype(str).where(df.notnull(), "")
    fieldnames = list(df.columns.astype(str))
    rows = [dict(zip(fieldnames, map(lambda x: "" if x is None else str(x), row))) for row in df.to_numpy()]
    return fieldnames, rows

def build_canonical_headers_from_excel(selected_files: List[Path]) -> Tuple[List[str], List[Tuple[Path, List[str], List[Dict[str, str]]]]]:
    """
    Returns:
      headers_ordered: union headers (first file order, then appended)
      file_info: list of tuples (path, fieldnames, rows)
    """
    headers_ordered: List[str] = []
    seen_keys: Set[str] = set()
    file_info = []
    for i, path in enumerate(selected_files):
        fields, rows = try_read_excel_first_sheet(path)
        if not fields:
            print(f"WARNING: '{path.name}' has no header row; skipping.")
            continue
        file_info.append((path, fields, rows))
        if i == 0:
            for h in fields:
                key = normalize_header(h)
                if key not in seen_keys:
                    headers_ordered.append(h)
                    seen_keys.add(key)
        else:
            for h in fields:
                key = normalize_header(h)
                if key not in seen_keys:
                    headers_ordered.append(h)
                    seen_keys.add(key)
    return headers_ordered, file_info

def align_and_write_excel(output_path: Path, headers: List[str], file_info: List[Tuple[Path, List[str], List[Dict[str, str]]]]):
    """
    Write combined rows to an XLSX file with ID as the first column.
    """
    ensure_pandas_for_excel()

    headers_with_id = ["ID"] + headers
    key_to_canonical: Dict[str, str] = {normalize_header(h): h for h in headers}

    total_rows = sum(len(rows) for _, _, rows in file_info)
    ids = generate_unique_ids(total_rows)
    id_iter = iter(ids)

    # Accumulate rows in canonical order
    combined_rows: List[Dict[str, str]] = []
    for path, fieldnames, rows in file_info:
        # Map this file's fields to canonical headers
        field_to_canonical: Dict[str, str] = {}
        for fh in fieldnames:
            key = normalize_header(fh)
            field_to_canonical[fh] = key_to_canonical.get(key, fh)

        for row in rows:
            out_row = {h: "" for h in headers_with_id}
            out_row["ID"] = next(id_iter)
            for fh, val in row.items():
                if fh is None:
                    continue
                canon = field_to_canonical.get(fh)
                if canon is None:
                    continue
                out_row[canon] = "" if val is None else str(val)
            combined_rows.append(out_row)

    # Write via pandas to xlsx
    df_out = pd.DataFrame(combined_rows, columns=headers_with_id)
    try:
        # Let pandas decide engine; typically openpyxl for .xlsx
        df_out.to_excel(output_path, index=False)
    except ImportError as e:
        raise RuntimeError(
            "Writing .xlsx requires 'openpyxl'. Install with:\n"
            "  pip install openpyxl"
        ) from e

    print(f"Wrote {len(combined_rows)} rows to: {output_path}")

# ------------------------ New prompts/flow ------------------------

def choose_directory() -> str:
    r"""
    Implements:
      DEFAULT DIRECTORY: C:\Users\JCox\Downloads\
      USE DEFAULT? Y/N
    If N: loop on ENTER DIRECTORY:, verifying exists. Accepts quoted/unquoted.
    Confirm chosen directory before proceeding.
    """
    print(f"DEFAULT DIRECTORY: {DEFAULT_DIR}")
    while True:
        yn = input_nonempty("USE DEFAULT? Y/N ").strip().lower()
        if yn in ("y", "yes"):
            chosen = DEFAULT_DIR
        elif yn in ("n", "no"):
            while True:
                entered = clean_dir_path(input_nonempty("ENTER DIRECTORY: "))
                if not dir_exists(entered):
                    print("DIRECTORY DOES NOT EXIST")
                    continue
                # Confirm
                conf = input_nonempty(f"CONFIRM DIRECTORY [{entered}] Y/N ").strip().lower()
                if conf in ("y", "yes"):
                    chosen = entered
                    break
                # else loop back to ENTER DIRECTORY
            # once confirmed, break outer
        else:
            print("Please answer Y or N.")
            continue

        # If default path chosen, still confirm to mirror spec wording (not strictly required, but consistent)
        if yn in ("y", "yes"):
            conf = input_nonempty(f"CONFIRM DIRECTORY [{chosen}] Y/N ").strip().lower()
            if conf not in ("y", "yes"):
                # If not confirmed, fall back to manual entry loop
                continue
        return chosen

def choose_file_type() -> str:
    """
    Ask:
        1) XLS/XLSX
        2) CSV
    Return "excel" or "csv"
    """
    print("\n1) XLS/XLSX")
    print("2) CSV")
    while True:
        sel = input_nonempty("\nWHICH TYPE OF FILE SHOULD BE COMBINED? ").strip()
        if sel == "1":
            return "excel"
        if sel == "2":
            return "csv"
        print("Please enter 1 or 2.")

# ------------------------ Main ------------------------

def main():
    print("=== Combine CSVs or Excel by Header (Case-Insensitive) & Add Unique 6-Char ID (v3) ===")

    # Directory chooser loop
    dir_path = choose_directory()

    # File type chooser
    ftype = choose_file_type()

    if ftype == "csv":
        files = list_csv_files(dir_path)
        if not files:
            print("No CSV files found in that directory. Exiting.")
            sys.exit(1)
        print("\nCSV files found:")
        for idx, p in enumerate(files, start=1):
            print(f"{idx}. {p.name}")
        print("")
        selection = parse_selection(len(files))
        selected_files = [files[i-1] for i in selection]

        print("\nYou chose to combine these files (in this order):")
        for p in selected_files:
            print(f"- {p.name}")
        yn = input_nonempty("Final confirmation? (Y/N): ").strip().lower()
        if yn not in ("y", "yes"):
            print("\nOkay, starting over.\n")
            return main()

        headers, file_info = build_canonical_headers_from_csv(selected_files)
        if not file_info:
            print("No readable CSVs were selected. Exiting.")
            sys.exit(1)

        out_path = Path(dir_path) / "COMBINED.csv"
        align_and_write_csv(out_path, headers, file_info)
        print("\nDone!")

    else:  # ftype == "excel"
        try:
            ensure_pandas_for_excel()
        except RuntimeError as e:
            print(str(e))
            sys.exit(1)

        files = list_excel_files(dir_path)
        if not files:
            print("No XLS/XLSX files found in that directory. Exiting.")
            sys.exit(1)
        print("\nExcel files found:")
        for idx, p in enumerate(files, start=1):
            print(f"{idx}. {p.name}")
        print("")
        selection = parse_selection(len(files))
        selected_files = [files[i-1] for i in selection]

        print("\nYou chose to combine these files (in this order):")
        for p in selected_files:
            print(f"- {p.name}")
        yn = input_nonempty("Final confirmation? (Y/N): ").strip().lower()
        if yn not in ("y", "yes"):
            print("\nOkay, starting over.\n")
            return main()

        try:
            headers, file_info = build_canonical_headers_from_excel(selected_files)
        except RuntimeError as e:
            print(str(e))
            sys.exit(1)

        if not file_info:
            print("No readable Excel sheets were selected. Exiting.")
            sys.exit(1)

        out_path = Path(dir_path) / "COMBINED.xlsx"
        try:
            align_and_write_excel(out_path, headers, file_info)
        except RuntimeError as e:
            print(str(e))
            sys.exit(1)
        print("\nDone!")

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nOperation canceled by user.")
        sys.exit(1)
