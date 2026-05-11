import argparse
import json
import os
import re
import sys

import pandas as pd
from iso3166 import countries


def normalize_source_col(value: str) -> str:
    if value is None:
        return ""
    return str(value).rstrip(" \t").lower()


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


class ProcessingError(Exception):
    pass


def alpha2_to_country_upper(code: str):
    if not isinstance(code, str):
        return None
    normalized = code.strip().upper()
    if normalized == "US":
        return pd.NA
    if not re.fullmatch(r"[A-Z]{2}", normalized):
        return None
    try:
        return countries.get(normalized).name.upper()
    except Exception:
        return None


def read_input_file(path: str) -> pd.DataFrame:
    ext = os.path.splitext(path)[1].lower()
    try:
        if ext in [".xlsx", ".xls"]:
            return pd.read_excel(path, engine="openpyxl")
        if ext == ".csv":
            return pd.read_csv(path, dtype=str)
    except Exception as exc:
        raise ProcessingError(f"Could not read file: {exc}") from exc

    raise ProcessingError(f"Unsupported file type '{ext}'. Use .xlsx, .xls, or .csv")


def transform_country_values(df: pd.DataFrame):
    source_country_key = "shipping_country"
    col_lookup = {normalize_source_col(c): c for c in df.columns}
    actual_country_col = col_lookup.get(source_country_key)
    if not actual_country_col:
        return None

    def transform_country(value):
        if pd.isna(value):
            return value
        if isinstance(value, str) and value.strip().upper() == "US":
            return pd.NA
        converted = alpha2_to_country_upper(value)
        return converted if converted is not None else value

    df[actual_country_col] = df[actual_country_col].apply(transform_country)
    return actual_country_col


def rename_columns(df: pd.DataFrame):
    rename_map = {}
    for original_col in df.columns:
        key = normalize_source_col(original_col)
        if key in SRC_KEYS:
            rename_map[original_col] = SRC_KEYS[key]
    df.rename(columns=rename_map, inplace=True)


def calculate_counts(df: pd.DataFrame):
    country_col = None
    if "Country" in df.columns:
        country_col = "Country"
    else:
        lookup = {normalize_source_col(c): c for c in df.columns}
        country_col = lookup.get("shipping_country")

    if country_col is None:
        total = int(len(df.index))
        return total, 0, total

    col = df[country_col]
    normalized = col.astype(str).str.strip()

    is_blank = col.isna() | normalized.eq("")
    is_pr = normalized.str.upper().eq("PUERTO RICO")
    domestic_count = int((is_blank | is_pr).sum())

    international_mask = ~(is_blank | is_pr)
    international_count = int(international_mask.sum())
    total_count = domestic_count + international_count

    return domestic_count, international_count, total_count


def process_dark_report(input_file: str, job_number: str):
    if not os.path.isfile(input_file):
        raise ProcessingError("File not found. Please check the selected path.")

    if not re.fullmatch(r"\d{5}", job_number):
        raise ProcessingError("Job number must be exactly five digits.")

    df = read_input_file(input_file)
    transform_country_values(df)
    rename_columns(df)

    output_dir = os.path.dirname(input_file)
    output_name = f"{job_number} THE DARK REPORT.csv"
    output_path = os.path.join(output_dir, output_name)

    try:
        df.to_csv(output_path, index=False, encoding="utf-8-sig")
    except Exception as exc:
        raise ProcessingError(f"Could not save CSV: {exc}") from exc

    domestic_count, international_count, total_count = calculate_counts(df)

    return {
        "ok": True,
        "job_number": job_number,
        "input_file": input_file,
        "output_file": output_path,
        "domestic_count": domestic_count,
        "international_count": international_count,
        "total_count": total_count,
    }


def parse_args(argv):
    parser = argparse.ArgumentParser(description="Process THE DARK REPORT data file.")
    parser.add_argument("--input-file", required=True, help="Path to the input CSV/XLS/XLSX file.")
    parser.add_argument("--job-number", required=True, help="Five-digit job number.")
    parser.add_argument("--json", action="store_true", help="Emit JSON output.")
    return parser.parse_args(argv)


def main(argv=None):
    if argv is None:
        argv = sys.argv[1:]

    try:
        args = parse_args(argv)
        result = process_dark_report(args.input_file, args.job_number)
        if args.json:
            print(json.dumps(result))
        else:
            print(f"Saved: {result['output_file']}")
            print(f"Domestic: {result['domestic_count']}")
            print(f"International: {result['international_count']}")
            print(f"Total: {result['total_count']}")
        return 0
    except ProcessingError as exc:
        payload = {"ok": False, "error": str(exc)}
        print(json.dumps(payload))
        return 1
    except Exception as exc:
        payload = {"ok": False, "error": f"Unexpected error: {exc}"}
        print(json.dumps(payload))
        return 1


if __name__ == "__main__":
    sys.exit(main())
