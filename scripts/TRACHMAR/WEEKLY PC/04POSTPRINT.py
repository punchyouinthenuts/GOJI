import os
import shutil
from datetime import datetime
import sys
import traceback
import re
import json
import time
import tkinter as tk
from tkinter import messagebox

def print_status(message):
    """Print status message to stdout and flush"""
    print(message)
    sys.stdout.flush()

def print_error(message):
    """Print error message to stderr and flush"""
    print(f"ERROR: {message}", file=sys.stderr)
    sys.stderr.flush()

def print_warning(message):
    """Print warning message to stderr and flush"""
    print(f"WARNING: {message}", file=sys.stderr)
    sys.stderr.flush()

CANONICAL_TM_WEEKLY_BASE = r"C:\Goji\AUTOMATION\TRACHMAR\WEEKLY PC"
POSTPRINT_MARKER_FILES_START = "=== POSTPRINT_FILES ==="
POSTPRINT_MARKER_FILES_END = "=== END_POSTPRINT_FILES ==="
POSTPRINT_FAIL_REASON_PREFIX = "POSTPRINT_FAIL_REASON="
AMBIGUOUS_TIME_SKEW_MS = 10 * 60 * 1000

def resolve_tm_weekly_base_path():
    """Resolve WEEKLY PC runtime path using canonical path plus optional non-legacy override."""
    configured_tm_base = os.environ.get("GOJI_TM_BASE_PATH", "").strip()
    if configured_tm_base:
        configured_weekly_path = (
            configured_tm_base
            if configured_tm_base.replace("\\", "/").upper().endswith("/WEEKLY PC")
            else os.path.join(configured_tm_base, "WEEKLY PC")
        )
        normalized_configured = configured_weekly_path.replace("\\", "/").upper()
        if normalized_configured.startswith("C:/GOJI/TRACHMAR"):
            print_warning(
                "Configured GOJI_TM_BASE_PATH resolves to legacy C:\\Goji\\TRACHMAR\\WEEKLY PC and will be ignored. "
                "Use C:\\Goji\\AUTOMATION\\TRACHMAR."
            )
        elif os.path.exists(configured_weekly_path):
            return configured_weekly_path
        else:
            print_warning(f"Configured GOJI_TM_BASE_PATH not found: {configured_weekly_path}")

    if os.path.exists(CANONICAL_TM_WEEKLY_BASE):
        return CANONICAL_TM_WEEKLY_BASE

    os.makedirs(CANONICAL_TM_WEEKLY_BASE, exist_ok=True)
    print_warning(f"Created canonical WEEKLY PC runtime path: {CANONICAL_TM_WEEKLY_BASE}")
    return CANONICAL_TM_WEEKLY_BASE

def emit_failure_reason(reason_code, detail):
    detail_text = str(detail).strip() if detail is not None else ""
    if detail_text:
        print_status(f"{POSTPRINT_FAIL_REASON_PREFIX}{reason_code}|{detail_text}")
    else:
        print_status(f"{POSTPRINT_FAIL_REASON_PREFIX}{reason_code}")

def parse_session_start_utc_ms(raw_value):
    if raw_value is None:
        return None
    text = str(raw_value).strip()
    if not text:
        return None
    try:
        parsed = int(text)
    except ValueError:
        return None
    return parsed if parsed > 0 else None

def load_baseline_manifest(baseline_manifest_path):
    path = str(baseline_manifest_path).strip()
    if not path:
        return None, "Baseline manifest path not provided"

    try:
        with open(path, "r", encoding="utf-8") as handle:
            payload = json.load(handle)
    except Exception as exc:
        return None, f"Unable to read baseline manifest: {exc}"

    files = payload.get("files")
    if not isinstance(files, list):
        return None, "Baseline manifest missing 'files' array"

    baseline_by_path = {}
    for entry in files:
        if not isinstance(entry, dict):
            return None, "Baseline manifest contains invalid entry type"

        raw_path = str(entry.get("path", "")).strip()
        if not raw_path:
            return None, "Baseline manifest entry missing path"
        normalized_path = os.path.normcase(os.path.abspath(raw_path))

        if "size" not in entry or "mtime_utc_ms" not in entry:
            return None, f"Baseline manifest entry missing size/mtime for {normalized_path}"

        try:
            size = int(entry["size"])
            mtime_utc_ms = int(entry["mtime_utc_ms"])
        except (TypeError, ValueError):
            return None, f"Baseline manifest entry has invalid size/mtime for {normalized_path}"

        baseline_by_path[normalized_path] = {
            "size": size,
            "mtime_utc_ms": mtime_utc_ms
        }

    return baseline_by_path, None

def list_print_pdf_files(print_dir):
    current_files = []

    for name in os.listdir(print_dir):
        if not name.lower().endswith(".pdf"):
            continue

        absolute_path = os.path.abspath(os.path.join(print_dir, name))
        if not os.path.isfile(absolute_path):
            continue

        stat_result = os.stat(absolute_path)
        current_files.append({
            "path": absolute_path,
            "key": os.path.normcase(absolute_path),
            "name": os.path.basename(absolute_path),
            "size": int(stat_result.st_size),
            "mtime_utc_ms": int(stat_result.st_mtime * 1000)
        })

    current_files.sort(key=lambda entry: entry["path"].lower())
    return current_files

def detect_current_run_print_files(print_dir, session_start_utc_ms, baseline_by_path):
    current_files = list_print_pdf_files(print_dir)
    changed_candidates = []

    for entry in current_files:
        baseline_entry = baseline_by_path.get(entry["key"])
        is_new_or_changed = (
            baseline_entry is None
            or baseline_entry["size"] != entry["size"]
            or baseline_entry["mtime_utc_ms"] != entry["mtime_utc_ms"]
        )
        if not is_new_or_changed:
            continue

        if entry["mtime_utc_ms"] < session_start_utc_ms:
            continue

        changed_candidates.append(entry)

    if not changed_candidates:
        return None, "STALE_PROTECTION_EMPTY_RESULT", "No new/changed PRINT PDFs passed session boundary checks"

    non_print_named = []
    for entry in changed_candidates:
        upper_name = entry["name"].upper()
        if "PRINT" not in upper_name or "WEEKLY" not in upper_name:
            non_print_named.append(entry["path"])

    if non_print_named:
        return None, "STALE_PROTECTION_AMBIGUOUS_CANDIDATE_SET", (
            "Changed PDF candidates include non-weekly-print names: "
            + ", ".join(non_print_named)
        )

    mtime_values = [entry["mtime_utc_ms"] for entry in changed_candidates]
    if len(mtime_values) > 1 and (max(mtime_values) - min(mtime_values)) > AMBIGUOUS_TIME_SKEW_MS:
        return None, "STALE_PROTECTION_AMBIGUOUS_CANDIDATE_SET", (
            f"Changed PDF candidates span more than {AMBIGUOUS_TIME_SKEW_MS}ms; refusing ambiguous set"
        )

    return [entry["path"] for entry in changed_candidates], None, None

def validate_parameters(job_number, month, week, year):
    """Validate input parameters"""
    errors = []
    
    if not job_number or not job_number.strip():
        errors.append("Job number is empty")
    elif not re.match(r'^\d{5}$', job_number.strip()):
        errors.append("Job number must be exactly 5 digits")
    
    if not month or not month.strip():
        errors.append("Month is empty")
    elif not re.match(r'^(0[1-9]|1[0-2])$', month.strip()):
        errors.append("Month must be 01-12 format")
    
    if not week or not week.strip():
        errors.append("Week is empty")
    elif not re.match(r'^\d{1,2}$', week.strip()):
        errors.append("Week must be a valid day number")
    
    if not year or not year.strip():
        errors.append("Year is empty")
    elif not re.match(r'^\d{4}$', year.strip()):
        errors.append("Year must be 4 digits")
    
    return errors

def clear_directory_contents(directory):
    """Clear contents of a directory while preserving the directory itself"""
    try:
        if not os.path.exists(directory):
            print_warning(f"Directory does not exist, skipping clear: {directory}")
            return True
            
        if not os.access(directory, os.W_OK):
            print_error(f"No write permission to clear directory: {directory}")
            return False
        
        items_cleared = 0
        for item in os.listdir(directory):
            item_path = os.path.join(directory, item)
            try:
                if os.path.isfile(item_path):
                    os.remove(item_path)
                    items_cleared += 1
                elif os.path.isdir(item_path):
                    shutil.rmtree(item_path)
                    os.makedirs(item_path)
                    items_cleared += 1
            except Exception as e:
                print_error(f"Failed to clear item {item}: {str(e)}")
                return False
        
        print_status(f"Cleared {items_cleared} items from {os.path.basename(directory)}")
        return True
        
    except Exception as e:
        print_error(f"Error clearing directory {directory}: {str(e)}")
        return False

def generate_renamed_filename(original_filename, job_number, month, week):
    """Generate new filename with job number and date prefix"""
    try:
        prefix = f"{job_number} {month}.{week}"
        if original_filename.startswith(prefix):
            print_status(f"File already has correct prefix: {original_filename}")
            return original_filename
        new_filename = f"{prefix} {original_filename}"
        print_status(f"Renamed: {original_filename} -> {new_filename}")
        return new_filename
    except Exception as e:
        print_error(f"Error generating renamed filename: {str(e)}")
        return original_filename

def copy_files_with_verification_and_rename(
    source_dir,
    dest_dir,
    job_number,
    month,
    week,
    file_pattern="*",
    source_file_paths=None
):
    """Copy files with verification, renaming, and return list of copied files"""
    copied_files = []
    
    try:
        if not os.path.exists(source_dir):
            print_error(f"Source directory does not exist: {source_dir}")
            return None
            
        if not os.access(source_dir, os.R_OK):
            print_error(f"No read permission for source directory: {source_dir}")
            return None
        
        os.makedirs(dest_dir, exist_ok=True)
        
        if not os.access(dest_dir, os.W_OK):
            print_error(f"No write permission for destination directory: {dest_dir}")
            return None
        
        file_suffix = file_pattern.replace("*", "")
        source_files = []
        if source_file_paths is None:
            for file in os.listdir(source_dir):
                if file_pattern == "*" or file.endswith(file_suffix):
                    source_files.append(os.path.join(source_dir, file))
        else:
            for source_file_path in source_file_paths:
                source_file = os.path.abspath(source_file_path)
                if not os.path.isfile(source_file):
                    print_error(f"Source file does not exist: {source_file}")
                    return None
                file = os.path.basename(source_file)
                if file_pattern == "*" or file.endswith(file_suffix):
                    source_files.append(source_file)

        files_found = len(source_files)
        for source_file in source_files:
            file = os.path.basename(source_file)
            renamed_file = generate_renamed_filename(file, job_number, month, week)
            dest_file = os.path.join(dest_dir, renamed_file)
            
            # FIXED: Use copy2 instead of move operation
            shutil.copy2(source_file, dest_file)
            
            if os.path.exists(dest_file):
                source_size = os.path.getsize(source_file)
                dest_size = os.path.getsize(dest_file)
                if source_size == dest_size:
                    copied_files.append(dest_file)
                    print_status(f"  - Copied and renamed: {file} -> {renamed_file} ({source_size} bytes)")
                else:
                    print_error(f"Size mismatch for {file}: source={source_size}, dest={dest_size}")
                    return None
            else:
                print_error(f"Failed to copy {file} - destination file not found")
                return None
        
        print_status(f"Successfully copied and renamed {len(copied_files)} of {files_found} files")
        return copied_files
        
    except Exception as e:
        print_error(f"Error copying files from {source_dir}: {str(e)}")
        return None

def copy_files_with_verification(source_dir, dest_dir, file_pattern="*"):
    """Copy files with verification and return list of copied files (no renaming)"""
    copied_files = []
    
    try:
        if not os.path.exists(source_dir):
            print_error(f"Source directory does not exist: {source_dir}")
            return None
            
        if not os.access(source_dir, os.R_OK):
            print_error(f"No read permission for source directory: {source_dir}")
            return None
        
        os.makedirs(dest_dir, exist_ok=True)
        
        if not os.access(dest_dir, os.W_OK):
            print_error(f"No write permission for destination directory: {dest_dir}")
            return None
        
        files_found = 0
        for file in os.listdir(source_dir):
            if file_pattern == "*" or file.endswith(file_pattern.replace("*", "")):
                files_found += 1
                source_file = os.path.join(source_dir, file)
                dest_file = os.path.join(dest_dir, file)
                
                shutil.copy2(source_file, dest_file)
                
                if os.path.exists(dest_file):
                    source_size = os.path.getsize(source_file)
                    dest_size = os.path.getsize(dest_file)
                    if source_size == dest_size:
                        copied_files.append(dest_file)
                        print_status(f"  - Copied {file} ({source_size} bytes)")
                    else:
                        print_error(f"Size mismatch for {file}: source={source_size}, dest={dest_size}")
                        return None
                else:
                    print_error(f"Failed to copy {file} - destination file not found")
                    return None
        
        print_status(f"Successfully copied {len(copied_files)} of {files_found} files")
        return copied_files
        
    except Exception as e:
        print_error(f"Error copying files from {source_dir}: {str(e)}")
        return None

def check_network_availability(nas_path):
    """Check if the network drive is available with retries"""
    retries = 3
    for attempt in range(retries):
        try:
            if os.path.exists(nas_path) and os.access(nas_path, os.W_OK):
                return True
            else:
                print_warning(f"Network drive check failed (attempt {attempt + 1}/{retries})")
                time.sleep(2)
        except OSError as e:
            print_warning(f"Network drive access error (attempt {attempt + 1}/{retries}): {str(e)}")
            time.sleep(2)
    return False

def show_network_unavailable_popup():
    """Display a pop-up warning that the network drive is unavailable"""
    try:
        root = tk.Tk()
        root.withdraw()  # Hide the main window
        root.attributes('-topmost', True)  # Bring to front
        root.focus_force()  # Force focus
        
        # Make sure the popup appears immediately
        root.update()
        
        messagebox.showwarning(
            title="Network Drive Unavailable",
            message="NETWORK DRIVE UNAVAILABLE\nPRINT FILE SAVED TO DESKTOP FOLDER"
        )
        root.destroy()
        
        # Also print to console for logging
        print_status("*** NETWORK DRIVE UNAVAILABLE ***")
        print_status("*** PRINT FILE SAVED TO DESKTOP FOLDER ***")
        
    except Exception as e:
        # If GUI fails for any reason, fall back to console output
        print_status("*** NETWORK DRIVE UNAVAILABLE ***")
        print_status("*** PRINT FILE SAVED TO DESKTOP FOLDER ***")
        print_warning(f"Could not show popup dialog: {str(e)}")

def post_print_process(job_number, month, week, year, session_start_raw, baseline_manifest_path):
    """
    Handles post-print processing tasks
    
    Args:
        job_number: 5-digit job number from jobNumberBoxTMWPC
        month: Month number from monthDDboxTMWPC (2-digit format)
        week: Week number from weekDDboxTMWPC (day of month)
        year: Year value from yearDDboxTMWPC (4-digit format)
    """
    operations_completed = []

    try:
        print_status("=== POST PRINT PROCESS ===")
        
        validation_errors = validate_parameters(job_number, month, week, year)
        if validation_errors:
            for error in validation_errors:
                print_error(f"Parameter validation: {error}")
            return False
        
        job_number = job_number.strip()
        month = month.strip()
        week = week.strip()
        year = year.strip()
        week_number = f"{month}.{week}"
        
        print_status(f"Processing job {job_number}, week {week_number}, year {year}")
        
        session_start_utc_ms = parse_session_start_utc_ms(session_start_raw)
        if session_start_utc_ms is None:
            emit_failure_reason(
                "STALE_PROTECTION_MISSING_SESSION_SIGNAL",
                "Missing or invalid print_session_start_utc_ms"
            )
            print_error("Missing or invalid print session boundary signal")
            return False

        baseline_by_path, baseline_error = load_baseline_manifest(baseline_manifest_path)
        if baseline_error is not None:
            emit_failure_reason("STALE_PROTECTION_BASELINE_UNREADABLE", baseline_error)
            print_error(baseline_error)
            return False

        # Define paths
        weekly_base_path = resolve_tm_weekly_base_path()
        source_print_path = os.path.join(weekly_base_path, "JOB", "PRINT")
        nas_base_path = f"\\\\NAS1069D9\\AMPrintData\\{year}_SrcFiles\\T\\Trachmar"
        job_folder_path = os.path.join(nas_base_path, job_number)
        week_folder_path = os.path.join(job_folder_path, week_number)
        fallback_path = os.path.join(r"C:\Users\JCox\Desktop\MOVE TO NETWORK DRIVE", job_number, week_number)
        print_status(f"Source PRINT path: {source_print_path}")
        print_status(f"Session boundary (UTC ms): {session_start_utc_ms}")
        print_status(f"Baseline entries: {len(baseline_by_path)}")
        
        if not os.path.exists(source_print_path):
            print_error(f"Print source path does not exist: {source_print_path}")
            return False
        
        if not os.access(source_print_path, os.R_OK):
            print_error(f"No read permission for print source: {source_print_path}")
            return False
        
        try:
            postprint_files, fail_code, fail_detail = detect_current_run_print_files(
                source_print_path,
                session_start_utc_ms,
                baseline_by_path
            )
        except Exception as e:
            emit_failure_reason("STALE_PROTECTION_BASELINE_UNREADABLE", f"Failed during PDF detection: {e}")
            print_error(f"Failed during current-run PDF detection: {e}")
            return False

        if fail_code is not None:
            emit_failure_reason(fail_code, fail_detail)
            print_error(fail_detail)
            return False

        if not check_network_availability(nas_base_path):
            print_warning(f"Network drive unavailable: {nas_base_path}")
            print_status(f"Using fallback directory: {fallback_path}")
            destination_path = fallback_path
            show_network_unavailable_popup()
        else:
            destination_path = week_folder_path

        print_status(f"Destination: {destination_path}")

        try:
            print_status("Creating destination directory structure...")
            os.makedirs(destination_path, exist_ok=True)
            print_status(f"Created/verified folder: {destination_path}")
            operations_completed.append(("create_dirs", destination_path))
        except Exception as e:
            print_error(f"Failed to create folders: {str(e)}")
            return False

        print_status("Copying and renaming current-run PDF files...")
        copied_postprint_files = copy_files_with_verification_and_rename(
            source_print_path,
            destination_path,
            job_number,
            month,
            week,
            "*.pdf",
            source_file_paths=postprint_files
        )
        if copied_postprint_files is None:
            rollback(operations_completed)
            return False
        operations_completed.append(("copy_pdf", copied_postprint_files))

        print_status("=== OUTPUT_PATH ===")
        print_status(destination_path)
        print_status("=== END_OUTPUT_PATH ===")

        # Emit exact destination file paths for GOJI popup population.
        print_status(POSTPRINT_MARKER_FILES_START)
        for file_path in copied_postprint_files:
            print_status(os.path.abspath(file_path))
        print_status(POSTPRINT_MARKER_FILES_END)

        print_status("=== POST PRINT SUMMARY ===")
        print_status(f"Job: {job_number} ({week_number}/{year})")
        print_status(f"Current-run PRINT PDFs copied: {len(copied_postprint_files)}")
        print_status("POST PRINT PROCESS COMPLETED SUCCESSFULLY!")
        
        return True
        
    except Exception as e:
        emit_failure_reason("STALE_PROTECTION_BASELINE_UNREADABLE", f"Unexpected post-print failure: {e}")
        print_error(f"Unexpected error in post-print process: {str(e)}")
        traceback.print_exc()
        rollback(operations_completed)
        return False

def rollback(operations_completed):
    """Roll back completed operations in case of an error"""
    print_status("ROLLBACK: Attempting to undo completed operations...")
    
    rollback_errors = 0
    
    for operation_type, operation_data in reversed(operations_completed):
        try:
            if operation_type == "copy_pdf":
                for file_path in operation_data:
                    if os.path.exists(file_path):
                        os.remove(file_path)
                        print_status(f"  - Deleted file: {os.path.basename(file_path)}")
                
            elif operation_type == "copy_job_contents":
                for item_path in operation_data:
                    if os.path.exists(item_path):
                        if os.path.isfile(item_path):
                            os.remove(item_path)
                        elif os.path.isdir(item_path):
                            shutil.rmtree(item_path)
                        print_status(f"  - Deleted: {os.path.basename(item_path)}")
                        
            elif operation_type == "create_dirs":
                print_status("Directories left intact (safe to leave)")
            
        except Exception as e:
            print_error(f"Error during rollback of {operation_type}: {str(e)}")
            rollback_errors += 1
    
    if rollback_errors > 0:
        print_error(f"Rollback completed with {rollback_errors} errors - manual cleanup may be needed")
    else:
        print_status("Rollback completed successfully")

if __name__ == "__main__":
    print_status("=== POST PRINT SCRIPT ===")
    
    try:
        if len(sys.argv) >= 7:
            job_number = sys.argv[1]
            month = sys.argv[2]
            week = sys.argv[3]
            year = sys.argv[4]
            session_start_utc_ms = sys.argv[5]
            baseline_manifest_path = sys.argv[6]
            
            print_status(
                "Parameters: "
                f"Job={job_number}, Month={month}, Week={week}, Year={year}, "
                f"SessionUTCms={session_start_utc_ms}, BaselineManifest={baseline_manifest_path}"
            )
            
            success = post_print_process(
                job_number,
                month,
                week,
                year,
                session_start_utc_ms,
                baseline_manifest_path
            )
            
            if success:
                print_status("Script completed successfully")
                sys.exit(0)
            else:
                print_error("Script completed with errors")
                sys.exit(1)
        else:
            emit_failure_reason(
                "STALE_PROTECTION_MISSING_SESSION_SIGNAL",
                "Missing required post-print parameters"
            )
            print_error("Missing required parameters")
            print_status(
                "Usage: python 04POSTPRINT.py "
                "<job_number> <month> <week> <year> <print_session_start_utc_ms> <baseline_manifest_path>"
            )
            print_status("This script should normally be called from the GOJI application")
            sys.exit(1)
            
    except KeyboardInterrupt:
        print_error("Script interrupted by user")
        sys.exit(2)
    except Exception as e:
        print_error(f"Unexpected script error: {str(e)}")
        traceback.print_exc()
        sys.exit(3)
