import os
import shutil
from datetime import datetime
from pathlib import Path

def find_latest_backup(filename, backup_dir):
    # Remove timestamp pattern from backup filename to find original name
    base_name = filename.split('_')[0] + '.csv'
    matching_files = []
    
    # Find all backup versions of this file
    for root, _, files in os.walk(backup_dir):
        for file in files:
            if file.startswith(base_name.replace('.csv', '')):
                full_path = os.path.join(root, file)
                matching_files.append(full_path)
    
    # Return most recent backup if found
    return max(matching_files, key=os.path.getctime) if matching_files else None

def rollback_files():
    BACKUP_DIR = r"C:\Users\JCox\Desktop\AUTOMATION\RAC\WEEKLY\CSVs WITH QUOTES BACKUP"
    BASE_DIR = r"C:\Users\JCox\Desktop\AUTOMATION\RAC"
    
    # Find all CSV files in backup directory
    for root, _, files in os.walk(BACKUP_DIR):
        for file in files:
            if file.endswith('.csv'):
                backup_path = os.path.join(root, file)
                
                # Calculate original file path
                rel_path = os.path.relpath(root, BACKUP_DIR)
                orig_name = file.split('_')[0] + '.csv'
                orig_path = os.path.join(BASE_DIR, rel_path, orig_name)
                
                # Restore file
                os.makedirs(os.path.dirname(orig_path), exist_ok=True)
                shutil.copy2(backup_path, orig_path)
                os.remove(backup_path)
    
    # Clean up empty directories
    for root, dirs, files in os.walk(BACKUP_DIR, topdown=False):
        for dir in dirs:
            dir_path = os.path.join(root, dir)
            if not os.listdir(dir_path):
                os.rmdir(dir_path)

if __name__ == "__main__":
    print("STARTING ROLLBACK PROCESS...")
    try:
        rollback_files()
        print("ROLLBACK COMPLETED SUCCESSFULLY")
    except Exception as e:
        print(f"ERROR DURING ROLLBACK: {str(e)}")
    input("PRESS ANY KEY TO EXIT")
