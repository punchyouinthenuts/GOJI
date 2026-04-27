import pandas as pd
import os
import sys
import json
import shutil
from pathlib import Path
from datetime import datetime

CANONICAL_TM_ROOT = r"C:\Goji\AUTOMATION\TRACHMAR"

def resolve_tm_root():
    if os.path.isdir(CANONICAL_TM_ROOT):
        return CANONICAL_TM_ROOT
    os.makedirs(CANONICAL_TM_ROOT, exist_ok=True)
    print("=== INFO: CREATED_CANONICAL_TRACHMAR_ROOT C:\\Goji\\AUTOMATION\\TRACHMAR ===")
    return CANONICAL_TM_ROOT

# Define directories for GOJI
WEEKLY_IDO_BASE = os.path.join(resolve_tm_root(), "WEEKLY IDO FULL")
TARGET_DIR = os.path.join(WEEKLY_IDO_BASE, "RAW FILES")
TEMP_DIR = os.path.join(WEEKLY_IDO_BASE, "TEMP")
ROLLBACK_LOG = os.path.join(TEMP_DIR, "rollback_02.log")

# Global rollback tracking
rollback_operations = []

def log_operation(operation_type, file_path, backup_path=None):
    """Log an operation for potential rollback"""
    operation = {
        "type": operation_type,  # "backup", "modify"
        "file_path": str(file_path),
        "backup_path": (str(backup_path) if backup_path is not None else None),
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
            if operation["type"] == "backup":
                # Restore original file from backup
                if (operation["backup_path"] and 
                    os.path.exists(operation["backup_path"]) and 
                    os.path.exists(operation["file_path"])):
                    
                    shutil.move(operation["backup_path"], operation["file_path"])
                    print(f"=== ROLLBACK_RESTORED: {os.path.basename(operation['file_path'])} ===")
                    
        except Exception as e:
            print(f"=== ROLLBACK_ERROR: {str(e)} ===")
    
    # Clean up any remaining backup files and rollback log
    try:
        if os.path.exists(TEMP_DIR):
            for file in os.listdir(TEMP_DIR):
                if file.startswith("backup_02_"):
                    backup_file = os.path.join(TEMP_DIR, file)
                    if os.path.exists(backup_file):
                        os.remove(backup_file)
                        
        if os.path.exists(ROLLBACK_LOG):
            os.remove(ROLLBACK_LOG)
            
        if os.path.exists(TEMP_DIR) and not os.listdir(TEMP_DIR):
            os.rmdir(TEMP_DIR)
            
    except Exception as e:
        print(f"=== ROLLBACK_CLEANUP_ERROR: {str(e)} ===")
        
    print("=== ROLLBACK_COMPLETE ===")

def create_backup(file_path):
    """Create backup copy of file before modification"""
    try:
        os.makedirs(TEMP_DIR, exist_ok=True)
        
        # Create unique backup filename
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S_%f")
        file_name = os.path.basename(file_path)
        backup_name = f"backup_02_{timestamp}_{file_name}"
        backup_path = os.path.join(TEMP_DIR, backup_name)
        
        # Copy file to backup location
        shutil.copy2(file_path, backup_path)
        
        # Log the backup operation
        log_operation("backup", file_path, backup_path)
        
        print(f"=== CREATED_BACKUP: {file_name} ===")
        return backup_path
        
    except Exception as e:
        raise Exception(f"Failed to create backup for {file_path}: {str(e)}")

def format_phone(number):
    """Format phone number to XXX-XXX-XXXX format"""
    try:
        num_str = ''.join(filter(str.isdigit, str(number).split('.')[0]))  # Remove decimal part
        if len(num_str) != 10:
            return None
        return f"{num_str[:3]}-{num_str[3:6]}-{num_str[6:]}"
    except:
        return None

def is_valid_phone_column(df, column):
    """Validate that column contains valid phone numbers"""
    # Only consider non-null values
    non_null_phones = df[column].dropna()
    
    if len(non_null_phones) == 0:
        # No phone numbers present; treat as valid (nothing to format)
        return True
    
    # Count digits before decimal point
    def count_digits(x):
        return sum(c.isdigit() for c in str(x).split('.')[0]) == 10
    
    valid_numbers = non_null_phones.apply(count_digits)
    valid_count = valid_numbers.sum()
    total_non_null = len(non_null_phones)
    
    validation_ratio = valid_count / total_non_null if total_non_null > 0 else 0
    
    print(f"=== PHONE_VALIDATION: {total_non_null} total, {valid_count} valid, {validation_ratio:.2f} ratio ===")
    
    return validation_ratio >= 0.8

def process_file(file_path):
    """Process a single file for phone number formatting"""
    file_name = os.path.basename(file_path)
    print(f"=== PROCESSING_FILE: {file_name} ===")
    
    try:
        # Create backup before processing
        backup_path = create_backup(file_path)
        
        # Read the file
        if file_path.suffix.lower() == '.csv':
            df = pd.read_csv(file_path)
        else:
            df = pd.read_excel(file_path)
            
        # Check for required column
        if 'office_phone' not in df.columns:
            raise Exception(f"Column 'office_phone' not found in {file_name}")
            
        # Validate phone number data
        if not is_valid_phone_column(df, 'office_phone'):
            raise Exception(f"Invalid phone number data in {file_name}")
            
        # Format phone numbers
        original_count = len(df['office_phone'].dropna())
        df['office_phone'] = df['office_phone'].apply(format_phone)
        formatted_count = len(df['office_phone'].dropna())
        
        print(f"=== FORMATTED_PHONES: {formatted_count}/{original_count} ===")
        
        # Save the modified file
        if file_path.suffix.lower() == '.csv':
            df.to_csv(file_path, index=False)
        else:
            df.to_excel(file_path, index=False)
            
        print(f"=== FILE_PROCESSED_SUCCESSFULLY: {file_name} ===")
        return True
        
    except Exception as e:
        raise Exception(f"Failed to process {file_name}: {str(e)}")

def main():
    """Main processing function"""
    try:
        print("=== STARTING_PHONE_NUMBER_PROCESSING ===")
        
        # Check if target directory exists
        if not os.path.exists(TARGET_DIR):
            raise Exception(f"Target directory does not exist: {TARGET_DIR}")
        
        # Get all CSV, XLS, and XLSX files
        target_path = Path(TARGET_DIR)
        file_patterns = ['*.csv', '*.xls', '*.xlsx']
        all_files = []
        
        for pattern in file_patterns:
            all_files.extend(target_path.glob(pattern))
        
        if not all_files:
            print("=== NO_FILES_FOUND ===")
            print("=== SCRIPT_SUCCESS ===")
            return 0
        
        print(f"=== FOUND_FILES: {len(all_files)} ===")
        
        # Process each file
        processed_count = 0
        for file_path in all_files:
            try:
                process_file(file_path)
                processed_count += 1
            except Exception as e:
                # If any file fails, rollback everything
                print(f"=== FILE_PROCESSING_ERROR: {str(e)} ===")
                raise Exception(f"Processing failed on file {file_path.name}: {str(e)}")
        
        # Save rollback log for BAT coordination
        save_rollback_log()
        
        print(f"=== ALL_FILES_PROCESSED_SUCCESSFULLY: {processed_count} ===")
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

