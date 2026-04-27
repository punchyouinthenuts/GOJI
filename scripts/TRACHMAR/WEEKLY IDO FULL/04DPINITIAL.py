import os
import sys
import json
import glob
import shutil
import pandas as pd
from datetime import datetime
from pathlib import Path

CANONICAL_TM_ROOT = r"C:\Goji\AUTOMATION\TRACHMAR"

def resolve_tm_root():
    if os.path.isdir(CANONICAL_TM_ROOT):
        return CANONICAL_TM_ROOT
    os.makedirs(CANONICAL_TM_ROOT, exist_ok=True)
    print("=== INFO: CREATED_CANONICAL_TRACHMAR_ROOT C:\\Goji\\AUTOMATION\\TRACHMAR ===")
    return CANONICAL_TM_ROOT

# Define directories for GOJI
WEEKLY_IDO_BASE = os.path.join(resolve_tm_root(), "WEEKLY IDO FULL")
RAW_PATH = os.path.join(WEEKLY_IDO_BASE, "RAW FILES")
INPUT_PATH = os.path.join(WEEKLY_IDO_BASE, "BM INPUT")
TEMP_DIR = os.path.join(WEEKLY_IDO_BASE, "TEMP")
ROLLBACK_LOG = os.path.join(TEMP_DIR, "rollback_04.log")

# Global rollback tracking
rollback_operations = []

def log_operation(operation_type, source_path, dest_path=None, created_file=None):
    """Log an operation for potential rollback"""
    operation = {
        "type": operation_type,  # "move", "create", "mkdir"
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
        print(f"=== MOVED_FILE: {os.path.basename(source)} -> {os.path.basename(destination)} ===")

def safe_copy(source, destination):
    """Safely copy file with rollback logging"""
    shutil.copy2(source, destination)
    log_operation("create", source, created_file=destination)
    print(f"=== COPIED_FILE: {os.path.basename(source)} -> {os.path.basename(destination)} ===")

def handle_existing_input_csv():
    """Handle existing INPUT.csv file by moving to old folder"""
    input_csv = os.path.join(INPUT_PATH, "INPUT.csv")
    
    if os.path.exists(input_csv):
        old_path = os.path.join(INPUT_PATH, "old")
        
        # Create old directory if it doesn't exist
        safe_makedirs(old_path)
        
        # Create timestamped filename
        timestamp = datetime.now().strftime("%Y%m%d-%H%M")
        old_filename = f"INPUT_{timestamp}.csv"
        old_file_path = os.path.join(old_path, old_filename)
        
        # Move existing file
        safe_move(input_csv, old_file_path)
        
        return old_file_path
    
    return None

def find_target_file(file_number):
    """Find the file matching the specified number"""
    try:
        # Get list of numbered files (both XLSX and CSV)
        raw_files = [f for f in os.listdir(RAW_PATH) 
                    if (f.endswith('.xlsx') or f.endswith('.csv')) and f[:2].isdigit()]
        
        if not raw_files:
            raise Exception("No numbered XLSX or CSV files found in RAW FILES directory")
        
        # Ensure file_number is 2 digits with leading zero
        file_number_padded = file_number.zfill(2)
        
        # Find matching file
        target_file = None
        for file in raw_files:
            if file.startswith(file_number_padded + " "):
                target_file = file
                break
        
        if not target_file:
            raise Exception(f"No file found starting with '{file_number_padded} ' in RAW FILES directory")
        
        return target_file
        
    except Exception as e:
        raise Exception(f"Error finding target file: {str(e)}")

def process_target_file(target_file):
    """Process the selected target file"""
    try:
        # Ensure input directory exists
        safe_makedirs(INPUT_PATH)
        
        source_file_path = os.path.join(RAW_PATH, target_file)
        destination_path = os.path.join(INPUT_PATH, "INPUT.csv")
        
        if not os.path.exists(source_file_path):
            raise Exception(f"Source file does not exist: {source_file_path}")
        
        if target_file.endswith('.xlsx'):
            # Convert XLSX to CSV
            print("=== CONVERTING_XLSX_TO_CSV ===")
            try:
                df = pd.read_excel(source_file_path)
                df.to_csv(destination_path, index=False)
                log_operation("create", source_file_path, created_file=destination_path)
                print("=== XLSX_CONVERTED_TO_CSV ===")
            except Exception as e:
                raise Exception(f"Failed to convert XLSX to CSV: {str(e)}")
        else:
            # Direct copy for CSV files
            print("=== COPYING_CSV_FILE ===")
            safe_copy(source_file_path, destination_path)
            print("=== CSV_FILE_COPIED ===")
        
        return source_file_path, destination_path
        
    except Exception as e:
        raise Exception(f"Error processing target file: {str(e)}")

def main():
    """Main processing function"""
    try:
        print("=== STARTING_DPINITIAL_PROCESSING ===")
        
        # Check command line arguments
        if len(sys.argv) != 2:
            raise Exception("Usage: python 04DPINITIAL.py <file_number>")
        
        file_number = sys.argv[1].strip()
        
        if not file_number:
            raise Exception("File number cannot be empty")
        
        print(f"=== PROCESSING_FILE_NUMBER: {file_number} ===")
        
        # Verify directories exist
        if not os.path.exists(RAW_PATH):
            raise Exception(f"RAW FILES directory does not exist: {RAW_PATH}")
        
        # Handle existing INPUT.csv
        moved_file = handle_existing_input_csv()
        if moved_file:
            print(f"=== EXISTING_INPUT_MOVED: {os.path.basename(moved_file)} ===")
        
        # Find target file
        target_file = find_target_file(file_number)
        print(f"=== TARGET_FILE_FOUND: {target_file} ===")
        
        # Process the target file
        source_path, dest_path = process_target_file(target_file)
        
        # Save rollback log for coordination
        save_rollback_log()
        
        print(f"=== FILE_PROCESSED_SUCCESSFULLY ===")
        print(f"=== SOURCE: {source_path} ===")
        print(f"=== DESTINATION: {dest_path} ===")
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

