import os
import shutil
import msvcrt

# Define paths
source_dir = r"C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\DATA PROCESSING"
backup_dir = r"C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\DATA PROCESSING\BACKUP"

# Ensure backup directory exists
if not os.path.exists(backup_dir):
    os.makedirs(backup_dir)

# Look for zip files
zip_files = [f for f in os.listdir(source_dir) if f.endswith('.zip') and 
             ('PREFLIGHT' in f or 'PROCESSED' in f)]

if zip_files:
    # Move each zip file to backup
    for zip_file in zip_files:
        source_path = os.path.join(source_dir, zip_file)
        backup_path = os.path.join(backup_dir, zip_file)
        shutil.move(source_path, backup_path)
        print(f"Moved {zip_file} to backup directory")
else:
    print("No PREFLIGHT or PROCESSED ZIP files found!")
    print("PRESS ANY KEY TO CONTINUE...")
    msvcrt.getch()
