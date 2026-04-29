#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""
Combine selected CSVs or Excel (XLS/XLSX) by header name (case-insensitive),
union all columns, and insert a unique 6-char uppercase alphanumeric ID column
as the first column.

Interactive mode (legacy):
- Default directory prompt/loop
- File type chooser (Excel-only or CSV-only)
- Numbered file selection + confirmations
- CSV flow writes COMBINED.csv
- Excel flow writes COMBINED.xlsx

GOJI CLI mode:
- Accepts explicit input files via --input-files
- Supports mixed CSV/XLS/XLSX in one run
- Always writes CSV to --output-file
- No terminal prompts
"""
import argparse
import csv
import random
import re
import string
import sys
from pathlib import Path
from typing import Dict, Iterable, List, Set, Tuple

# Optional import for Excel; imported lazily where needed
try:
    import pandas as pd  # type: ignore
except Exception:
    pd = None

RANDOM_ALPHABET = string.ascii_uppercase + string.digits
PREFERRED_ENCODINGS = ("cp1252", "utf-8-sig", "utf-8", "latin-1", "utf-16", "utf-16le", "utf-16be")
DEFAULT_DIR = r"C:\Users\JCox\Downloads"
ALLOWED_INPUT_EXTENSIONS = {".csv", ".xls", ".xlsx"}


def input_nonempty(prompt: str) -> str:
    while True:
        try:
            s = input(prompt).strip()
        except EOFError:
            print("\nNo input received. Exiting.")
            sys.exit(1)
        if s:
            return s


def clean_dir_path(p: str) -> str:
    return p.strip().strip('"').strip("'")


def dir_exists(path_str: str) -> bool:
    p = Path(path_str)
    return p.exists() and p.is_dir()


def normalize_header(h: str) -> str:
    return (h or "").strip().lower()


def generate_unique_ids(n: int) -> List[str]:
    ids: Set[str] = set()
    out: List[str] = []
    while len(out) < n:
        token = "".join(random.choices(RANDOM_ALPHABET, k=6))
        if token not in ids:
            ids.add(token)
            out.append(token)
    return out


def list_files_by_ext(dir_path: str, exts: Tuple[str, ...]) -> List[Path]:
    p = Path(dir_path)
    if not p.exists() or not p.is_dir():
        print(f"ERROR: '{dir_path}' is not a valid directory.")
        return []
    exts_lc = tuple(e.lower() for e in exts)
    files = [f for f in sorted(p.iterdir()) if f.is_file() and f.suffix.lower() in exts_lc]
    return files


def list_csv_files(dir_path: str) -> List[Path]:
    return list_files_by_ext(dir_path, (".csv",))


def list_excel_files(dir_path: str) -> List[Path]:
    return list_files_by_ext(dir_path, (".xls", ".xlsx"))


def parse_selection(max_index: int) -> List[int]:
    while True:
        raw = input_nonempty("Enter the numbers of the files to combine (e.g., 1,2,3): ")
        parts = re.split(r"[,\s]+", raw.strip().strip(','))
        nums = []
        ok = True
        for part in parts:
            if not part:
                continue
            if not part.isdigit():
                ok = False
                break
            n = int(part)
            if n < 1 or n > max_index:
                ok = False
                break
            nums.append(n)
        if not ok or not nums:
            print("Invalid input. Please enter numbers from the list, separated by commas.")
            continue
        unique_nums = sorted(set(nums), key=lambda x: nums.index(x))
        print("You selected:", ", ".join(map(str, unique_nums)))
        yn = input_nonempty("Proceed? (Y/N): ").strip().lower()
        if yn in ("y", "yes"):
            return unique_nums
        print("\nOkay, let's try again.\n")


def try_read_csv_with_encodings(path: Path, encodings: Iterable[str] = None) -> Tuple[str, List[str], List[Dict[str, str]]]:
    if encodings is None:
        encodings = PREFERRED_ENCODINGS
    last_err = None
    for enc in encodings:
        try:
            with open(path, "r", encoding=enc, newline="") as f:
                reader = csv.DictReader(f)
                fieldnames = list(reader.fieldnames) if reader.fieldnames else []
                rows = [row for row in reader]
            return enc, fieldnames, rows
        except Exception as e:
            last_err = e
    raise RuntimeError(f"Failed to decode/parse '{path.name}' with tried encodings: {list(encodings)}") from last_err


def build_canonical_headers_from_csv(selected_files: List[Path]) -> Tuple[List[str], List[Tuple[Path, str, List[str], List[Dict[str, str]]]]]:
    headers_ordered: List[str] = []
    seen_keys: Set[str] = set()
    file_info = []
    for i, path in enumerate(selected_files):
        enc, fields, rows = try_read_csv_with_encodings(path)
        if not fields:
            print(f"WARNING: '{path.name}' has no header row; skipping.")
            continue
        file_info.append((path, enc, fields, rows))
        if i == 0:
            for h in fields:
                key = normalize_header(h)
                if key not in seen_keys:
                    headers_ordered.append(h)
                    seen_keys.add(key)
        else:
            for h in fields:
                key = normalize_header(h)
                if key not in seen_keys:
                    headers_ordered.append(h)
                    seen_keys.add(key)
    return headers_ordered, file_info


def align_and_write_csv(output_path: Path, headers: List[str], file_info: List[Tuple[Path, str, List[str], List[Dict[str, str]]]]):
    headers_with_id = ["ID"] + headers
    key_to_canonical: Dict[str, str] = {normalize_header(h): h for h in headers}

    total_rows = sum(len(rows) for _, _, _, rows in file_info)
    ids = generate_unique_ids(total_rows)
    id_iter = iter(ids)

    with open(output_path, "w", encoding="utf-8-sig", newline="") as f_out:
        writer = csv.DictWriter(f_out, fieldnames=headers_with_id, quoting=csv.QUOTE_MINIMAL)
        writer.writeheader()
        written = 0
        for _, _, fieldnames, rows in file_info:
            file_field_to_canonical: Dict[str, str] = {}
            for fh in fieldnames:
                key = normalize_header(fh)
                file_field_to_canonical[fh] = key_to_canonical.get(key, fh)

            for row in rows:
                out_row = {h: "" for h in headers_with_id}
                out_row["ID"] = next(id_iter)
                for fh, val in row.items():
                    if fh is None:
                        continue
                    canon = file_field_to_canonical.get(fh)
                    if canon is None:
                        continue
                    out_row[canon] = val if val is not None else ""
                writer.writerow(out_row)
                written += 1
    print(f"Wrote {written} rows to: {output_path}")


def ensure_pandas_for_excel():
    if pd is None:
        raise RuntimeError(
            "Excel support requires 'pandas'. Please install with:\n"
            "  pip install pandas openpyxl xlrd==1.2.0"
        )


def try_read_excel_first_sheet(path: Path) -> Tuple[List[str], List[Dict[str, str]]]:
    ensure_pandas_for_excel()
    try:
        df = pd.read_excel(path, sheet_name=0, dtype=str)
    except ImportError as e:
        raise RuntimeError(
            f"Failed to read '{path.name}'. For .xlsx, install 'openpyxl'. "
            f"For .xls, install 'xlrd==1.2.0'.\nOriginal error: {e}"
        ) from e
    except Exception as e:
        raise RuntimeError(f"Failed to read '{path.name}': {e}") from e

    df = df.astype(str).where(df.notnull(), "")
    fieldnames = list(df.columns.astype(str))
    rows = [dict(zip(fieldnames, map(lambda x: "" if x is None else str(x), row))) for row in df.to_numpy()]
    return fieldnames, rows


def build_canonical_headers_from_excel(selected_files: List[Path]) -> Tuple[List[str], List[Tuple[Path, List[str], List[Dict[str, str]]]]]:
    headers_ordered: List[str] = []
    seen_keys: Set[str] = set()
    file_info = []
    for i, path in enumerate(selected_files):
        fields, rows = try_read_excel_first_sheet(path)
        if not fields:
            print(f"WARNING: '{path.name}' has no header row; skipping.")
            continue
        file_info.append((path, fields, rows))
        if i == 0:
            for h in fields:
                key = normalize_header(h)
                if key not in seen_keys:
                    headers_ordered.append(h)
                    seen_keys.add(key)
        else:
            for h in fields:
                key = normalize_header(h)
                if key not in seen_keys:
                    headers_ordered.append(h)
                    seen_keys.add(key)
    return headers_ordered, file_info


def align_and_write_excel(output_path: Path, headers: List[str], file_info: List[Tuple[Path, List[str], List[Dict[str, str]]]]):
    ensure_pandas_for_excel()

    headers_with_id = ["ID"] + headers
    key_to_canonical: Dict[str, str] = {normalize_header(h): h for h in headers}

    total_rows = sum(len(rows) for _, _, rows in file_info)
    ids = generate_unique_ids(total_rows)
    id_iter = iter(ids)

    combined_rows: List[Dict[str, str]] = []
    for _, fieldnames, rows in file_info:
        field_to_canonical: Dict[str, str] = {}
        for fh in fieldnames:
            key = normalize_header(fh)
            field_to_canonical[fh] = key_to_canonical.get(key, fh)

        for row in rows:
            out_row = {h: "" for h in headers_with_id}
            out_row["ID"] = next(id_iter)
            for fh, val in row.items():
                if fh is None:
                    continue
                canon = field_to_canonical.get(fh)
                if canon is None:
                    continue
                out_row[canon] = "" if val is None else str(val)
            combined_rows.append(out_row)

    df_out = pd.DataFrame(combined_rows, columns=headers_with_id)
    try:
        df_out.to_excel(output_path, index=False)
    except ImportError as e:
        raise RuntimeError(
            "Writing .xlsx requires 'openpyxl'. Install with:\n"
            "  pip install openpyxl"
        ) from e

    print(f"Wrote {len(combined_rows)} rows to: {output_path}")


def read_any_supported_file(path: Path) -> Tuple[List[str], List[Dict[str, str]]]:
    ext = path.suffix.lower()
    if ext == ".csv":
        _, fields, rows = try_read_csv_with_encodings(path)
        return fields, rows
    if ext in (".xls", ".xlsx"):
        return try_read_excel_first_sheet(path)
    raise RuntimeError(f"Unsupported file type: {path.name}")


def build_canonical_headers_from_mixed(selected_files: List[Path]) -> Tuple[List[str], List[Tuple[Path, List[str], List[Dict[str, str]]]]]:
    headers_ordered: List[str] = []
    seen_keys: Set[str] = set()
    file_info: List[Tuple[Path, List[str], List[Dict[str, str]]]] = []

    for path in selected_files:
        fields, rows = read_any_supported_file(path)
        if not fields:
            print(f"WARNING: '{path.name}' has no header row; skipping.")
            continue

        file_info.append((path, fields, rows))
        for h in fields:
            key = normalize_header(h)
            if key not in seen_keys:
                headers_ordered.append(h)
                seen_keys.add(key)

    return headers_ordered, file_info


def align_and_write_mixed_csv(output_path: Path,
                              headers: List[str],
                              file_info: List[Tuple[Path, List[str], List[Dict[str, str]]]]) -> None:
    headers_with_id = ["ID"] + headers
    key_to_canonical: Dict[str, str] = {normalize_header(h): h for h in headers}

    total_rows = sum(len(rows) for _, _, rows in file_info)
    ids = generate_unique_ids(total_rows)
    id_iter = iter(ids)

    with open(output_path, "w", encoding="utf-8-sig", newline="") as f_out:
        writer = csv.DictWriter(f_out, fieldnames=headers_with_id, quoting=csv.QUOTE_MINIMAL)
        writer.writeheader()

        written = 0
        for _, fieldnames, rows in file_info:
            file_field_to_canonical: Dict[str, str] = {}
            for fh in fieldnames:
                key = normalize_header(fh)
                file_field_to_canonical[fh] = key_to_canonical.get(key, fh)

            for row in rows:
                out_row = {h: "" for h in headers_with_id}
                out_row["ID"] = next(id_iter)
                for fh, val in row.items():
                    if fh is None:
                        continue
                    canon = file_field_to_canonical.get(fh)
                    if canon is None:
                        continue
                    out_row[canon] = "" if val is None else str(val)
                writer.writerow(out_row)
                written += 1

    print(f"Wrote {written} rows to: {output_path}")


def choose_directory() -> str:
    print(f"DEFAULT DIRECTORY: {DEFAULT_DIR}")
    while True:
        yn = input_nonempty("USE DEFAULT? Y/N ").strip().lower()
        if yn in ("y", "yes"):
            chosen = DEFAULT_DIR
        elif yn in ("n", "no"):
            while True:
                entered = clean_dir_path(input_nonempty("ENTER DIRECTORY: "))
                if not dir_exists(entered):
                    print("DIRECTORY DOES NOT EXIST")
                    continue
                conf = input_nonempty(f"CONFIRM DIRECTORY [{entered}] Y/N ").strip().lower()
                if conf in ("y", "yes"):
                    chosen = entered
                    break
        else:
            print("Please answer Y or N.")
            continue

        if yn in ("y", "yes"):
            conf = input_nonempty(f"CONFIRM DIRECTORY [{chosen}] Y/N ").strip().lower()
            if conf not in ("y", "yes"):
                continue
        return chosen


def choose_file_type() -> str:
    print("\n1) XLS/XLSX")
    print("2) CSV")
    while True:
        sel = input_nonempty("\nWHICH TYPE OF FILE SHOULD BE COMBINED? ").strip()
        if sel == "1":
            return "excel"
        if sel == "2":
            return "csv"
        print("Please enter 1 or 2.")


def main_interactive():
    print("=== Combine CSVs or Excel by Header (Case-Insensitive) & Add Unique 6-Char ID (v3) ===")

    dir_path = choose_directory()
    ftype = choose_file_type()

    if ftype == "csv":
        files = list_csv_files(dir_path)
        if not files:
            print("No CSV files found in that directory. Exiting.")
            sys.exit(1)
        print("\nCSV files found:")
        for idx, p in enumerate(files, start=1):
            print(f"{idx}. {p.name}")
        print("")
        selection = parse_selection(len(files))
        selected_files = [files[i - 1] for i in selection]

        print("\nYou chose to combine these files (in this order):")
        for p in selected_files:
            print(f"- {p.name}")
        yn = input_nonempty("Final confirmation? (Y/N): ").strip().lower()
        if yn not in ("y", "yes"):
            print("\nOkay, starting over.\n")
            return main_interactive()

        headers, file_info = build_canonical_headers_from_csv(selected_files)
        if not file_info:
            print("No readable CSVs were selected. Exiting.")
            sys.exit(1)

        out_path = Path(dir_path) / "COMBINED.csv"
        align_and_write_csv(out_path, headers, file_info)
        print("\nDone!")

    else:
        try:
            ensure_pandas_for_excel()
        except RuntimeError as e:
            print(str(e))
            sys.exit(1)

        files = list_excel_files(dir_path)
        if not files:
            print("No XLS/XLSX files found in that directory. Exiting.")
            sys.exit(1)
        print("\nExcel files found:")
        for idx, p in enumerate(files, start=1):
            print(f"{idx}. {p.name}")
        print("")
        selection = parse_selection(len(files))
        selected_files = [files[i - 1] for i in selection]

        print("\nYou chose to combine these files (in this order):")
        for p in selected_files:
            print(f"- {p.name}")
        yn = input_nonempty("Final confirmation? (Y/N): ").strip().lower()
        if yn not in ("y", "yes"):
            print("\nOkay, starting over.\n")
            return main_interactive()

        try:
            headers, file_info = build_canonical_headers_from_excel(selected_files)
        except RuntimeError as e:
            print(str(e))
            sys.exit(1)

        if not file_info:
            print("No readable Excel sheets were selected. Exiting.")
            sys.exit(1)

        out_path = Path(dir_path) / "COMBINED.xlsx"
        try:
            align_and_write_excel(out_path, headers, file_info)
        except RuntimeError as e:
            print(str(e))
            sys.exit(1)
        print("\nDone!")


def parse_cli_args(argv: List[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="GOJI mode: combine explicit input files into one CSV output."
    )
    parser.add_argument(
        "--input-files",
        nargs="+",
        required=True,
        help="One or more input files (.csv, .xls, .xlsx). Order is processing precedence.",
    )
    parser.add_argument(
        "--output-file",
        required=True,
        help="Output CSV file path.",
    )
    return parser.parse_args(argv)


def normalize_input_file_paths(raw_paths: List[str]) -> List[Path]:
    normalized: List[Path] = []
    for raw in raw_paths:
        cleaned = raw.strip().strip('"').strip("'")
        path = Path(cleaned)

        if not path.exists() or not path.is_file():
            raise RuntimeError(f"Input file not found: {cleaned}")

        ext = path.suffix.lower()
        if ext not in ALLOWED_INPUT_EXTENSIONS:
            raise RuntimeError(f"Unsupported input type '{ext}' for file: {cleaned}")

        normalized.append(path)

    return normalized


def run_cli_mode(argv: List[str]) -> int:
    try:
        args = parse_cli_args(argv)
        selected_files = normalize_input_file_paths(args.input_files)
        output_path = Path(args.output_file.strip().strip('"').strip("'"))
    except Exception as e:
        print(f"ERROR: {e}")
        return 1

    print("GOJI CLI mode: combining files in provided order:")
    for idx, file_path in enumerate(selected_files, start=1):
        print(f"{idx}. {file_path}")

    if any(p.suffix.lower() in (".xls", ".xlsx") for p in selected_files):
        try:
            ensure_pandas_for_excel()
        except RuntimeError as e:
            print(f"ERROR: {e}")
            return 1

    if output_path.exists():
        print(f"WARNING: Output file exists and will be overwritten: {output_path}")

    output_path.parent.mkdir(parents=True, exist_ok=True)

    try:
        headers, file_info = build_canonical_headers_from_mixed(selected_files)
    except Exception as e:
        print(f"ERROR: {e}")
        return 1

    if not file_info:
        print("ERROR: No readable input files were provided.")
        return 1

    try:
        align_and_write_mixed_csv(output_path, headers, file_info)
    except Exception as e:
        print(f"ERROR: {e}")
        return 1

    print("SUCCESS: Combine completed.")
    return 0


if __name__ == "__main__":
    try:
        if len(sys.argv) == 1:
            main_interactive()
        else:
            raise SystemExit(run_cli_mode(sys.argv[1:]))
    except KeyboardInterrupt:
        print("\nOperation canceled by user.")
        sys.exit(1)