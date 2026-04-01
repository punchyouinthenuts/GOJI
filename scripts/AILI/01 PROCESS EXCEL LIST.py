import argparse
import sys
from pathlib import Path

import pandas as pd

DOMESTIC_COLUMN_ORDER = [
    "First Name",
    "Middle Name",
    "Last Name",
    "Name Suffix",
    "Business",
    "Address Line 1",
    "City",
    "State",
    "ZIP Code",
]

INTERNATIONAL_COLUMN_ORDER = [
    "First Name",
    "Middle Name",
    "Last Name",
    "Name Suffix",
    "Business",
    "Address Line 1",
    "Address Line 2",
    "City",
    "State",
    "ZIP Code",
    "Country",
]

DOMESTIC_MAPPINGS = {
    "First": "First Name",
    "Middle": "Middle Name",
    "Last": "Last Name",
    "Suffix": "Name Suffix",
    "SGA Agency Office": "Business",
    "Address": "Address Line 1",
    "City": "City",
    "State": "State",
    "Zip": "ZIP Code",
}

CANADA_MAPPINGS = {
    "First": "First Name",
    "Middle": "Middle Name",
    "Last": "Last Name",
    "Suffix": "Name Suffix",
    "SGA Agency Office": "Business",
    "Address": "Address Line 1",
    "Address 2": "Address Line 2",
    "City": "City",
    "Prov": "State",
    "Postal Code": "ZIP Code",
}

NEW_ZEALAND_MAPPINGS = {
    "First": "First Name",
    "Middle": "Middle Name",
    "Last": "Last Name",
    "Suffix": "Name Suffix",
    "SGA Agency Office": "Business",
    "Address": "Address Line 1",
    "Address 2": "Address Line 2",
    "Suburb": "City",
    "City": "State",
    "Postal Code": "ZIP Code",
}

OUTPUT_NAMES = {
    "SPOTLIGHT": {
        "domestic": "SL DOMESTIC.csv",
        "international": "SL INTERNATIONAL.csv",
    },
    "AO SPOTLIGHT": {
        "domestic": "AO SL DOMESTIC.csv",
        "international": "AO SL INTERNATIONAL.csv",
    },
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--base-dir", required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--source-file", required=True)
    return parser.parse_args()


def resolve_source_file(base_dir: Path, source_file_arg: str) -> Path:
    source_path = Path(source_file_arg)
    if source_path.is_absolute():
        return source_path
    return base_dir / "ORIGINAL" / source_path


def normalize_version(version: str) -> str:
    normalized = " ".join(version.strip().upper().split())
    if normalized not in OUTPUT_NAMES:
        raise ValueError(f"Unsupported version: {version}")
    return normalized


def find_sheet_mapping(excel_file: Path) -> dict[str, str | None]:
    sheet_names = pd.ExcelFile(excel_file).sheet_names
    return {
        "us": next((s for s in sheet_names if "US" in s.upper() and "NEW YORK" not in s.upper()), None),
        "ny": next((s for s in sheet_names if "NY" in s.upper() or "NEW YORK" in s.upper()), None),
        "can": next((s for s in sheet_names if "CA" in s.upper() or "CAN" in s.upper() or "CANADA" in s.upper()), None),
        "nz": next((s for s in sheet_names if "NZ" in s.upper() or "NEW ZEALAND" in s.upper()), None),
    }


def find_header_row(df: pd.DataFrame) -> int | None:
    for index, row in df.iterrows():
        first_value = "" if row.empty else str(row.iloc[0]).strip()
        if first_value == "First":
            return int(index)
    return None


def process_sheet(
    excel_file: Path,
    sheet_name: str | None,
    mappings: dict[str, str],
    column_order: list[str],
    default_country: str = "",
) -> pd.DataFrame:
    if not sheet_name:
        return pd.DataFrame(columns=column_order)

    df = pd.read_excel(excel_file, sheet_name=sheet_name, header=None)
    header_row = find_header_row(df)
    if header_row is None:
        return pd.DataFrame(columns=column_order)

    df.columns = ["" if pd.isna(value) else str(value).strip() for value in df.iloc[header_row]]
    df = df.iloc[header_row + 1 :].reset_index(drop=True)
    df = df.rename(columns=mappings)

    for column in column_order:
        if column not in df.columns:
            if column == "Country" and default_country:
                df[column] = default_country
            else:
                df[column] = ""

    if "Country" in df.columns and default_country:
        df["Country"] = df["Country"].fillna("").replace("", default_country)

    result = df[column_order].copy()
    result = result.fillna("")
    result = result[result.apply(lambda row: any(str(value).strip() for value in row), axis=1)].reset_index(drop=True)
    return result


def write_csv(df: pd.DataFrame, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    df.to_csv(path, index=False)


def main() -> int:
    args = parse_args()
    base_dir = Path(args.base_dir)
    input_dir = base_dir / "INPUT"
    version = normalize_version(args.version)
    source_file = resolve_source_file(base_dir, args.source_file)

    if not source_file.exists():
        raise FileNotFoundError(f"Source Excel file not found: {source_file}")

    sheet_mapping = find_sheet_mapping(source_file)

    us_df = process_sheet(source_file, sheet_mapping["us"], DOMESTIC_MAPPINGS, DOMESTIC_COLUMN_ORDER)
    ny_df = process_sheet(source_file, sheet_mapping["ny"], DOMESTIC_MAPPINGS, DOMESTIC_COLUMN_ORDER)
    domestic_df = pd.concat([us_df, ny_df], ignore_index=True)

    can_df = process_sheet(
        source_file,
        sheet_mapping["can"],
        CANADA_MAPPINGS,
        INTERNATIONAL_COLUMN_ORDER,
        default_country="CANADA",
    )
    nz_df = process_sheet(
        source_file,
        sheet_mapping["nz"],
        NEW_ZEALAND_MAPPINGS,
        INTERNATIONAL_COLUMN_ORDER,
        default_country="NEW ZEALAND",
    )
    international_df = pd.concat([can_df, nz_df], ignore_index=True)

    domestic_output = input_dir / OUTPUT_NAMES[version]["domestic"]
    international_output = input_dir / OUTPUT_NAMES[version]["international"]

    write_csv(domestic_df, domestic_output)
    write_csv(international_df, international_output)

    print(f"Source file: {source_file}")
    print(f"Domestic CSV written: {domestic_output} ({len(domestic_df)} records)")
    print(f"International CSV written: {international_output} ({len(international_df)} records)")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        sys.exit(1)