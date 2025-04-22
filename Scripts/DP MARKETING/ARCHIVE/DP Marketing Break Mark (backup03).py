import pandas as pd
import os
import sys
import shutil
import csv
from string import ascii_letters
from itertools import islice
import tempfile
import re
import random

def validate_header_name(header, existing_headers=None):
    if existing_headers is None:
        existing_headers = []
    
    # Check length
    if len(header) == 0:
        return False, "Header cannot be empty"
    if len(header) > 50:
        return False, "Header name too long (max 50 characters)"
    
    # Check for invalid characters
    invalid_chars = r'[<>:"/\\|?*]'
    if re.search(invalid_chars, header):
        return False, "Header contains invalid characters"
    
    # Check uniqueness
    if header in existing_headers:
        return False, "Header must be unique"
    
    return True, ""

def is_header_row(row):
    # Check for common header patterns
    header_patterns = [
        r'name|address|city|state|zip|phone|email',
        r'first|last|street|line|number',
        r'id|code|reference|date'
    ]
    
    row_str = ' '.join(str(x).lower() for x in row if pd.notna(x))
    return any(re.search(pattern, row_str) for pattern in header_patterns)

def display_column_samples(df, col_idx):
    column = df.iloc[:, col_idx]
    valid_samples = [(idx, val) for idx, val in enumerate(column) if pd.notna(val)]
    if valid_samples:
        samples = random.sample(valid_samples, min(5, len(valid_samples)))
        print(f"\nSample data from column {chr(65 + col_idx)}:")
        for row_idx, value in sorted(samples):
            print(f"Row {row_idx + 1}: {value}")

def display_headers(headers):
    print("\nHEADER ROW IS AS FOLLOWS:")
    for idx, header in enumerate(headers, 1):
        col_letter = chr(64 + idx)  # A=1, B=2, etc.
        header_text = header if pd.notna(header) and header.strip() != '' else ''
        print(f"{idx}. (Col {col_letter}) {header_text}")

def scan_directory(path):
    txt_files = [f for f in os.listdir(path) if f.endswith('.txt')]
    if not txt_files:
        print("No TXT files found in directory")
        return None
    
    print("\nAvailable TXT files:")
    for idx, file in enumerate(txt_files, 1):
        print(f"{idx}: {file}")
    
    while True:
        try:
            choice = int(input("\nSelect file number to process: "))
            if 1 <= choice <= len(txt_files):
                return os.path.join(path, txt_files[choice-1])
            print("Invalid selection")
        except ValueError:
            print("Please enter a valid number")

def process_headers(df):
    # Check last row for headers
    if is_header_row(df.iloc[-1]):
        print("\nHeader row found at bottom of file. Moving to top...")
        headers = df.iloc[-1].tolist()
        df = df.iloc[:-1].copy()
        df.columns = headers
        df = df.reset_index(drop=True)
    elif is_header_row(df.iloc[0]):
        headers = df.iloc[0].tolist()
        df = df.iloc[1:].copy()
        df.columns = headers
        df = df.reset_index(drop=True)
    
    while True:
        display_headers(df.columns)
        if input("\nDO YOU WANT TO MAKE ANY CHANGES? (Y/N): ").upper() != 'Y':
            break
            
        numbers = input("\nWHICH HEADERS DO YOU WANT TO CHANGE? ").strip()
        numbers = [int(n.strip()) for n in numbers.split(',') if n.strip().isdigit()]
        
        for num in numbers:
            if 1 <= num <= len(df.columns):
                display_column_samples(df, num-1)
                while True:
                    new_header = input(f"\nENTER HEADER {num} NAME: ").strip()
                    valid, message = validate_header_name(new_header, 
                                                       [h for i, h in enumerate(df.columns) if i != num-1])
                    if valid:
                        new_cols = list(df.columns)
                        new_cols[num-1] = new_header
                        df.columns = new_cols
                        break
                    print(f"Invalid header: {message}")
    
    return df

def analyze_data_structure(df):
    complete_rows = df.notna().all(axis=1)
    if complete_rows.any():
        first_complete_row = df.loc[complete_rows].iloc[0]
        complete_row_idx = df.index[complete_rows][0]
        return first_complete_row, complete_row_idx
    
    # If no complete rows, find best row for each column
    best_data = {}
    for col in df.columns:
        first_valid = df[col].first_valid_index()
        if first_valid is not None:
            best_data[col] = (df.at[first_valid, col], first_valid)
    
    return best_data, None

def create_headers(df):
    headers = []
    existing_headers = []
    
    best_data, complete_row_idx = analyze_data_structure(df)
    
    for col_idx in range(len(df.columns)):
        if complete_row_idx is not None:
            preview_data = best_data[col_idx]
            row_info = f" (row {complete_row_idx + 1})" if complete_row_idx > 0 else ""
        else:
            preview_data, row_idx = best_data[col_idx]
            row_info = f" (row {row_idx + 1})"
        
        while True:
            print(f"\nColumn {col_idx + 1} Preview: {preview_data}{row_info}")
            header = input("Enter header name: ").strip()
            valid, message = validate_header_name(header, existing_headers)
            
            if valid:
                headers.append(header)
                existing_headers.append(header)
                break
            print(f"Invalid header: {message}")
    
    return headers

def display_headers_with_preview(df):
    print("\nCurrent Headers with Preview:")
    for idx, (header, col) in enumerate(zip(df.columns, df.values.T), 1):
        first_valid = next((val for val in col if pd.notna(val)), "")
        row_idx = next((i, val) for i, val in enumerate(col) if pd.notna(val))[0]
        row_info = f" (row {row_idx + 1})" if row_idx > 0 else ""
        print(f"{idx}. {header}: {first_valid}{row_info}")

def process_file(file_path):
    with tempfile.NamedTemporaryFile(suffix='.csv', delete=False) as temp_file:
        temp_csv_path = temp_file.name
        
    try:
        # Initial read and conversion
        df = pd.read_csv(file_path, header=None)
        
        # Process headers
        df = process_headers(df)
        
        # Save temporary CSV
        df.to_csv(temp_csv_path, index=False)
        return df, temp_csv_path
        
    except Exception as e:
        print(f"Error processing file: {e}")
        if os.path.exists(temp_csv_path):
            os.unlink(temp_csv_path)
        return None, None

def get_next_name(base_path):
    if not os.path.exists(base_path):
        return base_path
    
    directory, basename = os.path.split(base_path)
    return next(
        (os.path.join(directory, f"{basename}_{letter}")
         for letter in ascii_letters 
         if not os.path.exists(os.path.join(directory, f"{basename}_{letter}"))),
        None
    )

def validate_filename(filename):
    invalid_chars = r'[<>:"/\\|?*]'
    if re.search(invalid_chars, filename):
        return False, "Filename contains invalid characters"
    if len(filename) > 255:
        return False, "Filename too long"
    return True, ""

def main():
    downloads_path = r"C:\Users\JCox\Downloads"
    
    # File selection
    input_file = scan_directory(downloads_path)
    if not input_file:
        return
    
    # Process file and create headers
    df, temp_csv_path = process_file(input_file)
    if df is None:
        return
    
    try:
        # Add break marks column
        df.insert(0, 'BREAKMARK', '')
        
        # Process TRAY column
        if 'TRAY' in df.columns:
            mask = df['TRAY'].str.match(r'^\d+$', na=False)
            unique_trays = df.loc[mask, 'TRAY'].unique()
            
            for tray in unique_trays[:-1]:
                df.loc[df[df['TRAY'] == tray].index[-1], 'BREAKMARK'] = '##'
            
            if len(unique_trays) > 0:
                last_tray = unique_trays[-1]
                df.loc[df[df['TRAY'] == last_tray].index[-1], 'BREAKMARK'] = '###'
        
        # Get job number and handle file naming
        job_number = input("ENTER JOB NUMBER: ")
        default_filename = f"{job_number} DP Marketing_WBREAK"
        
        print(f"\nGenerated filename: {default_filename}.csv")
        if input("RENAME? (Y/N): ").upper() == 'Y':
            while True:
                new_filename = input("ENTER NEW FILE NAME: ").strip()
                valid, message = validate_filename(new_filename)
                
                if not valid:
                    print(f"Invalid filename: {message}")
                    continue
                
                print(f"\nCONFIRM {new_filename}? (Y/N): ")
                if input().upper() == 'Y':
                    final_filename = new_filename
                    break
        else:
            final_filename = default_filename
        
        # Save to network location
        network_folder = fr'\\NAS1069D9\AMPrintData\2025_SrcFiles\D\DP Marketing\{job_number}'
        alternative_folder = r'C:\Users\JCox\Desktop\MOVE TO BUSKRO'
        
        try:
            os.makedirs(network_folder, exist_ok=True)
            final_path = os.path.join(network_folder, f"{final_filename}.csv")
        except OSError:
            print(f"Error accessing network drive. Saving to alternative location.")
            final_path = os.path.join(alternative_folder, f"{final_filename}.csv")
            os.makedirs(alternative_folder, exist_ok=True)
        
        df.to_csv(final_path, index=False, quoting=csv.QUOTE_ALL)
        
        # Copy to W drive
        try:
            w_drive_dest = os.path.join('W:\\', f"{final_filename}.csv")
            shutil.copy2(final_path, w_drive_dest)
        except OSError as e:
            print(f"Error copying to W drive: {e}")
        
    finally:
        # Cleanup
        if temp_csv_path and os.path.exists(temp_csv_path):
            os.unlink(temp_csv_path)
    
    print("\nProcessing complete!")
    while input("\nPress X to terminate...").upper() != 'X':
        pass

if __name__ == "__main__":
    main()

