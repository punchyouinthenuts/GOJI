#!/usr/bin/env python3
"""Strictly reconcile TMMA Bulk Mailer exports and create both customer files."""

from __future__ import annotations

import argparse
import csv
import hashlib
import os
import re
import sys
import tempfile
from dataclasses import dataclass
from decimal import Decimal, InvalidOperation
from pathlib import Path

import pandas as pd


DEFAULT_BASE_PATH = Path(r"C:/Goji/AUTOMATION/TRACHMAR/MA")
SUPPORTED_EXTENSIONS = {".xlsx", ".xls", ".csv"}
REQUIRED_SOURCE_HEADERS = [
    "Provider First",
    "Provider Last",
    "Location Address 1",
    "Location Address 2",
    "Location City",
    "Location State",
    "Location Zip",
    "Payee Name",
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
MOVE_HEADERS = [
    "MATCHID",
    "Address Line 1",
    "Address Line 2",
    "City",
    "State",
    "ZIP Code",
]
NOT_MAILED_HEADERS = ["MATCHID", "User Text 3"]
CURRENT_HEADERS = [
    "Current Address Line 1",
    "Current Address Line 2",
    "Current City",
    "Current State",
    "Current ZIP Code",
]
MATCHID_PATTERN = re.compile(r"^[A-Z]{2}[0-9]{4}$")
READ_ENCODINGS = ("utf-8-sig", "utf-8", "cp1252")


class FinalProcessError(RuntimeError):
    pass


@dataclass(frozen=True)
class CsvTable:
    headers: list[str]
    rows: list[list[str]]

    def column(self, header: str) -> list[str]:
        index = self.headers.index(header)
        return [row[index] for row in self.rows]


def emit(prefix: str, message: str) -> None:
    print(f"{prefix}: {message}", flush=True)


def parse_count(raw_count: str) -> int:
    normalized = raw_count.replace(",", "").replace(" ", "")
    if not normalized or not normalized.isdigit():
        raise FinalProcessError(
            f"TMMA count '{raw_count}' is invalid; enter a positive whole number."
        )
    count = int(normalized)
    if count <= 0:
        raise FinalProcessError("TMMA count must be greater than zero.")
    return count


def validate_job_number(job_number: str) -> str:
    if len(job_number) != 5 or not job_number.isdigit():
        raise FinalProcessError(
            f"TMMA job number '{job_number}' is invalid; expected exactly five digits."
        )
    return job_number


def file_digest(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def locate_source(raw_input: Path) -> Path:
    if not raw_input.is_dir():
        raise FinalProcessError(f"Required RAW INPUT directory was not found: {raw_input}")
    sources = sorted(path for path in raw_input.iterdir() if path.is_file())
    if not sources:
        raise FinalProcessError(f"No source file exists directly in {raw_input}.")
    if len(sources) > 1:
        names = ", ".join(path.name for path in sources)
        raise FinalProcessError(
            f"Expected exactly one source file in RAW INPUT but found {len(sources)}: {names}"
        )
    source = sources[0]
    if source.suffix.lower() not in SUPPORTED_EXTENSIONS:
        raise FinalProcessError(
            f"Unsupported RAW INPUT extension '{source.suffix or '(none)'}' for '{source.name}'."
        )
    return source


def csv_header_and_encoding(source: Path) -> tuple[list[str], str]:
    failures: list[str] = []
    for encoding in READ_ENCODINGS:
        try:
            with source.open("r", encoding=encoding, newline="") as stream:
                reader = csv.reader(stream)
                return next(reader), encoding
        except StopIteration as exc:
            raise FinalProcessError(f"RAW INPUT CSV is empty: {source.name}") from exc
        except (UnicodeDecodeError, csv.Error) as exc:
            failures.append(f"{encoding}: {exc}")
        except OSError as exc:
            raise FinalProcessError(f"Cannot read RAW INPUT CSV '{source.name}': {exc}") from exc
    raise FinalProcessError(
        f"Cannot decode RAW INPUT CSV '{source.name}'. " + " | ".join(failures)
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
        raise FinalProcessError(
            f"Cannot read RAW INPUT workbook '{source.name}': {exc}"
        ) from exc
    if preview.empty:
        raise FinalProcessError(f"RAW INPUT workbook is empty: {source.name}")
    return [str(value) for value in preview.iloc[0].tolist()]


def validate_unique_headers(headers: list[str], label: str) -> None:
    normalized: dict[str, list[str]] = {}
    for header in headers:
        normalized.setdefault(str(header).strip().casefold(), []).append(str(header))
    duplicates = [values for values in normalized.values() if len(values) > 1]
    if duplicates:
        names = ["/".join(values) for values in duplicates]
        raise FinalProcessError(
            f"{label} contains duplicate/ambiguous header(s): {', '.join(names)}"
        )


def read_raw_source(source: Path) -> tuple[list[str], list[list[str]]]:
    if source.suffix.lower() == ".csv":
        headers, encoding = csv_header_and_encoding(source)
        validate_unique_headers(headers, "RAW INPUT")
        missing = [header for header in REQUIRED_SOURCE_HEADERS if header not in headers]
        if missing:
            raise FinalProcessError(
                "RAW INPUT missing required header(s): " + ", ".join(missing)
            )
        try:
            frame = pd.read_csv(
                source,
                dtype=str,
                keep_default_na=False,
                encoding=encoding,
            )
        except Exception as exc:
            raise FinalProcessError(
                f"Cannot read RAW INPUT CSV '{source.name}': {exc}"
            ) from exc
    else:
        headers = excel_headers(source)
        validate_unique_headers(headers, "RAW INPUT")
        missing = [header for header in REQUIRED_SOURCE_HEADERS if header not in headers]
        if missing:
            raise FinalProcessError(
                "RAW INPUT missing required header(s): " + ", ".join(missing)
            )
        try:
            frame = pd.read_excel(
                source,
                dtype=str,
                keep_default_na=False,
            )
        except Exception as exc:
            raise FinalProcessError(
                f"Cannot read RAW INPUT workbook '{source.name}': {exc}"
            ) from exc

    if frame.isna().any(axis=None):
        raise FinalProcessError(
            "RAW INPUT contains unexpected null values after string-safe reading."
        )
    return [str(column) for column in frame.columns], [
        [str(value) for value in row]
        for row in frame.itertuples(index=False, name=None)
    ]


def read_csv_table(path: Path, label: str) -> CsvTable:
    if not path.is_file():
        raise FinalProcessError(f"Required {label} was not found: {path}")
    if path.stat().st_size == 0:
        raise FinalProcessError(f"Required {label} is zero bytes: {path}")

    failures: list[str] = []
    for encoding in READ_ENCODINGS:
        try:
            with path.open("r", encoding=encoding, newline="") as stream:
                parsed = list(csv.reader(stream))
            if not parsed:
                raise FinalProcessError(f"{label} has no header row.")
            headers = parsed[0]
            if not headers or not any(header != "" for header in headers):
                raise FinalProcessError(f"{label} has an invalid blank header row.")
            validate_unique_headers(headers, label)
            rows: list[list[str]] = []
            for row_number, row in enumerate(parsed[1:], start=2):
                if not row:
                    continue
                if len(row) != len(headers):
                    raise FinalProcessError(
                        f"{label} row {row_number} has {len(row)} fields; expected {len(headers)}."
                    )
                rows.append(row)
            return CsvTable(headers, rows)
        except UnicodeDecodeError as exc:
            failures.append(f"{encoding}: {exc}")
        except csv.Error as exc:
            raise FinalProcessError(f"{label} is not a readable CSV file: {exc}") from exc
        except OSError as exc:
            raise FinalProcessError(f"Cannot read {label}: {exc}") from exc
    raise FinalProcessError(
        f"Cannot decode {label} using supported encodings. " + " | ".join(failures)
    )


def require_headers(table: CsvTable, required: list[str], label: str) -> None:
    missing = [header for header in required if header not in table.headers]
    if missing:
        raise FinalProcessError(
            f"{label} missing required header(s): {', '.join(missing)}"
        )


def validate_match_ids(
    table: CsvTable,
    label: str,
    known_ids: set[str] | None = None,
) -> list[str]:
    require_headers(table, ["MATCHID"], label)
    ids = table.column("MATCHID")
    blanks = [str(index + 1) for index, value in enumerate(ids) if value == ""]
    if blanks:
        raise FinalProcessError(
            f"Blank MATCHID in {label} at data row(s): {', '.join(blanks[:10])}"
        )
    malformed = sorted({value for value in ids if not MATCHID_PATTERN.fullmatch(value)})
    if malformed:
        raise FinalProcessError(
            f"Malformed MATCHID in {label}: {', '.join(malformed[:10])}"
        )
    seen: set[str] = set()
    duplicates: set[str] = set()
    for value in ids:
        if value in seen:
            duplicates.add(value)
        seen.add(value)
    if duplicates:
        raise FinalProcessError(
            f"Duplicate MATCHID in {label}: {', '.join(sorted(duplicates)[:10])}"
        )
    if known_ids is not None:
        unknown = sorted(set(ids) - known_ids)
        if unknown:
            raise FinalProcessError(
                f"Unknown MATCHID in {label}: {', '.join(unknown[:10])}"
            )
    return ids


def clean_name_part(value: str) -> str:
    return " ".join(value.split())


def expected_input_rows(
    raw_headers: list[str],
    raw_rows: list[list[str]],
    source_ids: list[str],
) -> list[list[str]]:
    positions = {header: raw_headers.index(header) for header in REQUIRED_SOURCE_HEADERS}
    expected: list[list[str]] = []
    for match_id, row in zip(source_ids, raw_rows):
        first = clean_name_part(row[positions["Provider First"]])
        last = clean_name_part(row[positions["Provider Last"]])
        addressee = " ".join(part for part in (first, last) if part)
        expected.append(
            [
                match_id,
                row[positions["Location Address 1"]],
                row[positions["Location Address 2"]],
                row[positions["Location City"]],
                row[positions["Location State"]],
                row[positions["Location Zip"]],
                row[positions["Payee Name"]],
                addressee,
            ]
        )
    return expected


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


def pallet_number_is_minus_one(value: str) -> bool:
    normalized = value.strip()
    if not normalized:
        return False
    try:
        return Decimal(normalized) == Decimal("-1")
    except InvalidOperation:
        return False


def sanitize_output(
    output_path: Path,
    output_table: CsvTable,
    source_ids: set[str],
) -> tuple[CsvTable, list[str]]:
    require_headers(output_table, ["MATCHID", "Pallet Number"], "OUTPUT.csv")
    raw_output_ids = validate_match_ids(output_table, "OUTPUT.csv", source_ids)
    matchid_index = output_table.headers.index("MATCHID")
    pallet_index = output_table.headers.index("Pallet Number")

    removed_ids: list[str] = []
    retained_rows: list[list[str]] = []
    retained_ids: list[str] = []
    for row, match_id in zip(output_table.rows, raw_output_ids):
        if pallet_number_is_minus_one(row[pallet_index]):
            removed_ids.append(match_id)
        else:
            retained_rows.append(row.copy())
            retained_ids.append(match_id)

    stage_path = create_stage_path(output_path)
    try:
        write_csv(stage_path, output_table.headers, retained_rows)
        verified = read_csv_table(stage_path, "staged sanitized OUTPUT.csv")
        if verified.headers != output_table.headers:
            raise FinalProcessError(
                "Sanitized OUTPUT.csv verification failed; header order changed."
            )
        if verified.rows != retained_rows:
            raise FinalProcessError(
                "Sanitized OUTPUT.csv verification failed; retained cell values or row order changed."
            )
        if len(verified.rows) != len(output_table.rows) - len(removed_ids):
            raise FinalProcessError(
                "Sanitized OUTPUT.csv verification failed; retained row count is incorrect."
            )
        if any(pallet_number_is_minus_one(row[pallet_index]) for row in verified.rows):
            raise FinalProcessError(
                "Sanitized OUTPUT.csv verification failed; a Pallet Number = -1 row remains."
            )
        verified_ids = validate_match_ids(
            verified, "staged sanitized OUTPUT.csv", source_ids
        )
        if verified_ids != retained_ids:
            raise FinalProcessError(
                "Sanitized OUTPUT.csv verification failed; retained MATCHIDs changed."
            )

        staged_digest = file_digest(stage_path)
        try:
            os.replace(stage_path, output_path)
        except OSError as exc:
            raise FinalProcessError(
                f"Could not atomically replace OUTPUT.csv with its sanitized version: {exc}"
            ) from exc
        if file_digest(output_path) != staged_digest:
            raise FinalProcessError(
                "Sanitized OUTPUT.csv failed digest verification after atomic replacement."
            )
        promoted = read_csv_table(output_path, "sanitized OUTPUT.csv")
        if promoted.headers != verified.headers or promoted.rows != verified.rows:
            raise FinalProcessError(
                "Sanitized OUTPUT.csv changed during atomic replacement."
            )
    finally:
        stage_path.unlink(missing_ok=True)

    if removed_ids:
        noun = "record" if len(removed_ids) == 1 else "records"
        emit(
            "INFO",
            f"Removed {len(removed_ids)} OUTPUT {noun} with Pallet Number = -1 "
            "before TMMA reconciliation.",
        )
        emit(
            "INFO",
            f"Sanitized OUTPUT.csv contains {len(retained_rows)} mailed records.",
        )
    return promoted, removed_ids


def remove_generated_file(path: Path, expected_digest: str) -> str | None:
    try:
        if not path.exists():
            return None
        if file_digest(path) != expected_digest:
            return f"Refused to remove changed rollback target: {path}"
        path.unlink()
        return None
    except OSError as exc:
        return f"Could not remove rollback target {path}: {exc}"


def promote_final_pair(
    merged_stage: Path,
    merged_target: Path,
    output_stage: Path,
    output_target: Path,
    raw_output: Path,
) -> None:
    merged_digest = file_digest(merged_stage)
    output_digest = file_digest(output_stage)
    promoted: list[tuple[Path, str]] = []
    try:
        os.rename(merged_stage, merged_target)
        promoted.append((merged_target, merged_digest))
        os.rename(output_stage, output_target)
        promoted.append((output_target, output_digest))

        if file_digest(merged_target) != merged_digest:
            raise OSError("promoted merged file failed digest verification")
        if file_digest(output_target) != output_digest:
            raise OSError("promoted customer OUTPUT failed digest verification")
        raw_output.unlink()
    except Exception as exc:
        rollback_errors: list[str] = []
        for path, digest in reversed(promoted):
            error = remove_generated_file(path, digest)
            if error:
                rollback_errors.append(error)
        detail = (
            " Rollback issue(s): " + " | ".join(rollback_errors)
            if rollback_errors
            else ""
        )
        raise FinalProcessError(
            f"Could not complete both TMMA final outputs; no partial success was accepted: {exc}.{detail}"
        ) from exc
    finally:
        merged_stage.unlink(missing_ok=True)
        output_stage.unlink(missing_ok=True)


def run(base_path: Path, raw_job_number: str, raw_count: str) -> tuple[Path, Path]:
    job_number = validate_job_number(raw_job_number)
    entered_count = parse_count(raw_count)
    data_path = base_path / "DATA"
    raw_input = base_path / "RAW INPUT"
    source_path = locate_source(raw_input)

    merged_target = data_path / f"{source_path.stem}_MERGED.csv"
    output_target = data_path / f"{job_number} TM MA_{entered_count}.csv"
    for target in (merged_target, output_target):
        if target.exists():
            raise FinalProcessError(
                f"Final target already exists and will not be overwritten: {target}"
            )

    mapping_path = data_path / "TMMA SOURCE.csv"
    input_path = data_path / "INPUT.csv"
    move_path = data_path / "MOVE UPDATES.csv"
    not_mailed_path = data_path / "NOT MAILED.csv"
    output_path = data_path / "OUTPUT.csv"

    raw_headers, raw_rows = read_raw_source(source_path)
    mapping = read_csv_table(mapping_path, "TMMA SOURCE.csv")
    input_table = read_csv_table(input_path, "INPUT.csv")
    move_table = read_csv_table(move_path, "MOVE UPDATES.csv")
    not_mailed_table = read_csv_table(not_mailed_path, "NOT MAILED.csv")
    output_table = read_csv_table(output_path, "OUTPUT.csv")

    expected_mapping_headers = ["MATCHID", *raw_headers]
    if mapping.headers != expected_mapping_headers:
        raise FinalProcessError(
            "TMMA SOURCE.csv headers/order do not exactly match MATCHID + RAW INPUT columns."
        )
    if len(mapping.rows) != len(raw_rows):
        raise FinalProcessError(
            f"TMMA SOURCE.csv contains {len(mapping.rows)} records but RAW INPUT contains {len(raw_rows)}."
        )
    if [row[1:] for row in mapping.rows] != raw_rows:
        raise FinalProcessError(
            "TMMA SOURCE.csv source values/order do not exactly match RAW INPUT."
        )

    source_ids = validate_match_ids(mapping, "TMMA SOURCE.csv")
    source_id_set = set(source_ids)

    if input_table.headers != INPUT_HEADERS:
        raise FinalProcessError(
            "INPUT.csv schema must be exactly: " + ", ".join(INPUT_HEADERS)
        )
    input_ids = validate_match_ids(input_table, "INPUT.csv", source_id_set)
    if input_ids != source_ids:
        raise FinalProcessError(
            "TMMA SOURCE.csv and INPUT.csv do not contain the same ordered MATCHID sequence."
        )
    if input_table.rows != expected_input_rows(raw_headers, raw_rows, source_ids):
        raise FinalProcessError(
            "INPUT.csv transformed values no longer correspond exactly to TMMA SOURCE.csv."
        )

    require_headers(move_table, MOVE_HEADERS, "MOVE UPDATES.csv")
    move_ids = validate_match_ids(move_table, "MOVE UPDATES.csv", source_id_set)

    require_headers(not_mailed_table, NOT_MAILED_HEADERS, "NOT MAILED.csv")
    not_mailed_ids = validate_match_ids(
        not_mailed_table, "NOT MAILED.csv", source_id_set
    )
    user_text_index = not_mailed_table.headers.index("User Text 3")
    bad_status_rows = [
        str(index + 1)
        for index, row in enumerate(not_mailed_table.rows)
        if row[user_text_index] != "14"
    ]
    if bad_status_rows:
        raise FinalProcessError(
            "NOT MAILED.csv User Text 3 must be exactly 14 at data row(s): "
            + ", ".join(bad_status_rows[:10])
        )

    output_table, pallet_minus_one_ids = sanitize_output(
        output_path, output_table, source_id_set
    )
    output_ids = validate_match_ids(output_table, "sanitized OUTPUT.csv", source_id_set)
    if not output_ids:
        raise FinalProcessError(
            "OUTPUT.csv contains no mailed records; TMMA requires a positive mailing count."
        )
    if len(output_ids) != entered_count:
        raise FinalProcessError(
            f"Count mismatch. TMMA count is {entered_count} but OUTPUT.csv contains "
            f"{len(output_ids)} data records."
        )

    output_set = set(output_ids)
    not_mailed_set = set(not_mailed_ids)
    pallet_minus_one_set = set(pallet_minus_one_ids)
    effective_not_mailed_set = not_mailed_set | pallet_minus_one_set
    duplicate_automatic_14 = sorted(not_mailed_set & pallet_minus_one_set)
    if duplicate_automatic_14:
        emit(
            "INFO",
            f"{len(duplicate_automatic_14)} Pallet Number = -1 MATCHID(s) were already "
            "present in NOT MAILED.csv and were counted once as mailed=14.",
        )

    overlap = sorted(output_set & effective_not_mailed_set)
    if overlap:
        raise FinalProcessError(
            "MATCHID appears in both OUTPUT.csv and NOT MAILED.csv/effective 14 classification: "
            + ", ".join(overlap[:10])
        )
    accounted = output_set | effective_not_mailed_set
    missing = sorted(source_id_set - accounted)
    if missing:
        raise FinalProcessError(
            "Source MATCHID appears in neither OUTPUT.csv nor NOT MAILED.csv/effective 14 classification: "
            + ", ".join(missing[:10])
        )
    if accounted != source_id_set:
        raise FinalProcessError(
            "Sanitized OUTPUT.csv and the effective 14 set do not form a complete source partition."
        )

    move_set = set(move_ids)
    mailed_moves = sorted(move_set - effective_not_mailed_set)
    if mailed_moves:
        raise FinalProcessError(
            "MOVE UPDATES.csv MATCHID is not present in NOT MAILED.csv or the automatic "
            "Pallet Number = -1 set: "
            + ", ".join(mailed_moves[:10])
        )

    move_positions = {header: move_table.headers.index(header) for header in MOVE_HEADERS}
    move_by_id = {row[move_positions["MATCHID"]]: row for row in move_table.rows}
    merged_headers = [*raw_headers, "mailed", *CURRENT_HEADERS]
    merged_rows: list[list[str]] = []
    for match_id, source_row in zip(source_ids, raw_rows):
        status = "13" if match_id in output_set else "14"
        current_values = ["", "", "", "", ""]
        if match_id in move_by_id:
            move_row = move_by_id[match_id]
            current_values = [
                move_row[move_positions["Address Line 1"]],
                move_row[move_positions["Address Line 2"]],
                move_row[move_positions["City"]],
                move_row[move_positions["State"]],
                move_row[move_positions["ZIP Code"]],
            ]
        merged_rows.append([*source_row, status, *current_values])

    if len(merged_rows) != len(raw_rows):
        raise FinalProcessError("Merged row count does not equal RAW INPUT row count.")
    if sum(row[len(raw_headers)] == "13" for row in merged_rows) != len(output_ids):
        raise FinalProcessError("Merged mailed=13 count does not equal OUTPUT count.")
    if sum(row[len(raw_headers)] == "14" for row in merged_rows) != len(effective_not_mailed_set):
        raise FinalProcessError("Merged mailed=14 count does not equal the effective 14 count.")

    matchid_index = output_table.headers.index("MATCHID")
    customer_output_headers = [
        header for index, header in enumerate(output_table.headers) if index != matchid_index
    ]
    if not customer_output_headers:
        raise FinalProcessError(
            "OUTPUT.csv contains no customer-facing columns after removing MATCHID."
        )
    customer_output_rows = [
        [value for index, value in enumerate(row) if index != matchid_index]
        for row in output_table.rows
    ]

    merged_stage = create_stage_path(merged_target)
    output_stage = create_stage_path(output_target)
    try:
        write_csv(merged_stage, merged_headers, merged_rows)
        write_csv(output_stage, customer_output_headers, customer_output_rows)

        verified_merged = read_csv_table(merged_stage, "staged _MERGED.csv")
        if verified_merged.headers != merged_headers or verified_merged.rows != merged_rows:
            raise FinalProcessError(
                "Staged _MERGED.csv verification failed; schema or values changed after writing."
            )
        if "MATCHID" in verified_merged.headers:
            raise FinalProcessError("Internal MATCHID remained in staged _MERGED.csv.")

        verified_output = read_csv_table(output_stage, "staged customer OUTPUT")
        if (
            verified_output.headers != customer_output_headers
            or verified_output.rows != customer_output_rows
        ):
            raise FinalProcessError(
                "Staged customer OUTPUT verification failed; non-MATCHID data changed."
            )
        if "MATCHID" in verified_output.headers:
            raise FinalProcessError("Internal MATCHID remained in staged customer OUTPUT.")
        verified_pallet_index = verified_output.headers.index("Pallet Number")
        if any(
            pallet_number_is_minus_one(row[verified_pallet_index])
            for row in verified_output.rows
        ):
            raise FinalProcessError(
                "Pallet Number = -1 remained in staged customer OUTPUT."
            )
        if len(verified_output.rows) != entered_count:
            raise FinalProcessError(
                "Customer OUTPUT count changed after MATCHID removal."
            )

        promote_final_pair(
            merged_stage,
            merged_target,
            output_stage,
            output_target,
            output_path,
        )
    finally:
        merged_stage.unlink(missing_ok=True)
        output_stage.unlink(missing_ok=True)

    emit(
        "SUCCESS",
        f"Created {merged_target.name} with {len(merged_rows)} source records and "
        f"{output_target.name} with {entered_count} mailed records.",
    )
    print(f"TMMA_FINAL_OUTPUT_FILE={output_target.resolve()}", flush=True)
    print(f"TMMA_FINAL_MERGED_FILE={merged_target.resolve()}", flush=True)
    return output_target, merged_target


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Finalize TMMA OUTPUT.csv and create the source-based merged file"
    )
    parser.add_argument("job_number")
    parser.add_argument("count")
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
        run(args.base_path, args.job_number, args.count)
        return 0
    except FinalProcessError as exc:
        emit("ERROR", str(exc))
        return 1
    except Exception as exc:
        emit("ERROR", f"Unexpected FINAL processing failure: {exc}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
