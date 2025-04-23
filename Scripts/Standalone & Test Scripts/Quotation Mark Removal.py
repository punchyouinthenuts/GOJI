import os
import csv
import shutil
import sys
from datetime import datetime

def create_backup(file_path):
    timestamp = datetime.now().strftime("_%Y%m%d-%H%M")
    backup_path = f"{os.path.splitext(file_path)[0]}{timestamp}{os.path.splitext(file_path)[1]}"
    shutil.copy2(file_path, backup_path)
    return backup_path

def remove_quotes(file_path):
    # Create temporary file path
    temp_path = file_path.replace('.csv', '_temp.csv')
    
    try:
        # Read the original file
        with open(file_path, 'r', encoding='latin1') as csv_file:
            content = csv_file.read()
            # Remove triple quotes
            content = content.replace('"""', '')
            reader = csv.reader(content.splitlines())
            
            # Write to temporary file
            with open(temp_path, 'w', encoding='latin1', newline='') as temp_file:
                writer = csv.writer(temp_file)
                for row in reader:
                    writer.writerow(row)
        
        # Replace original with fixed version
        os.remove(file_path)
        os.rename(temp_path, file_path)
        
    except Exception as e:
        if os.path.exists(temp_path):
            os.remove(temp_path)
        raise e

def main():
    while True:
        print("\nWHICH FILE NEEDS QUOTE REMOVAL?:")
        file_path = input().strip('"')
        
        if os.path.exists(file_path):
            try:
                # Create backup first
                backup_path = create_backup(file_path)
                print(f"\nBackup created: {os.path.basename(backup_path)}")
                
                # Process the file
                remove_quotes(file_path)
                print("\nQUOTE REMOVAL COMPLETE!")
                
                print("\nDO YOU WANT TO PROCESS ANOTHER FILE? Y/N")
                if input().upper() != 'Y':
                    print("\nALL FILES PROCESSED! Press any key to exit...")
                    input()
                    sys.exit()
                    
            except Exception as e:
                print(f"\nERROR PROCESSING FILE: {str(e)}")
                print("Press any key to continue...")
                input()
                
        else:
            print(f"\nFile not found: {file_path}")

if __name__ == "__main__":
    main()
