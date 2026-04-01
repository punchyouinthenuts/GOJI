import os
import re
import sys
import pandas as pd
from iso3166 import countries

# ---------- helpers ----------

def normalize_source_col(s: str) -> str:
    if s is None:
        return ""
    s = str(s).rstrip(" \t")
    return s.lower()

# mapping: source -> destination
SRC_TO_DST = {
    "shipping_first_name": "First Name",
    "shipping_last_name": "Last Name",
    "shipping_address_1": "Address Line 1",
    "shipping_address_2": "Address Line 2",
    "shipping_postcode": "ZIP Code",
    "shipping_city": "City",
    "shipping_state": "State",
    "shipping_country": "Country",
    "shipping_company": "Business",
}
SRC_KEYS = {k.lower(): v for k, v in SRC_TO_DST.items()}

def alpha2_to_country_upper(code: str):
    if not isinstance(code, str):
        return None
    c = code.strip().upper()
    if c == "US":
        return pd.NA  # blank out US
    if not re.fullmatch(r"[A-Z]{2}", c):
        return None
    try:
        return countries.get(c).name.upper()
    except Exception:
        return None

def prompt(msg: str) -> str:
    try:
        return input(msg)
    except EOFError:
        return ""

def wait_for_exit():
    while True:
        v = input("FILE SUCCESSFULLY PROCESSED! PRESS X TO TERMINATE... ").strip().lower()
        if v == "x":
            break

def show_summary(df: pd.DataFrame):
    """Print summary of counts by country + domestic, then a blank line."""
    country_col = None
    if "Country" in df.columns:
        country_col = "Country"
    else:
        col_lookup = {normalize_source_col(c): c for c in df.columns}
        if "shipping_country" in col_lookup:
            country_col = col_lookup["shipping_country"]

    if country_col is None:
        print("WARNING: Country column not found; summary skipped.\n")
        return

    col = df[country_col]

    # Domestic = blank OR "PUERTO RICO"
    is_blank = col.isna() | (col.astype(str).str.strip() == "")
    is_pr = col.astype(str).str.strip().str.upper().eq("PUERTO RICO")
    domestic_count = (is_blank | is_pr).sum()

    # Non-PR, non-blank countries
    mask_foreign = ~(is_blank | is_pr)
    foreign_counts = (
        col[mask_foreign]
        .astype(str).str.strip().str.upper()
        .value_counts(dropna=True)
        .sort_values(ascending=False)
    )

    total = domestic_count + int(foreign_counts.sum())

    print()
    print(f"DOMESTIC: {domestic_count}")
    for country, cnt in foreign_counts.items():
        print(f"{country}: {cnt}")
    print(f"TOTAL: {total}")
    print()  # blank line

# ---------- main ----------

def main():
    path = prompt("INPUT FILE LOCATION: ").strip()
    if (path.startswith('"') and path.endswith('"')) or (path.startswith("'") and path.endswith("'")):
        path = path[1:-1]

    if not os.path.isfile(path):
        print("ERROR: File not found. Please check the path.")
        sys.exit(1)

    # Determine file type
    ext = os.path.splitext(path)[1].lower()

    try:
        if ext in [".xlsx", ".xls"]:
            df = pd.read_excel(path, engine="openpyxl")
        elif ext == ".csv":
            # Read as string to avoid ZIP codes or country codes being misinterpreted
            df = pd.read_csv(path, dtype=str)
        else:
            print(f"ERROR: Unsupported file type '{ext}'. Please use .xlsx, .xls, or .csv")
            sys.exit(1)
    except Exception as e:
        print(f"ERROR: Could not read file: {e}")
        sys.exit(1)

    # --------- Country processing ---------
    src_country_key = "shipping_country"
    col_lookup = {normalize_source_col(c): c for c in df.columns}
    if src_country_key in col_lookup:
        actual_country_col = col_lookup[src_country_key]

        def transform_country(v):
            if pd.isna(v):
                return v
            if isinstance(v, str) and v.strip().upper() == "US":
                return pd.NA
            conv = alpha2_to_country_upper(v)
            return conv if conv is not None else v

        df[actual_country_col] = df[actual_country_col].apply(transform_country)

    # --------- Column renaming ---------
    rename_map = {}
    for original_col in df.columns:
        key = normalize_source_col(original_col)
        if key in SRC_KEYS:
            rename_map[original_col] = SRC_KEYS[key]
    df.rename(columns=rename_map, inplace=True)

    # --------- Job number ---------
    while True:
        job = prompt("INPUT JOB NUMBER: ").strip()
        if re.fullmatch(r"\d{5}", job):
            break
        print("Invalid job number. Enter exactly five digits (e.g., 12345).")

    # --------- Output ---------
    out_dir = os.path.dirname(path)
    out_name = f"{job} THE DARK REPORT.csv"
    out_path = os.path.join(out_dir, out_name)
    try:
        df.to_csv(out_path, index=False, encoding="utf-8-sig")
        print(f"Saved: {out_path}")
    except Exception as e:
        print(f"ERROR: Could not save CSV: {e}")
        sys.exit(1)

    # --------- Summary ---------
    show_summary(df)

    # --------- Exit ---------
    wait_for_exit()

if __name__ == "__main__":
    main()
