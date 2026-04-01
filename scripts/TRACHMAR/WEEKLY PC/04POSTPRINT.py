import os
import shutil
from datetime import datetime
import sys
import traceback
import re
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

def copy_files_with_verification_and_rename(source_dir, dest_dir, job_number, month, week, file_pattern="*"):
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
        
        files_found = 0
        for file in os.listdir(source_dir):
            if file_pattern == "*" or file.endswith(file_pattern.replace("*", "")):
                files_found += 1
                source_file = os.path.join(source_dir, file)
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

def post_print_process(job_number, month, week, year):
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
        
        # Define paths
        nas_base_path = f"\\\\NAS1069D9\\AMPrintData\\{year}_SrcFiles\\T\\Trachmar"
        job_folder_path = os.path.join(nas_base_path, job_number)
        week_folder_path = os.path.join(job_folder_path, week_number)
        fallback_path = os.path.join(r"C:\Users\JCox\Desktop\MOVE TO NETWORK DRIVE", job_number, week_number)
        
        source_print_path = r"C:\Goji\TRACHMAR\WEEKLY PC\JOB\PRINT"
        
        # Check network drive availability
        use_fallback = False
        if not check_network_availability(nas_base_path):
            print_warning(f"Network drive unavailable: {nas_base_path}")
            print_status(f"Using fallback directory: {fallback_path}")
            destination_path = fallback_path
            use_fallback = True
            show_network_unavailable_popup()
        else:
            destination_path = week_folder_path
        
        print_status(f"Destination: {destination_path}")
        
        if not os.path.exists(source_print_path):
            print_error(f"Print source path does not exist: {source_print_path}")
            return False
        
        if not os.access(source_print_path, os.R_OK):
            print_error(f"No read permission for print source: {source_print_path}")
            return False
        
        try:
            print_status("Creating destination directory structure...")
            os.makedirs(destination_path, exist_ok=True)
            print_status(f"Created/verified folder: {destination_path}")
            operations_completed.append(("create_dirs", destination_path))
        except Exception as e:
            print_error(f"Failed to create folders: {str(e)}")
            return False
        
        print_status("Copying and renaming PDF files...")
        pdf_files = copy_files_with_verification_and_rename(
            source_print_path, destination_path, job_number, month, week, "*.pdf"
        )
        if pdf_files is None:
            rollback(operations_completed)
            return False
        elif len(pdf_files) == 0:
            print_warning("No PDF files found to copy")
        else:
            operations_completed.append(("copy_pdf", pdf_files))
            print_status(f"Successfully copied and renamed {len(pdf_files)} PDF files")
        
        # File management is handled by the application when jobs are closed
        print_status("Post-print processing complete - job files remain active for continued work")
        
        # FIXED: Output the correct path format for the GUI to capture
        print_status("=== OUTPUT_PATH ===")
        print_status(destination_path)
        print_status("=== END_OUTPUT_PATH ===")
        
        print_status("=== POST PRINT SUMMARY ===")
        print_status(f"Job: {job_number} ({week_number}/{year})")
        print_status(f"PDF files copied: {len(pdf_files) if pdf_files else 0}")
        print_status("Job folders preserved for continued work")
        print_status("POST PRINT PROCESS COMPLETED SUCCESSFULLY!")
        
        return True
        
    except Exception as e:
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
        if len(sys.argv) >= 5:
            job_number = sys.argv[1]
            month = sys.argv[2]
            week = sys.argv[3]
            year = sys.argv[4]
            
            print_status(f"Parameters: Job={job_number}, Month={month}, Week={week}, Year={year}")
            
            success = post_print_process(job_number, month, week, year)
            
            if success:
                print_status("Script completed successfully")
                sys.exit(0)
            else:
                print_error("Script completed with errors")
                sys.exit(1)
        else:
            print_error("Missing required parameters")
            print_status("Usage: python 04POSTPRINT.py <job_number> <month> <week> <year>")
            print_status("This script should normally be called from the GOJI application")
            sys.exit(1)
            
    except KeyboardInterrupt:
        print_error("Script interrupted by user")
        sys.exit(2)
    except Exception as e:
        print_error(f"Unexpected script error: {str(e)}")
        traceback.print_exc()
        sys.exit(3)