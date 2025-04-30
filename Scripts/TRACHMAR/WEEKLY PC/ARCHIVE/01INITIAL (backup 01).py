import os
import glob
import re
import shutil
from datetime import datetime

def process_fhk_file():
    # Define paths
    downloads_path = os.path.expanduser(r"C:\Users\JCox\Downloads")
    weekly_base_path = r"C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\WEEKLY"
    job_input_path = os.path.join(weekly_base_path, "JOB", "INPUT")
    
    # Search for FHK files
    fhk_files = glob.glob(os.path.join(downloads_path, "*FHK*"))
    
    if not fhk_files:
        print("No FHK files found in Downloads folder")
        return
    
    # Get the most recent file
    fhk_file = max(fhk_files, key=os.path.getctime)
    filename = os.path.basename(fhk_file)
    
    # Extract last 4 digits from filename
    digits = re.findall(r'\d{4}', filename)
    
    if not digits:
        print("No 4-digit sequence found in filename")
        return
    
    # Process the file
    TMDATE = digits[-1]
    folder_name = f"{TMDATE[:2]}-{TMDATE[2:]}"
    new_folder_path = os.path.join(weekly_base_path, folder_name)
    
    # Create directory and process files
    try:
        os.makedirs(new_folder_path, exist_ok=True)
        destination_path = os.path.join(new_folder_path, filename)
        
        # Move and copy files
        shutil.move(fhk_file, destination_path)
        shutil.copy2(destination_path, os.path.join(job_input_path, filename))
        
        # Log success
        print(f"Successfully processed file:")
        print(f"- Created folder: {new_folder_path}")
        print(f"- Moved file to: {destination_path}")
        print(f"- Copied file to: {job_input_path}")
        
    except Exception as e:
        print(f"Error processing file: {str(e)}")

if __name__ == "__main__":
    process_fhk_file()
