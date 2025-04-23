import os
from pathlib import Path

# Define the directory path
folder_path = r"C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\DATA PROCESSING\RAW FILES"

# Get all xlsx and csv files in the directory (not in subfolders)
target_files = [f for f in os.listdir(folder_path) if f.endswith(('.xlsx', '.csv'))]

# Sort the files to ensure consistent numbering
target_files.sort()

# Rename files with numerical prefix
for index, filename in enumerate(target_files, start=1):
    # Create the new filename with 2-digit prefix
    new_filename = f"{index:02d} {filename}"
    
    # Create full file paths
    old_file_path = os.path.join(folder_path, filename)
    new_file_path = os.path.join(folder_path, new_filename)
    
    # Rename the file
    os.rename(old_file_path, new_file_path)
    print(f"Renamed: {filename} -> {new_filename}")
