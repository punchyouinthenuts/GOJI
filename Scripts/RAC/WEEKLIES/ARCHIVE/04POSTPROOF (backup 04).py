```python
import os
from pathlib import Path
import shutil
import zipfile
import argparse
import json
import csv
import pandas as pd
import sys
import tempfile

# Constants
JOB_TYPES = ['CBC', 'EXC', 'INACTIVE', 'NCWO', 'PREPIF']

# Helper Functions
def detect_delimiter(file_path):
    """Detects the delimiter used in a CSV file based on the first few lines."""
    with open(file_path, 'r', encoding='latin1') as file:
        lines = [file.readline() for _ in range(3)]
        for line in lines:
            if '\t' in line and ',' not in line:
                return '\t'
            elif ',' in line and '\t' not in line:
                return ','
        return ','

def create_quoted_temp_file(original_path):
    """Creates a temporary CSV file with proper quoting based on the detected delimiter."""
    delimiter = detect_delimiter(original_path)
    temp_dir = tempfile.mkdtemp()
    temp_path = os.path.join(temp_dir, os.path.basename(original_path))
    with open(original_path, 'r', newline='', encoding='latin1') as infile, \
         open(temp_path, 'w', newline='', encoding='latin1') as outfile:
        reader = csv.reader(infile, delimiter=delimiter)
        quoting = csv.QUOTE_MINIMAL if delimiter == '\t' else csv.QUOTE_ALL
        writer = csv.writer(outfile, delimiter=delimiter, quoting=quoting)
        for row in reader:
            writer.writerow(row)
    return temp_path, temp_dir

def create_temp_copy(original_path, new_name, temp_dir):
    """Creates a temporary copy of a file with a new name in the temp directory."""
    temp_path = temp_dir / new_name
    shutil.copy2(original_path, temp_path)
    return temp_path

def zip_files(temp_dir, zip_filename, files):
    """Creates a ZIP file from temporary files in the specified directory."""
    zip_filename.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(zip_filename, 'w', zipfile.ZIP_DEFLATED) as zipf:
        for file in files:
            zipf.write(file, file.name)

def count_input_records(base_path, job_type):
    """Counts the total number of records in input files for a given job type."""
    input_path = base_path / job_type / "JOB" / "INPUT"
    total = 0
    if input_path.exists():
        for file in input_path.glob("*.csv"):
            try:
                with open(file, 'r', encoding='latin1') as f:
                    total += sum(1 for row in csv.reader(f)) - 1  # Subtract header
            except Exception as e:
                print(f"Error counting input {file}: {e}")
    return total

def count_output_versions(base_path, job_type):
    """Counts output records by version (PR, CANC, US) for a given job type."""
    output_path = base_path / job_type / "JOB" / "OUTPUT"
    counts = {'PR': 0, 'CANC': 0, 'US': 0}
    files = {
        'CBC': ['CBC2WEEKLYREFORMAT.csv', 'CBC3WEEKLYREFORMAT.csv'],
        'EXC': ['EXC_OUTPUT.csv'],
        'INACTIVE': ['PR-PO.csv', 'PR-PU.csv', 'A-PO.csv', 'A-PU.csv', 'AT-PO.csv', 'AT-PU.csv'],
        'NCWO': ['1-A_OUTPUT.csv', '1-AP_OUTPUT.csv', '2-A_OUTPUT.csv', '2-AP_OUTPUT.csv'],
        'PREPIF': ['PRE_PIF.csv']
    }
    version_mappings = {
        'CBC': {'PR': 'PR', 'CANC': 'CANC', 'US': 'A'},
        'EXC': {'PR': 'PR', 'US': 'A'},
        'INACTIVE': {'PR': 'PR', 'US': 'A'},
        'NCWO': {'PR': 'PR', 'US': 'A'},
        'PREPIF': {'PR': 'PR', 'US': 'A'}
    }

    if not output_path.exists():
        return counts

    for file in files[job_type]:
        file_path = output_path / file
        if file_path.exists():
            temp_path, temp_dir = create_quoted_temp_file(file_path)
            try:
                df = pd.read_csv(temp_path, low_memory=False, encoding='latin1')
                if job_type == 'CBC':
                    version_col = 'Creative_Version_Cd'
                elif job_type == 'EXC':
                    version_col = df.columns[13]  # Adjust if column index changes
                elif job_type == 'INACTIVE':
                    version_col = 'Creative_Version_Cd'
                elif job_type == 'NCWO':
                    version_col = 'Creative_Version_Cd'
                elif job_type == 'PREPIF':
                    version_col = df.columns[24]  # Adjust if column index changes

                if version_col in df.columns:
                    for version, count in df[version_col].value_counts().items():
                        if pd.notna(version):
                            version_str = str(version).upper()
                            if version_mappings[job_type]['PR'] in version_str:
                                counts['PR'] += count
                            elif 'CANC' in version_mappings[job_type] and version_mappings[job_type]['CANC'] in version_str:
                                counts['CANC'] += count
                            elif version_mappings[job_type]['US'] in version_str:
                                counts['US'] += count
            except Exception as e:
                print(f"Error counting output {file_path}: {e}")
            finally:
                shutil.rmtree(temp_dir)
    return counts

def process_job_type(job_type, job_number, week_number, base_path, regen_files=None, version=None, postage_mapping=None):
    """Processes a job type, creating temporary proof files for zipping and generating counts."""
    combined_input = f"{job_number} {week_number}"
    source = base_path / job_type / "JOB"
    proof_directory = source / "PROOF"
    zip_folder = base_path / "WEEKLY" / "WEEKLY_ZIP"
    zip_suffix = {
        'CBC': "CBCPROOFS",
        'EXC': "EXCPROOFS",
        'INACTIVE': "INACTIVE PROOFS",
        'NCWO': "NCWO PROOFS",
        'PREPIF': "PREPIF PROOFS"
    }[job_type]

    # Create temporary directory
    temp_dir = Path(tempfile.mkdtemp())
    temp_files = []

    if regen_files and version:
        # Regeneration mode: process specific PDF files with version suffix
        zip_filename = zip_folder / f"{combined_input} {zip_suffix}_v{version}.zip"
        for file in regen_files:
            original_path = proof_directory / file
            if original_path.exists() and original_path.suffix.lower() == '.pdf':
                base_name = original_path.stem
                new_name = f"{base_name}_v{version}{original_path.suffix}"
                temp_file = create_temp_copy(original_path, new_name, temp_dir)
                print(f"Created temporary PDF: {temp_file}")
                temp_files.append(temp_file)
    else:
        # Normal mode: process all PDF and CSV files in PROOF with prefix
        zip_filename = zip_folder / f"{combined_input} {zip_suffix}.zip"
        for file_path in proof_directory.glob("*.pdf"):
            new_name = f"{combined_input}-{file_path.name}"
            temp_file = create_temp_copy(file_path, new_name, temp_dir)
            print(f"Created temporary PDF: {temp_file}")
            temp_files.append(temp_file)
        for file_path in proof_directory.glob("*.csv"):
            new_name = f"{combined_input}-{file_path.name}"
            temp_file = create_temp_copy(file_path, new_name, temp_dir)
            print(f"Created temporary CSV: {temp_file}")
            temp_files.append(temp_file)

    # Zip temporary files
    zip_files(temp_dir, zip_filename, temp_files)

    # Clean up temporary directory
    shutil.rmtree(temp_dir, ignore_errors=True)

    # Generate counts using original files
    input_count = count_input_records(base_path, job_type)
    output_counts = count_output_versions(base_path, job_type)
    total_output = sum(output_counts.values())
    difference = total_output - input_count

    # Prepare comparison and JSON data
    projects = {
        'CBC': ['CBC 2', 'CBC 3'],
        'EXC': ['EXC'],
        'INACTIVE': ['INACTIVE A-PO', 'INACTIVE A-PU'],
        'NCWO': ['NCWO 1-A', 'NCWO 1-AP', 'NCWO 2-A', 'NCWO 2-AP'],
        'PREPIF': ['PREPIF']
    }[job_type]

    comparison_data = []
    json_counts = []
    for project in projects:
        comparison_data.append({
            "group": project,
            "input_count": input_count,
            "output_count": total_output,
            "difference": difference
        })
        json_counts.append({
            "job_number": job_number,
            "week": week_number,
            "project": project,
            "pr_count": output_counts['PR'],
            "canc_count": output_counts['CANC'],
            "us_count": output_counts['US'],
            "postage": postage_mapping.get(project, 0.0) if postage_mapping else 0.0
        })

    # Output results
    print(f"\n{job_type} processing completed successfully!")
    print("\nComparison of Counts:")
    print("-" * 70)
    print("Group                          Input       Output      Difference")
    print("-" * 70)
    for data in comparison_data:
        error_flag = " [ERROR!]" if data["difference"] > 0 else ""
        print(f"{data['group']:<30} {data['input_count']:>7,d}     {data['output_count']:>7,d}     {data['difference']:>7,d}{error_flag}")

    output = {"counts": json_counts, "comparison": comparison_data}
    print("\n===JSON_START===")
    print(json.dumps(output, indent=2))
    print("===JSON_END===")

def main():
    parser = argparse.ArgumentParser(description="Post-Proof Processing for a single job type")
    parser.add_argument("--base_path", required=True, help="Base directory (e.g., C:\\Goji\\RAC)")
    parser.add_argument("--job_type", required=True, help="Job type to process (e.g., CBC)")
    parser.add_argument("--job_number", required=True, help="Job number for the job type")
    parser.add_argument("--week", required=True, help="Week number in MM.DD format")
    parser.add_argument("--proof_files", nargs='+', help="List of specific proof files to process (optional)")
    parser.add_argument("--version", type=int, help="Version number for regeneration (optional, min 2)")
    # Add postage arguments
    parser.add_argument("--cbc2_postage", type=float, default=0.0, help="Postage for CBC 2")
    parser.add_argument("--cbc3_postage", type=float, default=0.0, help="Postage for CBC 3")
    parser.add_argument("--exc_postage", type=float, default=0.0, help="Postage for EXC")
    parser.add_argument("--inactive_po_postage", type=float, default=0.0, help="Postage for INACTIVE A-PO")
    parser.add_argument("--inactive_pu_postage", type=float, default=0.0, help="Postage for INACTIVE A-PU")
    parser.add_argument("--ncwo1_a_postage", type=float, default=0.0, help="Postage for NCWO 1-A")
    parser.add_argument("--ncwo2_a_postage", type=float, default=0.0, help="Postage for NCWO 2-A")
    parser.add_argument("--ncwo1_ap_postage", type=float, default=0.0, help="Postage for NCWO 1-AP")
    parser.add_argument("--ncwo2_ap_postage", type=float, default=0.0, help="Postage for NCWO 2-AP")
    parser.add_argument("--prepif_postage", type=float, default=0.0, help="Postage for PREPIF")
    args = parser.parse_args()

    base_path = Path(args.base_path)
    if args.job_type not in JOB_TYPES:
        print(f"Invalid job type: {args.job_type}. Must be one of {JOB_TYPES}")
        sys.exit(1)
    if args.version and args.version < 2:
        print("Version number must be 2 or higher for regeneration.")
        sys.exit(1)

    # Create postage mapping
    postage_mapping = {
        'CBC 2': args.cbc2_postage,
        'CBC 3': args.cbc3_postage,
        'EXC': args.exc_postage,
        'INACTIVE A-PO': args.inactive_po_postage,
        'INACTIVE A-PU': args.inactive_pu_postage,
        'NCWO 1-A': args.ncwo1_a_postage,
        'NCWO 2-A': args.ncwo2_a_postage,
        'NCWO 1-AP': args.ncwo1_ap_postage,
        'NCWO 2-AP': args.ncwo2_ap_postage,
        'PREPIF': args.prepif_postage
    }

    process_job_type(
        args.job_type,
        args.job_number,
        args.week,
        base_path,
        args.proof_files,
        args.version,
        postage_mapping
    )

if __name__ == "__main__":
    main()
```