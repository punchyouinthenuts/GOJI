#!/usr/bin/env python3
"""Create the durable TMMA MATCHID mapping and Bulk Mailer INPUT.csv."""

from __future__ import annotations

import argparse
import csv
import hashlib
import os
import random
import re
import shutil
import string
import sys
import tempfile
from pathlib import Path

import pandas as pd


DEFAULT_BASE_PATH = Path(r"C:/Goji/AUTOMATION/TRACHMAR/MA")
SUPPORTED_EXTENSIONS = {".xlsx", ".xls", ".csv"}
REQUIRED_HEADERS = [
    "Provider First",
    "Provider Last",
    "Location Address 1",
    "Location Address 2",
    "Location City",
    "Location State",
    "Location Zip",
    "Payee Name",
]
GENERATED_HEADERS = [
    "MATCHID",
    "mailed",
    "Current Address Line 1",
    "Current Address Line 2",
    "Current City",
    "Current State",
    "Current ZIP Code",
]
INPUT_HEADERS = [
    "MATCHID",
    "Address Line 1",
    "Address Line 2",
    "City",
    "State",
    "ZIP Code",
    "Business",
    "Addressee",
]
DOWNSTREAM_EXPORTS = ["MOVE UPDATES.csv", "NOT MAILED.csv", "OUTPUT.csv"]
MATCHID_PATTERN = re.compile(r"^[A-Z]{2}[0-9]{4}$")
MAX_PER_PREFIX = 9999
TOTAL_CAPACITY = 26 * 26 * MAX_PER_PREFIX
READ_ENCODINGS = ("utf-8-sig", "utf-8", "cp1252")


class ProcessingError(RuntimeError):
    pass


def emit(prefix: str, message: str) -> None:
    print(f"{prefix}: {message}", flush=True)


def file_digest(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def csv_header_and_encoding(source: Path) -> tuple[list[str], str]:
    failures: list[str] = []
    for encoding in READ_ENCODINGS:
        try:
            with source.open("r", encoding=encoding, newline="") as stream:
                reader = csv.reader(stream)
                return next(reader), encoding
        except StopIteration as exc:
            raise ProcessingError(f"Source CSV is empty: {source.name}") from exc
        except (UnicodeDecodeError, csv.Error) as exc:
            failures.append(f"{encoding}: {exc}")
        except OSError as exc:
            raise ProcessingError(f"Cannot read source CSV '{source.name}': {exc}") from exc
    raise ProcessingError(
        f"Cannot decode source CSV '{source.name}' using supported encodings. "
        + " | ".join(failures)
    )


def excel_headers(source: Path) -> list[str]:
    try:
        preview = pd.read_excel(
            source,
            header=None,
            nrows=1,
            dtype=str,
            keep_default_na=False,
        )
    except Exception as exc:
        raise ProcessingError(f"Cannot read source workbook '{source.name}': {exc}") from exc
    if preview.empty:
        raise ProcessingError(f"Source workbook is empty: {source.name}")
    return [str(value) for value in preview.iloc[0].tolist()]


def validate_source_headers(headers: list[str]) -> None:
    normalized_positions: dict[str, list[str]] = {}
    for header in headers:
        normalized_positions.setdefault(str(header).strip().casefold(), []).append(str(header))

    ambiguous_required = [
        required
        for required in REQUIRED_HEADERS
        if len(normalized_positions.get(required.casefold(), [])) > 1
    ]
    if ambiguous_required:
        raise ProcessingError(
            "Duplicate/ambiguous required header(s): " + ", ".join(ambiguous_required)
        )

    missing = [header for header in REQUIRED_HEADERS if headers.count(header) != 1]
    if missing:
        raise ProcessingError("Missing required header(s): " + ", ".join(missing))

    duplicate_groups = [values for values in normalized_positions.values() if len(values) > 1]
    if duplicate_groups:
        names = [" / ".join(values) for values in duplicate_groups]
        raise ProcessingError("Duplicate/ambiguous source header(s): " + ", ".join(names))

    conflicts = [
        original
        for generated in GENERATED_HEADERS
        for original in normalized_positions.get(generated.casefold(), [])
    ]
    if conflicts:
        raise ProcessingError(
            "Source contains conflicting generated header(s): " + ", ".join(conflicts)
        )


def read_source(source: Path) -> pd.DataFrame:
    extension = source.suffix.lower()
    if extension == ".csv":
        headers, encoding = csv_header_and_encoding(source)
        validate_source_headers(headers)
        try:
            frame = pd.read_csv(
                source,
                dtype=str,
                keep_default_na=False,
                encoding=encoding,
            )
        except Exception as exc:
            raise ProcessingError(f"Cannot read source CSV '{source.name}': {exc}") from exc
    else:
        headers = excel_headers(source)
        validate_source_headers(headers)
        try:
            frame = pd.read_excel(
                source,
                dtype=str,
                keep_default_na=False,
            )
        except Exception as exc:
            raise ProcessingError(f"Cannot read source workbook '{source.name}': {exc}") from exc

    if frame.isna().any(axis=None):
        raise ProcessingError("Source contains unexpected null values after string-safe reading.")
    return frame


def clean_name_part(value: object) -> str:
    if value is None:
        return ""
    return " ".join(str(value).split())


def generate_match_ids(record_count: int) -> list[str]:
    if record_count > TOTAL_CAPACITY:
        raise ProcessingError(
            f"Source contains {record_count} records; MATCHID capacity is {TOTAL_CAPACITY}."
        )

    chooser = random.SystemRandom()
    used_prefixes: set[str] = set()

    def new_prefix() -> str:
        while True:
            prefix = "".join(chooser.choice(string.ascii_uppercase) for _ in range(2))
            if prefix not in used_prefixes:
                used_prefixes.add(prefix)
                return prefix

    ids: list[str] = []
    if record_count:
        prefix = new_prefix()
        counter = 1
        for _ in range(record_count):
            if counter > MAX_PER_PREFIX:
                prefix = new_prefix()
                counter = 1
            ids.append(f"{prefix}{counter:04d}")
            counter += 1
    validate_match_ids(ids, "generated MATCHID values")
    return ids


def validate_match_ids(ids: list[str], label: str) -> None:
    blank_rows = [str(index + 1) for index, value in enumerate(ids) if value == ""]
    if blank_rows:
        raise ProcessingError(f"Blank MATCHID in {label} at data row(s): {', '.join(blank_rows)}")
    malformed = sorted({value for value in ids if not MATCHID_PATTERN.fullmatch(value)})
    if malformed:
        raise ProcessingError(f"Malformed MATCHID in {label}: {', '.join(malformed[:10])}")
    if len(ids) != len(set(ids)):
        seen: set[str] = set()
        duplicates: set[str] = set()
        for value in ids:
            if value in seen:
                duplicates.add(value)
            seen.add(value)
        raise ProcessingError(f"Duplicate MATCHID in {label}: {', '.join(sorted(duplicates)[:10])}")


def frame_rows(frame: pd.DataFrame) -> list[list[str]]:
    return [[str(value) for value in row] for row in frame.itertuples(index=False, name=None)]


def expected_input_rows(frame: pd.DataFrame, match_ids: list[str]) -> list[list[str]]:
    addressees = [
        " ".join(part for part in (clean_name_part(first), clean_name_part(last)) if part)
        for first, last in zip(frame["Provider First"], frame["Provider Last"])
    ]
    rows: list[list[str]] = []
    for index, match_id in enumerate(match_ids):
        rows.append(
            [
                match_id,
                str(frame.iloc[index]["Location Address 1"]),
                str(frame.iloc[index]["Location Address 2"]),
                str(frame.iloc[index]["Location City"]),
                str(frame.iloc[index]["Location State"]),
                str(frame.iloc[index]["Location Zip"]),
                str(frame.iloc[index]["Payee Name"]),
                addressees[index],
            ]
        )
    return rows


def read_csv_table(path: Path, label: str) -> tuple[list[str], list[list[str]]]:
    if not path.is_file():
        raise ProcessingError(f"{label} was not found: {path}")
    if path.stat().st_size == 0:
        raise ProcessingError(f"{label} is zero bytes.")

    failures: list[str] = []
    for encoding in READ_ENCODINGS:
        try:
            with path.open("r", encoding=encoding, newline="") as stream:
                rows = list(csv.reader(stream))
            if not rows:
                raise ProcessingError(f"{label} has no header row.")
            headers = rows[0]
            data_rows: list[list[str]] = []
            for row_number, row in enumerate(rows[1:], start=2):
                if not any(cell.strip() for cell in row):
                    continue
                if len(row) != len(headers):
                    raise ProcessingError(
                        f"{label} row {row_number} has {len(row)} fields; expected {len(headers)}."
                    )
                data_rows.append(row)
            return headers, data_rows
        except UnicodeDecodeError as exc:
            failures.append(f"{encoding}: {exc}")
        except csv.Error as exc:
            raise ProcessingError(f"{label} is not a readable CSV file: {exc}") from exc
        except OSError as exc:
            raise ProcessingError(f"Cannot read {label}: {exc}") from exc
    raise ProcessingError(
        f"Cannot decode {label} using supported encodings. " + " | ".join(failures)
    )


def reusable_match_ids(mapping_path: Path, frame: pd.DataFrame) -> tuple[list[str] | None, str]:
    try:
        headers, rows = read_csv_table(mapping_path, "TMMA SOURCE.csv")
        expected_headers = ["MATCHID", *[str(column) for column in frame.columns]]
        if headers != expected_headers:
            return None, "TMMA SOURCE.csv headers/order do not match the current RAW INPUT"
        if len(rows) != len(frame.index):
            return None, "TMMA SOURCE.csv record count does not match the current RAW INPUT"

        ids = [row[0] for row in rows]
        validate_match_ids(ids, "TMMA SOURCE.csv")
        if [row[1:] for row in rows] != frame_rows(frame):
            return None, "TMMA SOURCE.csv source values/order do not match the current RAW INPUT"
        return ids, ""
    except ProcessingError as exc:
        return None, str(exc)


def create_stage_path(target: Path) -> Path:
    descriptor, name = tempfile.mkstemp(
        prefix=f".{target.name}.", suffix=".tmp", dir=target.parent
    )
    os.close(descriptor)
    return Path(name)


def write_csv(path: Path, headers: list[str], rows: list[list[str]]) -> None:
    with path.open("w", encoding="utf-8-sig", newline="") as stream:
        writer = csv.writer(stream, lineterminator="\n")
        writer.writerow(headers)
        writer.writerows(rows)


def promote_pair(
    source_stage: Path,
    source_target: Path,
    input_stage: Path,
    input_target: Path,
) -> None:
    targets = [source_target, input_target]
    stages = [source_stage, input_stage]
    backups: dict[Path, Path] = {}
    existed = {target: target.exists() for target in targets}
    try:
        for target in targets:
            if target.exists():
                backup = create_stage_path(target.with_name(target.name + ".backup"))
                shutil.copy2(target, backup)
                backups[target] = backup

        for stage, target in zip(stages, targets):
            os.replace(stage, target)
    except Exception as exc:
        rollback_errors: list[str] = []
        for target in targets:
            try:
                if existed[target]:
                    backup = backups.get(target)
                    if backup and backup.exists():
                        os.replace(backup, target)
                elif target.exists():
                    target.unlink()
            except OSError as rollback_exc:
                rollback_errors.append(f"{target}: {rollback_exc}")
        detail = f" Pair rollback issue(s): {' | '.join(rollback_errors)}" if rollback_errors else ""
        raise ProcessingError(
            f"Could not atomically promote TMMA SOURCE.csv and INPUT.csv: {exc}.{detail}"
        ) from exc
    finally:
        for path in [*stages, *backups.values()]:
            path.unlink(missing_ok=True)


def locate_source(raw_input: Path) -> Path:
    sources = sorted(path for path in raw_input.iterdir() if path.is_file())
    if not sources:
        raise ProcessingError(f"No source file exists directly in {raw_input}.")
    if len(sources) > 1:
        names = ", ".join(path.name for path in sources)
        raise ProcessingError(
            f"Expected exactly one source file in RAW INPUT but found {len(sources)}: {names}"
        )
    source = sources[0]
    if source.suffix.lower() not in SUPPORTED_EXTENSIONS:
        raise ProcessingError(
            f"Unsupported source extension '{source.suffix or '(none)'}' for '{source.name}'. "
            "Supported extensions are .xlsx, .xls, and .csv."
        )
    return source


def run(base_path: Path) -> Path:
    raw_input = base_path / "RAW INPUT"
    data = base_path / "DATA"
    raw_input.mkdir(parents=True, exist_ok=True)
    data.mkdir(parents=True, exist_ok=True)

    source = locate_source(raw_input)
    raw_digest = file_digest(source)
    emit("INFO", f"Reading TMMA source without modifying it: {source}")
    frame = read_source(source)

    mapping_target = data / "TMMA SOURCE.csv"
    input_target = data / "INPUT.csv"
    downstream = [data / name for name in DOWNSTREAM_EXPORTS if (data / name).exists()]

    match_ids: list[str] | None = None
    mismatch_reason = ""
    if mapping_target.exists():
        match_ids, mismatch_reason = reusable_match_ids(mapping_target, frame)
        if match_ids is not None:
            emit("INFO", "Reusing existing MATCHIDs from verified TMMA SOURCE.csv.")

    if match_ids is None:
        if downstream:
            names = ", ".join(path.name for path in downstream)
            reason = mismatch_reason or "TMMA SOURCE.csv does not exist"
            raise ProcessingError(
                f"Cannot regenerate MATCHIDs because downstream Bulk Mailer artifact(s) exist: {names}. "
                f"{reason}. Remove or archive the stale exports before rerunning INITIAL."
            )
        if mismatch_reason:
            emit("WARNING", f"{mismatch_reason}; generating a new safe MATCHID mapping.")
        match_ids = generate_match_ids(len(frame.index))

    validate_match_ids(match_ids, "TMMA source mapping")
    source_headers = ["MATCHID", *[str(column) for column in frame.columns]]
    source_rows = [[match_ids[index], *row] for index, row in enumerate(frame_rows(frame))]
    input_rows = expected_input_rows(frame, match_ids)

    mapping_stage = create_stage_path(mapping_target)
    input_stage = create_stage_path(input_target)
    try:
        write_csv(mapping_stage, source_headers, source_rows)
        write_csv(input_stage, INPUT_HEADERS, input_rows)

        verified_source_headers, verified_source_rows = read_csv_table(
            mapping_stage, "staged TMMA SOURCE.csv"
        )
        verified_input_headers, verified_input_rows = read_csv_table(
            input_stage, "staged INPUT.csv"
        )
        if verified_source_headers != source_headers or verified_source_rows != source_rows:
            raise ProcessingError(
                "TMMA SOURCE.csv verification failed; source values changed after writing."
            )
        if verified_input_headers != INPUT_HEADERS or verified_input_rows != input_rows:
            raise ProcessingError(
                "INPUT.csv verification failed; transformed values changed after writing."
            )

        source_ids = [row[0] for row in verified_source_rows]
        input_ids = [row[0] for row in verified_input_rows]
        validate_match_ids(source_ids, "staged TMMA SOURCE.csv")
        validate_match_ids(input_ids, "staged INPUT.csv")
        if source_ids != input_ids:
            raise ProcessingError(
                "INITIAL verification failed: SOURCE and INPUT MATCHID order differs."
            )
        if len(source_ids) != len(frame.index):
            raise ProcessingError(
                "INITIAL verification failed: SOURCE/INPUT record counts differ."
            )

        source_zip_index = source_headers.index("Location Zip")
        input_zip_index = INPUT_HEADERS.index("ZIP Code")
        expected_zips = [str(value) for value in frame["Location Zip"]]
        if [row[source_zip_index] for row in source_rows] != expected_zips:
            raise ProcessingError("TMMA SOURCE.csv ZIP verification failed.")
        if [row[input_zip_index] for row in input_rows] != expected_zips:
            raise ProcessingError("INPUT.csv ZIP verification failed.")
        if file_digest(source) != raw_digest:
            raise ProcessingError(
                "RAW INPUT changed while INITIAL was running; no working files were promoted."
            )

        promote_pair(mapping_stage, mapping_target, input_stage, input_target)
    finally:
        mapping_stage.unlink(missing_ok=True)
        input_stage.unlink(missing_ok=True)

    emit(
        "SUCCESS",
        f"Created verified TMMA SOURCE.csv and INPUT.csv with {len(match_ids)} records.",
    )
    return input_target


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Create TMMA MATCHID mapping and INPUT.csv"
    )
    parser.add_argument(
        "--base-path",
        type=Path,
        default=DEFAULT_BASE_PATH,
        help="TMMA base directory (defaults to the GOJI runtime MA directory)",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        run(args.base_path)
        return 0
    except ProcessingError as exc:
        emit("ERROR", str(exc))
        return 1
    except Exception as exc:
        emit("ERROR", f"Unexpected INITIAL processing failure: {exc}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
