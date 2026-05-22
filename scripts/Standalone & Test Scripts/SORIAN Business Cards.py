# SORIAN Business Cards Cleaner - v2
# Updates: two-space pipes, M/O prefixes, mobile first, dash-to-period phone formatting.
import os
import re
import sys
import traceback
from pathlib import Path

import pandas as pd

REQUIRED_COLUMNS = [
    "Name",
    "Title",
    "Address",
    "Work Phone #",
    "Mobile Phone #",
    "Email",
    "With UV ",
    "Potentially with UV",
    "Where to Ship?",
    "Shipping Location",
]

STATE_ZIP_RE = re.compile(r"^([A-Z]{2})\s+(\d{5}(?:-\d{4})?)$", re.IGNORECASE)
FULL_ADDRESS_RE = re.compile(
    r"^(?P<street>.+?),\s*(?P<city>[^,]+?),\s*(?P<state>[A-Z]{2})\s+(?P<zip>\d{5}(?:-\d{4})?)$",
    re.IGNORECASE,
)


def wait_for_quit() -> None:
    print("\nPRESS Q TO QUIT...")
    while True:
        value = input().strip().lower()
        if value == "q":
            sys.exit(1)


def wait_for_exit() -> None:
    print("\nPRESS X TO EXIT...")
    while True:
        value = input().strip().lower()
        if value == "x":
            sys.exit(0)


def clean_path(raw_path: str) -> Path:
    cleaned = raw_path.strip().strip('"').strip("'")
    return Path(cleaned).expanduser().resolve()


def blank_to_empty(value) -> str:
    if pd.isna(value):
        return ""
    text = str(value).strip()
    if text.lower() in {"nan", "none", "null"}:
        return ""
    return text


def combine_nonblank(*values: object, sep: str = "  |  ") -> str:
    parts = [blank_to_empty(value) for value in values]
    parts = [part for part in parts if part]
    return sep.join(parts)




def format_phone_number(value: object, prefix: str) -> str:
    """Normalize a phone number and prepend its source prefix.

    Examples:
    Work: 727-348-9876 -> O: 727.348.9876
    Mobile: 512-969-3214 -> M: 512.969.3214
    """
    text = blank_to_empty(value)
    if not text:
        return ""

    # Remove existing source labels if the input file already contains them.
    text = re.sub(r"^\s*[OM]\s*:\s*", "", text, flags=re.IGNORECASE).strip()

    # Standardize common separators to periods while preserving extensions/other text.
    text = re.sub(r"(?<=\d)[\s\-\.]+(?=\d)", ".", text)

    # Handle common US 10-digit / 11-digit phone strings, including parentheses.
    digits = re.sub(r"\D", "", text)
    if len(digits) == 10:
        text = f"{digits[0:3]}.{digits[3:6]}.{digits[6:10]}"
    elif len(digits) == 11 and digits.startswith("1"):
        text = f"1.{digits[1:4]}.{digits[4:7]}.{digits[7:11]}"

    return f"{prefix}: {text}"


def combine_phone_numbers(work_phone: object, mobile_phone: object) -> str:
    # Mobile should come first when both are present.
    mobile = format_phone_number(mobile_phone, "M")
    work = format_phone_number(work_phone, "O")
    return combine_nonblank(mobile, work)

def split_address(address: object) -> tuple[str, str]:
    text = blank_to_empty(address)
    if not text:
        return "", ""

    # Normal expected format:
    # Street Address, Optional Suite/Floor/etc., City, ST ZIP
    pieces = [part.strip() for part in text.split(",") if part.strip()]

    for index in range(len(pieces) - 1, 0, -1):
        state_zip_match = STATE_ZIP_RE.match(pieces[index])
        if state_zip_match:
            city = pieces[index - 1]
            street = ", ".join(pieces[: index - 1])
            state = state_zip_match.group(1).upper()
            zip_code = state_zip_match.group(2)
            return street, f"{city}, {state} {zip_code}"

    # Fallback for addresses that still end in: ..., City, ST ZIP
    full_match = FULL_ADDRESS_RE.match(text)
    if full_match:
        street = full_match.group("street").strip()
        city = full_match.group("city").strip()
        state = full_match.group("state").upper()
        zip_code = full_match.group("zip")
        return street, f"{city}, {state} {zip_code}"

    # If the address cannot be confidently split, keep it instead of destroying data.
    return text, ""


def make_output_path(input_path: Path) -> Path:
    return input_path.with_name(f"{input_path.stem}_CLEAN.csv")


def normalize_header(value: object) -> str:
    return blank_to_empty(value).strip().lower()


def find_header_row(raw_df: pd.DataFrame) -> int:
    required_for_detection = {"name", "title", "address", "work phone #", "mobile phone #", "email"}

    for row_index in range(min(len(raw_df), 50)):
        row_values = {normalize_header(value) for value in raw_df.iloc[row_index].tolist()}
        if required_for_detection.issubset(row_values):
            return row_index

    raise ValueError(
        "Could not find the header row. Expected a row containing at least: "
        "Name, Title, Address, Work Phone #, Mobile Phone #, Email"
    )


def finalize_header(raw_df: pd.DataFrame) -> pd.DataFrame:
    header_row = find_header_row(raw_df)
    headers = [blank_to_empty(value) for value in raw_df.iloc[header_row].tolist()]
    df = raw_df.iloc[header_row + 1:].copy()
    df.columns = headers

    # Drop fully empty rows and fully empty columns caused by template spacing.
    df = df.replace(r"^\s*$", "", regex=True)
    df = df.dropna(how="all")
    df = df.loc[:, [blank_to_empty(column) != "" for column in df.columns]]
    df = df.fillna("")
    return df


def read_input_file(input_path: Path) -> pd.DataFrame:
    suffix = input_path.suffix.lower()

    if suffix == ".csv":
        raw_df = pd.read_csv(input_path, dtype=str, keep_default_na=False, header=None)
        return finalize_header(raw_df)

    if suffix == ".xlsx":
        raw_df = pd.read_excel(input_path, dtype=str, keep_default_na=False, header=None, engine="openpyxl")
        return finalize_header(raw_df)

    if suffix == ".xls":
        try:
            raw_df = pd.read_excel(input_path, dtype=str, keep_default_na=False, header=None, engine="xlrd")
            return finalize_header(raw_df)
        except ImportError as exc:
            raise RuntimeError(
                "This script can read .xls files only when the optional 'xlrd' package is installed. "
                "Install it with: pip install xlrd"
            ) from exc

    raise ValueError("Input file must be an XLSX, XLS, or CSV file.")


def validate_columns(df: pd.DataFrame) -> None:
    # Rename headers by trimmed text so 'With UV' and 'With UV ' both work.
    rename_map = {}
    trimmed_lookup = {str(column).strip().lower(): column for column in df.columns}

    for required_column in REQUIRED_COLUMNS:
        key = required_column.strip().lower()
        if key in trimmed_lookup and trimmed_lookup[key] != required_column:
            rename_map[trimmed_lookup[key]] = required_column

    if rename_map:
        df.rename(columns=rename_map, inplace=True)

    missing = [column for column in REQUIRED_COLUMNS if column not in df.columns]
    if missing:
        raise ValueError(
            "Missing required column(s): " + ", ".join(repr(column) for column in missing)
        )


def process_file(input_path: Path) -> Path:
    if not input_path.exists():
        raise FileNotFoundError(f"File not found: {input_path}")

    df = read_input_file(input_path)
    validate_columns(df)

    address_index = list(df.columns).index("Address")
    phone_index = list(df.columns).index("Work Phone #")

    split_values = df["Address"].apply(split_address)
    df["Street Address"] = split_values.apply(lambda item: item[0])
    df["City State ZIP"] = split_values.apply(lambda item: item[1])
    df["Address Line"] = df.apply(
        lambda row: combine_nonblank(row["Street Address"], row["City State ZIP"]), axis=1
    )

    df["Phone #"] = df.apply(
        lambda row: combine_phone_numbers(row["Work Phone #"], row["Mobile Phone #"]), axis=1
    )

    columns = list(df.columns)

    # Remove original and temporary columns.
    remove_columns = {"Address", "Work Phone #", "Mobile Phone #", "Street Address", "City State ZIP"}
    columns = [column for column in columns if column not in remove_columns and column not in {"Address Line", "Phone #"}]

    # Insert final columns where the original data lived.
    # Insert Address Line where Address used to be.
    address_insert_index = min(address_index, len(columns))
    columns.insert(address_insert_index, "Address Line")

    # Insert Phone # where Work Phone # used to be, adjusted after Address replacement/removal.
    phone_insert_index = min(phone_index - 1, len(columns))
    if phone_insert_index < 0:
        phone_insert_index = len(columns)
    columns.insert(phone_insert_index, "Phone #")

    df = df[columns]

    output_path = make_output_path(input_path)
    df.to_csv(output_path, index=False, encoding="utf-8-sig")
    return output_path


def main() -> None:
    try:
        print("Enter the XLSX, XLS, or CSV file path to process:")
        input_path = clean_path(input("> "))
        output_path = process_file(input_path)
        print("\nSUCCESS: File processed successfully.")
        print(f"Saved cleaned CSV to: {output_path}")
        wait_for_exit()
    except Exception as exc:
        print("\nERROR: The file could not be processed.")
        print(f"Cause: {exc}")
        print("\nTechnical details:")
        traceback.print_exc()
        wait_for_quit()


if __name__ == "__main__":
    main()
