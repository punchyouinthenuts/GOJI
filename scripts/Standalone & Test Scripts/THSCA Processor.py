#!/usr/bin/env python3
"""
THSCA XLSX Processor (v2 - fixes 'nan' issues)
----------------------------------------------
Key fixes:
- Normalize missing values BEFORE casting to string to avoid literal "nan" text.
- Use safe joining that treats None/NaN/"nan"/"None" as empty strings.
- Trim whitespace consistently after joins.
"""

import re
import sys
import time
from pathlib import Path

try:
    import pandas as pd
    import numpy as np
except ImportError:
    print("This script requires the 'pandas' and 'openpyxl' packages. Install with: pip install pandas openpyxl")
    sys.exit(1)


def prompt_for_file() -> Path:
    while True:
        try:
            print("INPUT FILE FOR PROCESSING: ", end="")
            raw = input().strip()
        except (EOFError, KeyboardInterrupt):
            sys.exit(1)

        # Allow quotes around the path (Windows 'Copy as path')
        if (raw.startswith('"') and raw.endswith('"')) or (raw.startswith("'") and raw.endswith("'")):
            raw = raw[1:-1]

        p = Path(raw)
        if not p.exists():
            print("Path does not exist. Please try again.")
            continue
        if p.is_dir():
            print("You provided a directory. Please provide a path to an .xlsx file.")
            continue
        if p.suffix.lower() != ".xlsx":
            print("File must be an .xlsx Excel file. Please try again.")
            continue
        return p.resolve()


def prompt_for_job_number() -> str:
    pattern = re.compile(r"^\d{5}$")
    while True:
        try:
            print("INPUT JOB NUMBER: ", end="")
            raw = input().strip()
        except (EOFError, KeyboardInterrupt):
            sys.exit(1)

        # Allow quotes around the job number as well (just in case)
        if (raw.startswith('"') and raw.endswith('"')) or (raw.startswith("'") and raw.endswith("'")):
            raw = raw[1:-1]

        if pattern.match(raw):
            return raw
        print("Job number must be exactly five digits (e.g., 12345). Please try again.")


def find_col(df: pd.DataFrame, target: str) -> str:
    """
    Finds a column name in df that matches target case-insensitively
    and ignoring leading/trailing spaces.
    Raises ValueError if not found.
    """
    tnorm = target.strip().lower()
    for col in df.columns:
        if str(col).strip().lower() == tnorm:
            return col
    raise ValueError(f"Required column '{target}' not found in the sheet.")


def sanitize_cell(x) -> str:
    """Convert cell to a clean string; treat NaN/None/'nan'/'none' as empty."""
    if x is None or (isinstance(x, float) and np.isnan(x)):
        return ""
    s = str(x).strip()
    if s.lower() in {"nan", "none", "nat"}:
        return ""
    return s


def trim_join(*parts) -> str:
    """Join string parts with a single space, skipping empties, and trim the result."""
    cleaned = [sanitize_cell(p) for p in parts if sanitize_cell(p)]
    return " ".join(cleaned).strip()


def main():
    xlsx_path = prompt_for_file()

    try:
        # Read with object dtype, keep NaN as NaN, then sanitize
        df = pd.read_excel(xlsx_path, dtype=object)
    except Exception as e:
        print(f"Failed to read Excel file: {e}")
        sys.exit(1)

    # Sanitize every cell to avoid literal 'nan' text
    df = df.applymap(sanitize_cell)

    # --- Step 2: Build FULL NAME for blanks from FIRST, MIDDLE, LAST, SUFFIX ---
    try:
        col_fullname = find_col(df, "FULL NAME")
        col_first = find_col(df, "FIRST")
        col_middle = find_col(df, "MIDDLE")
        col_last = find_col(df, "LAST")
        col_suffix = find_col(df, "SUFFIX")
    except ValueError as e:
        print(e)
        sys.exit(1)

    # Only fill where FULL NAME is blank
    mask_blank_full = df[col_fullname].str.strip().eq("")
    if mask_blank_full.any():
        df.loc[mask_blank_full, col_fullname] = [
            trim_join(f, m, l, s)
            for f, m, l, s in zip(
                df.loc[mask_blank_full, col_first],
                df.loc[mask_blank_full, col_middle],
                df.loc[mask_blank_full, col_last],
                df.loc[mask_blank_full, col_suffix],
            )
        ]

    # --- Step 3: Insert combined address column after ADDRESS LINE 2 and before CITY ---
    try:
        col_addr1 = find_col(df, "ADDRESS LINE 1")
        col_addr2 = find_col(df, "ADDRESS LINE 2")
        _ = find_col(df, "CITY")  # just to ensure it exists per spec
    except ValueError as e:
        print(e)
        sys.exit(1)

    # Combine safely (no 'nan')
    combined_series = [
        trim_join(a1, a2) for a1, a2 in zip(df[col_addr1], df[col_addr2])
    ]

    # Insert new blank-named column after ADDRESS LINE 2
    insert_idx = df.columns.get_loc(col_addr2) + 1
    df.insert(insert_idx, "", combined_series)

    # --- Step 4: Delete specified columns ---
    cols_to_drop = [col_first, col_middle, col_last, col_suffix, col_addr1, col_addr2]
    existing = [c for c in cols_to_drop if c in df.columns]
    if existing:
        df.drop(columns=existing, inplace=True)

    # --- Step 5: Rename columns ---
    # Find our blank-named column and rename to 'ADDRESS LINE 1'
    blank_cols = [c for c in df.columns if str(c).strip() == ""]
    if not blank_cols:
        print("Could not locate the combined address column to rename.")
        sys.exit(1)
    combined_col_actual = blank_cols[0]

    rename_map = {combined_col_actual: "ADDRESS LINE 1"}

    # Optional renames if present
    try:
        rename_map[find_col(df, "Individual__PrimaryOrganization__Name")] = "BUSINESS"
    except ValueError:
        pass

    try:
        rename_map[find_col(df, "ZIP")] = "ZIP CODE"
    except ValueError:
        pass

    df.rename(columns=rename_map, inplace=True)

    # --- Step 6: Save CSV ---
    job = prompt_for_job_number()
    out_name = f"{job} THSCA.csv"
    out_path = xlsx_path.parent / out_name

    try:
        df.to_csv(out_path, index=False, encoding="utf-8-sig")
    except Exception as e:
        print(f"Failed to save CSV: {e}")
        sys.exit(1)

    print("PROCESS COMPLETE! TERMINATING SCRIPT...")
    time.sleep(3.5)


if __name__ == "__main__":
    main()
