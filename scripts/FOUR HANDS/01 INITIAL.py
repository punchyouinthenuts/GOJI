import os
import sys
import glob
import pandas as pd
import tempfile
import shutil
import time
import traceback
import re
from pathlib import Path

# === INITIALIZING ===
print("=== INITIALIZING ===")

# --- Define GOJI paths ---
BASE_DIR = Path(r"C:\Goji\AUTOMATION\FOUR HANDS")
SOURCE_DIR = BASE_DIR / "ORIGINAL"

RES_INPUT_DIR = BASE_DIR / "RESIDENTIAL" / "INPUT"
HOSP_INPUT_DIR = BASE_DIR / "HOSPITALITY" / "INPUT"

RES_OUTPUT_FILE = RES_INPUT_DIR / "INPUT.csv"
HOSP_OUTPUT_FILE = HOSP_INPUT_DIR / "INPUT.csv"

# --- Ensure directories exist ---
SOURCE_DIR.mkdir(parents=True, exist_ok=True)
RES_INPUT_DIR.mkdir(parents=True, exist_ok=True)
HOSP_INPUT_DIR.mkdir(parents=True, exist_ok=True)

# --- Sheet names ---
SHEET_RESIDENTIAL = "Residential"
SHEET_COMMERCIAL = "Commercial"
SHEET_NEW_ADDRESSES = "New Addresses"  # optional

# --- Constants ---
COLUMNS_TO_REMOVE = [
    "Account Source",
    "Account Owner",
    "Contact Owner",
    "Account: Created Date↑",
    "Account: Created Date",
    "Customer Status",
    "Social Profile: Instagram"
]

COLUMN_RENAME = {
    "Account Name": "Business",
    "Billing Address Line 1": "Address Line 1",
    "Billing City": "City",
    "Billing State/Province": "State",
    "Billing Zip/Postal Code": "ZIP Code",
    "Mailing Country": "Country"
}

TEXT_ONLY_STATE_COL = "Billing State/Province (text only)"
TEXT_ONLY_COUNTRY_COL = "Mailing Country (text only)"

def is_valid_customer_number(value):
    try:
        str_value = str(value).strip()
        if str_value.lower() in ['nan', '']:
            return False
        float_value = float(str_value)
        return float_value.is_integer()
    except (ValueError, TypeError):
        return False

def is_valid_value(value):
    value = str(value).strip()
    return value not in ['', '-', '.', ',', 'nan', 'None']

def handle_text_only_column(df, main_col, text_only_col):
    has_main = main_col in df.columns
    has_text_only = text_only_col in df.columns
    if has_text_only:
        if not has_main:
            df = df.rename(columns={text_only_col: main_col})
            invalid_mask = ~df[main_col].apply(is_valid_value)
            df.loc[invalid_mask, main_col] = ''
        else:
            df[main_col] = df[main_col].astype(str).str.strip()
            df[text_only_col] = df[text_only_col].astype(str).str.strip()
            blank_main_mask = df[main_col].isin(['', 'nan', 'None'])
            valid_text_only_mask = df[text_only_col].apply(is_valid_value)
            rows_to_update = blank_main_mask & valid_text_only_mask
            if rows_to_update.sum() > 0:
                print(f"Transferring {rows_to_update.sum()} values from {text_only_col} → {main_col}")
            df.loc[rows_to_update, main_col] = df.loc[rows_to_update, text_only_col]
        df = df.drop(columns=[text_only_col], errors='ignore')
    return df

def cleanup_temp_dir(temp_dir):
    if temp_dir and os.path.exists(temp_dir):
        shutil.rmtree(temp_dir, ignore_errors=True)

def _normalize_sheet_name(s: str) -> str:
    return "".join(ch.lower() for ch in str(s) if ch.isalnum())

def resolve_sheet_name(xlsx_file: str, desired_name: str):
    """
    Matches desired sheet name using:
      1) exact
      2) case-insensitive
      3) normalized (remove spaces/punct)
      4) fuzzy token containment
    """
    try:
        import openpyxl
        wb = openpyxl.load_workbook(xlsx_file, read_only=True, data_only=True)
        sheets = wb.sheetnames
    except Exception:
        return desired_name

    if desired_name in sheets:
        return desired_name

    desired_lower = desired_name.lower()
    for s in sheets:
        if s.lower() == desired_lower:
            return s

    desired_norm = _normalize_sheet_name(desired_name)
    for s in sheets:
        if _normalize_sheet_name(s) == desired_norm:
            return s

    tokens = [t for t in re.split(r"[^A-Za-z0-9]+", desired_name.lower()) if t]
    for s in sheets:
        sl = s.lower()
        if all(t in sl for t in tokens):
            return s

    return None

def read_and_normalize_sheet(xlsx_file: str, sheet_name: str) -> pd.DataFrame:
    df_raw = pd.read_excel(xlsx_file, engine='openpyxl', sheet_name=sheet_name, header=None)

    header_row = None
    for idx, row in df_raw.iterrows():
        row_values = [str(val).strip() for val in row if pd.notna(val)]
        if 'Customer Number' in row_values:
            header_row = idx
            break

    if header_row is None:
        raise ValueError(f"'Customer Number' header not found in sheet '{sheet_name}' of {xlsx_file}")

    df = df_raw.loc[header_row:].dropna(how='all').reset_index(drop=True)
    df.columns = df.iloc[0]
    df = df.drop(0).reset_index(drop=True)
    df.columns = df.columns.astype(str).str.strip()

    if 'Customer Number' not in df.columns:
        raise ValueError(f"Column 'Customer Number' not found after header normalization in {xlsx_file} ({sheet_name})")

    df = df[df['Customer Number'].apply(is_valid_customer_number)]
    df = df.rename(columns=COLUMN_RENAME)

    df = handle_text_only_column(df, 'State', TEXT_ONLY_STATE_COL)
    df = handle_text_only_column(df, 'Country', TEXT_ONLY_COUNTRY_COL)

    df = df.drop(columns=[c for c in COLUMNS_TO_REMOVE if c in df.columns], errors='ignore')

    for col in df.select_dtypes(include=['object']).columns:
        df[col] = df[col].astype(str).str.encode('utf-8', 'ignore').str.decode('utf-8', 'ignore')

    df = df.dropna(how='all')
    return df

def main():
    temp_dir = tempfile.mkdtemp()
    try:
        print("=== READING FILES ===")
        xlsx_files = glob.glob(str(SOURCE_DIR / "*.xlsx"))
        if not xlsx_files:
            print("ERROR: No XLSX files found in ORIGINAL folder.")
            sys.exit(1)

        residential_dfs = []
        hospitality_dfs = []

        for xlsx_file in xlsx_files:
            try:
                print(f"Reading: {os.path.basename(xlsx_file)}")

                # --- Residential (required) ---
                res_sheet = resolve_sheet_name(xlsx_file, SHEET_RESIDENTIAL)
                if not res_sheet:
                    raise ValueError(f"Worksheet '{SHEET_RESIDENTIAL}' not found in {xlsx_file}")
                residential_dfs.append(read_and_normalize_sheet(xlsx_file, res_sheet))

                # --- Commercial (required for Hospitality baseline) ---
                com_sheet = resolve_sheet_name(xlsx_file, SHEET_COMMERCIAL)
                if not com_sheet:
                    raise ValueError(f"Worksheet '{SHEET_COMMERCIAL}' not found in {xlsx_file}")
                hospitality_dfs.append(read_and_normalize_sheet(xlsx_file, com_sheet))

                # --- New Addresses (optional; add to Hospitality) ---
                new_sheet = resolve_sheet_name(xlsx_file, SHEET_NEW_ADDRESSES)
                if not new_sheet:
                    print(f"WARNING: Sheet '{SHEET_NEW_ADDRESSES}' not found in {os.path.basename(xlsx_file)}; Hospitality will use Commercial only.")
                else:
                    hospitality_dfs.append(read_and_normalize_sheet(xlsx_file, new_sheet))

            except Exception as e:
                print(f"ERROR processing {xlsx_file}: {e}")
                traceback.print_exc()
                cleanup_temp_dir(temp_dir)
                sys.exit(1)

        if not residential_dfs:
            print("ERROR: No valid Residential data created.")
            cleanup_temp_dir(temp_dir)
            sys.exit(1)

        if not hospitality_dfs:
            print("ERROR: No valid Hospitality data created (Commercial missing?).")
            cleanup_temp_dir(temp_dir)
            sys.exit(1)

        print("=== MERGING FILES ===")
        combined_res = pd.concat(residential_dfs, ignore_index=True).dropna(how='all')
        combined_hosp = pd.concat(hospitality_dfs, ignore_index=True).dropna(how='all')

        print("=== SAVING OUTPUTS ===")
        combined_res.to_csv(RES_OUTPUT_FILE, index=False, encoding='utf-8-sig')
        combined_hosp.to_csv(HOSP_OUTPUT_FILE, index=False, encoding='utf-8-sig')

        cleanup_temp_dir(temp_dir)

        print("=== COMPLETE ===")

        # Keep GOJI-compatible single path marker; point to the FH root.
        print("=== NAS_FOLDER_PATH ===")
        print(str(BASE_DIR))
        print("=== END_NAS_FOLDER_PATH ===")

        # Extra info (safe if GOJI ignores it)
        print(f"RESIDENTIAL OUTPUT: {RES_OUTPUT_FILE}")
        print(f"HOSPITALITY OUTPUT: {HOSP_OUTPUT_FILE}")

        print("PROCESS COMPLETE")
        sys.exit(0)

    except Exception as e:
        print("FATAL ERROR:", str(e))
        traceback.print_exc()
        cleanup_temp_dir(temp_dir)
        sys.exit(1)

if __name__ == "__main__":
    main()