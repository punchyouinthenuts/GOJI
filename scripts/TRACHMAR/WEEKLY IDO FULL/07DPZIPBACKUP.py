
import os
import sys
import json
import shutil
from datetime import datetime

CANONICAL_TM_ROOT = r"C:\Goji\AUTOMATION\TRACHMAR"
LEGACY_TM_ROOT = r"C:\Goji\TRACHMAR"

def resolve_tm_root():
    if os.path.isdir(CANONICAL_TM_ROOT):
        return CANONICAL_TM_ROOT
    if os.path.isdir(LEGACY_TM_ROOT):
        print("=== WARNING: USING_LEGACY_TRACHMAR_ROOT C:\\Goji\\TRACHMAR ===")
        return LEGACY_TM_ROOT
    os.makedirs(CANONICAL_TM_ROOT, exist_ok=True)
    print("=== INFO: CREATED_CANONICAL_TRACHMAR_ROOT C:\\Goji\\AUTOMATION\\TRACHMAR ===")
    return CANONICAL_TM_ROOT

# Define directories for GOJI
SOURCE_DIR = os.path.join(resolve_tm_root(), "WEEKLY IDO FULL")
BACKUP_DIR = os.path.join(SOURCE_DIR, "BACKUP")
TEMP_DIR = os.path.join(SOURCE_DIR, "TEMP")
ROLLBACK_LOG = os.path.join(TEMP_DIR, "rollback_07.log")

# ZIP filename prefixes to capture (aligned with 06DPZIP updates)
ZIP_PREFIXES = ("PROCESSED_", "PFDATA_", "PDF_")

# Global rollback tracking
rollback_operations = []

def log_operation(operation_type, source_path, dest_path=None, created_file=None):
    """Log an operation for potential rollback"""
    operation = {
        "type": operation_type,  # e.g., "move", "mkdir", "cleanup"
        "source": source_path,
        "destination": dest_path,
        "created_file": created_file,
        "timestamp": datetime.now().isoformat()
    }
    rollback_operations.append(operation)

def save_rollback_log():
    """Persist rollback operations to log file"""
    try:
        os.makedirs(TEMP_DIR, exist_ok=True)
        with open(ROLLBACK_LOG, 'w', encoding='utf-8') as f:
            json.dump(rollback_operations, f, indent=2)
        print(f"=== ROLLBACK_LOG_SAVED: {ROLLBACK_LOG} ===")
    except Exception as e:
        print(f"=== ERROR_SAVING_ROLLBACK_LOG: {str(e)} ===")

def perform_rollback():
    """Attempt to undo file moves where possible"""
    print("=== STARTING_ROLLBACK ===")
    for operation in reversed(rollback_operations):
        try:
            if operation["type"] == "move":
                # Move file back from destination to source if still present
                if operation["destination"] and os.path.exists(operation["destination"]):
                    os.makedirs(os.path.dirname(operation["source"]), exist_ok=True)
                    shutil.move(operation["destination"], operation["source"])
                    print(f"=== ROLLBACK_MOVED: {operation['destination']} -> {operation['source']} ===")
            elif operation["type"] == "mkdir":
                # Remove created directory if empty
                created = operation.get("created_file")
                if created and os.path.exists(created):
                    try:
                        os.rmdir(created)
                        print(f"=== ROLLBACK_REMOVED_DIR: {created} ===")
                    except OSError:
                        # Not empty; leave it
                        pass
            elif operation["type"] == "cleanup":
                # Non-reversible informational steps
                print("=== ROLLBACK_NOTE: Cleanup operation cannot be undone ===")
        except Exception as e:
            print(f"=== ROLLBACK_ERROR: {str(e)} ===")
    # Try to remove rollback log if done
    try:
        if os.path.exists(ROLLBACK_LOG):
            os.remove(ROLLBACK_LOG)
        if os.path.exists(TEMP_DIR) and not os.listdir(TEMP_DIR):
            os.rmdir(TEMP_DIR)
    except Exception:
        pass
    print("=== ROLLBACK_COMPLETE ===")

def safe_makedirs(path):
    """Create a directory if missing; log for rollback"""
    if not os.path.exists(path):
        os.makedirs(path)
        log_operation("mkdir", path, created_file=path)
        print(f"=== CREATED_DIRECTORY: {path} ===")

def unique_backup_path(dest_dir, filename, timestamp):
    """Return a non-colliding destination path in BACKUP by appending timestamp if needed."""
    base, ext = os.path.splitext(filename)
    candidate = os.path.join(dest_dir, filename)
    if not os.path.exists(candidate):
        return candidate
    # If a file exists, append timestamp
    suffixed = f"{base}_{timestamp}{ext}"
    return os.path.join(dest_dir, suffixed)

def scan_zip_files():
    """Find ZIP files to back up. Accepts prefixes or any *_TM_FILES_* pattern."""
    matches = []
    if not os.path.exists(SOURCE_DIR):
        return matches
    for file in os.listdir(SOURCE_DIR):
        if not file.lower().endswith('.zip'):
            continue
        if file.startswith(ZIP_PREFIXES) or "_TM_FILES_" in file:
            matches.append(file)
    return sorted(matches)

def safe_move(src, dst):
    """Move file with rollback logging"""
    if os.path.exists(src):
        shutil.move(src, dst)
        log_operation("move", src, dst)
        print(f"=== MOVED_FILE: {os.path.basename(src)} -> {os.path.basename(dst)} ===")

def main():
    try:
        print("=== STARTING_DPZIP_BACKUP ===")

        # Ensure required dirs
        safe_makedirs(BACKUP_DIR)
        safe_makedirs(TEMP_DIR)  # for rollback log and any temp needs

        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        print(f"=== TIMESTAMP: {timestamp} ===")

        # Discover candidate ZIPs in SOURCE_DIR
        zip_files = scan_zip_files()
        if not zip_files:
            print("=== NO_PROCESSED_PFDATA_OR_PDF_ZIP_FILES_IN_SOURCE_DIRECTORY ===")
            print("=== SCRIPT_SUCCESS ===")
            return 0

        print(f"=== ZIP_FILES_FOUND: {len(zip_files)} ===")
        for z in zip_files:
            print(f"=== ZIP_CANDIDATE: {z} ===")

        moved_count = 0

        # Move each ZIP into BACKUP (ensuring no filename collisions)
        for file in zip_files:
            src_path = os.path.join(SOURCE_DIR, file)
            dst_path = unique_backup_path(BACKUP_DIR, file, timestamp)
            safe_move(src_path, dst_path)
            moved_count += 1

        # Persist rollback info for potential undo
        save_rollback_log()

        print(f"=== TOTAL_ZIP_FILES_MOVED_TO_BACKUP: {moved_count} ===")
        print("=== BACKUP_COMPLETED_SUCCESSFULLY ===")
        print("=== SCRIPT_SUCCESS ===")
        return 0

    except Exception as e:
        print("=== SCRIPT_ERROR ===")
        print(f"=== ERROR_MESSAGE: {str(e)} ===")
        perform_rollback()
        return 1

if __name__ == "__main__":
    sys.exit(main())
