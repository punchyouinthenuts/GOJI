import os
import re
import shutil
from datetime import datetime
import zipfile

def create_backup(source_dir, exclude_folders):
    timestamp = datetime.now().strftime("%Y%m%d-%H%M")
    backup_name = f"BACKUP_{timestamp}.zip"
    backup_path = os.path.join(source_dir, "BACKUP ZIPS", backup_name)
    
    os.makedirs(os.path.join(source_dir, "BACKUP ZIPS"), exist_ok=True)
    
    with zipfile.ZipFile(backup_path, 'w', zipfile.ZIP_DEFLATED) as zipf:
        for root, dirs, files in os.walk(source_dir):
            dirs[:] = [d for d in dirs if d not in exclude_folders]
            
            for file in files:
                file_path = os.path.join(root, file)
                if file == os.path.basename(__file__):
                    continue
                    
                arcname = os.path.relpath(file_path, source_dir)
                zipf.write(file_path, arcname)
    
    return backup_path

def update_file_references(directory, old_text, new_text, exclude_folders):
    for root, dirs, files in os.walk(directory):
        dirs[:] = [d for d in dirs if d not in exclude_folders]
        
        python_files = [f for f in files if f.endswith('.py') and f != os.path.basename(__file__)]
        
        for file in python_files:
            file_path = os.path.join(root, file)
            
            with open(file_path, 'r') as f:
                content = f.read()
            
            if old_text in content:
                updated_content = content.replace(old_text, new_text)
                
                with open(file_path, 'w') as f:
                    f.write(updated_content)
                print(f"Updated: {file_path}")

def get_valid_year():
    while True:
        try:
            old_year = input("ENTER OLD YEAR: ")
            if len(old_year) == 4 and old_year.isdigit():
                return int(old_year)
            else:
                print("Please enter a valid 4-digit year.")
        except ValueError:
            print("Please enter a valid 4-digit year.")

def main():
    directory = r"C:\Users\JCox\Desktop\AUTOMATION\PYTHON SCRIPTS"
    exclude_folders = ["BACKUP ZIPS"]
    
    # Get the old year from user input
    old_year = get_valid_year()
    new_year = old_year + 1
    
    old_text = f"{old_year}_SrcFiles"
    new_text = f"{new_year}_SrcFiles"
    
    print(f"\nWill replace '{old_text}' with '{new_text}'")
    
    # Create backup first
    print("\nCreating backup...")
    backup_path = create_backup(directory, exclude_folders)
    print(f"Backup created: {backup_path}")
    
    # Perform the updates
    print("\nUpdating file references...")
    update_file_references(directory, old_text, new_text, exclude_folders)
    print("Update complete!")

if __name__ == "__main__":
    main()
