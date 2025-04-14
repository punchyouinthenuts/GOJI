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

def has_displayed_quotes(line):
    if '"' not in line:
        return False
        
    parts = []
    if '\t' in line:
        parts = line.split('\t')
    else:
        parts = line.split(',')
    
    for part in parts:
        part = part.strip()
        if '"' in part:
            if not (part.startswith('"') and part.endswith('"')):
                if '""' not in part:
                    return True
    return False

def verify_file_needs_correction(file_path):
    try:
        with open(file_path, 'r', encoding='latin-1') as f:
            header = next(f, None)  # Skip header
            for _ in range(5):
                line = next(f, None)
                if line and has_displayed_quotes(line):
                    return True
    except:
        return False

def get_quoted_data(file_path):
    encodings = ['utf-8', 'latin-1', 'cp1252']
    for encoding in encodings:
        try:
            with open(file_path, 'r', encoding=encoding) as f:
                quoted_rows = {}
                for row_num, line in enumerate(f, 1):
                    if has_displayed_quotes(line):
                        quoted_rows[row_num] = line.strip()
                return quoted_rows
        except UnicodeDecodeError:
            continue
    return {}

def count_quoted_rows(csv_file):
    quoted_data = get_quoted_data(csv_file)
    return len(quoted_data)

def has_quotes(csv_file):
    encodings = ['utf-8', 'latin-1', 'cp1252']
    for encoding in encodings:
        try:
            with open(csv_file, 'r', encoding=encoding) as f:
                for line in f:
                    if has_displayed_quotes(line):
                        return True
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
    filename, ext = os.path.splitext(backup_path)
    backup_path = f"{filename}{timestamp}{ext}"
    os.makedirs(backup_dir, exist_ok=True)
    shutil.copy2(file_path, backup_path)
    return backup_path

def display_file_entries(file_path, num_entries):
    quoted_rows = get_quoted_data(file_path)
    sorted_rows = sorted(quoted_rows.keys())
    
    if num_entries == 'ALL':
        display_rows = sorted_rows
    else:
        display_rows = sorted_rows[:int(num_entries)]
    
    for row_num in display_rows:
        print(f"[{row_num}] {quoted_rows[row_num]}")

def display_bad_files(bad_files, numbered=False):
    for idx, file in enumerate(bad_files, 1):
        file_type = get_file_type(file)
        dir_path = get_last_two_dirs(os.path.dirname(file))
        file_name = os.path.basename(file)
        entries = count_quoted_rows(file)
        if numbered:
            print(f"{idx}) [{file_type}]  {dir_path}  {file_name} ({entries} ENTRIES)")
        else:
            print(f"[{file_type}]  {dir_path}  {file_name} ({entries} ENTRIES)")

def examine_files(bad_files):
    while True:
        print("\n")
        display_bad_files(bad_files, numbered=True)
        print("\nWHICH FILE WOULD YOU LIKE TO EXAMINE? ENTER LIST NUMBER OR Z TO GO BACK")
        choice = input().upper()
        
        if choice == 'Z':
            return
        
        try:
            file_idx = int(choice)
            if 1 <= file_idx <= len(bad_files):
                file_path = bad_files[file_idx - 1]
                total_entries = count_quoted_rows(file_path)
                
                print("\nHOW MANY ENTRIES WOULD YOU LIKE DISPLAYED?")
                entry_choice = input().upper()
                
                if entry_choice == 'NONE':
                    continue
                elif entry_choice == 'ALL' or (entry_choice.isdigit() and 1 <= int(entry_choice) <= total_entries):
                    display_file_entries(file_path, entry_choice)
                    print("\nARE YOU READY TO PROCESS THE BAD FILES? Y/N")
                    return input().upper()
        except ValueError:
            continue

def process_files(bad_files):
    # First create backups of all files
    for file in bad_files:
        backup_path = create_backup(file)
        
    for file in bad_files:
        # Create temporary txt filename
        txt_path = file.replace('.csv', '.txt')
        
        try:
            # Convert to tab-delimited txt
            with open(file, 'r', encoding='latin-1') as csv_file:
                content = csv_file.read()
                # Convert CSV to tab-delimited
                reader = csv.reader(content.splitlines())
                tab_content = '\n'.join('\t'.join(row) for row in reader)
            
            # Save as txt file
            with open(txt_path, 'w', encoding='latin-1') as txt_file:
                txt_file.write(tab_content)
            
            # Clean quotes from txt file
            with open(txt_path, 'r', encoding='latin-1') as txt_file:
                content = txt_file.read()
                # Enhanced quote removal
                content = content.replace('"', '')
                content = content.replace(' "', ' ')
                content = content.replace('."', '.')
                content = content.replace(' 23"', ' 23')
                content = content.replace(' 215"', ' 215')
                
                # Write cleaned content back to txt file
                with open(txt_path, 'w', encoding='latin-1') as txt_file:
                    txt_file.write(content)
            
            # Convert back to CSV and overwrite original
            with open(txt_path, 'r', encoding='latin-1') as txt_file:
                reader = csv.reader(txt_file, delimiter='\t')
                with open(file, 'w', encoding='latin-1', newline='') as csv_file:
                    writer = csv.writer(csv_file)
                    writer.writerows(reader)
                    
            # Remove temporary txt file
            os.remove(txt_path)
            
        except Exception as e:
            print(f"Error processing {file}: {str(e)}")
            if os.path.exists(txt_path):
                os.remove(txt_path)
            raise e

def main():
    bad_files = scan_directories()
    
    if not bad_files:
        print("\nNO BAD FILES DETECTED! PRESS ANY KEY TO EXIT")
        input()
        sys.exit()

    while True:
        print("\nTHE FOLLOWING FILES HAVE BEEN IDENTIFIED AS NEEDING CORRECTION")
        display_bad_files(bad_files)
        print("\nARE YOU READY TO PROCESS THESE FILES? Y/N")
        choice = input().upper()
        
        if choice == 'N':
            while True:
                print("PRESS X TO EXIT, Z TO GO BACK, OR E TO EXAMINE")
                sub_choice = input().upper()
                if sub_choice == 'X':
                    sys.exit()
                elif sub_choice == 'Z':
                    break
                elif sub_choice == 'E':
                    process_choice = examine_files(bad_files)
                    if process_choice == 'Y':
                        choice = 'Y'
                        break
                    elif process_choice == 'N':
                        continue
                
        if choice == 'Y':
            try:
                process_files(bad_files)
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
