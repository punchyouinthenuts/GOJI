import os
import zipfile
import glob
import csv
import shutil
import sys
import time
import traceback
import random
import string

CANONICAL_TM_ROOT = r"C:\Goji\AUTOMATION\TRACHMAR"

def resolve_tm_root():
    if os.path.isdir(CANONICAL_TM_ROOT):
        return CANONICAL_TM_ROOT
    os.makedirs(CANONICAL_TM_ROOT, exist_ok=True)
    print("INFO: created canonical TRACHMAR root C:\\Goji\\AUTOMATION\\TRACHMAR.")
    return CANONICAL_TM_ROOT

# ==============================
# Cleaning helpers
# ==============================

def strip_bom(text):
    """Remove BOM or BOM-like artifacts from a string."""
    if text is None:
        return ''
    if not isinstance(text, str):
        text = str(text)
    # Standard BOM character
    text = text.replace('\ufeff', '')
    # UTF-8 BOM mis-decoded as cp1252 often appears as ï»¿
    if text.startswith('ï»¿'):
        text = text[len('ï»¿'):]
    return text

def is_bad_row(row):
    """
    Determine whether a row is 'bad'.
    A row is considered bad if:
    - All cells are empty or whitespace.
    - The row has no alphanumeric characters at all (e.g., only control chars like 0x1A).
    """
    if not row:
        return True

    normalized = [(cell or '').strip() for cell in row]

    # Entirely empty / whitespace
    if all(cell == "" for cell in normalized):
        return True

    joined = ''.join(normalized)

    # If there are no alphanumeric characters, treat as garbage
    if not any(ch.isalnum() for ch in joined):
        return True

    return False

def clean_csv_file(csv_file_path):
    """
    Clean a CSV file in-place:
    - Normalize BOM from header and rows.
    - Keep the first header row as canonical.
    - Remove duplicate header rows (including BOM-variants).
    - Remove 'bad' rows (e.g., only 0x1A, empty, or non-alphanumeric).
    Logs removals to console.
    """
    print(f"\n=== CLEANING CSV FILE: {os.path.basename(csv_file_path)} ===")

    temp_file_path = csv_file_path + '.clean'

    with open(csv_file_path, 'r', newline='', encoding='cp1252') as f_in:
        reader = csv.reader(f_in)
        rows = list(reader)

    if not rows:
        print("  File is empty, nothing to clean.")
        return

    # First row is assumed to be the header (even if BOM-affected)
    raw_header = rows[0]
    cleaned_header = [strip_bom(col).strip() for col in raw_header]
    header_tuple = tuple(cleaned_header)

    with open(temp_file_path, 'w', newline='', encoding='cp1252') as f_out:
        writer = csv.writer(f_out)
        # Write cleaned header
        writer.writerow(cleaned_header)

        # Process remaining rows
        for idx, row in enumerate(rows[1:], start=2):  # 1-based line numbers
            cleaned_row = [strip_bom(col).strip() for col in row]

            # Bad row?
            if is_bad_row(cleaned_row):
                print(f"  Removed bad row {idx} in {os.path.basename(csv_file_path)}")
                continue

            # Duplicate header row?
            if tuple(cleaned_row) == header_tuple:
                print(f"  Removed duplicate header at row {idx} in {os.path.basename(csv_file_path)}")
                continue

            writer.writerow(cleaned_row)

    # Replace original with cleaned version
    os.replace(temp_file_path, csv_file_path)
    print(f"  Cleaning complete for {os.path.basename(csv_file_path)}")


# ==============================
# Existing helpers (modified pipeline uses them after cleaning)
# ==============================

def generate_unique_id(global_unique_ids):
    """Generate a 10-character unique ID using capital letters and numbers"""
    while True:
        uid = ''.join(random.choices(string.ascii_uppercase + string.digits, k=10))
        if uid not in global_unique_ids:
            global_unique_ids.add(uid)
            return uid

def add_unique_id_column(csv_file_path, global_unique_ids):
    """Add UNIQUE ID column to a CSV file"""
    temp_file_path = csv_file_path + '.temp'
    
    with open(csv_file_path, 'r', newline='', encoding='cp1252') as f_in:
        csv_reader = csv.reader(f_in)
        
        with open(temp_file_path, 'w', newline='', encoding='cp1252') as f_out:
            csv_writer = csv.writer(f_out)
            
            # Process header row
            headers = next(csv_reader)
            new_headers = ['UNIQUE ID'] + headers
            csv_writer.writerow(new_headers)
            
            # Process data rows
            row_count = 0
            for row in csv_reader:
                unique_id = generate_unique_id(global_unique_ids)
                new_row = [unique_id] + row
                csv_writer.writerow(new_row)
                row_count += 1
    
    # Replace original file with modified file
    os.replace(temp_file_path, csv_file_path)
    
    return row_count

def rollback(moved_files, created_files, extracted_files, zip_deleted):
    """Restore everything to original state"""
    print("\nROLLING BACK CHANGES...")
    
    # Remove any created files
    for file_path in created_files:
        if os.path.exists(file_path):
            try:
                os.remove(file_path)
                print(f"Removed: {file_path}")
            except Exception as e:
                print(f"Failed to remove {file_path}: {e}")
    
    # Restore any moved files
    for dest_path, orig_path in moved_files:
        if os.path.exists(dest_path):
            try:
                if os.path.exists(orig_path):
                    os.remove(orig_path)
                shutil.move(dest_path, orig_path)
                print(f"Restored: {orig_path}")
            except Exception as e:
                print(f"Failed to restore {orig_path}: {e}")
    
    # Remove extracted files
    for extracted_file in extracted_files:
        if os.path.exists(extracted_file):
            try:
                os.remove(extracted_file)
                print(f"Removed extracted file: {extracted_file}")
            except Exception as e:
                print(f"Failed to remove {extracted_file}: {e}")
    
    # Note about ZIP file
    if zip_deleted:
        print(f"Note: ZIP file was deleted: {zip_deleted}")
        print("ZIP file cannot be automatically restored.")
    
    print("ROLLBACK COMPLETE")


def check_folder_state(input_dir):
    """Check if both ZIP and CSV files exist (unclean state)"""
    zip_files = glob.glob(os.path.join(input_dir, "*.zip"))
    csv_files = glob.glob(os.path.join(input_dir, "*.csv"))
    
    if zip_files and csv_files:
        print("ERROR: Both ZIP and CSV files detected in RAW INPUT folder.")
        print("This indicates the folder was not properly cleaned between jobs.")
        print("Please manually clean the RAW INPUT folder before running this script.")
        return False
    
    return True

def extract_zip_if_present(input_dir):
    """Extract ZIP file if present, then delete it. Returns (extracted_files, zip_deleted)"""
    zip_files = glob.glob(os.path.join(input_dir, "*.zip"))
    
    if not zip_files:
        return [], None
    
    # Get the most recent ZIP file
    zip_file = max(zip_files, key=os.path.getctime)
    print(f"Found ZIP file: {os.path.basename(zip_file)}")
    print(f"ZIP file size: {os.path.getsize(zip_file)} bytes")
    
    extracted_files = []
    
    # Extract all files
    with zipfile.ZipFile(zip_file, 'r') as zip_ref:
        extracted_names = zip_ref.namelist()
        zip_ref.extractall(input_dir)
        
        # Track extracted files for rollback
        for filename in extracted_names:
            full_path = os.path.join(input_dir, filename)
            if os.path.exists(full_path):
                extracted_files.append(full_path)
        
        print(f"Extracted {len(extracted_files)} files from ZIP")
    
    # Delete the ZIP file
    os.remove(zip_file)
    print("ZIP file deleted after extraction")
    
    return extracted_files, zip_file

def validate_and_merge_headers(file_paths):
    """Validate headers across files and create unified header list"""
    all_headers = []
    unified_headers = []
    
    # Collect headers from all files
    for file_path in file_paths:
        with open(file_path, 'r', newline='', encoding='cp1252') as f:
            reader = csv.reader(f)
            headers = next(reader)
        
        all_headers.append((file_path, headers))
        
        # Add any new headers to unified list
        for header in headers:
            if header not in unified_headers:
                unified_headers.append(header)
    
    # Check if all files have identical headers
    first_headers = all_headers[0][1]
    identical_headers = all(headers == first_headers for _, headers in all_headers)
    
    if not identical_headers:
        print("WARNING: Headers differ between files. Merging with union of all headers.")
        for file_path, headers in all_headers:
            missing_headers = [h for h in unified_headers if h not in headers]
            if missing_headers:
                print(f"File {os.path.basename(file_path)} missing headers: {missing_headers}")
    
    return unified_headers

def process_dataframe_for_input(input_reader, output_writer, row_count_ref):
    """Apply column renaming and filtering logic"""
    # Column mapping
    column_mapping = {
        'First_Name': 'First Name',
        'Last_Name': 'Last Name',
        'Subscriber_Address_1': 'Address Line 1',
        'Subscriber_Address_2': 'Address Line 2',
        'Subscriber_City': 'City',
        'Subscriber_State': 'State',
        'Subscriber_Zip': 'ZIP Code'
    }
    
    # Columns to keep
    columns_to_keep = [
        'UNIQUE ID',
        'Subscriber_ID',
        'First Name',
        'Last Name',
        'Address Line 1',
        'Address Line 2',
        'City',
        'State',
        'ZIP Code'
    ]
    
    # Read first row to get and validate headers (DictReader: this is first data row)
    row = next(input_reader, None)
    if row is None:
        return
    
    # Apply column mapping
    original_headers = list(row.keys())
    mapped_headers = [column_mapping.get(h, h) for h in original_headers]
    
    # Verify all required columns exist after mapping
    missing_columns = [col for col in columns_to_keep if col not in mapped_headers]
    if missing_columns:
        raise ValueError(f"Missing required columns: {missing_columns}")
    
    # Write header
    output_writer.writerow(columns_to_keep)
    
    # Write first data row (filtered)
    filtered_row = []
    for col in columns_to_keep:
        try:
            idx = mapped_headers.index(col)
            original_col = original_headers[idx]
            filtered_row.append(row[original_col])
        except (ValueError, KeyError):
            filtered_row.append('')
    output_writer.writerow(filtered_row)
    row_count_ref[0] += 1
    
    # Process remaining rows
    for row in input_reader:
        filtered_row = []
        for col in columns_to_keep:
            try:
                idx = mapped_headers.index(col)
                original_col = original_headers[idx]
                filtered_row.append(row[original_col])
            except (ValueError, KeyError):
                filtered_row.append('')
        output_writer.writerow(filtered_row)
        row_count_ref[0] += 1

def create_input_csv_single(source_file, input_file_path):
    row_count = [0]
    with open(source_file, 'r', newline='', encoding='cp1252') as f_in:
        reader = csv.DictReader(f_in)
        with open(input_file_path, 'w', newline='', encoding='cp1252') as f_out:
            writer = csv.writer(f_out)
            process_dataframe_for_input(reader, writer, row_count)
    return row_count[0]

def create_input_csv_multi(source_files, input_file_path, unified_headers):
    column_mapping = {
        'First_Name': 'First Name',
        'Last_Name': 'Last Name',
        'Subscriber_Address_1': 'Address Line 1',
        'Subscriber_Address_2': 'Address Line 2',
        'Subscriber_City': 'City',
        'Subscriber_State': 'State',
        'Subscriber_Zip': 'ZIP Code'
    }
    columns_to_keep = [
        'UNIQUE ID',
        'Subscriber_ID',
        'First Name',
        'Last Name',
        'Address Line 1',
        'Address Line 2',
        'City',
        'State',
        'ZIP Code'
    ]
    mapped_unified_headers = [column_mapping.get(h, h) for h in unified_headers]
    missing_columns = [col for col in columns_to_keep if col not in mapped_unified_headers]
    if missing_columns:
        raise ValueError(f"Missing required columns across all files: {missing_columns}")
    row_count = 0
    with open(input_file_path, 'w', newline='', encoding='cp1252') as f_out:
        writer = csv.writer(f_out)
        writer.writerow(columns_to_keep)
        for i, source_file in enumerate(source_files):
            print(f"Combining file {i+1}/{len(source_files)}: {os.path.basename(source_file)}")
            with open(source_file, 'r', newline='', encoding='cp1252') as f_in:
                reader = csv.DictReader(f_in)
                original_headers = list(reader.fieldnames)
                mapped_file_headers = [column_mapping.get(h, h) for h in original_headers]
                for row in reader:
                    filtered_row = []
                    for col in columns_to_keep:
                        try:
                            idx = mapped_file_headers.index(col)
                            original_col = original_headers[idx]
                            filtered_row.append(row[original_col])
                        except (ValueError, KeyError):
                            filtered_row.append('')
                    writer.writerow(filtered_row)
                    row_count += 1
    return row_count

def main():
    print("=== SCRIPT INITIALIZATION ===")
    print(f"Python version: {sys.version}")
    print(f"Current working directory: {os.getcwd()}")
    print(f"Script arguments: {sys.argv}")
    
    moved_files = []
    created_files = []
    extracted_files = []
    zip_deleted = None
    global_unique_ids = set()
    try:
        print("\n=== DEFINING DIRECTORY PATHS ===")
        tm_root = resolve_tm_root()
        fl_er_base = os.path.join(tm_root, "FL ER")
        raw_input_dir = os.path.join(fl_er_base, "RAW INPUT")
        data_dir = os.path.join(fl_er_base, "DATA")
        data_original_dir = os.path.join(data_dir, "ORIGINAL")
        print(f"RAW Input Directory: {raw_input_dir}")
        print(f"Data Directory: {data_dir}")
        print(f"Data Original Directory: {data_original_dir}")
        
        print("\n=== CHECKING DIRECTORY EXISTENCE ===")
        print(f"RAW Input Directory exists: {os.path.exists(raw_input_dir)}")
        print(f"Data Directory exists: {os.path.exists(data_dir)}")
        print(f"Data Original Directory exists: {os.path.exists(data_original_dir)}")
        if not os.path.exists(raw_input_dir):
            print(f"ERROR: RAW Input Directory does not exist: {raw_input_dir}")
            raise Exception(f"Required input directory does not exist: {raw_input_dir}")
        
        print("\n=== CREATING OUTPUT DIRECTORIES ===")
        os.makedirs(data_dir, exist_ok=True)
        os.makedirs(data_original_dir, exist_ok=True)
        
        print("\n=== CHECKING FOLDER STATE ===")
        if not check_folder_state(raw_input_dir):
            print("Terminating in 5 seconds...")
            time.sleep(5)
            sys.exit(1)
        
        print("\n=== EXTRACTING ZIP IF PRESENT ===")
        extracted_files, zip_deleted = extract_zip_if_present(raw_input_dir)
        
        print("\n=== SCANNING FOR FILES ===")
        csv_pattern = os.path.join(raw_input_dir, '*.csv')
        csv_files = glob.glob(csv_pattern)
        print(f"Found {len(csv_files)} CSV file(s): {[os.path.basename(f) for f in csv_files]}")
        if not csv_files:
            raise Exception("No CSV files found in RAW INPUT directory")
        
        print("\n=== MOVING FILES TO DATA DIRECTORY ===")
        moved_csv_files = []
        for csv_file in csv_files:
            filename = os.path.basename(csv_file)
            dest_path = os.path.join(data_dir, filename)
            if os.path.exists(dest_path):
                raise FileExistsError(f"File already exists in destination: {dest_path}")
            shutil.move(csv_file, dest_path)
            moved_files.append((dest_path, csv_file))
            moved_csv_files.append(dest_path)
            print(f"Moved: {filename}")
        
        # ======================================
        # NEW STEP: CLEAN CSV FILES BEFORE IDs
        # ======================================
        print("\n=== CLEANING CSV FILES BEFORE UNIQUE ID ASSIGNMENT ===")
        for i, csv_file in enumerate(moved_csv_files):
            print(f"Cleaning file {i+1}/{len(moved_csv_files)}: {os.path.basename(csv_file)}")
            clean_csv_file(csv_file)
        
        print(f"\n=== ADDING UNIQUE ID COLUMNS ===")
        total_ids = 0
        for i, csv_file in enumerate(moved_csv_files):
            print(f"Processing file {i+1}/{len(moved_csv_files)}: {os.path.basename(csv_file)}")
            row_count = add_unique_id_column(csv_file, global_unique_ids)
            total_ids += row_count
            print(f"  Added {row_count} unique IDs")
        print(f"Generated {len(global_unique_ids)} globally unique IDs across all files")
        
        print(f"\n=== CREATING INPUT.csv ===")
        input_file_path = os.path.join(data_dir, "INPUT.csv")
        if os.path.exists(input_file_path):
            raise FileExistsError("INPUT.csv already exists in DATA folder")
        if len(moved_csv_files) == 1:
            rows_written = create_input_csv_single(moved_csv_files[0], input_file_path)
        else:
            unified_headers = validate_and_merge_headers(moved_csv_files)
            rows_written = create_input_csv_multi(moved_csv_files, input_file_path, unified_headers)
        created_files.append(input_file_path)
        print(f"Created INPUT.csv with {rows_written} rows")
        
        print(f"\n=== MOVING ORIGINALS TO ORIGINAL FOLDER ===")
        for csv_file in moved_csv_files:
            filename = os.path.basename(csv_file)
            dest_path = os.path.join(data_original_dir, filename)
            if os.path.exists(dest_path):
                backup_path = dest_path + f".backup_{int(time.time())}"
                shutil.move(dest_path, backup_path)
                print(f"Backed up existing file to: {backup_path}")
            shutil.move(csv_file, dest_path)
            for i, (current, original) in enumerate(moved_files):
                if current == csv_file:
                    moved_files[i] = (dest_path, original)
                    break
            print(f"Moved to ORIGINAL: {filename}")
        
        print("\n" + "="*50)
        print("SUCCESS!")
        print("="*50)
        print(f"[OK] {len(moved_csv_files)} CSV file(s) processed")
        print(f"[OK] {len(global_unique_ids)} globally unique IDs generated")
        print(f"[OK] INPUT.csv created with {rows_written} rows")
        print(f"[OK] Original files moved to ORIGINAL folder")
        
        print("\nPROCESS COMPLETE! TERMINATING...")
        time.sleep(2.5)
        sys.exit(0)
        
    except Exception as e:
        print("\n=== ERROR OCCURRED ===")
        print(f"ERROR: {str(e)}")
        print("Stack trace:")
        traceback.print_exc()
        print("\n=== INITIATING ROLLBACK ===")
        rollback(moved_files, created_files, extracted_files, zip_deleted)
        print("Script exiting with error code 1")
        sys.exit(1)

if __name__ == "__main__":
    main()

