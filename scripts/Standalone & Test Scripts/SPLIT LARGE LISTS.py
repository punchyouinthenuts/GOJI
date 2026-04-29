import argparse
import json
import os
import sys

import pandas as pd


def clean_path(path_value):
    return path_value.strip().strip('"').strip("'")


def validate_input_file(file_path):
    cleaned = clean_path(file_path)
    if not cleaned:
        return False, "File path is required."

    if not os.path.exists(cleaned):
        return False, "File does not exist."

    ext = os.path.splitext(cleaned)[1].lower()
    if ext not in [".xls", ".xlsx", ".csv"]:
        return False, "File must be XLS, XLSX, or CSV."

    return True, cleaned


def read_spreadsheet(file_path):
    ext = os.path.splitext(file_path)[1].lower()
    try:
        if ext in [".xls", ".xlsx"]:
            data_frame = pd.read_excel(file_path, sheet_name=0)
        else:
            data_frame = pd.read_csv(file_path)
    except Exception as error:
        raise RuntimeError(f"Error reading file: {error}") from error

    return data_frame, len(data_frame)


def calculate_split_sizes(total_records, parts):
    base_size = total_records // parts
    remainder = total_records % parts
    return [base_size + 1 if i < remainder else base_size for i in range(parts)]


def split_dataframe(data_frame, parts):
    sizes = calculate_split_sizes(len(data_frame), parts)
    splits = []
    start_index = 0
    for size in sizes:
        splits.append(data_frame.iloc[start_index:start_index + size])
        start_index += size
    return splits, sizes


def save_splits(splits, sizes, base_name, output_dir):
    outputs = []
    for index, (split_df, size) in enumerate(zip(splits, sizes), 1):
        output_file = os.path.join(output_dir, f"{base_name} {index:02d}.csv")
        split_df.to_csv(output_file, index=False)
        outputs.append({
            "part": index,
            "path": output_file,
            "records": int(size),
        })
    return outputs


def split_option_to_parts(option):
    if option == "1":
        return 2
    if option == "2":
        return 3
    if option == "3":
        return 4
    return 0


def cli_inspect(input_file):
    is_valid, result = validate_input_file(input_file)
    if not is_valid:
        print(f"ERROR: {result}")
        return 1

    try:
        _, record_count = read_spreadsheet(result)
    except Exception as error:
        print(f"ERROR: {error}")
        return 1

    payload = {
        "file": result,
        "file_name": os.path.basename(result),
        "base_name": os.path.splitext(os.path.basename(result))[0],
        "record_count": int(record_count),
    }
    print("SPLIT_INFO_JSON: " + json.dumps(payload, ensure_ascii=False))
    print(f"SUCCESS: Loaded {record_count} record(s) from {result}")
    return 0


def cli_split(input_file, parts, output_dir, base_name):
    is_valid, result = validate_input_file(input_file)
    if not is_valid:
        print(f"ERROR: {result}")
        return 1

    if parts not in [2, 3, 4]:
        print("ERROR: --parts must be 2, 3, or 4.")
        return 1

    cleaned_output_dir = clean_path(output_dir)
    if not cleaned_output_dir:
        print("ERROR: --output-dir is required.")
        return 1

    if not os.path.isdir(cleaned_output_dir):
        print(f"ERROR: Output directory does not exist: {cleaned_output_dir}")
        return 1

    resolved_base_name = clean_path(base_name) if base_name else ""
    if not resolved_base_name:
        resolved_base_name = os.path.splitext(os.path.basename(result))[0]

    try:
        data_frame, record_count = read_spreadsheet(result)
        splits, sizes = split_dataframe(data_frame, parts)
        outputs = save_splits(splits, sizes, resolved_base_name, cleaned_output_dir)
    except Exception as error:
        print(f"ERROR: {error}")
        return 1

    for output in outputs:
        print(f"{os.path.basename(output['path'])}: {output['records']} records")

    result_payload = {
        "file": result,
        "parts": int(parts),
        "record_count": int(record_count),
        "output_dir": cleaned_output_dir,
        "base_name": resolved_base_name,
        "outputs": outputs,
    }
    print("SPLIT_RESULT_JSON: " + json.dumps(result_payload, ensure_ascii=False))
    print("SUCCESS: Split completed.")
    return 0


def run_interactive():
    while True:
        file_path = input("ENTER LIST TO SCAN: ")
        is_valid, result = validate_input_file(file_path)
        if not is_valid:
            print(result)
            continue

        try:
            data_frame, record_count = read_spreadsheet(result)
        except Exception as error:
            print(error)
            continue

        print(f"TOTAL RECORD COUNT: {record_count}")
        print("HOW DO YOU WANT TO SPLIT THE LIST?")
        print("1) ½")
        print("2) ⅓")
        print("3) ¼")
        split_option = input("ENTER NUMBER: ").strip()
        parts = split_option_to_parts(split_option)
        if parts == 0:
            print("Invalid option. Please enter 1, 2, or 3.")
            continue

        base_name = input("ENTER FILE NAME: ").strip()
        if not base_name:
            print("File name is required.")
            continue

        output_dir = os.path.dirname(result)

        try:
            splits, sizes = split_dataframe(data_frame, parts)
            outputs = save_splits(splits, sizes, base_name, output_dir)
        except Exception as error:
            print(f"Error saving files: {error}")
            continue

        for output in outputs:
            print(f"{os.path.basename(output['path'])}: {output['records']} records")

        again = input("DO YOU WANT TO PROCESS ANOTHER LIST? Y/N: ").strip().upper()
        if again != "Y":
            break


def parse_args(argv):
    parser = argparse.ArgumentParser(add_help=True)
    parser.add_argument("--mode", choices=["inspect", "split"])
    parser.add_argument("--file")
    parser.add_argument("--parts", type=int)
    parser.add_argument("--output-dir")
    parser.add_argument("--base-name")
    return parser.parse_args(argv)


def main(argv=None):
    if argv is None:
        argv = sys.argv[1:]

    if not argv:
        run_interactive()
        return 0

    args = parse_args(argv)
    if args.mode == "inspect":
        if not args.file:
            print("ERROR: --file is required for --mode inspect")
            return 1
        return cli_inspect(args.file)

    if args.mode == "split":
        if not args.file:
            print("ERROR: --file is required for --mode split")
            return 1
        if args.parts is None:
            print("ERROR: --parts is required for --mode split")
            return 1
        if not args.output_dir:
            print("ERROR: --output-dir is required for --mode split")
            return 1
        return cli_split(args.file, args.parts, args.output_dir, args.base_name)

    print("ERROR: Unsupported mode")
    return 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print("\nOperation canceled by user.")
        sys.exit(1)
