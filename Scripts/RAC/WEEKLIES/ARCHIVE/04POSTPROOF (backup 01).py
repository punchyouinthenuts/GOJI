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
    """Detect the delimiter of the file by analyzing the first few lines."""
    with open(file_path, 'r', encoding='latin1') as file:
        lines = [file.readline() for _ in range(3)]
        for line in lines:
            if '\t' in line and ',' not in line:
                return '\t'
            elif ',' in line and '\t' not in line:
                return ','
        return ','  # Fallback to comma if unclear

def create_quoted_temp_file(original_path):
    """Create a temporary file with appropriate delimiter handling."""
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

# File Processing Functions
def rename_files_in_directory(directory, prefix):
    """Rename files in the directory with the given prefix."""
    try:
        directory_path = Path(directory)
        if not directory_path.exists() or not any(directory_path.iterdir()):
            print(f"Directory {directory} is empty or doesn't exist")
            return
        for file_path in directory_path.iterdir():
            if file_path.is_file():
                new_name = f"{prefix}-{file_path.name}"
                file_path.rename(directory_path / new_name)
    except Exception as e:
        print(f"Error renaming files in {directory}: {e}")

def delete_files_in_directory(directory):
    """Delete all files in the directory."""
    try:
        directory_path = Path(directory)
        if not directory_path.exists():
            return
        for file_path in directory_path.iterdir():
            if file_path.is_file():
                file_path.unlink()
    except Exception as e:
        print(f"Error deleting files in {directory}: {e}")

def zip_proof_files(directory, zip_filename):
    """Create a ZIP file of the proof directory."""
    try:
        Path(zip_filename).parent.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(zip_filename, 'w', zipfile.ZIP_DEFLATED) as zipf:
            directory_path = Path(directory)
            for file_path in directory_path.rglob('*'):
                if file_path.is_file():
                    arcname = file_path.relative_to(directory_path)
                    zipf.write(file_path, arcname)
    except Exception as e:
        print(f"Error creating zip file {zip_filename}: {e}")

def process_job_type(job_type, job_number, week_number, base_path):
    """Process files for a specific job type."""
    combined_input = f"{job_number} {week_number}"
    source = base_path / job_type / "JOB"
    proof_directory = source / "PROOF"
    zip_folder = base_path / "WEEKLY" / "WEEKLY_ZIP"
    zip_suffixes = {
        'CBC': "CBCPROOFS",
        'EXC': "EXCPROOFS",
        'INACTIVE': "INACTIVE PROOFS",
        'NCWO': "NCWO PROOFS",
        'PREPIF': "PREPIF PROOFS"
    }
    zip_filename = zip_folder / f"{combined_input} {zip_suffixes[job_type]}.zip"

    # Rename files in INPUT, OUTPUT, PROOF
    for directory in [source / 'INPUT', source / 'OUTPUT', source / 'PROOF']:
        rename_files_in_directory(directory, combined_input)

    # Delete files in INPUT, OUTPUT, PROOF after renaming
    for directory in [source / 'INPUT', source / 'OUTPUT', source / 'PROOF']:
        delete_files_in_directory(directory)

    # Create ZIP of proof files
    zip_proof_files(proof_directory, zip_filename)
    print(f"{job_type} processing completed successfully!")

# Counting Functions
def count_input_records(base_path, job_type, prefix):
    """Count total records in input files for comparison."""
    input_path = base_path / job_type / "JOB" / "INPUT"
    total = 0
    if input_path.exists():
        for file in input_path.glob(f"{prefix}-*"):
            try:
                with open(file, 'r', encoding='latin1') as f:
                    total += sum(1 for row in csv.reader(f)) - 1  # Subtract header
            except Exception as e:
                print(f"Error counting input {file}: {e}")
    return total

def count_output_versions(base_path, job_type, prefix):
    """Count PR, CANC, and US versions in output files."""
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
        file_path = output_path / f"{prefix}-{file}"
        if file_path.exists():
            temp_path, temp_dir = create_quoted_temp_file(file_path)
            try:
                df = pd.read_csv(temp_path, low_memory=False, encoding='latin1')
                if job_type == 'CBC':
                    version_col = 'Creative_Version_Cd'
                    if 'CBC2' in file:
                        project = 'CBC 2'
                    else:
                        project = 'CBC 3'
                elif job_type == 'EXC':
                    version_col = df.columns[13]
                    project = 'EXC'
                elif job_type == 'INACTIVE':
                    version_col = 'Creative_Version_Cd'
                    if 'PO' in file:
                        project = 'INACTIVE A-PO'
                    else:
                        project = 'INACTIVE A-PU'
                elif job_type == 'NCWO':
                    version_col = 'Creative_Version_Cd'
                    if '1-A' in file:
                        project = 'NCWO 1-A'
                    elif '1-AP' in file:
                        project = 'NCWO 1-AP'
                    elif '2-A' in file:
                        project = 'NCWO 2-A'
                    else:
                        project = 'NCWO 2-AP'
                elif job_type == 'PREPIF':
                    version_col = df.columns[24]
                    project = 'PREPIF'

                if version_col in df.columns:
                    for version, count in df[version_col].value_counts().items():
                        if pd.notna(version):
                            version_str = str(version).upper()
                            if version_mappings[job_type]['PR'] in version_str:
                                counts['PR'] += count
                            elif version_mappings[job_type]['CANC'] in version_str:
                                counts['CANC'] += count
                            elif version_mappings[job_type]['US'] in version_str:
                                counts['US'] += count
            except Exception as e:
                print(f"Error counting output {file_path}: {e}")
            finally:
                shutil.rmtree(temp_dir)
    return counts

def main():
    """Main function to execute post-proof processing and counts."""
    parser = argparse.ArgumentParser(description="Post-Proof Processing")
    parser.add_argument("--base_path", required=True, help="Base directory (e.g., C:\\Goji\\RAC)")
    parser.add_argument("--week", required=True, help="Week number in MM.DD format")
    parser.add_argument("--cbc_job", required=True, help="CBC job number")
    parser.add_argument("--exc_job", required=True, help="EXC job number")
    parser.add_argument("--inactive_job", required=True, help="INACTIVE job number")
    parser.add_argument("--ncwo_job", required=True, help="NCWO job number")
    parser.add_argument("--prepif_job", required=True, help="PREPIF job number")
    parser.add_argument("--cbc2_postage", type=float, default=0.0, help="CBC 2 postage")
    parser.add_argument("--cbc3_postage", type=float, default=0.0, help="CBC 3 postage")
    parser.add_argument("--exc_postage", type=float, default=0.0, help="EXC postage")
    parser.add_argument("--inactive_apo_postage", type=float, default=0.0, help="INACTIVE A-PO postage")
    parser.add_argument("--inactive_apu_postage", type=float, default=0.0, help="INACTIVE A-PU postage")
    parser.add_argument("--ncwo_1a_postage", type=float, default=0.0, help="NCWO 1-A postage")
    parser.add_argument("--ncwo_1ap_postage", type=float, default=0.0, help="NCWO 1-AP postage")
    parser.add_argument("--ncwo_2a_postage", type=float, default=0.0, help="NCWO 2-A postage")
    parser.add_argument("--ncwo_2ap_postage", type=float, default=0.0, help="NCWO 2-AP postage")
    parser.add_argument("--prepif_postage", type=float, default=0.0, help="PREPIF postage")
    args = parser.parse_args()

    job_numbers = {
        'CBC': args.cbc_job,
        'EXC': args.exc_job,
        'INACTIVE': args.inactive_job,
        'NCWO': args.ncwo_job,
        'PREPIF': args.prepif_job
    }
    postage_values = {
        'CBC 2': args.cbc2_postage,
        'CBC 3': args.cbc3_postage,
        'EXC': args.exc_postage,
        'INACTIVE A-PO': args.inactive_apo_postage,
        'INACTIVE A-PU': args.inactive_apu_postage,
        'NCWO 1-A': args.ncwo_1a_postage,
        'NCWO 1-AP': args.ncwo_1ap_postage,
        'NCWO 2-A': args.ncwo_2a_postage,
        'NCWO 2-AP': args.ncwo_2ap_postage,
        'PREPIF': args.prepif_postage
    }
    week_number = args.week
    base_path = Path(args.base_path)

    try:
        print("Starting Post-Proof Processing...")
        for job_type in JOB_TYPES:
            print(f"\nProcessing {job_type}...")
            process_job_type(job_type, job_numbers[job_type], week_number, base_path)

        # Run counts and comparisons
        projects = [
            ('CBC', 'CBC 2'), ('CBC', 'CBC 3'), ('EXC', 'EXC'), 
            ('INACTIVE', 'INACTIVE A-PO'), ('INACTIVE', 'INACTIVE A-PU'),
            ('NCWO', 'NCWO 1-A'), ('NCWO', 'NCWO 1-AP'), ('NCWO', 'NCWO 2-A'), ('NCWO', 'NCWO 2-AP'),
            ('PREPIF', 'PREPIF')
        ]
        comparison_data = []
        json_counts = []

        for job_type, project in projects:
            prefix = f"{job_numbers[job_type]} {week_number}"
            input_count = count_input_records(base_path, job_type, prefix)
            output_counts = count_output_versions(base_path, job_type, prefix)
            total_output = sum(output_counts.values())
            difference = total_output - input_count
            comparison_data.append({
                "group": project,
                "input_count": input_count,
                "output_count": total_output,
                "difference": difference
            })
            json_counts.append({
                "job_number": job_numbers[job_type],
                "week": week_number,
                "project": project,
                "pr_count": output_counts['PR'],
                "canc_count": output_counts['CANC'],
                "us_count": output_counts['US'],
                "postage": postage_values[project]
            })

        # Print comparison table
        print("\nComparison of Counts:")
        print("-" * 70)
        print("Group                          Input       Output      Difference")
        print("-" * 70)
        for data in comparison_data:
            error_flag = " [ERROR!]" if data["difference"] > 0 else ""
            print(f"{data['group']:<30} {data['input_count']:>7,d}     {data['output_count']:>7,d}     {data['difference']:>7,d}{error_flag}")

        # Output JSON data
        output = {"counts": json_counts, "comparison": comparison_data}
        print("\n===JSON_START===")
        print(json.dumps(output, indent=2))
        print("===JSON_END===")

    except Exception as e:
        print(f"An error occurred: {str(e)}")
        sys.exit(1)

if __name__ == "__main__":
    main()