import argparse
import csv
import json
import shutil
import sys
import zipfile
from datetime import datetime
from decimal import Decimal, ROUND_HALF_UP
from pathlib import Path

MAIL_PIECE_SIZES = {
    40: {"CAN": Decimal("6.15"), "NZ": Decimal("11.55")},
    44: {"CAN": Decimal("6.65"), "NZ": Decimal("12.95")},
    48: {"CAN": Decimal("7.60"), "NZ": Decimal("15.75")},
    52: {"CAN": Decimal("7.60"), "NZ": Decimal("15.75")},
    56: {"CAN": Decimal("7.60"), "NZ": Decimal("15.75")},
    60: {"CAN": Decimal("7.60"), "NZ": Decimal("15.75")},
    64: {"CAN": Decimal("7.60"), "NZ": Decimal("15.75")},
}

INVALID_ADDRESS_FILENAME = "AIL - INVALID ADDRESS RECORDS.csv"
POPUP_DATA_FILENAME = "aili_popup_data.json"
DOMESTIC_OUTPUT_NAMES = {
    "SPOTLIGHT": "SPOTLIGHT DOMESTIC.csv",
    "AO SPOTLIGHT": "AO SPOTLIGHT DOMESTIC.csv",
}
INTERNATIONAL_INPUT_NAMES = {
    "SPOTLIGHT": "SL INTERNATIONAL.csv",
    "AO SPOTLIGHT": "AO SL INTERNATIONAL.csv",
}
INTERNATIONAL_OUTPUT_NAMES = {
    "SPOTLIGHT": "SPOTLIGHT INTERNATIONAL.csv",
    "AO SPOTLIGHT": "AO SPOTLIGHT INTERNATIONAL.csv",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", required=True, choices=["prepare", "archive"])
    parser.add_argument("--base-dir", required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--job-number", required=True)
    parser.add_argument("--issue-number", required=True)
    parser.add_argument("--page-count", required=True, type=int)
    parser.add_argument("--domestic-postage", required=True)
    parser.add_argument("--count", required=True, type=int)
    return parser.parse_args()


def normalize_version(version: str) -> str:
    normalized = " ".join(version.strip().upper().split())
    if normalized not in DOMESTIC_OUTPUT_NAMES:
        raise ValueError(f"Unsupported version: {version}")
    return normalized


def quantize_money(value: Decimal) -> Decimal:
    return value.quantize(Decimal("0.01"), rounding=ROUND_HALF_UP)


def money_string(value: Decimal) -> str:
    return f"${format(quantize_money(value), ',.2f')}"


def clean_label(value: str) -> str:
    cleaned = " ".join(value.strip().split())
    return cleaned.replace("/", "-")


def descriptor(version: str, issue_number: str) -> str:
    return f"AILI {clean_label(version)} ISSUE {clean_label(issue_number)}"


def csv_row_count(path: Path) -> int:
    with path.open("r", newline="", encoding="utf-8-sig") as handle:
        reader = csv.reader(handle)
        next(reader, None)
        return sum(1 for row in reader if any(str(value).strip() for value in row))


def csv_dict_rows(path: Path) -> list[dict[str, str]]:
    with path.open("r", newline="", encoding="utf-8-sig") as handle:
        return list(csv.DictReader(handle))


def count_domestic_breakdown(path: Path) -> tuple[int, int]:
    total = 0
    ny = 0
    for row in csv_dict_rows(path):
        if not any(str(value).strip() for value in row.values()):
            continue
        total += 1

        state_value = str(row.get("State", "")).strip().upper()
        if not state_value:
            city_state_zip = str(row.get("City State ZIP Code", "")).strip()
            if city_state_zip:
                parts = city_state_zip.split()
                if len(parts) >= 2:
                    state_value = parts[-2].strip().upper()

        if state_value == "NY":
            ny += 1
    return total, ny


def count_international_breakdown(path: Path) -> tuple[int, int]:
    can = 0
    nz = 0
    for row in csv_dict_rows(path):
        if not any(str(value).strip() for value in row.values()):
            continue
        country_value = str(row.get("Country", "")).strip().upper()
        if country_value == "CANADA":
            can += 1
        elif country_value == "NEW ZEALAND":
            nz += 1
    return can, nz


def build_paths(base_dir: Path, version: str) -> dict[str, Path]:
    input_dir = base_dir / "INPUT"
    output_dir = base_dir / "OUTPUT"
    archive_dir = base_dir / "ARCHIVE"
    original_dir = base_dir / "ORIGINAL"
    return {
        "base": base_dir,
        "input": input_dir,
        "output": output_dir,
        "archive": archive_dir,
        "original": original_dir,
        "domestic_output": output_dir / DOMESTIC_OUTPUT_NAMES[version],
        "international_input": input_dir / INTERNATIONAL_INPUT_NAMES[version],
        "international_output": output_dir / INTERNATIONAL_OUTPUT_NAMES[version],
        "invalid_output": output_dir / INVALID_ADDRESS_FILENAME,
        "popup_json": output_dir / POPUP_DATA_FILENAME,
    }


def ensure_prepare_inputs(paths: dict[str, Path], expected_count: int) -> None:
    if not paths["domestic_output"].exists():
        raise FileNotFoundError(f"Bulk Mailer domestic output file not found: {paths['domestic_output']}")
    if not paths["invalid_output"].exists():
        raise FileNotFoundError(f"Invalid address file not found: {paths['invalid_output']}")
    if not paths["international_input"].exists():
        raise FileNotFoundError(f"International input file not found: {paths['international_input']}")

    domestic_rows = csv_row_count(paths["domestic_output"])
    if domestic_rows != expected_count:
        raise ValueError(
            f"Domestic count mismatch. GOJI count={expected_count}, domestic output rows={domestic_rows}"
        )


def copy_international_to_output(paths: dict[str, Path]) -> None:
    paths["output"].mkdir(parents=True, exist_ok=True)
    shutil.copy2(paths["international_input"], paths["international_output"])


def build_table_rows(
    version: str,
    job_number: str,
    issue_number: str,
    page_count: int,
    domestic_postage: Decimal,
    count: int,
    domestic_output: Path,
    international_output: Path,
) -> list[list[str]]:
    if page_count not in MAIL_PIECE_SIZES:
        raise ValueError(f"Unsupported page count: {page_count}")
    if count <= 0:
        raise ValueError("Count must be greater than zero")

    domestic_total_rows, ny_pieces = count_domestic_breakdown(domestic_output)
    if domestic_total_rows != count:
        raise ValueError(
            f"Domestic row count changed after validation. GOJI count={count}, domestic output rows={domestic_total_rows}"
        )

    us_pieces = count - ny_pieces
    can_pieces, nz_pieces = count_international_breakdown(international_output)

    average_per_piece = domestic_postage / Decimal(count)
    us_total_postage = average_per_piece * Decimal(us_pieces)
    ny_total_postage = domestic_postage - us_total_postage

    can_per_piece = MAIL_PIECE_SIZES[page_count]["CAN"]
    nz_per_piece = MAIL_PIECE_SIZES[page_count]["NZ"]
    can_total_postage = can_per_piece * Decimal(can_pieces)
    nz_total_postage = nz_per_piece * Decimal(nz_pieces)
    international_total = can_total_postage + nz_total_postage
    combined_total = domestic_postage + international_total

    report_description = f"{descriptor(version, issue_number)}"

    return [
        [job_number, report_description, money_string(domestic_postage), str(count), money_string(average_per_piece), "STD", "FLT", "1165"],
        ["", "US", money_string(us_total_postage), str(us_pieces), money_string(average_per_piece), "STD", "FLT", "1165"],
        ["", "NY", money_string(ny_total_postage), str(ny_pieces), money_string(average_per_piece), "STD", "FLT", "1165"],
        ["", "CAN", money_string(can_total_postage), str(can_pieces), money_string(can_per_piece), "FC", "FLT", "METERED"],
        ["", "NZ", money_string(nz_total_postage), str(nz_pieces), money_string(nz_per_piece), "FC", "FLT", "METERED"],
        ["", "COMBINED TOTAL", money_string(combined_total), "", "", "", "", ""],
        ["", "INTERNATIONAL TOTAL", money_string(international_total), "", "", "", "", ""],
    ]


def write_popup_json(paths: dict[str, Path], table_rows: list[list[str]]) -> None:
    payload = {
        "table_rows": table_rows,
        "invalid_address_file": INVALID_ADDRESS_FILENAME,
    }
    paths["popup_json"].write_text(json.dumps(payload, indent=2), encoding="utf-8")


def safe_stem(text: str) -> str:
    cleaned = clean_label(text)
    allowed = []
    for char in cleaned:
        if char.isalnum() or char in {" ", "-", "_"}:
            allowed.append(char)
    return "".join(allowed).strip()


def renamed_output_name(path: Path, version: str, job_number: str, issue_number: str) -> str:
    base_descriptor = f"{safe_stem(job_number)} {safe_stem(descriptor(version, issue_number))}"
    upper_name = path.name.upper()
    suffix = path.suffix.lower()

    if upper_name == INVALID_ADDRESS_FILENAME.upper():
        return f"{base_descriptor} INVALID ADDRESS RECORDS{suffix}"

    if "DOMESTIC" in upper_name:
        return f"{base_descriptor} DOMESTIC_{csv_row_count(path)}{suffix}"

    if "INTERNATIONAL" in upper_name:
        return f"{base_descriptor} INTERNATIONAL_{csv_row_count(path)}{suffix}"

    return f"{base_descriptor} {safe_stem(path.stem)}{suffix}"


def rename_output_files(paths: dict[str, Path], version: str, job_number: str, issue_number: str) -> None:
    for path in sorted(paths["output"].iterdir()):
        if not path.is_file():
            continue
        new_name = renamed_output_name(path, version, job_number, issue_number)
        target = path.with_name(new_name)
        if target == path:
            continue
        if target.exists():
            target.unlink()
        path.rename(target)


def create_archive(paths: dict[str, Path], version: str, job_number: str, issue_number: str) -> Path:
    paths["archive"].mkdir(parents=True, exist_ok=True)
    zip_name = f"{safe_stem(job_number)} {safe_stem(descriptor(version, issue_number))}_{datetime.now().strftime('%Y%m%d')}.zip"
    zip_path = paths["archive"] / zip_name
    if zip_path.exists():
        zip_path.unlink()

    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for folder_key in ("input", "output"):
            folder = paths[folder_key]
            for item in sorted(folder.rglob("*")):
                if item.is_file():
                    archive.write(item, item.relative_to(paths["base"]))

    return zip_path


def clear_working_folders(paths: dict[str, Path]) -> None:
    for folder_key in ("original", "input", "output"):
        folder = paths[folder_key]
        if not folder.exists():
            continue
        for item in folder.iterdir():
            if item.is_dir():
                shutil.rmtree(item)
            else:
                item.unlink()


def prepare_mode(args: argparse.Namespace, version: str, paths: dict[str, Path]) -> int:
    domestic_postage = Decimal(args.domestic_postage)
    ensure_prepare_inputs(paths, args.count)
    copy_international_to_output(paths)
    table_rows = build_table_rows(
        version=version,
        job_number=args.job_number,
        issue_number=args.issue_number,
        page_count=args.page_count,
        domestic_postage=domestic_postage,
        count=args.count,
        domestic_output=paths["domestic_output"],
        international_output=paths["international_output"],
    )
    write_popup_json(paths, table_rows)
    print(f"Popup JSON written: {paths['popup_json']}")
    return 0


def archive_mode(args: argparse.Namespace, version: str, paths: dict[str, Path]) -> int:
    rename_output_files(paths, version, args.job_number, args.issue_number)
    zip_path = create_archive(paths, version, args.job_number, args.issue_number)
    clear_working_folders(paths)
    print(f"Archive ZIP created: {zip_path}")
    return 0


def main() -> int:
    args = parse_args()
    version = normalize_version(args.version)
    paths = build_paths(Path(args.base_dir), version)

    for folder_key in ("base", "input", "output", "archive", "original"):
        paths[folder_key].mkdir(parents=True, exist_ok=True)

    if args.mode == "prepare":
        return prepare_mode(args, version, paths)
    return archive_mode(args, version, paths)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        sys.exit(1)
