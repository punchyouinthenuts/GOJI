import os
import sys
import json
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
FOLDER_PATH = os.path.join(WEEKLY_IDO_BASE, "RAW FILES")
TEMP_DIR = os.path.join(WEEKLY_IDO_BASE, "TEMP")
ROLLBACK_LOG = os.path.join(TEMP_DIR, "rollback_03.log")

# Global rollback tracking
rollback_operations = []

def log_operation(operation_type, original_path, new_path):
    """Log an operation for potential rollback"""
    operation = {
        "type": operation_type,  # "rename"
        "original_path": original_path,
        "new_path": new_path,
        "original_name": os.path.basename(original_path),
        "new_name": os.path.basename(new_path),
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
            if operation["type"] == "rename":
                # Rename file back to original name
                if os.path.exists(operation["new_path"]):
                    os.rename(operation["new_path"], operation["original_path"])
                    print(f"=== ROLLBACK_RENAMED: {operation['new_name']} -> {operation['original_name']} ===")
                else:
                    print(f"=== ROLLBACK_WARNING: File not found: {operation['new_path']} ===")
                    
        except Exception as e:
            print(f"=== ROLLBACK_ERROR: Failed to restore {operation['original_name']}: {str(e)} ===")
    
    # Clean up rollback log
    try:
        if os.path.exists(ROLLBACK_LOG):
            os.remove(ROLLBACK_LOG)
        if os.path.exists(TEMP_DIR) and not os.listdir(TEMP_DIR):
            os.rmdir(TEMP_DIR)
    except Exception as e:
        print(f"=== ROLLBACK_CLEANUP_ERROR: {str(e)} ===")
        
    print("=== ROLLBACK_COMPLETE ===")

def safe_rename(old_path, new_path):
    """Safely rename file with rollback logging"""
    try:
        # Check if source file exists
        if not os.path.exists(old_path):
            raise Exception(f"Source file does not exist: {old_path}")
        
        # Check if destination already exists
        if os.path.exists(new_path):
            raise Exception(f"Destination file already exists: {new_path}")
        
        # Check if we have permission to rename
        if not os.access(old_path, os.W_OK):
            raise Exception(f"No write permission for file: {old_path}")
        
        # Check if directory is writable
        directory = os.path.dirname(old_path)
        if not os.access(directory, os.W_OK):
            raise Exception(f"No write permission for directory: {directory}")
        
        # Perform the rename
        os.rename(old_path, new_path)
        
        # Log the operation for rollback
        log_operation("rename", old_path, new_path)
        
        print(f"=== RENAMED: {os.path.basename(old_path)} -> {os.path.basename(new_path)} ===")
        
    except PermissionError as e:
        raise Exception(f"Permission denied - file may be open in another application: {os.path.basename(old_path)}")
    except FileExistsError as e:
        raise Exception(f"Destination file already exists: {os.path.basename(new_path)}")
    except OSError as e:
        raise Exception(f"System error renaming {os.path.basename(old_path)}: {str(e)}")
    except Exception as e:
        raise Exception(f"Failed to rename {os.path.basename(old_path)}: {str(e)}")

def main():
    """Main processing function"""
    try:
        print("=== STARTING_FILE_RENAMING ===")
        
        # Check if target directory exists
        if not os.path.exists(FOLDER_PATH):
            raise Exception(f"Target directory does not exist: {FOLDER_PATH}")
        
        # Get all xlsx and csv files in the directory (not in subfolders)
        try:
            target_files = [f for f in os.listdir(FOLDER_PATH) 
                          if f.endswith(('.xlsx', '.csv')) and os.path.isfile(os.path.join(FOLDER_PATH, f))]
        except Exception as e:
            raise Exception(f"Failed to read directory contents: {str(e)}")
        
        if not target_files:
            print("=== NO_FILES_TO_RENAME ===")
            print("=== SCRIPT_SUCCESS ===")
            return 0
        
        # Sort the files to ensure consistent numbering
        target_files.sort()
        
        print(f"=== FOUND_FILES: {len(target_files)} ===")
        
        # Rename files with numerical prefix
        renamed_count = 0
        for index, filename in enumerate(target_files, start=1):
            try:
                # Create the new filename with 2-digit prefix
                new_filename = f"{index:02d} {filename}"
                
                # Create full file paths
                old_file_path = os.path.join(FOLDER_PATH, filename)
                new_file_path = os.path.join(FOLDER_PATH, new_filename)
                
                # Perform safe rename with rollback support
                safe_rename(old_file_path, new_file_path)
                renamed_count += 1
                
            except Exception as e:
                # If any rename fails, this is fatal - rollback everything
                print(f"=== RENAME_ERROR: {str(e)} ===")
                raise Exception(f"Failed to rename file {filename}: {str(e)}")
        
        # Save rollback log for BAT coordination
        save_rollback_log()
        
        print(f"=== ALL_FILES_RENAMED_SUCCESSFULLY: {renamed_count} ===")
        print("=== SCRIPT_SUCCESS ===")
        return 0
        
    except Exception as e:
        print(f"=== SCRIPT_ERROR ===")
        print(f"=== ERROR_MESSAGE: {str(e)} ===")
        print(f"=== POSSIBLE_CAUSES: File may be open in another application, insufficient permissions, or disk full ===")
        
        # Perform rollback
        perform_rollback()
        
        return 1

if __name__ == "__main__":
    sys.exit(main())

