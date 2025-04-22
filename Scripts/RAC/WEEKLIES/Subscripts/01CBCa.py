import os
import sys
import pandas as pd
import argparse
import re

def process_cbc_file(file_path, output_path):
    """
    Process a CBC CSV file by formatting the CUSTOM_03 column.
    
    Args:
        file_path (str): Path to the input CSV file
        output_path (str): Path to save the reformatted CSV
    
    Returns:
        bool: True if successful, False if an error occurs
    """
    try:
        data = pd.read_csv(file_path, encoding='ISO-8859-1')
        if 'CUSTOM_03' not in data.columns:
            raise KeyError("CUSTOM_03 column not found in the CSV file")
        data['CUSTOM_03'] = data['CUSTOM_03'].apply(lambda x: f"{float(x):,.2f}" if pd.notnull(x) else x)
        data.to_csv(output_path, index=False)
        return True
    except (UnicodeDecodeError, ValueError, KeyError) as e:
        print(f"Error processing {file_path}: {str(e)}", file=sys.stderr)
        return False

def generate_proof_data(input_path, output_path):
    """
    Generate proof data by grouping and filtering CSV data.
    
    Args:
        input_path (str): Path to the reformatted CSV file
        output_path (str): Path to save the proof data CSV
    
    Returns:
        bool: True if successful, False if an error occurs
    """
    try:
        df = pd.read_csv(input_path)
        result = []
        for version, group in df.groupby('Creative_Version_Cd'):
            licensed = group[group['Store_License'].notna()].head(1)
            unlicensed = group[group['Store_License'].isna()]
            version_records = pd.concat([licensed, unlicensed]).head(15)
            result.append(version_records)
        final_df = pd.concat(result)
        final_df.to_csv(output_path, index=False)
        return True
    except Exception as e:
        print(f"Error generating proof data for {input_path}: {str(e)}", file=sys.stderr)
        return False

def rename_ppwk_files(folder_path, prefix):
    """
    Rename specific PDF files in the PPWK folder with a prefix.
    
    Args:
        folder_path (str): Path to the PPWK Temp folder
        prefix (str): Prefix to add to filenames (e.g., '12345_04.07')
    
    Returns:
        bool: True if all renames succeed, False if an error occurs
    """
    target_files = [
        "CBC2_PresortReport.PDF",
        "CBC2_PT.PDF",
        "CBC2_TT.PDF",
        "CBC3_PresortReport.PDF",
        "CBC3_PT.PDF",
        "CBC3_TT.PDF"
    ]
    success = True
    for filename in os.listdir(folder_path):
        if filename in target_files:
            old_path = os.path.join(folder_path, filename)
            new_filename = f"{prefix}_{filename}"
            new_path = os.path.join(folder_path, new_filename)
            try:
                os.rename(old_path, new_path)
                print(f"Renamed {filename} to {new_filename}")
            except Exception as e:
                print(f"Error renaming {filename}: {str(e)}", file=sys.stderr)
                success = False
    return success

def main():
    # Parse command-line arguments from the GUI
    parser = argparse.ArgumentParser(description="CBC File Processor")
    parser.add_argument("base_path", help="Base path for RAC directories (e.g., C:\\Goji\\RAC)")
    parser.add_argument("job_num", help="CBC Job Number (5 digits)")
    parser.add_argument("week", help="Week number (format: MM.DD)")
    args = parser.parse_args()

    # Validate job_num and week
    if len(args.job_num) != 5 or not args.job_num.isdigit():
        print("Error: JOB NUMBER REQUIRES FIVE DIGITS", file=sys.stderr)
        sys.exit(1)
    if not re.match(r'\d{2}\.\d{2}', args.week):
        print("Error: WEEK NUMBER NEEDS TO BE FORMATTED LIKE 00.00", file=sys.stderr)
        sys.exit(1)

    # Define file paths in the JOB working directories
    cbc2_input = os.path.join(args.base_path, 'CBC', 'JOB', 'OUTPUT', 'CBC2_WEEKLY.csv')
    cbc2_output = os.path.join(args.base_path, 'CBC', 'JOB', 'OUTPUT', 'CBC2WEEKLYREFORMAT.csv')
    cbc3_input = os.path.join(args.base_path, 'CBC', 'JOB', 'OUTPUT', 'CBC3_WEEKLY.csv')
    cbc3_output = os.path.join(args.base_path, 'CBC', 'JOB', 'OUTPUT', 'CBC3WEEKLYREFORMAT.csv')
    cbc2_proof_output = os.path.join(args.base_path, 'CBC', 'JOB', 'PROOF', 'CBC2WEEKLYREFORMAT-PD.csv')
    cbc3_proof_output = os.path.join(args.base_path, 'CBC', 'JOB', 'PROOF', 'CBC3WEEKLYREFORMAT-PD.csv')

    # Define PPWK folder path as specified
    ppwk_folder = r"C:\Users\JCox\Desktop\PPWK Temp"

    # Check if input files exist
    if not os.path.exists(cbc2_input):
        print(f"Error: Input file not found: {cbc2_input}", file=sys.stderr)
        sys.exit(1)
    if not os.path.exists(cbc3_input):
        print(f"Error: Input file not found: {cbc3_input}", file=sys.stderr)
        sys.exit(1)

    # Check if PPWK folder exists
    if not os.path.exists(ppwk_folder):
        print(f"Error: PPWK Temp folder not found: {ppwk_folder}", file=sys.stderr)
        sys.exit(1)

    # Process CBC files
    if not process_cbc_file(cbc2_input, cbc2_output):
        print("Error processing CBC2_WEEKLY.csv", file=sys.stderr)
        sys.exit(1)
    if not process_cbc_file(cbc3_input, cbc3_output):
        print("Error processing CBC3_WEEKLY.csv", file=sys.stderr)
        sys.exit(1)

    # Generate proof data
    if not generate_proof_data(cbc2_output, cbc2_proof_output):
        print("Error generating proof data for CBC2", file=sys.stderr)
        sys.exit(1)
    if not generate_proof_data(cbc3_output, cbc3_proof_output):
        print("Error generating proof data for CBC3", file=sys.stderr)
        sys.exit(1)

    # Rename PPWK files with job_num and week prefix
    prefix = f"{args.job_num}_{args.week}"
    if not rename_ppwk_files(ppwk_folder, prefix):
        print("Error renaming PPWK files", file=sys.stderr)
        sys.exit(1)

    # Success message
    print("ALL CBC FILES SUCCESSFULLY PROCESSED!")

if __name__ == "__main__":
    main()