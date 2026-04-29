import argparse
import json
import os
import sys

import pandas as pd


def clean_file_path(file_path):
    return file_path.strip().strip('"\'')


def read_file(file_path):
    file_ext = os.path.splitext(file_path)[1].lower()

    if file_ext in ['.txt', '.csv']:
        encodings = ['utf-8', 'latin1', 'iso-8859-1', 'cp1252']
        last_error = None
        for encoding in encodings:
            try:
                return pd.read_csv(file_path, encoding=encoding)
            except UnicodeDecodeError as e:
                last_error = e
                continue
            except Exception as e:
                last_error = e
                break
        if last_error:
            raise RuntimeError(f"Could not read text/csv file: {last_error}")
        raise RuntimeError("Could not read text/csv file")

    if file_ext in ['.xls', '.xlsx']:
        try:
            return pd.read_excel(file_path)
        except Exception as e:
            raise RuntimeError(f"Could not read excel file: {e}") from e

    raise RuntimeError(f"Unsupported file format: {file_ext}")


def save_file(df, file_path):
    file_ext = os.path.splitext(file_path)[1].lower()

    if file_ext in ['.txt', '.csv']:
        df.to_csv(file_path, index=False)
        return

    if file_ext in ['.xls', '.xlsx']:
        df.to_excel(file_path, index=False)
        return

    raise RuntimeError(f"Unsupported file format for save: {file_ext}")


def display_headers(df):
    print("\nCurrent Headers:")
    for idx, header in enumerate(df.columns, 1):
        print(f"{idx}: {header}")


def get_valid_numbers(max_num):
    while True:
        user_input = input("\nWHICH HEADERS WOULD YOU LIKE TO CHANGE? ").strip()
        numbers = [num.strip() for num in user_input.split(',')]

        try:
            numbers = [int(num) for num in numbers]
            if all(1 <= num <= max_num for num in numbers):
                return numbers
            else:
                print("Numbers must be within the range of available headers")
        except ValueError:
            print("Please enter valid numbers separated by commas")


def parse_changes_json(changes_json_arg):
    raw = changes_json_arg.strip()

    if os.path.exists(raw) and os.path.isfile(raw):
        with open(raw, 'r', encoding='utf-8') as f:
            raw = f.read()

    try:
        payload = json.loads(raw)
    except json.JSONDecodeError as e:
        raise RuntimeError(f"Invalid changes JSON payload: {e}") from e

    if not isinstance(payload, list):
        raise RuntimeError("Changes payload must be a JSON array")

    parsed = []
    for entry in payload:
        if not isinstance(entry, dict):
            continue

        if 'index' not in entry or 'name' not in entry:
            continue

        try:
            index = int(entry['index'])
        except (TypeError, ValueError):
            continue

        name = str(entry['name']).strip()
        if not name:
            continue

        parsed.append((index, name))

    return parsed


def cli_mode_headers(file_path):
    cleaned = clean_file_path(file_path)
    if not cleaned:
        print("ERROR: Missing file path")
        return 1

    if not os.path.exists(cleaned):
        print(f"ERROR: File not found: {cleaned}")
        return 1

    try:
        df = read_file(cleaned)
    except Exception as e:
        print(f"ERROR: {e}")
        return 1

    headers = [str(col) for col in df.columns.tolist()]
    print("HEADERS_JSON: " + json.dumps(headers, ensure_ascii=False))
    print(f"SUCCESS: Loaded {len(headers)} headers from {cleaned}")
    return 0


def cli_mode_apply(file_path, changes_json_arg):
    cleaned = clean_file_path(file_path)
    if not cleaned:
        print("ERROR: Missing file path")
        return 1

    if not os.path.exists(cleaned):
        print(f"ERROR: File not found: {cleaned}")
        return 1

    try:
        df = read_file(cleaned)
    except Exception as e:
        print(f"ERROR: {e}")
        return 1

    try:
        changes = parse_changes_json(changes_json_arg)
    except Exception as e:
        print(f"ERROR: {e}")
        return 1

    if not changes:
        print("WARNING: No header changes to apply.")
        return 0

    headers = df.columns.tolist()
    applied = []

    for idx, name in changes:
        if idx < 0 or idx >= len(headers):
            continue
        old_name = headers[idx]
        headers[idx] = name
        applied.append((idx + 1, str(old_name), name))

    if not applied:
        print("WARNING: No valid header indexes to apply.")
        return 0

    df.columns = headers

    try:
        save_file(df, cleaned)
    except Exception as e:
        print(f"ERROR: {e}")
        return 1

    print(f"SUCCESS: Saved {len(applied)} header change(s) to {cleaned}")
    for display_index, old_name, new_name in applied:
        print(f"CHANGED: {display_index}: '{old_name}' -> '{new_name}'")
    return 0


def interactive_mode():
    while True:
        file_path = input("Enter the path to your file: ")
        file_path = clean_file_path(file_path)

        if not os.path.exists(file_path):
            print("File not found")
            continue

        try:
            df = read_file(file_path)
        except Exception as e:
            print(f"Error reading file: {e}")
            continue

        while True:
            display_headers(df)
            numbers = get_valid_numbers(len(df.columns))

            old_headers = df.columns.tolist()
            for num in numbers:
                print(f"\n{num}.")
                new_name = input().strip()
                old_headers[num - 1] = new_name

            df.columns = old_headers
            display_headers(df)

            try:
                save_file(df, file_path)
                print(f"\nChanges saved to {file_path}")
            except Exception as e:
                print(f"Error saving file: {e}")

            while True:
                change_more = input("\nDO YOU WANT TO CHANGE ANY HEADERS? Y/N ").upper()
                if change_more in ['Y', 'N']:
                    break
                print("Please enter Y or N")

            if change_more == 'N':
                break

        while True:
            process_more = input("\nARE THERE ANY OTHER FILES YOU WANT TO PROCESS? Y/N ").upper()
            if process_more in ['Y', 'N']:
                break
            print("Please enter Y or N")

        if process_more == 'N':
            break

    while True:
        exit_input = input("\nPress X to terminate...").upper()
        if exit_input == 'X':
            sys.exit()


def parse_args(argv):
    parser = argparse.ArgumentParser(add_help=True)
    parser.add_argument('--mode', choices=['headers', 'apply'])
    parser.add_argument('--file')
    parser.add_argument('--changes-json')
    return parser.parse_args(argv)


def main(argv=None):
    if argv is None:
        argv = sys.argv[1:]

    if not argv:
        interactive_mode()
        return 0

    args = parse_args(argv)

    if not args.mode:
        interactive_mode()
        return 0

    if args.mode == 'headers':
        if not args.file:
            print("ERROR: --file is required for --mode headers")
            return 1
        return cli_mode_headers(args.file)

    if args.mode == 'apply':
        if not args.file:
            print("ERROR: --file is required for --mode apply")
            return 1
        if args.changes_json is None:
            print("ERROR: --changes-json is required for --mode apply")
            return 1
        return cli_mode_apply(args.file, args.changes_json)

    print("ERROR: Unsupported mode")
    return 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print("\nOperation canceled by user.")
        sys.exit(1)