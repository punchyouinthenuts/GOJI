import os
import csv
import shutil
import re
import sys
import json
import zipfile
from datetime import datetime

# Define directories for GOJI
RAW_FILES_DIR = r"C:\Goji\TRACHMAR\WEEKLY IDO FULL\RAW FILES"
PROCESSED_DIR = os.path.join(RAW_FILES_DIR, "PROCESSED")
TEMP_DIR = r"C:\Goji\TRACHMAR\WEEKLY IDO FULL\TEMP"
ROLLBACK_LOG = os.path.join(TEMP_DIR, "rollback_01.log")

# Global rollback tracking
rollback_operations = []

def log_operation(operation_type, source_path, dest_path=None, created_file=None):
    """Log an operation for potential rollback"""
    operation = {
        "type": operation_type,  # "move", "create", "mkdir", "extract", "delete"
        "source": source_path,
        "destination": dest_path,
        "created_file": created_file,
        "timestamp": datetime.now().isoformat()
    }
    rollback_operations.append(operation)

def save_rollback_log():
    """Save rollback operations to log file"""
    try:
        os.makedirs(TEMP_DIR, exist_ok=True)
        with open(ROLLBACK_LOG, 'w') as f:
            json.dump(rollback_operations, f, indent=2)
        print(f"=== ROLLBACK_LOG_SAVED: {ROLLBACK_LOG} ===")
    except Exception as e:
        print(f"=== ERROR_SAVING_ROLLBACK_LOG: {str(e)} ===")

def perform_rollback():
    """Perform complete rollback of all operations"""
    print("=== STARTING_ROLLBACK ===")
    
    # Reverse the operations list to undo in reverse order
    for operation in reversed(rollback_operations):
        try:
            if operation["type"] == "move":
                # Move file back to original location
                if os.path.exists(operation["destination"]):
                    shutil.move(operation["destination"], operation["source"])
                    print(f"=== ROLLBACK_MOVED: {operation['destination']} -> {operation['source']} ===")
                    
            elif operation["type"] == "create":
                # Delete created file
                if operation["created_file"] and os.path.exists(operation["created_file"]):
                    os.remove(operation["created_file"])
                    print(f"=== ROLLBACK_DELETED: {operation['created_file']} ===")
                    
            elif operation["type"] == "mkdir":
                # Remove created directory if empty
                if operation["created_file"] and os.path.exists(operation["created_file"]):
                    try:
                        os.rmdir(operation["created_file"])
                        print(f"=== ROLLBACK_REMOVED_DIR: {operation['created_file']} ===")
                    except OSError:
                        # Directory not empty, leave it
                        pass
            
            elif operation["type"] == "extract":
                # Delete extracted file
                if operation["created_file"] and os.path.exists(operation["created_file"]):
                    os.remove(operation["created_file"])
                    print(f"=== ROLLBACK_DELETED_EXTRACTED: {operation['created_file']} ===")
                    
            elif operation["type"] == "delete":
                # Restore deleted ZIP file (if we backed it up)
                if operation["destination"] and os.path.exists(operation["destination"]):
                    shutil.move(operation["destination"], operation["source"])
                    print(f"=== ROLLBACK_RESTORED_ZIP: {operation['source']} ===")
                        
        except Exception as e:
            print(f"=== ROLLBACK_ERROR: {str(e)} ===")
    
    # Clean up rollback log
    try:
        if os.path.exists(ROLLBACK_LOG):
            os.remove(ROLLBACK_LOG)
        if os.path.exists(TEMP_DIR) and not os.listdir(TEMP_DIR):
            os.rmdir(TEMP_DIR)
    except:
        pass
        
    print("=== ROLLBACK_COMPLETE ===")

def safe_makedirs(path):
    """Safely create directory with rollback logging"""
    if not os.path.exists(path):
        os.makedirs(path)
        log_operation("mkdir", path, created_file=path)
        print(f"=== CREATED_DIRECTORY: {path} ===")

def safe_move(source, destination):
    """Safely move file with rollback logging"""
    if os.path.exists(source):
        shutil.move(source, destination)
        log_operation("move", source, destination)
        print(f"=== MOVED_FILE: {os.path.basename(source)} ===")

def safe_create_file(file_path, content_func, *args):
    """Safely create file with rollback logging"""
    content_func(file_path, *args)
    log_operation("create", file_path, created_file=file_path)
    print(f"=== CREATED_FILE: {os.path.basename(file_path)} ===")

def extract_zip_files():
    """Extract all ZIP files in RAW_FILES_DIR and delete them"""
    print("=== CHECKING_FOR_ZIP_FILES ===")
    
    zip_files = [f for f in os.listdir(RAW_FILES_DIR) if f.lower().endswith('.zip')]
    
    if not zip_files:
        print("=== NO_ZIP_FILES_FOUND ===")
        return
    
    print(f"=== FOUND_ZIP_FILES: {len(zip_files)} ===")
    
    for zip_filename in zip_files:
        zip_path = os.path.join(RAW_FILES_DIR, zip_filename)
        print(f"=== EXTRACTING_ZIP: {zip_filename} ===")
        
        try:
            # Create backup of ZIP file before deletion
            backup_zip_path = os.path.join(TEMP_DIR, f"backup_zip_{zip_filename}")
            safe_makedirs(TEMP_DIR)
            shutil.copy2(zip_path, backup_zip_path)
            
            with zipfile.ZipFile(zip_path, 'r') as zip_ref:
                # Get list of files in ZIP
                file_list = zip_ref.namelist()
                print(f"=== ZIP_CONTENTS: {len(file_list)} files ===")
                
                # Extract each file
                for file_info in zip_ref.infolist():
                    # Skip directories
                    if file_info.is_dir():
                        continue
                    
                    # Get just the filename (no path)
                    filename = os.path.basename(file_info.filename)
                    
                    # Skip empty filenames or hidden files
                    if not filename or filename.startswith('.'):
                        continue
                    
                    # Extract file to RAW_FILES_DIR
                    extracted_path = os.path.join(RAW_FILES_DIR, filename)
                    
                    # Read file data from ZIP
                    with zip_ref.open(file_info) as source:
                        with open(extracted_path, 'wb') as target:
                            shutil.copyfileobj(source, target)
                    
                    # Log extraction for rollback
                    log_operation("extract", zip_path, created_file=extracted_path)
                    print(f"=== EXTRACTED: {filename} ===")
            
            # Delete the ZIP file after successful extraction
            log_operation("delete", zip_path, dest_path=backup_zip_path)
            os.remove(zip_path)
            print(f"=== DELETED_ZIP: {zip_filename} ===")
            
        except zipfile.BadZipFile:
            raise Exception(f"Invalid ZIP file: {zip_filename}")
        except Exception as e:
            raise Exception(f"Failed to extract {zip_filename}: {str(e)}")
    
    print("=== ZIP_EXTRACTION_COMPLETE ===")

# Helper Functions
def is_number(s):
    """Check if a string represents a number (integer or float)."""
    return re.match(r'^-?\d+\.?\d*$', s) is not None

def convert_to_int_if_number(s):
    """Convert a string to an integer if it represents a number, otherwise return unchanged."""
    if is_number(s):
        try:
            return str(int(float(s)))
        except ValueError:
            return s
    return s

# File creation functions for safe_create_file
def write_csv_file(file_path, header, rows):
    """Write CSV file with header and rows"""
    with open(file_path, 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=header)
        writer.writeheader()
        writer.writerows(rows)

# Main Processing Functions
def process_full_file(file_path):
    """
    Process a FULL file, splitting by language and member3_id, ensuring no decimals.
    Treats 'Undetermined' language_indicator as 'English' and updates it in output.
    """
    file_name = os.path.basename(file_path)
    date_part = file_name.split('_')[-1].replace('.csv', '')
    
    print(f"=== PROCESSING_FULL_FILE: {file_name} ===")

    # Read the FULL file
    try:
        with open(file_path, 'r', newline='') as f:
            reader = csv.DictReader(f)
            rows = list(reader)
            header = reader.fieldnames
    except Exception as e:
        raise Exception(f"Failed to read file {file_path}: {str(e)}")

    # Convert all numeric values to integers
    rows = [{k: convert_to_int_if_number(v) for k, v in row.items()} for row in rows]

    # Split into English and Spanish rows, treating 'Undetermined' as 'English'
    english_rows = []
    spanish_rows = []
    for row in rows:
        original_lang = row['language_indicator'].strip().lower()
        if original_lang == 'undetermined':
            row['language_indicator'] = 'English'
        if original_lang in ['english', 'undetermined']:
            english_rows.append(row)
        elif original_lang == 'spanish':
            spanish_rows.append(row)
        else:
            print(f"=== WARNING: Unknown language_indicator '{row['language_indicator']}' ===")

    # Process English rows if they exist
    if english_rows:
        english_file = os.path.join(RAW_FILES_DIR, f"FHK_Full_English_{date_part}.csv")
        safe_create_file(english_file, write_csv_file, header, english_rows)
        # Further process the English file
        process_language_file(english_file, 'English', date_part)

    # Process Spanish rows if they exist
    if spanish_rows:
        spanish_file = os.path.join(RAW_FILES_DIR, f"02 FHK_Full_Spanish_{date_part}.csv")
        safe_create_file(spanish_file, write_csv_file, header, spanish_rows)
        # Further process the Spanish file
        process_language_file(spanish_file, 'Spanish', date_part)

    # Move original file to PROCESSED
    dest_path = os.path.join(PROCESSED_DIR, file_name)
    safe_move(file_path, dest_path)

def process_language_file(lang_file, lang, date_part):
    """Split language-specific file by member3_id, ensuring no decimals."""
    print(f"=== PROCESSING_LANGUAGE_FILE: {lang} ===")
    
    try:
        with open(lang_file, 'r', newline='') as f:
            reader = csv.DictReader(f)
            rows = list(reader)
            header = reader.fieldnames
    except Exception as e:
        raise Exception(f"Failed to read language file {lang_file}: {str(e)}")

    # Split based on member3_id
    has_member3_rows = [row for row in rows if row['member3_id'].strip()]
    no_member3_rows = [row for row in rows if not row['member3_id'].strip()]

    # Write 1-2 file (no member3_id) ONLY if there are rows
    if no_member3_rows:
        one_two_file = os.path.join(RAW_FILES_DIR, f"FHK_Full_{lang}-1-2_{date_part}.csv")
        safe_create_file(one_two_file, write_csv_file, header, no_member3_rows)

    # Write 3-4 file (has member3_id), ONLY if there are rows
    if has_member3_rows:
        three_four_file = os.path.join(RAW_FILES_DIR, f"FHK_Full_{lang}-3-4_{date_part}.csv")
        safe_create_file(three_four_file, write_csv_file, header, has_member3_rows)

    # Move language file to PROCESSED
    dest_path = os.path.join(PROCESSED_DIR, os.path.basename(lang_file))
    safe_move(lang_file, dest_path)

def process_ido_file(file_path):
    """Process an IDO file, splitting by member3_id, ensuring no decimals."""
    file_name = os.path.basename(file_path)
    date_part = file_name.split('_')[-1].replace('.csv', '')
    
    print(f"=== PROCESSING_IDO_FILE: {file_name} ===")

    # Read the IDO file
    try:
        with open(file_path, 'r', newline='') as f:
            reader = csv.DictReader(f)
            rows = list(reader)
            header = reader.fieldnames
    except Exception as e:
        raise Exception(f"Failed to read IDO file {file_path}: {str(e)}")

    # Convert all numeric values to integers
    rows = [{k: convert_to_int_if_number(v) for k, v in row.items()} for row in rows]

    # Split based on member3_id
    has_member3_rows = [row for row in rows if row['member3_id'].strip()]
    no_member3_rows = [row for row in rows if not row['member3_id'].strip()]

    # Write 1-2 file (no member3_id) ONLY if there are rows
    if no_member3_rows:
        one_two_file = os.path.join(RAW_FILES_DIR, f"FHK_IDO-1-2_{date_part}.csv")
        safe_create_file(one_two_file, write_csv_file, header, no_member3_rows)

    # Write 3-4 file (has member3_id) ONLY if there are rows
    if has_member3_rows:
        three_four_file = os.path.join(RAW_FILES_DIR, f"FHK_IDO-3-4_{date_part}.csv")
        safe_create_file(three_four_file, write_csv_file, header, has_member3_rows)

    # Move original file to PROCESSED
    dest_path = os.path.join(PROCESSED_DIR, file_name)
    safe_move(file_path, dest_path)

def main():
    try:
        print("=== STARTING_INPUT_FILE_PROCESSING ===")
        
        # NEW: Extract ZIP files first
        extract_zip_files()
        
        # Ensure PROCESSED directory exists
        safe_makedirs(PROCESSED_DIR)
        
        # Scan and process files (existing logic)
        files = [f for f in os.listdir(RAW_FILES_DIR) 
                if f.endswith('.csv') and ("FHK_Full" in f or "FHK_IDO" in f)]
        
        if not files:
            print("=== NO_FILES_TO_PROCESS ===")
            print("=== SCRIPT_SUCCESS ===")
            return 0
            
        print(f"=== FOUND_FILES: {len(files)} ===")
        
        for file in files:
            file_path = os.path.join(RAW_FILES_DIR, file)
            if "FHK_Full" in file:
                process_full_file(file_path)
            elif "FHK_IDO" in file:
                process_ido_file(file_path)
        
        # Save rollback log for BAT coordination
        save_rollback_log()
        
        print("=== ALL_FILES_PROCESSED_SUCCESSFULLY ===")
        print("=== SCRIPT_SUCCESS ===")
        return 0
        
    except Exception as e:
        print(f"=== SCRIPT_ERROR ===")
        print(f"=== ERROR_MESSAGE: {str(e)} ===")
        
        # Perform rollback
        perform_rollback()
        
        return 1

if __name__ == "__main__":
    sys.exit(main())