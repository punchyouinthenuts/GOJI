
import os
import sys
import json
import zipfile
import shutil
from datetime import datetime

# Define directories for GOJI
BASE_PATH = r"C:\Goji\TRACHMAR\WEEKLY IDO FULL"
SOURCE_DIRS = {
    'PROCESSED': os.path.join(BASE_PATH, "PROCESSED"),
    'PREFLIGHT': os.path.join(BASE_PATH, "PREFLIGHT")
}
BACKUP_DIR = os.path.join(BASE_PATH, "BACKUP")
PARENT_DIR = BASE_PATH
TEMP_DIR = os.path.join(BASE_PATH, "TEMP")
ROLLBACK_LOG = os.path.join(TEMP_DIR, "rollback_06.log")

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

def safe_create_zip(zip_filename, source_dir, dir_type):
    """Create ZIP of CSV files; inside-zip names get _MERGED"""
    try:
        with zipfile.ZipFile(zip_filename, 'w') as zipf:
            # Look for CSV files in source directory
            csv_files = []
            if os.path.exists(source_dir):
                for file in os.listdir(source_dir):
                    if file.lower().endswith('.csv'):
                        csv_files.append(file)
                        file_path = os.path.join(source_dir, file)
                        # Create new filename with _MERGED
                        base_name = os.path.splitext(file)[0]
                        merged_filename = f"{base_name}_MERGED.csv"
                        # Add file to zip with new name
                        zipf.write(file_path, merged_filename)
                        print(f"=== ADDED_TO_ZIP: {merged_filename} ===")

            if not csv_files:
                print(f"=== WARNING: No CSV files found in {dir_type} directory ===")

        # Log the ZIP creation
        log_operation("create", source_dir, created_file=zip_filename)
        print(f"=== ZIP_CREATED: {os.path.basename(zip_filename)} ===")

        return csv_files

    except Exception as e:
        raise Exception(f"Failed to create ZIP file {zip_filename}: {str(e)}")

def safe_create_zip_by_ext(zip_filename, source_dir, ext, rename_inside=False, rename_suffix="_MERGED"):
    """
    Create a ZIP from files matching extension 'ext' in source_dir.
    If rename_inside=True, append rename_suffix (before extension) inside the ZIP.
    Returns the list of original filenames that were zipped.
    """
    try:
        with zipfile.ZipFile(zip_filename, 'w') as zipf:
            matched = []
            if os.path.exists(source_dir):
                for file in os.listdir(source_dir):
                    if file.lower().endswith(ext.lower()):
                        matched.append(file)
                        file_path = os.path.join(source_dir, file)
                        if rename_inside:
                            base, _ = os.path.splitext(file)
                            in_zip_name = f"{base}{rename_suffix}{ext.lower()}"
                        else:
                            in_zip_name = file  # keep original name
                        zipf.write(file_path, in_zip_name)
                        print(f"=== ADDED_TO_ZIP: {in_zip_name} ===")

            if not matched:
                print(f"=== WARNING: No {ext} files found in {source_dir} ===")

        log_operation("create", source_dir, created_file=zip_filename)
        print(f"=== ZIP_CREATED: {os.path.basename(zip_filename)} ===")
        return matched

    except Exception as e:
        raise Exception(f"Failed to create ZIP file {zip_filename}: {str(e)}")

def build_zip_name(prefix, timestamp):
    return os.path.join(PARENT_DIR, f"{prefix}_TM_FILES_{timestamp}.zip")

def main():
    """Main processing function"""
    try:
        print("=== STARTING_DPZIP_PROCESSING ===")

        # Verify base directories exist
        for dir_type, source_dir in SOURCE_DIRS.items():
            if not os.path.exists(source_dir):
                print(f"=== WARNING: {dir_type} directory does not exist: {source_dir} ===")

        # Create backup directory if it doesn't exist
        safe_makedirs(BACKUP_DIR)

        # Get current timestamp
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        print(f"=== TIMESTAMP: {timestamp} ===")

        # Track total files processed
        total_files_moved = 0
        zip_files_created = []

        # Process CSVs in each directory (PREFLIGHT gets PFDATA_ prefix)
        for dir_type, source_dir in SOURCE_DIRS.items():
            if not os.path.exists(source_dir):
                print(f"=== SKIPPING: {dir_type} directory does not exist ===")
                continue

            # Change ZIP prefix for PREFLIGHT CSVs from PREFLIGHT_ to PFDATA_
            if dir_type == "PREFLIGHT":
                zip_prefix = "PFDATA"  # requested change
            else:
                zip_prefix = dir_type   # e.g., PROCESSED

            zip_filename = build_zip_name(zip_prefix, timestamp)
            print(f"=== PROCESSING_DIRECTORY: {dir_type} (CSV -> {os.path.basename(zip_filename)}) ===")

            # Create zip file for CSVs (inside zip names get _MERGED)
            csv_files = safe_create_zip(zip_filename, source_dir, dir_type)

            if csv_files:
                zip_files_created.append(os.path.basename(zip_filename))

                # Move CSV files to backup directory
                for file in csv_files:
                    file_path = os.path.join(source_dir, file)
                    backup_path = os.path.join(BACKUP_DIR, file)
                    safe_move(file_path, backup_path)
                    total_files_moved += 1
            else:
                # If no files were found, remove the empty ZIP
                if os.path.exists(zip_filename):
                    os.remove(zip_filename)
                    print(f"=== REMOVED_EMPTY_ZIP: {os.path.basename(zip_filename)} ===")

        # NEW: Process PDFs in PREFLIGHT (keep original names inside ZIP, prefix PDF_)
        preflight_dir = SOURCE_DIRS.get("PREFLIGHT")
        if preflight_dir and os.path.exists(preflight_dir):
            pdf_zip_filename = build_zip_name("PDF", timestamp)  # PDF_ prefix
            print(f"=== PROCESSING_DIRECTORY: PREFLIGHT (PDF -> {os.path.basename(pdf_zip_filename)}) ===")

            # Create ZIP with PDFs from PREFLIGHT, keeping original names
            pdf_files = safe_create_zip_by_ext(
                pdf_zip_filename,
                preflight_dir,
                ext=".pdf",
                rename_inside=False  # keep original names in ZIP
            )

            if pdf_files:
                zip_files_created.append(os.path.basename(pdf_zip_filename))
                # Move PDF files to backup
                for file in pdf_files:
                    file_path = os.path.join(preflight_dir, file)
                    backup_path = os.path.join(BACKUP_DIR, file)
                    safe_move(file_path, backup_path)
                    total_files_moved += 1
            else:
                if os.path.exists(pdf_zip_filename):
                    os.remove(pdf_zip_filename)
                    print(f"=== REMOVED_EMPTY_ZIP: {os.path.basename(pdf_zip_filename)} ===")
        else:
            print("=== SKIPPING: PREFLIGHT directory missing for PDF processing ===")

        if not zip_files_created:
            print("=== WARNING: No ZIP files were created (no matching files found) ===")
            print("=== SCRIPT_SUCCESS ===")
            return 0

        # Save rollback log for coordination
        save_rollback_log()

        print(f"=== ZIP_FILES_CREATED: {len(zip_files_created)} ===")
        for zip_file in zip_files_created:
            print(f"=== ZIP_FILE: {zip_file} ===")

        print(f"=== TOTAL_FILES_MOVED_TO_BACKUP: {total_files_moved} ===")

        print("=== ZIP_PROCESSING_COMPLETED_SUCCESSFULLY ===")
        print("=== ZIP_FILES_READY_FOR_EMAIL ===")
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
