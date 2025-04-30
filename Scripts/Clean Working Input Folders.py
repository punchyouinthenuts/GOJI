import os
import shutil
import glob
from datetime import datetime

# Configuration
BASE_PATH = r"C:\Goji\RAC"
JOB_TYPES = ["CBC", "EXC", "INACTIVE", "NCWO", "PREPIF"]
LOG_FILE = os.path.join(BASE_PATH, f"clean_input_log_{datetime.now().strftime('%Y%m%d_%H%M%S')}.txt")

def log_message(message):
    """Log a message to the console and a log file."""
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    formatted_message = f"[{timestamp}] {message}"
    print(formatted_message)
    with open(LOG_FILE, "a", encoding="utf-8") as f:
        f.write(formatted_message + "\n")

def clean_input_folder(folder_path):
    """Delete all files in the specified folder."""
    if not os.path.exists(folder_path):
        log_message(f"Folder does not exist: {folder_path}")
        return False
    
    # Get all files in the folder (not subdirectories)
    files = glob.glob(os.path.join(folder_path, "*"))
    success = True
    
    for file_path in files:
        try:
            if os.path.isfile(file_path):
                os.remove(file_path)
                log_message(f"Deleted file: {file_path}")
            else:
                log_message(f"Skipped non-file item (directory): {file_path}")
        except Exception as e:
            log_message(f"Error deleting {file_path}: {e}")
            success = False
    
    return success

def main():
    """Clean all files from INPUT folders in GOJI working directories."""
    log_message("Starting cleanup of INPUT folders...")
    
    all_success = True
    
    for job_type in JOB_TYPES:
        input_folder = os.path.join(BASE_PATH, job_type, "JOB", "INPUT")
        log_message(f"Processing folder: {input_folder}")
        if not clean_input_folder(input_folder):
            all_success = False
    
    if all_success:
        log_message("Cleanup completed successfully.")
    else:
        log_message("Cleanup completed with errors. Check log for details.")

if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        log_message(f"Fatal error: {e}")
        sys.exit(1)