import os
import sys
import time
from datetime import datetime

def print_progress(message, end='\r'):
    print(f"[{datetime.now().strftime('%H:%M:%S')}] {message}", end=end)
    if end == '\n':
        sys.stdout.flush()

def delete_files_keep_folders(base_path):
    """
    Deletes all files in base_path and its subdirectories, 
    but keeps the folder structure intact.
    """
    print_progress("Starting cleanup process...", end='\n')
    
    # Count total files to be deleted
    total_files = 0
    for root, dirs, files in os.walk(base_path):
        total_files += len(files)
    
    print_progress(f"Found {total_files} files to delete", end='\n')
    
    # Delete files
    deleted_count = 0
    errors = []
    
    for root, dirs, files in os.walk(base_path):
        for file in files:
            file_path = os.path.join(root, file)
            try:
                print_progress(f"Deleting ({deleted_count+1}/{total_files}): {file}")
                os.remove(file_path)
                deleted_count += 1
                time.sleep(0.05)  # Small delay to prevent system overload
            except Exception as e:
                errors.append((file_path, str(e)))
                print_progress(f"Error deleting {file}: {str(e)}", end='\n')
    
    # Report results
    print_progress(f"Cleanup complete. Deleted {deleted_count} of {total_files} files.", end='\n')
    
    if errors:
        print_progress(f"Encountered {len(errors)} errors:", end='\n')
        for file_path, error in errors:
            print_progress(f"  - {os.path.basename(file_path)}: {error}", end='\n')
    else:
        print_progress("All files were deleted successfully.", end='\n')

def main():
    try:
        base_path = r"C:\Users\JCox\Desktop\AUTOMATION\SISNEROS\CLEANUP"
        
        # Confirm before proceeding
        print(f"This will delete ALL FILES in {base_path} and its subfolders.")
        print("The folders themselves will be preserved.")
        confirm = input("Are you sure you want to proceed? (y/n): ")
        
        if confirm.lower() != 'y':
            print("Operation cancelled.")
            return
        
        delete_files_keep_folders(base_path)
        
        print("\nCLEANUP PROCESS COMPLETED")
        while True:
            if input("Press X to terminate...").lower() == 'x':
                break
                
    except Exception as e:
        print(f"\nError occurred: {str(e)}")
        while True:
            if input("Press X to terminate...").lower() == 'x':
                break

if __name__ == "__main__":
    main()