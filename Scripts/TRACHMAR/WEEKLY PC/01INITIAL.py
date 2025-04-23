import os
import glob
import re
import shutil
from datetime import datetime

def get_next_available_folder(base_path):
    if not os.path.exists(base_path):
        return base_path
    
    base_name = os.path.basename(base_path)
    parent_dir = os.path.dirname(base_path)
    
    for letter in 'ABCDEFGHIJKLMNOPQRSTUVWXYZ':
        new_name = f"{base_name}_{letter}"
        new_path = os.path.join(parent_dir, new_name)
        if not os.path.exists(new_path):
            return new_path
    return None

def process_fhk_file():
    downloads_path = os.path.expanduser(r"C:\Users\JCox\Downloads")
    weekly_base_path = r"C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\WEEKLY"
    job_input_path = os.path.join(weekly_base_path, "JOB", "INPUT")
    
    fhk_files = glob.glob(os.path.join(downloads_path, "*FHK*"))
    
    if not fhk_files:
        print("No FHK files found in Downloads folder")
        return
    
    fhk_file = max(fhk_files, key=os.path.getctime)
    filename = os.path.basename(fhk_file)
    
    digits = re.findall(r'\d{4}', filename)
    
    if not digits:
        print("No 4-digit sequence found in filename")
        return
    
    TMDATE = digits[-1]
    folder_name = f"{TMDATE[:2]}-{TMDATE[2:]}"
    new_folder_path = os.path.join(weekly_base_path, folder_name)
    
    if os.path.exists(new_folder_path):
        response = input("FOLDER ALREADY EXISTS. WOULD YOU LIKE TO OVERWRITE? Y/N: ").upper()
        if response == 'Y':
            shutil.rmtree(new_folder_path)
        elif response == 'N':
            new_folder_path = get_next_available_folder(new_folder_path)
            if not new_folder_path:
                print("No available folder names found")
                return
        else:
            print("Invalid input. Script terminated.")
            return
    
    try:
        os.makedirs(new_folder_path, exist_ok=True)
        destination_path = os.path.join(new_folder_path, filename)
        input_destination = os.path.join(job_input_path, "FHK_WEEKLY.csv")
        
        shutil.move(fhk_file, destination_path)
        shutil.copy2(destination_path, input_destination)
        
        print(f"Successfully processed file:")
        print(f"- Created folder: {new_folder_path}")
        print(f"- Moved file to: {destination_path}")
        print(f"- Copied and renamed file to: {input_destination}")
        
    except Exception as e:
        print(f"Error processing file: {str(e)}")

if __name__ == "__main__":
    process_fhk_file()
