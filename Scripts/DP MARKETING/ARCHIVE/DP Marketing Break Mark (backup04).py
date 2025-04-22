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
    
    if len(header) == 0:
        return False, "Header cannot be empty"
    if len(header) > 50:
        return False, "Header name too long (max 50 characters)"
    
    invalid_chars = r'[<>:"/\\|?*]'
    if re.search(invalid_chars, header):
        return False, "Header contains invalid characters"
    
    if header in existing_headers:
        return False, "Header must be unique"
    
    return True, ""

def is_header_row(row):
    header_patterns = [
        r'name|address|city|state|zip|phone|email',
        r'first|last|street|line|number',
        r'id|code|reference|date'
    ]
    
    row_str = ' '.join(str(x).lower() for x in row if pd.notna(x))
    return any(re.search(pattern, row_str) for pattern in header_patterns)


def find_header_row(df):
    # Common header patterns with more variations
    header_patterns = [
        r'(?i)(full)?.*name',
        r'(?i)company',
        r'(?i)address',
        r'(?i)city.*state|state.*city',
        r'(?i)zip|postal',
        r'(?i)province',
        r'(?i)street',
        r'(?i)line.*[0-9]'
    ]
    
    for idx, row in df.iterrows():
        # Convert row to string, handling empty cells
        row_str = ' '.join(str(x).upper() for x in row if pd.notna(x) and str(x).strip())
        
        # Count matches in this row
        matches = sum(bool(re.search(pattern, row_str, re.IGNORECASE)) for pattern in header_patterns)
        
        # If we find 3 or more matches, it's likely our header row
        if matches >= 3:
            return idx
            
    # Fallback: Look for rows with common header-like characteristics
    for idx, row in df.iterrows():
        row_values = [str(x).strip() for x in row if pd.notna(x) and str(x).strip()]
        
        # Check if row has characteristics of headers:
        # - Mostly short strings (less than 30 chars)
        # - No numeric values
        # - No very long strings
        if row_values and all(len(x) < 30 for x in row_values) and \
           not any(x.replace('.','').isdigit() for x in row_values):
            return idx
    
    return None


def scan_directory(path, processed_files=None):
    if processed_files is None:
        processed_files = set()
    
    txt_files = [f for f in os.listdir(path) if f.endswith('.txt') and f not in processed_files]
    if not txt_files:
        print("No unprocessed TXT files found in directory")
        return None
    
    print("\nAvailable TXT files:")
    for idx, file in enumerate(txt_files, 1):
        print(f"{idx}: {file}")
    
    while True:
        try:
            choice = int(input("\nSelect file number to process: "))
            if 1 <= choice <= len(txt_files):
                return os.path.join(path, txt_files[choice-1]), txt_files[choice-1]
            print("Invalid selection")
        except ValueError:
            print("Please enter a valid number")

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
        col_letter = chr(64 + idx)
        header_text = str(header) if pd.notna(header) else ''
        header_text = header_text.strip() if isinstance(header_text, str) else header_text
        print(f"{idx}. (Col {col_letter}) {header_text}")

def process_headers(df):
    header_idx = find_header_row(df)
    
    if header_idx is not None:
        if header_idx != 0:
            print(f"\nHeader row found at row {header_idx + 1}. Moving to top...")
            headers = df.iloc[header_idx].tolist()
            df = pd.concat([df.iloc[:header_idx], df.iloc[header_idx+1:]], axis=0)
            df.columns = headers
            df = df.reset_index(drop=True)
        else:
            headers = df.iloc[0].tolist()
            df = df.iloc[1:].copy()
            df.columns = headers
            df = df.reset_index(drop=True)
    
    while True:
        display_headers(df.columns)
        make_changes = input("\nDO YOU WANT TO MAKE ANY CHANGES? (Y/N): ").strip().upper()
        if make_changes == 'N':
            break
        if make_changes == 'Y':
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

def sort_by_tray(df):
    if 'TRAY NUMBER' in df.columns:
        # Convert tray numbers to numeric, non-numeric values become NaN
        df['_tray_sort'] = pd.to_numeric(df['TRAY NUMBER'], errors='coerce')
        
        # Sort by the numeric column, NaN values will go to the end
        df = df.sort_values('_tray_sort')
        
        # Remove the temporary sorting column
        df = df.drop('_tray_sort', axis=1)
        
        # Reset the index to maintain continuous row numbers
        df = df.reset_index(drop=True)
    
    return df

def process_break_marks(df):
    # Look for TRAY or TRAY NUMBER column
    tray_column = None
    for col in df.columns:
        if isinstance(col, str) and col.upper() in ['TRAY', 'TRAY NUMBER']:
            tray_column = col
            break
    
    if tray_column:
        # Convert column to string type and clean data
        df[tray_column] = df[tray_column].astype(str).str.strip()
        
        # Get unique tray numbers, sorted numerically
        unique_trays = sorted(df[tray_column].unique(), key=lambda x: float(x) if x.replace('.', '').isdigit() else 0)
        
        # Add BREAKMARK column if it doesn't exist
        if 'BREAKMARK' not in df.columns:
            df.insert(0, 'BREAKMARK', '')
        
        # Set break marks for all trays except last
        for tray in unique_trays[:-1]:
            last_row_in_tray = df[df[tray_column] == tray].index[-1]
            df.at[last_row_in_tray, 'BREAKMARK'] = '##'
        
        # Set triple break mark for last tray
        if unique_trays:
            last_tray = unique_trays[-1]
            last_row = df[df[tray_column] == last_tray].index[-1]
            df.at[last_row, 'BREAKMARK'] = '###'
    
    return df

def process_file(file_path):
    with tempfile.NamedTemporaryFile(suffix='.csv', delete=False) as temp_file:
        temp_csv_path = temp_file.name
        
    try:
        df = pd.read_csv(file_path, header=None)
        df = process_headers(df)
        df = sort_by_tray(df)  # Added sorting step
        df = process_break_marks(df)
        
        df.to_csv(temp_csv_path, index=False)
        return df, temp_csv_path
        
    except Exception as e:
        print(f"Error processing file: {e}")
        if os.path.exists(temp_csv_path):
            os.unlink(temp_csv_path)
        return None, None

def validate_filename(filename):
    invalid_chars = r'[<>:"/\\|?*]'
    if re.search(invalid_chars, filename):
        return False, "Filename contains invalid characters"
    if len(filename) > 255:
        return False, "Filename too long"
    return True, ""

def main():
    downloads_path = r"C:\Users\JCox\Downloads"
    processed_files = set()
    
    while True:
        # File selection
        result = scan_directory(downloads_path, processed_files)
        if not result:
            break
            
        input_file, filename = result
        
        # Process file
        df, temp_csv_path = process_file(input_file)
        if df is None:
            continue
        
        try:
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
                    
                    if input(f"\nCONFIRM {new_filename}? (Y/N): ").upper() == 'Y':
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
            
            # Add to processed files
            processed_files.add(filename)
            
            if input("\nDO YOU WANT TO PROCESS ANOTHER LIST? (Y/N): ").upper() != 'Y':
                break
                
        finally:
            # Cleanup
            if temp_csv_path and os.path.exists(temp_csv_path):
                os.unlink(temp_csv_path)
    
    print("\nProcessing complete!")
    while input("\nPress X to terminate...").upper() != 'X':
        pass

if __name__ == "__main__":
    main()
