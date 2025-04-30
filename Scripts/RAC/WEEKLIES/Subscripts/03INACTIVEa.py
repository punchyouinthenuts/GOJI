import os
import sys
import random
import csv
import pandas as pd
import argparse

def format_szip(szip):
    """Format ZIP codes to 5 digits, padding with zeros if necessary."""
    if pd.isna(szip):
        return '00000'
    szip = str(szip).strip()
    if len(szip) < 5:
        return szip.zfill(5)
    return szip

def format_credit(value):
    """Format credit values to include commas and two decimal places."""
    if pd.isna(value):
        return value
    try:
        return '{:,.2f}'.format(float(str(value).replace(",", "")))
    except ValueError:
        return value

def get_random_sample(df, n=15):
    """Get a random sample of n records, prioritizing one with a Store_License if available."""
    if len(df) <= n:
        return df
    sample_df = pd.DataFrame()
    if df['Store_License'].notna().any():
        license_record = df[df['Store_License'].notna()].sample(n=1)
        remaining = df[~df.index.isin(license_record.index)].sample(n=n-1)
        sample_df = pd.concat([license_record, remaining])
    else:
        sample_df = df.sample(n=n)
    return sample_df

def save_txt_file(df, output_path):
    """Save DataFrame to a text file."""
    df.to_csv(output_path, index=False, encoding='utf-8')

def convert_txt_to_csv(txt_path):
    """Convert a text file to CSV with UTF-8 BOM encoding, preserving 'CREDIT' formatting."""
    csv_path = txt_path.replace('.txt', '.csv')
    # Read the header to determine the columns
    columns = pd.read_csv(txt_path, nrows=0).columns
    # If 'CREDIT' is in columns, read it as string
    if 'CREDIT' in columns:
        df = pd.read_csv(txt_path, dtype={'CREDIT': str}, encoding='utf-8')
    else:
        df = pd.read_csv(txt_path, encoding='utf-8')
    df.to_csv(csv_path, index=False, encoding='utf-8-sig')
    return csv_path

def process_file(file_name, input_dir, proof_dir):
    """Process input file and split records based on Creative_Version_Cd."""
    input_file_path = os.path.join(input_dir, file_name)
    df = pd.read_csv(input_file_path, encoding='utf-8')
    
    file_type = 'PU' if 'PU' in file_name else 'PO'
    
    if file_type == 'PU':
        df['First Name'] = df['First Name'].str.upper()
        df['CREDIT'] = pd.to_numeric(df['CREDIT'], errors='coerce')
        df['CREDIT'] = df['CREDIT'].apply(format_credit)
        df['CREDIT'] = df['CREDIT'].astype(str)
    
    # Process PR records
    pr_mask = df['Creative_Version_Cd'].str.contains('-PR-' + file_type + '$', regex=True, na=False)
    df_pr = df[pr_mask].copy()
    
    if not df_pr.empty:
        df_pr['MESSAGE'] = "La oferta es válida hasta el "
        df_pr['EXPDATE'] = df_pr['EXPIRES'].str[29:]
        df_pr['EXPIRES'] = df_pr['MESSAGE'] + df_pr['EXPDATE']
        df_pr['SZIP'] = df_pr['SZIP'].apply(format_szip)
        
        pr_output_file = os.path.join(input_dir, f"PR-{file_type}.txt")
        pr_proof_file = os.path.join(proof_dir, f"PR-{file_type}-PD.txt")
        
        save_txt_file(df_pr, pr_output_file)
        pr_sample = get_random_sample(df_pr, 15)
        save_txt_file(pr_sample, pr_proof_file)
    
    # Remove PR records from main dataframe
    df = df[~df.index.isin(df_pr.index)]
    
    # Process AT records
    at_mask = df['Creative_Version_Cd'].str.contains('-AT-' + file_type + '$', regex=True, na=False)
    df_at = df[at_mask].copy()
    
    if not df_at.empty:
        at_output_file = os.path.join(input_dir, f"AT-{file_type}.txt")
        at_proof_file = os.path.join(proof_dir, f"AT-{file_type}-PD.txt")
        
        save_txt_file(df_at, at_output_file)
        at_sample = get_random_sample(df_at, 15)
        save_txt_file(at_sample, at_proof_file)
    
    # Remove AT records from main dataframe
    df = df[~df.index.isin(df_at.index)]
    
    # Process A records (remaining)
    a_mask = df['Creative_Version_Cd'].str.contains('-A-' + file_type + '$', regex=True, na=False)
    df_a = df[a_mask].copy()
    
    if not df_a.empty:
        a_output_file = os.path.join(input_dir, f"A-{file_type}.txt")
        a_proof_file = os.path.join(proof_dir, f"A-{file_type}-PD.txt")
        
        save_txt_file(df_a, a_output_file)
        a_sample = get_random_sample(df_a, 15)
        save_txt_file(a_sample, a_proof_file)
    
    # Handle unmatched records
    remaining_df = df[~df.index.isin(df_a.index)]
    if not remaining_df.empty:
        print(f"Warning: {len(remaining_df)} records did not match any version pattern in {file_name}", file=sys.stderr)
        remaining_output_file = os.path.join(input_dir, f"UNMATCHED-{file_type}.txt")
        save_txt_file(remaining_df, remaining_output_file)

def convert_all_txt_to_csv(txt_files):
    """Convert all generated text files to CSV."""
    print("\nConverting TXT files to CSV...")
    for txt_file in txt_files:
        if os.path.exists(txt_file):
            csv_path = convert_txt_to_csv(txt_file)
            print(f"Converted: {txt_file} -> {csv_path}")

def cleanup_txt_files(txt_files):
    """Remove all generated text files after conversion."""
    print("\nCleaning up TXT files...")
    for txt_file in txt_files:
        if os.path.exists(txt_file):
            os.remove(txt_file)
            print(f"Removed: {txt_file}")

def main():
    """Main function to process INACTIVE job files."""
    # Parse command-line arguments (removed --week)
    parser = argparse.ArgumentParser(description="Process INACTIVE job files")
    parser.add_argument("--base_path", required=True, help="Base directory path (e.g., C:\\Goji\\RAC)")
    parser.add_argument("--job_num", required=True, help="Job number")
    args = parser.parse_args()

    input_dir = os.path.join(args.base_path, "INACTIVE", "JOB", "OUTPUT")
    proof_dir = os.path.join(args.base_path, "INACTIVE", "JOB", "PROOF")

    if not os.path.exists(input_dir) or not os.path.exists(proof_dir):
        print(f"Required directories not found: {input_dir} or {proof_dir}", file=sys.stderr)
        sys.exit(1)

    txt_files = []

    for file_name in ["A-PO.txt", "A-PU.txt"]:
        input_file_path = os.path.join(input_dir, file_name)
        if not os.path.exists(input_file_path):
            print(f"Input file not found: {file_name}", file=sys.stderr)
            sys.exit(1)
        
        process_file(file_name, input_dir, proof_dir)
        # Collect generated txt files
        for root, _, files in os.walk(input_dir):
            for file in files:
                if file.endswith('.txt'):
                    txt_files.append(os.path.join(root, file))
        for root, _, files in os.walk(proof_dir):
            for file in files:
                if file.endswith('.txt'):
                    txt_files.append(os.path.join(root, file))

    convert_all_txt_to_csv(txt_files)
    cleanup_txt_files(txt_files)

if __name__ == "__main__":
    main()