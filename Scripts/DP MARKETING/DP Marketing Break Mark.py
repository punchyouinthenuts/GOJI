import pandas as pd
import os
import shutil
import csv
import tempfile
import re
import random

# Validate header names for uniqueness and invalid characters
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

# Check if a row appears to be a header based on common patterns
def is_header_row(row):
    header_patterns = [
        r'name|address|city|state|zip|phone|email',
        r'first|last|street|line|number',
        r'id|code|reference|date'
    ]
    
    row_str = ' '.join(str(x).lower() for x in row if pd.notna(x))
    return any(re.search(pattern, row_str) for pattern in header_patterns)

# Locate the header row in the DataFrame
def find_header_row(df):
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
        row_str = ' '.join(str(x).upper() for x in row if pd.notna(x) and str(x).strip())
        matches = sum(bool(re.search(pattern, row_str, re.IGNORECASE)) for pattern in header_patterns)
        if matches >= 3:
            return idx
            
    for idx, row in df.iterrows():
        row_values = [str(x).strip() for x in row if pd.notna(x) and str(x).strip()]
        if row_values and all(len(x) < 30 for x in row_values) and \
           not any(x.replace('.','').isdigit() for x in row_values):
            return idx
    
    return None

# Scan directory for unprocessed TXT files and let user select one
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

# Display sample data from a specific column
def display_column_samples(df, col_idx):
    column = df.iloc[:, col_idx]
    valid_samples = [(idx, val) for idx, val in enumerate(column) if pd.notna(val)]
    if valid_samples:
        samples = random.sample(valid_samples, min(5, len(valid_samples)))
        print(f"\nSample data from column {chr(65 + col_idx)}:")
        for row_idx, value in sorted(samples):
            print(f"Row {row_idx + 1}: {value}")

# Display current headers to the user
def display_headers(headers):
    print("\nHEADER ROW IS AS FOLLOWS:")
    for idx, header in enumerate(headers, 1):
        col_letter = chr(64 + idx)
        header_text = str(header) if pd.notna(header) else ''
        header_text = header_text.strip() if isinstance(header_text, str) else header_text
        print(f"{idx}. (Col {col_letter}) {header_text}")

# Parse user input for column selections (e.g., "1,2,3" or "1-3" or "ALL")
def parse_column_selection(input_str, num_columns):
    input_str = input_str.strip()
    if input_str.upper() == "ALL":
        return list(range(1, num_columns + 1))
    
    selected = set()
    parts = [part.strip() for part in input_str.split(',')]
    for part in parts:
        if '-' in part:
            try:
                start, end = map(int, part.split('-'))
                if start > end:
                    print(f"Invalid range: {start}-{end} (start > end)")
                elif not (1 <= start <= num_columns and 1 <= end <= num_columns):
                    print(f"Range out of bounds: {start}-{end}")
                else:
                    selected.update(range(start, end + 1))
            except ValueError:
                print(f"Invalid range: {part}")
        else:
            try:
                num = int(part)
                if 1 <= num <= num_columns:
                    selected.add(num)
                else:
                    print(f"Column number out of range: {num}")
            except ValueError:
                print(f"Invalid input: {part}")
    return sorted(selected)

# Process and allow modification of headers
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
            numbers_input = input("\nWHICH HEADERS DO YOU WANT TO CHANGE? ").strip()
            numbers = parse_column_selection(numbers_input, len(df.columns))
            if not numbers:
                print("No valid headers selected.")
                continue
            for num in numbers:
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

# Sort DataFrame by tray number if the column exists
def sort_by_tray(df):
    if 'TRAY NUMBER' in df.columns:
        df['_tray_sort'] = pd.to_numeric(df['TRAY NUMBER'], errors='coerce')
        df = df.sort_values('_tray_sort')
        df = df.drop('_tray_sort', axis=1)
        df = df.reset_index(drop=True)
    return df

# Add break marks based on tray changes
def process_break_marks(df):
    tray_column = None
    for col in df.columns:
        if isinstance(col, str) and col.upper() in ['TRAY', 'TRAY NUMBER']:
            tray_column = col
            break
    
    if tray_column:
        df[tray_column] = df[tray_column].astype(str).str.strip()
        unique_trays = sorted(df[tray_column].unique(), key=lambda x: float(x) if x.replace('.', '').isdigit() else 0)
        if 'BREAKMARK' not in df.columns:
            df.insert(0, 'BREAKMARK', '')
        for tray in unique_trays[:-1]:
            last_row_in_tray = df[df[tray_column] == tray].index[-1]
            df.at[last_row_in_tray, 'BREAKMARK'] = '##'
        if unique_trays:
            last_tray = unique_trays[-1]
            last_row = df[df[tray_column] == last_tray].index[-1]
            df.at[last_row, 'BREAKMARK'] = '###'
    return df

# Process the input file and return the DataFrame and temporary file path
def process_file(file_path):
    with tempfile.NamedTemporaryFile(suffix='.csv', delete=False) as temp_file:
        temp_csv_path = temp_file.name
        
    try:
        df = pd.read_csv(file_path, header=None)
        df = process_headers(df)
        df = sort_by_tray(df)
        df = process_break_marks(df)
        df.to_csv(temp_csv_path, index=False)
        return df, temp_csv_path
    except Exception as e:
        print(f"Error processing file: {e}")
        if os.path.exists(temp_csv_path):
            os.unlink(temp_csv_path)
        return None, None

# Validate the output filename
def validate_filename(filename):
    invalid_chars = r'[<>:"/\\|?*]'
    if re.search(invalid_chars, filename):
        return False, "Filename contains invalid characters"
    if len(filename) > 255:
        return False, "Filename too long"
    return True, ""

# Main function to orchestrate the script
def main():
    downloads_path = r"C:\Users\JCox\Downloads"
    processed_files = set()
    
    while True:
        result = scan_directory(downloads_path, processed_files)
        if not result:
            break
            
        input_file, filename = result
        df, temp_csv_path = process_file(input_file)
        if df is None:
            continue
        
        try:
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
            
            # Define save locations
            w_drive_path = os.path.join('W:\\', f"{final_filename}.csv")
            alternative_folder = r'C:\Users\JCox\Desktop\MOVE TO BUSKRO'
            alternative_path = os.path.join(alternative_folder, f"{final_filename}.csv")
            
            # Attempt to save to W: drive first, fall back to alternative if unavailable
            save_path = w_drive_path if os.path.exists('W:\\') else alternative_path
            
            try:
                # Ensure the target directory exists
                os.makedirs(os.path.dirname(save_path), exist_ok=True)
                df.to_csv(save_path, index=False, quoting=csv.QUOTE_ALL)
                print(f"File saved to {save_path}")
            
            except OSError as e:
                print(f"Error saving to {save_path}: {e}")
                if 'W:\\' in save_path:
                    # Fallback to alternative location if W: fails
                    try:
                        os.makedirs(alternative_folder, exist_ok=True)
                        df.to_csv(alternative_path, index=False, quoting=csv.QUOTE_ALL)
                        print(f"File saved to alternative location: {alternative_path}")
                    except Exception as e2:
                        print(f"Error saving to alternative location: {e2}")
                else:
                    print("Failed to save the file.")
            
            processed_files.add(filename)
            if input("\nDO YOU WANT TO PROCESS ANOTHER LIST? (Y/N): ").upper() != 'Y':
                break
                
        finally:
            if temp_csv_path and os.path.exists(temp_csv_path):
                os.unlink(temp_csv_path)
    
    print("\nProcessing complete!")
    while input("\nPress X to terminate...").upper() != 'X':
        pass

if __name__ == "__main__":
    main()