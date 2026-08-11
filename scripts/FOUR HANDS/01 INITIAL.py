import csv
import hashlib
import json
import os
import re
import sys
import tempfile
import time
import traceback
from pathlib import Path

import openpyxl
import pandas as pd


print("=== INITIALIZING ===")

BASE_DIR = Path(r"C:\Goji\AUTOMATION\FOUR HANDS")
SOURCE_DIR = BASE_DIR / "ORIGINAL"
MANIFEST_FILE = BASE_DIR / ".goji_fourhands_state.json"

RES_INPUT_DIR = BASE_DIR / "RESIDENTIAL" / "INPUT"
HOSP_INPUT_DIR = BASE_DIR / "HOSPITALITY" / "INPUT"

RES_OUTPUT_FILE = RES_INPUT_DIR / "INPUT.csv"
HOSP_OUTPUT_FILE = HOSP_INPUT_DIR / "INPUT.csv"

DETECTION_MODE = "--detect-versions"
DETECTION_SOURCE_OPTION = "--source-dir"
DETECTION_BEGIN_MARKER = "=== FH_VERSION_DETECTION_BEGIN ==="
DETECTION_END_MARKER = "=== FH_VERSION_DETECTION_END ==="

SHEET_RESIDENTIAL = "Residential"
SHEET_COMMERCIAL = "Commercial"
SHEET_NEW_ADDRESSES = "New Addresses"

VERSION_RESIDENTIAL = "RESIDENTIAL"
VERSION_HOSPITALITY = "HOSPITALITY"
VERSION_ORDER = [VERSION_RESIDENTIAL, VERSION_HOSPITALITY]

IGNORE_MARKER_RE = re.compile(r"ignore\s*>\s*>\s*>", re.IGNORECASE)
STANDALONE_HOSPITALITY_RE = re.compile(
    r"^hospitality customer look books?\b",
    re.IGNORECASE,
)

COLUMNS_TO_REMOVE = [
    "Account Source",
    "Account Owner",
    "Contact Owner",
    "Account: Created Date↑",
    "Account: Created Date",
    "Customer Status",
    "Social Profile: Instagram",
]

COLUMN_RENAME = {
    "Account Name": "Business",
    "Billing Address Line 1": "Address Line 1",
    "Billing City": "City",
    "Billing State/Province": "State",
    "Billing Zip/Postal Code": "ZIP Code",
    "Mailing Country": "Country",
}

TEXT_ONLY_STATE_COL = "Billing State/Province (text only)"
TEXT_ONLY_COUNTRY_COL = "Mailing Country (text only)"

CANONICAL_COLUMNS = [
    "Customer Number",
    "Business",
    "First Name",
    "Last Name",
    "Address Line 1",
    "City",
    "State",
    "ZIP Code",
    "Country",
    "Primary Segment",
    "Secondary Segment",
]


def current_timestamp():
    return time.strftime("%Y-%m-%dT%H:%M:%S")


def parse_job_args():
    job_number = str(sys.argv[1]).strip() if len(sys.argv) > 1 else ""
    drop_number = str(sys.argv[2]).strip() if len(sys.argv) > 2 else "1"
    if not drop_number:
        drop_number = "1"
    return job_number, drop_number


def build_manifest(versions_present):
    job_number, drop_number = parse_job_args()
    now = current_timestamp()
    return {
        "job_number": job_number,
        "drop_number": drop_number,
        "versions_present": versions_present,
        "versions_complete": [],
        "outputs": {},
        "created_at": now,
        "updated_at": now,
    }


def is_valid_customer_number(value):
    try:
        str_value = str(value).strip()
        if str_value.lower() in ["nan", ""]:
            return False
        float_value = float(str_value)
        return float_value.is_integer()
    except (ValueError, TypeError):
        return False


def is_valid_value(value):
    value = str(value).strip()
    return value not in ["", "-", ".", ",", "nan", "None"]


def handle_text_only_column(df, main_col, text_only_col):
    has_main = main_col in df.columns
    has_text_only = text_only_col in df.columns
    if has_text_only:
        if not has_main:
            df = df.rename(columns={text_only_col: main_col})
            invalid_mask = ~df[main_col].apply(is_valid_value)
            df.loc[invalid_mask, main_col] = ""
        else:
            df[main_col] = df[main_col].astype(str).str.strip()
            df[text_only_col] = df[text_only_col].astype(str).str.strip()
            blank_main_mask = df[main_col].isin(["", "nan", "None"])
            valid_text_only_mask = df[text_only_col].apply(is_valid_value)
            rows_to_update = blank_main_mask & valid_text_only_mask
            if rows_to_update.sum() > 0:
                print(f"Transferring {rows_to_update.sum()} values from {text_only_col} to {main_col}")
            df.loc[rows_to_update, main_col] = df.loc[rows_to_update, text_only_col]
        df = df.drop(columns=[text_only_col], errors="ignore")
    return df


def _normalize_sheet_name(value):
    return "".join(ch.lower() for ch in str(value) if ch.isalnum())


def _is_ignore_marker(sheet_name):
    return IGNORE_MARKER_RE.fullmatch(str(sheet_name).strip()) is not None


def eligible_sheet_names(sheet_names):
    eligible = []
    for sheet_name in sheet_names:
        if _is_ignore_marker(sheet_name):
            break
        eligible.append(sheet_name)
    return eligible


def resolve_sheet_name(sheet_names, desired_name):
    """Resolve a desired name only within the already eligible sheet set."""
    if desired_name in sheet_names:
        return desired_name

    desired_lower = desired_name.lower()
    for sheet_name in sheet_names:
        if sheet_name.lower() == desired_lower:
            return sheet_name

    desired_norm = _normalize_sheet_name(desired_name)
    for sheet_name in sheet_names:
        if _normalize_sheet_name(sheet_name) == desired_norm:
            return sheet_name

    tokens = [
        token
        for token in re.split(r"[^A-Za-z0-9]+", desired_name.lower())
        if token
    ]
    for sheet_name in sheet_names:
        sheet_lower = sheet_name.lower()
        if all(token in sheet_lower for token in tokens):
            return sheet_name

    return None


def _matches_standalone_hospitality_filename(xlsx_file):
    stem = Path(xlsx_file).stem
    normalized_stem = re.sub(r"[\s_-]+", " ", stem).strip()
    return STANDALONE_HOSPITALITY_RE.match(normalized_stem) is not None


def _cell_has_value(value):
    return value is not None and str(value).strip() != ""


def _validate_and_order_schema(df, xlsx_file, sheet_name):
    duplicate_columns = [
        str(column)
        for column in df.columns[df.columns.duplicated()].tolist()
    ]
    if duplicate_columns:
        raise ValueError(
            f"Schema mismatch in '{Path(xlsx_file).name}' / '{sheet_name}': "
            f"duplicate columns after normalization: {duplicate_columns}"
        )

    missing = [column for column in CANONICAL_COLUMNS if column not in df.columns]
    unexpected = [column for column in df.columns if column not in CANONICAL_COLUMNS]
    if missing or unexpected:
        details = []
        if missing:
            details.append(f"missing required columns: {missing}")
        if unexpected:
            details.append(f"unexpected columns: {unexpected}")
        raise ValueError(
            f"Schema mismatch in '{Path(xlsx_file).name}' / '{sheet_name}': "
            + "; ".join(details)
        )

    return df.loc[:, CANONICAL_COLUMNS].copy()


def read_and_normalize_sheet(
    xlsx_file,
    worksheet,
    allow_missing_header=False,
):
    sheet_name = worksheet.title
    header = None
    data_rows = []
    saw_content = False

    for raw_row in worksheet.iter_rows(values_only=True):
        row = list(raw_row)
        if any(_cell_has_value(value) for value in row):
            saw_content = True

        if header is None:
            row_values = [str(value).strip() for value in row if value is not None]
            if "Customer Number" in row_values:
                header = [
                    str(value).strip() if value is not None else ""
                    for value in row
                ]
            continue

        if any(_cell_has_value(value) for value in row):
            normalized_row = row[: len(header)]
            if len(normalized_row) < len(header):
                normalized_row.extend([None] * (len(header) - len(normalized_row)))
            data_rows.append(normalized_row)

    if header is None:
        if allow_missing_header:
            return None, saw_content
        raise ValueError(
            f"'Customer Number' header not found in sheet '{sheet_name}' "
            f"of '{Path(xlsx_file).name}'"
        )

    duplicate_source_columns = [
        column for column in set(header) if column and header.count(column) > 1
    ]
    if duplicate_source_columns:
        raise ValueError(
            f"Schema mismatch in '{Path(xlsx_file).name}' / '{sheet_name}': "
            f"duplicate source columns: {sorted(duplicate_source_columns)}"
        )

    df = pd.DataFrame(data_rows, columns=header)
    if "Customer Number" not in df.columns:
        raise ValueError(
            f"Column 'Customer Number' not found after header normalization in "
            f"'{Path(xlsx_file).name}' / '{sheet_name}'"
        )

    df = df[df["Customer Number"].apply(is_valid_customer_number)].copy()
    df = df.rename(columns=COLUMN_RENAME)

    df = handle_text_only_column(df, "State", TEXT_ONLY_STATE_COL)
    df = handle_text_only_column(df, "Country", TEXT_ONLY_COUNTRY_COL)
    df = df.drop(
        columns=[column for column in COLUMNS_TO_REMOVE if column in df.columns],
        errors="ignore",
    )

    for column in df.select_dtypes(include=["object"]).columns:
        df[column] = (
            df[column]
            .astype(str)
            .str.encode("utf-8", "ignore")
            .str.decode("utf-8", "ignore")
        )

    df = df.dropna(how="all").reset_index(drop=True)
    return _validate_and_order_schema(df, xlsx_file, sheet_name), True


def _warn_unusual_standalone_segments(df, xlsx_file, sheet_name):
    expected = {
        "Primary Segment": "Design-Commercial",
        "Secondary Segment": "Hospitality",
    }
    for column, expected_value in expected.items():
        unusual = sorted(
            {
                str(value).strip()
                for value in df[column].tolist()
                if str(value).strip() and str(value).strip() != expected_value
            }
        )
        if unusual:
            print(
                f"WARNING: Standalone Hospitality source '{Path(xlsx_file).name}' / "
                f"'{sheet_name}' contains unusual {column} values: {unusual}. "
                "The rows remain classified as HOSPITALITY."
            )


def classify_workbook(xlsx_file, verbose=True):
    residential_dfs = []
    hospitality_dfs = []

    if verbose:
        print(f"Reading: {Path(xlsx_file).name}")

    try:
        workbook = openpyxl.load_workbook(
            xlsx_file,
            read_only=True,
            data_only=True,
        )
    except Exception as exc:
        raise ValueError(
            f"Unable to open FOUR HANDS workbook '{Path(xlsx_file).name}': {exc}"
        ) from exc

    try:
        all_sheet_names = workbook.sheetnames
        eligible_names = eligible_sheet_names(all_sheet_names)
        ignored_names = all_sheet_names[len(eligible_names) :]
        if ignored_names and _is_ignore_marker(ignored_names[0]) and verbose:
            print(
                f"Ignoring marker and sheets to its right in '{Path(xlsx_file).name}': "
                + ", ".join(ignored_names)
            )

        desired_mappings = [
            (SHEET_RESIDENTIAL, VERSION_RESIDENTIAL),
            (SHEET_COMMERCIAL, VERSION_HOSPITALITY),
            (SHEET_NEW_ADDRESSES, VERSION_HOSPITALITY),
        ]
        classified_sheets = {}
        for desired_name, version in desired_mappings:
            actual_name = resolve_sheet_name(eligible_names, desired_name)
            if not actual_name:
                continue
            existing_version = classified_sheets.get(actual_name)
            if existing_version and existing_version != version:
                raise ValueError(
                    f"Conflicting sheet classification in '{Path(xlsx_file).name}': "
                    f"'{actual_name}' matched multiple versions"
                )
            classified_sheets[actual_name] = version

        if classified_sheets:
            for sheet_name in eligible_names:
                version = classified_sheets.get(sheet_name)
                if not version:
                    continue
                df, _ = read_and_normalize_sheet(
                    xlsx_file,
                    workbook[sheet_name],
                )
                if df.empty:
                    print(
                        f"WARNING: Eligible sheet '{Path(xlsx_file).name}' / "
                        f"'{sheet_name}' contains no valid Customer Number rows."
                    )
                    continue
                if version == VERSION_RESIDENTIAL:
                    residential_dfs.append(df)
                else:
                    hospitality_dfs.append(df)
                if verbose:
                    print(
                        f"Classified '{Path(xlsx_file).name}' / '{sheet_name}' "
                        f"as {version}: {len(df)} valid rows"
                    )
            return residential_dfs, hospitality_dfs

        if not eligible_names:
            if verbose:
                print(
                    f"WARNING: '{Path(xlsx_file).name}' has no eligible sheets before "
                    "the IGNORE >>> marker."
                )
            return residential_dfs, hospitality_dfs

        if not _matches_standalone_hospitality_filename(xlsx_file):
            if verbose:
                print(
                    f"WARNING: No recognized eligible FOUR HANDS sheets found in "
                    f"'{Path(xlsx_file).name}'; standalone Hospitality fallback did not match."
                )
            return residential_dfs, hospitality_dfs

        fallback_candidates = []
        ambiguous_sheets = []
        for sheet_name in eligible_names:
            df, has_content = read_and_normalize_sheet(
                xlsx_file,
                workbook[sheet_name],
                allow_missing_header=True,
            )
            if df is None:
                if has_content:
                    ambiguous_sheets.append(sheet_name)
                continue
            if df.empty:
                print(
                    f"WARNING: Standalone Hospitality candidate "
                    f"'{Path(xlsx_file).name}' / '{sheet_name}' contains no valid "
                    "Customer Number rows."
                )
                continue
            fallback_candidates.append((sheet_name, df))

        if ambiguous_sheets:
            raise ValueError(
                f"Ambiguous standalone Hospitality workbook '{Path(xlsx_file).name}': "
                "eligible data-bearing sheets without the expected FOUR HANDS header: "
                + ", ".join(ambiguous_sheets)
            )
        if len(fallback_candidates) > 1:
            raise ValueError(
                f"Ambiguous standalone Hospitality workbook '{Path(xlsx_file).name}': "
                "more than one eligible data-bearing FOUR HANDS sheet was found: "
                + ", ".join(name for name, _ in fallback_candidates)
            )
        if not fallback_candidates:
            return residential_dfs, hospitality_dfs

        sheet_name, hospitality_df = fallback_candidates[0]
        _warn_unusual_standalone_segments(
            hospitality_df,
            xlsx_file,
            sheet_name,
        )
        hospitality_dfs.append(hospitality_df)
        if verbose:
            print(
                f"Classified standalone Hospitality source '{Path(xlsx_file).name}' / "
                f"'{sheet_name}': {len(hospitality_df)} valid rows"
            )
        return residential_dfs, hospitality_dfs
    finally:
        workbook.close()


def list_source_workbooks(source_dir):
    source_dir = Path(source_dir)
    if not source_dir.exists() or not source_dir.is_dir():
        return []
    return sorted(
        [
            path
            for path in source_dir.iterdir()
            if path.is_file() and path.suffix.casefold() == ".xlsx"
        ],
        key=lambda path: (path.name.casefold(), str(path).casefold()),
    )


def _sha256_file(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def validate_unique_workbook_hashes(xlsx_files):
    paths_by_hash = {}
    for path in xlsx_files:
        try:
            digest = _sha256_file(path)
        except OSError as exc:
            raise ValueError(
                f"Unable to hash FOUR HANDS workbook '{Path(path).name}': {exc}"
            ) from exc
        paths_by_hash.setdefault(digest, []).append(Path(path))

    duplicate_groups = [
        (digest, paths)
        for digest, paths in paths_by_hash.items()
        if len(paths) > 1
    ]
    if not duplicate_groups:
        return

    details = []
    for digest, paths in duplicate_groups:
        details.append(
            f"SHA-256 {digest}: " + ", ".join(str(path) for path in paths)
        )
    raise ValueError(
        "Exact duplicate FOUR HANDS source workbooks were found in ORIGINAL, possibly "
        "under renamed filenames. Remove the duplicate before retrying. "
        + " | ".join(details)
    )


def classify_workbooks(xlsx_files, verbose=True):
    residential_dfs = []
    hospitality_dfs = []
    for xlsx_file in sorted(
        [Path(path) for path in xlsx_files],
        key=lambda path: (path.name.casefold(), str(path).casefold()),
    ):
        file_residential, file_hospitality = classify_workbook(
            xlsx_file,
            verbose=verbose,
        )
        residential_dfs.extend(file_residential)
        hospitality_dfs.extend(file_hospitality)
    return residential_dfs, hospitality_dfs


def classify_source_directory(source_dir, verbose=True):
    xlsx_files = list_source_workbooks(source_dir)
    validate_unique_workbook_hashes(xlsx_files)
    residential_dfs, hospitality_dfs = classify_workbooks(
        xlsx_files,
        verbose=verbose,
    )
    return xlsx_files, residential_dfs, hospitality_dfs


def detected_versions(residential_dfs, hospitality_dfs):
    versions = []
    if residential_dfs:
        versions.append(VERSION_RESIDENTIAL)
    if hospitality_dfs:
        versions.append(VERSION_HOSPITALITY)
    return versions


def emit_detection_result(status, versions=None, error=""):
    result = {
        "status": status,
        "versions": versions or [],
    }
    if error:
        result["error"] = error

    print(DETECTION_BEGIN_MARKER)
    print(json.dumps(result, separators=(",", ":")))
    print(DETECTION_END_MARKER)


def detection_source_from_args(args):
    if args == [DETECTION_MODE]:
        return SOURCE_DIR
    if (
        len(args) == 3
        and args[0] == DETECTION_MODE
        and args[1] == DETECTION_SOURCE_OPTION
    ):
        return Path(args[2])
    raise ValueError(
        f"Usage: {os.path.basename(sys.argv[0])} {DETECTION_MODE} "
        f"[{DETECTION_SOURCE_OPTION} <directory>]"
    )


def run_detection_only(args):
    try:
        source_dir = detection_source_from_args(args)
        _, residential_dfs, hospitality_dfs = classify_source_directory(
            source_dir,
            verbose=False,
        )
        emit_detection_result(
            "ok",
            detected_versions(residential_dfs, hospitality_dfs),
        )
        return 0
    except Exception as exc:
        emit_detection_result("error", error=str(exc))
        traceback.print_exc()
        return 1


def _normalized_customer_key(value):
    try:
        number = float(str(value).strip())
        if number.is_integer():
            return str(int(number))
    except (ValueError, TypeError):
        pass
    return str(value).strip()


def _warn_customer_number_duplicates(df, version):
    keys = df["Customer Number"].apply(_normalized_customer_key)
    duplicate_mask = keys.duplicated(keep=False)
    if not duplicate_mask.any():
        return
    duplicate_keys = keys[duplicate_mask]
    print(
        f"WARNING: {version} contains {len(duplicate_keys)} rows across "
        f"{duplicate_keys.nunique()} duplicate Customer Number value(s). "
        "Duplicate records are preserved."
    )


def build_combined_outputs(residential_dfs, hospitality_dfs):
    buckets = {
        VERSION_RESIDENTIAL: residential_dfs,
        VERSION_HOSPITALITY: hospitality_dfs,
    }
    combined_outputs = {}
    for version in VERSION_ORDER:
        dataframes = buckets[version]
        if not dataframes:
            continue
        combined = pd.concat(dataframes, ignore_index=True).dropna(how="all")
        combined = combined.loc[:, CANONICAL_COLUMNS].reset_index(drop=True)
        if combined.empty:
            continue
        _warn_customer_number_duplicates(combined, version)
        combined_outputs[version] = combined
    return combined_outputs


def _stage_dataframe(df, final_path):
    final_path = Path(final_path)
    final_path.parent.mkdir(parents=True, exist_ok=True)
    file_descriptor, temp_name = tempfile.mkstemp(
        prefix=".goji_fourhands_",
        suffix=".csv.tmp",
        dir=str(final_path.parent),
    )
    temp_path = Path(temp_name)
    try:
        with os.fdopen(
            file_descriptor,
            "w",
            encoding="utf-8-sig",
            newline="",
        ) as staged_file:
            df.to_csv(staged_file, index=False)
            staged_file.flush()
            os.fsync(staged_file.fileno())

        with temp_path.open("r", encoding="utf-8-sig", newline="") as check_file:
            reader = csv.reader(check_file)
            header = next(reader, None)
            rows = list(reader)
        if header != CANONICAL_COLUMNS:
            raise ValueError(
                f"Staged output header verification failed for {final_path}: {header}"
            )
        if len(rows) != len(df):
            raise ValueError(
                f"Staged output row-count verification failed for {final_path}: "
                f"expected {len(df)}, found {len(rows)}"
            )
        if any(row == header for row in rows):
            raise ValueError(
                f"Staged output contains an embedded duplicate header row: {final_path}"
            )
        if temp_path.stat().st_size <= 0:
            raise ValueError(f"Staged output is empty: {final_path}")
        return temp_path
    except Exception:
        temp_path.unlink(missing_ok=True)
        raise


def _stage_manifest(manifest, manifest_path):
    manifest_path = Path(manifest_path)
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    file_descriptor, temp_name = tempfile.mkstemp(
        prefix=".goji_fourhands_",
        suffix=".json.tmp",
        dir=str(manifest_path.parent),
    )
    temp_path = Path(temp_name)
    try:
        with os.fdopen(file_descriptor, "w", encoding="utf-8", newline="") as staged_file:
            json.dump(manifest, staged_file, indent=2)
            staged_file.write("\n")
            staged_file.flush()
            os.fsync(staged_file.fileno())
        loaded = json.loads(temp_path.read_text(encoding="utf-8"))
        if loaded != manifest:
            raise ValueError("Staged FOUR HANDS manifest verification failed")
        return temp_path
    except Exception:
        temp_path.unlink(missing_ok=True)
        raise


def commit_generated_state(
    combined_outputs,
    manifest,
    res_output_file=RES_OUTPUT_FILE,
    hosp_output_file=HOSP_OUTPUT_FILE,
    manifest_file=MANIFEST_FILE,
):
    output_paths = {
        VERSION_RESIDENTIAL: Path(res_output_file),
        VERSION_HOSPITALITY: Path(hosp_output_file),
    }
    staged_outputs = {}
    staged_manifest = None
    try:
        for version in VERSION_ORDER:
            dataframe = combined_outputs.get(version)
            if dataframe is not None:
                staged_outputs[version] = _stage_dataframe(
                    dataframe,
                    output_paths[version],
                )
        staged_manifest = _stage_manifest(manifest, manifest_file)

        for version in VERSION_ORDER:
            staged_path = staged_outputs.get(version)
            if staged_path is not None:
                os.replace(staged_path, output_paths[version])
                print(f"{version} OUTPUT: {output_paths[version]}")

        for version in VERSION_ORDER:
            if version not in combined_outputs and output_paths[version].exists():
                output_paths[version].unlink()
                print(f"Removed stale generated {version} input: {output_paths[version]}")

        os.replace(staged_manifest, manifest_file)
        print(f"Manifest created: {manifest_file}")
    finally:
        for staged_path in staged_outputs.values():
            Path(staged_path).unlink(missing_ok=True)
        if staged_manifest is not None:
            Path(staged_manifest).unlink(missing_ok=True)


def process_initial(
    source_dir=SOURCE_DIR,
    res_output_file=RES_OUTPUT_FILE,
    hosp_output_file=HOSP_OUTPUT_FILE,
    manifest_file=MANIFEST_FILE,
):
    print("=== READING FILES ===")
    xlsx_files, residential_dfs, hospitality_dfs = classify_source_directory(
        source_dir,
        verbose=True,
    )
    if not xlsx_files:
        raise ValueError(f"No XLSX files found in source folder: {source_dir}")

    print("=== MERGING FILES ===")
    combined_outputs = build_combined_outputs(
        residential_dfs,
        hospitality_dfs,
    )
    versions_present = [
        version for version in VERSION_ORDER if version in combined_outputs
    ]
    if not versions_present:
        raise ValueError(
            "No supported non-empty FOUR HANDS version data was generated. "
            "Expected eligible Residential, Commercial, New Addresses, or an "
            "approved standalone Hospitality workbook."
        )

    manifest = build_manifest(versions_present)
    print("=== STAGING OUTPUTS ===")
    commit_generated_state(
        combined_outputs,
        manifest,
        res_output_file=res_output_file,
        hosp_output_file=hosp_output_file,
        manifest_file=manifest_file,
    )
    return versions_present, combined_outputs


def main():
    try:
        versions_present, _ = process_initial()
        print("=== COMPLETE ===")
        print("=== NAS_FOLDER_PATH ===")
        print(str(BASE_DIR))
        print("=== END_NAS_FOLDER_PATH ===")
        print(f"VERSIONS GENERATED: {', '.join(versions_present)}")
        print("PROCESS COMPLETE")
        return 0
    except Exception as exc:
        print("FATAL ERROR:", str(exc))
        traceback.print_exc()
        return 1


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == DETECTION_MODE:
        sys.exit(run_detection_only(sys.argv[1:]))
    sys.exit(main())
