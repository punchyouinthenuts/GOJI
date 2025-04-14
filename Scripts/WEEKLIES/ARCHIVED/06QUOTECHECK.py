import os
import csv
import shutil
from datetime import datetime
from pathlib import Path
import sys

# Base directories
BASE_DIR = r"C:\Users\JCox\Desktop\AUTOMATION\RAC"
BACKUP_DIR = rf"{BASE_DIR}\WEEKLY\CSVs WITH QUOTES BACKUP"

# Scan directories
SCAN_DIRS = [
    rf"{BASE_DIR}\CBC",
    rf"{BASE_DIR}\EXC",
    rf"{BASE_DIR}\INACTIVE_2310-DM07",
    rf"{BASE_DIR}\NCWO_4TH",
    rf"{BASE_DIR}\PREPIF"
]

def get_file_type(file_path):
    if "CBC" in file_path:
        return "CBC"
    elif "EXC" in file_path:
        return "EXC"
    elif "INACTIVE" in file_path:
        return "INACTIVE"
    elif "NCWO" in file_path:
        return "NCWO"
    elif "PREPIF" in file_path:
        return "PREPIF"
    return "UNKNOWN"

def get_last_two_dirs(file_path):
    parts = Path(file_path).parts
    if len(parts) >= 2:
        return os.path.join(parts[-2], parts[-1])
    return parts[-1] if parts else ""

def count_quoted_rows(csv_file):
    encodings = ['utf-8', 'latin-1', 'cp1252']
    for encoding in encodings:
        try:
            with open(csv_file, 'r', encoding=encoding) as f:
                rows_with_quotes = set()
                for row_num, line in enumerate(f, 1):
                    if '""' in line:
                        rows_with_quotes.add(row_num)
                return len(rows_with_quotes)
        except UnicodeDecodeError:
            continue
        except Exception as e:
            print(f"Error reading file {csv_file}: {str(e)}")
            return 0
    return 0

def has_quotes(csv_file):
    encodings = ['utf-8', 'latin-1', 'cp1252']
    for encoding in encodings:
        try:
            with open(csv_file, 'r', encoding=encoding) as f:
                content = f.read()
                return '""' in content
        except UnicodeDecodeError:
            continue
        except Exception as e:
            print(f"Error reading file {csv_file}: {str(e)}")
            return False
    return False

def scan_directories():
    bad_files = []
    for dir_path in SCAN_DIRS:
        for root, dirs, files in os.walk(dir_path):
            # Skip ARCHIVE and ART folders
            dirs[:] = [d for d in dirs if d not in ['ARCHIVE', 'ART']]
            
            for file in files:
                if file.lower().endswith('.csv'):
                    full_path = os.path.join(root, file)
                    if has_quotes(full_path):
                        bad_files.append(full_path)
    return bad_files

def create_backup(file_path):
    timestamp = datetime.now().strftime("_%Y%m%d-%H%M")
    rel_path = os.path.relpath(file_path, BASE_DIR)
    backup_path = os.path.join(BACKUP_DIR, rel_path)
    backup_dir = os.path.dirname(backup_path)
    
    # Create backup filename with timestamp
    filename, ext = os.path.splitext(backup_path)
    backup_path = f"{filename}{timestamp}{ext}"
    
    # Create directory structure if it doesn't exist
    os.makedirs(backup_dir, exist_ok=True)
    
    # Copy file to backup location
    shutil.copy2(file_path, backup_path)
    return backup_path

def remove_quotes(file_path):
    temp_file = file_path + '.tmp'
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # Remove double quotes
        content = content.replace('""', '')
        
        with open(temp_file, 'w', encoding='utf-8') as f:
            f.write(content)
        
        # Replace original file
        os.replace(temp_file, file_path)
    except Exception as e:
        if os.path.exists(temp_file):
            os.remove(temp_file)
        raise e

def main():
    bad_files = scan_directories()
    
    if not bad_files:
        print("\nNO BAD FILES DETECTED! PRESS ANY KEY TO EXIT")
        input()
        sys.exit()

    print("\nTHE FOLLOWING FILES HAVE BEEN IDENTIFIED AS NEEDING CORRECTION")
    for file in bad_files:
        file_type = get_file_type(file)
        dir_path = get_last_two_dirs(os.path.dirname(file))
        file_name = os.path.basename(file)
        entries = count_quoted_rows(file)
        print(f"[{file_type}]  {dir_path}  {file_name} ({entries} ENTRIES)")

    while True:
        print("\nARE YOU READY TO PROCESS THESE FILES? Y/N")
        choice = input().upper()
        
        if choice == 'N':
            print("PRESS X TO EXIT OR Z TO GO BACK")
            sub_choice = input().upper()
            if sub_choice == 'X':
                sys.exit()
            elif sub_choice == 'Z':
                continue
                
        elif choice == 'Y':
            try:
                # Backup files
                for file in bad_files:
                    backup_path = create_backup(file)
                
                # Remove quotes from original files
                for file in bad_files:
                    remove_quotes(file)
                
                print("ALL BAD FILES PROCESSED, PRESS ANY KEY TO EXIT")
                input()
                sys.exit()
                
            except Exception as e:
                print(f"An error occurred: {str(e)}")
                print("PRESS X TO TERMINATE")
                if input().upper() == 'X':
                    sys.exit()

if __name__ == "__main__":
    main()
