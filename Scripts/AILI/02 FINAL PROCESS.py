import os
import shutil
import csv
import re
import zipfile
import datetime
import tempfile
import sys
import traceback

class RollbackManager:
    def __init__(self):
        self.backup_dir = tempfile.mkdtemp(prefix="ail_backup_")
        self.actions = []
        print(f"Rollback system initialized. Backup directory: {self.backup_dir}")
    
    def backup_directory(self, dir_path):
        """Create a backup of a directory"""
        if not os.path.exists(dir_path):
            return
            
        dir_name = os.path.basename(dir_path)
        backup_path = os.path.join(self.backup_dir, dir_name)
        
        if os.path.exists(dir_path):
            shutil.copytree(dir_path, backup_path)
            self.actions.append(("restore_dir", dir_path, backup_path))
            print(f"Backed up directory: {dir_path}")
    
    def track_new_file(self, file_path):
        """Track a newly created file for potential deletion during rollback"""
        self.actions.append(("delete_file", file_path))
        print(f"Tracking new file for potential rollback: {file_path}")
    
    def track_renamed_file(self, old_path, new_path):
        """Track a renamed file for potential restoration during rollback"""
        self.actions.append(("rename_file", new_path, old_path))
        print(f"Tracking renamed file for potential rollback: {old_path} -> {new_path}")
    
    def rollback(self):
        """Perform rollback of all tracked actions"""
        print("\n*** PERFORMING ROLLBACK DUE TO ERROR ***")
        
        # Process actions in reverse order
        for action in reversed(self.actions):
            try:
                if action[0] == "restore_dir":
                    dir_path, backup_path = action[1], action[2]
                    if os.path.exists(dir_path):
                        shutil.rmtree(dir_path)
                    shutil.copytree(backup_path, dir_path)
                    print(f"Restored directory: {dir_path}")
                
                elif action[0] == "delete_file":
                    file_path = action[1]
                    if os.path.exists(file_path):
                        os.remove(file_path)
                        print(f"Deleted file: {file_path}")
                
                elif action[0] == "rename_file":
                    current_path, original_path = action[1], action[2]
                    if os.path.exists(current_path):
                        # Make sure the directory exists
                        os.makedirs(os.path.dirname(original_path), exist_ok=True)
                        shutil.move(current_path, original_path)
                        print(f"Restored file name: {current_path} -> {original_path}")
            
            except Exception as e:
                print(f"Error during rollback action {action}: {str(e)}")
        
        print("Rollback completed.")
    
    def cleanup(self):
        """Clean up backup files after successful execution"""
        try:
            shutil.rmtree(self.backup_dir)
            print("Rollback backup files cleaned up successfully.")
        except Exception as e:
            print(f"Warning: Could not clean up backup directory: {str(e)}")

def copy_international_file(rollback_mgr):
    print("Copying AIL INTERNATIONAL.csv to OUTPUT folder...")
    source_path = r"C:\Users\JCox\Desktop\AUTOMATION\AIL\INPUT\AIL INTERNATIONAL.csv"
    dest_path = r"C:\Users\JCox\Desktop\AUTOMATION\AIL\OUTPUT\AIL INTERNATIONAL.csv"
    
    if os.path.exists(source_path):
        # Create OUTPUT directory if it doesn't exist
        os.makedirs(os.path.dirname(dest_path), exist_ok=True)
        shutil.copy2(source_path, dest_path)
        rollback_mgr.track_new_file(dest_path)
        print(f"File copied successfully to {dest_path}")
        return True
    else:
        print(f"Error: Source file {source_path} not found!")
        return False

def get_job_number():
    while True:
        job_number = input("ENTER JOB NUMBER: ")
        if re.match(r'^\d{5}$', job_number):
            return job_number
        else:
            print("Error: Job number must be a 5-digit number. Please try again.")

def count_csv_rows(file_path):
    if not os.path.exists(file_path):
        print(f"Warning: File {file_path} not found!")
        return 0
    
    with open(file_path, 'r', newline='') as csvfile:
        reader = csv.reader(csvfile)
        # Skip header row
        next(reader, None)
        # Count remaining rows
        row_count = sum(1 for row in reader)
    
    return row_count

def rename_files_with_job_number(job_number, rollback_mgr):
    print("Renaming files with job number and row counts...")
    output_dir = r"C:\Users\JCox\Desktop\AUTOMATION\AIL\OUTPUT"
    
    # Get list of CSV files in OUTPUT directory
    csv_files = [f for f in os.listdir(output_dir) if f.endswith('.csv')]
    
    renamed_files = []
    
    for file in csv_files:
        old_path = os.path.join(output_dir, file)
        
        # For DOMESTIC and INTERNATIONAL files, count rows and add to filename
        if "DOMESTIC" in file or "INTERNATIONAL" in file:
            row_count = count_csv_rows(old_path)
            new_filename = f"{job_number} {file.split('.')[0]}_{row_count}.csv"
        else:
            new_filename = f"{job_number} {file}"
        
        new_path = os.path.join(output_dir, new_filename)
        os.rename(old_path, new_path)
        rollback_mgr.track_renamed_file(old_path, new_path)
        print(f"Renamed: {file} -> {new_filename}")
        renamed_files.append(new_path)
    
    return renamed_files

def copy_files_to_destination(files, rollback_mgr):
    # First try W: drive
    w_drive_accessible = False
    try:
        # Check if W: drive is accessible
        if os.path.exists("W:\\"):
            # Try to write a test file to verify write permissions
            test_file = "W:\\test_access.tmp"
            with open(test_file, 'w') as f:
                f.write("test")
            os.remove(test_file)
            w_drive_accessible = True
    except Exception:
        w_drive_accessible = False
    
    if w_drive_accessible:
        print("Copying renamed files to W: drive...")
        destination_dir = "W:\\"
    else:
        print("BUSKRO OFFLINE, SAVING TO DESKTOP FOLDER, Press C to continue...")
        while True:
            user_input = input().strip().lower()
            if user_input == 'c':
                break
        
        destination_dir = r"C:\Users\JCox\Desktop\MOVE TO BUSKRO"
        os.makedirs(destination_dir, exist_ok=True)
        print(f"Copying renamed files to {destination_dir}...")
    
    copied_files = []
    for file in files:
        if "DOMESTIC" in file or "INTERNATIONAL" in file:
            filename = os.path.basename(file)
            destination = os.path.join(destination_dir, filename)
            try:
                shutil.copy2(file, destination)
                rollback_mgr.track_new_file(destination)
                copied_files.append(destination)
                print(f"Copied {filename} to {destination_dir}")
            except Exception as e:
                print(f"Error copying {filename} to {destination_dir}: {str(e)}")
                raise
    
    return copied_files

def create_zip_archive(job_number, rollback_mgr):
    print("Creating ZIP archive...")
    
    # Create date string in format YYYYMMDD
    date_str = datetime.datetime.now().strftime("%Y%m%d")
    zip_filename = f"{job_number} AIL SPOTLIGHT_{date_str}.zip"
    archive_dir = r"C:\Users\JCox\Desktop\AUTOMATION\AIL\ARCHIVE"
    zip_path = os.path.join(archive_dir, zip_filename)
    
    # Create ARCHIVE directory if it doesn't exist
    os.makedirs(archive_dir, exist_ok=True)
    
    # Create a temporary directory to store copies of the folders
    temp_dir = tempfile.mkdtemp()
    
    try:
        # Copy INPUT and OUTPUT folders to temp directory
        input_dir = r"C:\Users\JCox\Desktop\AUTOMATION\AIL\INPUT"
        output_dir = r"C:\Users\JCox\Desktop\AUTOMATION\AIL\OUTPUT"
        
        temp_input_dir = os.path.join(temp_dir, "INPUT")
        temp_output_dir = os.path.join(temp_dir, "OUTPUT")
        
        shutil.copytree(input_dir, temp_input_dir)
        shutil.copytree(output_dir, temp_output_dir)
        
        # Create the ZIP file
        with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED) as zipf:
            # Add INPUT folder and its contents
            for root, dirs, files in os.walk(temp_input_dir):
                for file in files:
                    file_path = os.path.join(root, file)
                    arcname = os.path.join("INPUT", os.path.relpath(file_path, temp_input_dir))
                    zipf.write(file_path, arcname)
            
            # Add OUTPUT folder and its contents
            for root, dirs, files in os.walk(temp_output_dir):
                for file in files:
                    file_path = os.path.join(root, file)
                    arcname = os.path.join("OUTPUT", os.path.relpath(file_path, temp_output_dir))
                    zipf.write(file_path, arcname)
        
        rollback_mgr.track_new_file(zip_path)
        print(f"ZIP archive created: {zip_path}")
        return zip_path
    
    finally:
        # Clean up the temporary directory
        shutil.rmtree(temp_dir)

def wait_for_user_confirmation(prompt, key):
    while True:
        user_input = input(f"{prompt}, Press {key} to continue... ").strip().lower()
        if user_input == key.lower():
            return

def clear_directories():
    print("Clearing INPUT and OUTPUT directories...")
    
    input_dir = r"C:\Users\JCox\Desktop\AUTOMATION\AIL\INPUT"
    output_dir = r"C:\Users\JCox\Desktop\AUTOMATION\AIL\OUTPUT"
    
    # Clear INPUT directory
    for item in os.listdir(input_dir):
        item_path = os.path.join(input_dir, item)
        if os.path.isfile(item_path):
            os.remove(item_path)
        elif os.path.isdir(item_path):
            shutil.rmtree(item_path)
    
    # Clear OUTPUT directory
    for item in os.listdir(output_dir):
        item_path = os.path.join(output_dir, item)
        if os.path.isfile(item_path):
            os.remove(item_path)
        elif os.path.isdir(item_path):
            shutil.rmtree(item_path)
    
    print("Directories cleared successfully")

def main():
    print("Starting AIL File Processing Script")
    
    # Initialize rollback manager
    rollback_mgr = RollbackManager()
    
    try:
        # Backup directories before making any changes
        rollback_mgr.backup_directory(r"C:\Users\JCox\Desktop\AUTOMATION\AIL\INPUT")
        rollback_mgr.backup_directory(r"C:\Users\JCox\Desktop\AUTOMATION\AIL\OUTPUT")
        
        # Step 1: Copy AIL INTERNATIONAL.csv to OUTPUT folder
        if not copy_international_file(rollback_mgr):
            print("Error: Could not copy AIL INTERNATIONAL.csv. Exiting.")
            rollback_mgr.rollback()
            input("Press Enter to exit...")
            return
        
        # Step 2: Get job number from user
        job_number = get_job_number()
        
        # Step 3: Rename files with job number and row counts
        renamed_files = rename_files_with_job_number(job_number, rollback_mgr)
        
        # Step 4: Copy renamed files to destination (W: drive or desktop folder)
        copied_files = copy_files_to_destination(renamed_files, rollback_mgr)
        
        # Step 5: Create ZIP archive
        zip_path = create_zip_archive(job_number, rollback_mgr)
        
        # Step 6: Wait for user confirmation
        wait_for_user_confirmation("ATTACH BAD ADDRESS FILE TO EMAIL BEFORE CONTINUING", "C")
        
        # Step 7: Clear INPUT and OUTPUT directories
        clear_directories()
        
        # Step 8: Show completion message
        print("\nProcessing complete!")
        print(f"- AIL INTERNATIONAL.csv copied to OUTPUT folder")
        print(f"- Files renamed with job number {job_number} and row counts")
        print(f"- Renamed files copied to destination")
        print(f"- ZIP archive created at {zip_path}")
        print(f"- INPUT and OUTPUT folders cleared")
        
        # Clean up rollback files after successful execution
        rollback_mgr.cleanup()
        
        # Wait for user to press X to exit
        wait_for_user_confirmation("Press X to terminate", "X")
        
        print("Exiting script.")
        
    except Exception as e:
        print(f"\nERROR: An unexpected error occurred: {str(e)}")
        traceback.print_exc()
        
        # Perform rollback
        rollback_mgr.rollback()
        
        input("Press Enter to exit...")

if __name__ == "__main__":
    main()
