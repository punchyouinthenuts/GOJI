import os
import shutil
import glob

# Get NCWO week number from user
ncwo_week = input("ENTER NCWO WEEK NUMBER: ")

# Define base directory and new folder paths
base_dir = r"\\NAS1069D9\AMPrintData\2025_SrcFiles\I\Innerworkings\29169 DEC NCWO"
new_folder = os.path.join(base_dir, f"{ncwo_week} NCWO")
data_folder = os.path.join(new_folder, "DATA")

# Create the new folders
os.makedirs(data_folder, exist_ok=True)

# Source directory for CSV files
source_dir = r"C:\Users\JCox\Desktop\AUTOMATION\RAC\NCWO_4TH\DM03\PROOF"

# Copy all CSV files from source to DATA folder
csv_files = glob.glob(os.path.join(source_dir, "*.csv"))
for file in csv_files:
    shutil.copy2(file, data_folder)

print(f"Created folder structure and copied files for NCWO week {ncwo_week}")
