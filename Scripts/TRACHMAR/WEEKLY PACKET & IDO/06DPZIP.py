import os
import zipfile
import shutil
from datetime import datetime

# Define paths
source_dirs = {
    'PROCESSED': r"C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\DATA PROCESSING\PROCESSED",
    'PREFLIGHT': r"C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\DATA PROCESSING\PREFLIGHT"
}
backup_dir = r"C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\DATA PROCESSING\BACKUP"
parent_dir = r"C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\DATA PROCESSING"

# Create backup directory if it doesn't exist
if not os.path.exists(backup_dir):
    os.makedirs(backup_dir)

# Get current timestamp
timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")

# Process each directory
for dir_type, source_dir in source_dirs.items():
    zip_filename = os.path.join(parent_dir, f"{dir_type}_TM_FILES_{timestamp}.zip")
    
    # Create zip file
    with zipfile.ZipFile(zip_filename, 'w') as zipf:
        # Look for CSV files in source directory
        for file in os.listdir(source_dir):
            if file.lower().endswith('.csv'):
                file_path = os.path.join(source_dir, file)
                # Create new filename with _MERGED
                base_name = os.path.splitext(file)[0]
                merged_filename = f"{base_name}_MERGED.csv"
                # Add file to zip with new name
                zipf.write(file_path, merged_filename)
                # Move file to backup directory
                shutil.move(file_path, os.path.join(backup_dir, file))
    
    print(f"Process completed. ZIP file created: {zip_filename}")
