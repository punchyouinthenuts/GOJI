import os
import sys
import pandas as pd
import argparse
from datetime import timedelta
import csv

def format_large_numbers(x):
    """Format large numbers to remove decimal places."""
    if isinstance(x, (int, float)):
        return f"{x:.0f}"
    return x

def get_encoding(file_path):
    """Determine the encoding based on the file name: 'cp1252' for PR files, 'utf-8' otherwise."""
    base_name = os.path.basename(file_path)
    if 'PR' in base_name:
        return 'cp1252'
    return 'utf-8'

def main():
    """Main function to process PREPIF job files with dynamic encoding for PR files."""
    # Parse command-line arguments
    parser = argparse.ArgumentParser(description="Process PREPIF job files")
    parser.add_argument("--base_path", required=True, help="Base directory path (e.g., C:\\Goji\\RAC)")
    parser.add_argument("--job_num", required=True, help="Job number")
    parser.add_argument("--week", required=True, help="Week in MM.DD format")
    args = parser.parse_args()

    # Define dynamic directories
    input_dir = os.path.join(args.base_path, "PREPIF", "JOB", "OUTPUT")
    proof_dir = os.path.join(args.base_path, "PREPIF", "JOB", "PROOF")

    # Check if input directory exists
    if not os.path.exists(input_dir):
        print(f"Input directory not found: {input_dir}", file=sys.stderr)
        sys.exit(1)

    # Create proof directory if it doesn't exist
    if not os.path.exists(proof_dir):
        os.makedirs(proof_dir)

    # Define input file path
    file_name = "PRE_PIF.csv"
    input_file_path = os.path.join(input_dir, file_name)

    # Check if input file exists
    if not os.path.exists(input_file_path):
        print(f"Input file not found: {input_file_path}", file=sys.stderr)
        sys.exit(1)

    try:
        print("Starting PREPIF processing...")

        # Set pandas display options for consistent number formatting
        pd.set_option('display.float_format', lambda x: '%.0f' % x)

        # Load and process data
        print(f"Processing {file_name}...")
        df = pd.read_csv(input_file_path, encoding='utf-8')

        # Date processing
        df['BEGIN DATE'] = pd.to_datetime(df['BEGIN DATE']).dt.strftime('%m/%d/%Y')
        df['END DATE'] = pd.to_datetime(df['BEGIN DATE'], format='%m/%d/%Y') + timedelta(days=54)
        df['END DATE'] = df['END DATE'].dt.strftime('%m/%d/%Y')

        # Process PR records
        print("Processing PR records...")
        df_pr = df[df['Creative_Version_Cd'].str.contains('PR', na=False)].copy()
        df_pr['END DATE'] = pd.to_datetime(df_pr['END DATE'], format='%m/%d/%Y').dt.strftime('%d/%m/%Y')

        # Save PR output and proof files with dynamic encoding
        pr_output = os.path.join(input_dir, file_name.replace('.csv', '-PR.csv'))
        pr_proof = os.path.join(proof_dir, file_name.replace('.csv', '-PR-PD.csv'))

        df_pr.to_csv(
            pr_output,
            index=False,
            encoding=get_encoding(pr_output),
            float_format='%.0f',
            quoting=csv.QUOTE_ALL
        )
        df_pr_proof = df_pr[:15] if len(df_pr) > 15 else df_pr
        df_pr_proof.to_csv(
            pr_proof,
            index=False,
            encoding=get_encoding(pr_proof),
            float_format='%.0f',
            quoting=csv.QUOTE_ALL
        )

        # Process US records
        print("Processing US records...")
        df = df[~df.index.isin(df_pr.index)]
        us_output = os.path.join(input_dir, file_name.replace('.csv', '-US.csv'))
        df.to_csv(
            us_output,
            index=False,
            encoding=get_encoding(us_output),
            float_format='%.0f',
            quoting=csv.QUOTE_ALL
        )

        # Create proof subset with license requirements
        subset_with_license = df[df['Store_License'].notna()].sample(min(1, len(df[df['Store_License'].notna()])))
        remaining_records_needed = 15 - len(subset_with_license)
        subset_without_license = df[df['Store_License'].isna()].sample(min(remaining_records_needed, len(df[df['Store_License'].isna()])))
        final_subset = pd.concat([subset_with_license, subset_without_license])

        # Save US proof file with dynamic encoding
        us_proof = os.path.join(proof_dir, file_name.replace('.csv', '-US-PD.csv'))
        final_subset.to_csv(
            us_proof,
            index=False,
            encoding=get_encoding(us_proof),
            float_format='%.0f',
            quoting=csv.QUOTE_ALL
        )

        print("PREPIF processing completed successfully!")

    except Exception as e:
        error_message = f"CRITICAL ERROR DETECTED:\nType: {type(e).__name__}\nDetails: {str(e)}"
        print(error_message, file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()