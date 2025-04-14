import os
import csv
import shutil
import sys
from datetime import datetime
from pathlib import Path

class Config:
    """Configuration settings for the application."""
    BASE_DIR = r"C:\Users\JCox\Desktop\AUTOMATION\RAC"
    BACKUP_DIR = rf"{BASE_DIR}\WEEKLY\CSVs WITH QUOTES BACKUP"
    SCAN_DIRS = [
        rf"{BASE_DIR}\CBC\JOB",
        rf"{BASE_DIR}\EXC\JOB",
        rf"{BASE_DIR}\INACTIVE_2310-DM07\FOLDERS",
        rf"{BASE_DIR}\NCWO_4TH\DM03",
        rf"{BASE_DIR}\PREPIF\FOLDERS"
    ]
    EXCLUDE_DIRS = ['ARCHIVE', 'ART']
    ENCODINGS = ['utf-8', 'latin-1', 'cp1252']


class FileUtils:
    """Utility functions for file operations."""
    
    @staticmethod
    def get_file_type(file_path):
        """Determine file type based on path."""
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
    
    @staticmethod
    def get_last_two_dirs(file_path):
        """Get the last two directory names from path."""
        parts = Path(file_path).parts
        if len(parts) >= 2:
            return os.path.join(parts[-2], parts[-1])
        return parts[-1] if parts else ""
    
    @staticmethod
    def create_backup(file_path):
        """Create a timestamped backup of the file."""
        timestamp = datetime.now().strftime("_%Y%m%d-%H%M")
        rel_path = os.path.relpath(file_path, Config.BASE_DIR)
        backup_path = os.path.join(Config.BACKUP_DIR, rel_path)
        backup_dir = os.path.dirname(backup_path)
        filename, ext = os.path.splitext(backup_path)
        backup_path = f"{filename}{timestamp}{ext}"
        os.makedirs(backup_dir, exist_ok=True)
        shutil.copy2(file_path, backup_path)
        return backup_path


class QuoteDetector:
    """Methods for detecting and handling problematic quotes in CSV files."""
    
    @staticmethod
    def has_displayed_quotes(line):
        """Check if a line has improperly displayed quotes."""
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
    
    @staticmethod
    def verify_file_needs_correction(file_path):
        """Check first 5 lines to see if file needs correction."""
        for encoding in Config.ENCODINGS:
            try:
                with open(file_path, 'r', encoding=encoding) as f:
                    header = next(f, None)
                    for _ in range(5):
                        line = next(f, None)
                        if line and QuoteDetector.has_displayed_quotes(line):
                            return True
            except UnicodeDecodeError:
                continue
            except Exception:
                break
        return False
    
    @staticmethod
    def get_quoted_data(file_path):
        """Extract lines containing problematic quotes."""
        for encoding in Config.ENCODINGS:
            try:
                with open(file_path, 'r', encoding=encoding) as f:
                    quoted_rows = {}
                    for row_num, line in enumerate(f, 1):
                        if QuoteDetector.has_displayed_quotes(line):
                            quoted_rows[row_num] = line.strip()
                    return quoted_rows
            except UnicodeDecodeError:
                continue
        return {}
    
    @staticmethod
    def count_quoted_rows(csv_file):
        """Count rows with problematic quotes."""
        quoted_data = QuoteDetector.get_quoted_data(csv_file)
        return len(quoted_data)
    
    @staticmethod
    def has_quotes(csv_file):
        """Check if file has any problematic quotes."""
        for encoding in Config.ENCODINGS:
            try:
                with open(csv_file, 'r', encoding=encoding) as f:
                    for line in f:
                        if QuoteDetector.has_displayed_quotes(line):
                            return True
            except UnicodeDecodeError:
                continue
            except Exception as e:
                print(f"Error reading file {csv_file}: {str(e)}")
                return False
        return False


class FileFinder:
    """Methods for finding files that need processing."""
    
    @staticmethod
    def scan_directories():
        """Scan directories for CSV files with quote issues."""
        bad_files = []
        for dir_path in Config.SCAN_DIRS:
            for root, dirs, files in os.walk(dir_path):
                dirs[:] = [d for d in dirs if d not in Config.EXCLUDE_DIRS]
                for file in files:
                    if file.lower().endswith('.csv'):
                        full_path = os.path.join(root, file)
                        if QuoteDetector.has_quotes(full_path):
                            bad_files.append(full_path)
        return bad_files


class FileProcessor:
    """Methods for processing and correcting CSV files."""
    
    @staticmethod
    def process_files(bad_files):
        """Process all files in the bad_files list."""
        # First create backups of all files
        for file in bad_files:
            FileUtils.create_backup(file)
            
        # Then process each file
        for file in bad_files:
            txt_path = file.replace('.csv', '.txt')
            
            try:
                # Convert to tab-delimited format
                with open(file, 'r', encoding='utf-8', errors='replace') as csv_file:
                    content = csv_file.read()
                    reader = csv.reader(content.splitlines())
                    tab_content = '\n'.join('\t'.join(row) for row in reader)
                
                # Write to temporary text file
                with open(txt_path, 'w', encoding='utf-8') as txt_file:
                    txt_file.write(tab_content)
                
                # Remove quotes from text file
                with open(txt_path, 'r', encoding='utf-8') as txt_file:
                    content = txt_file.read()
                    content = content.replace('"', '')
                    content = content.replace(' "', ' ')
                    content = content.replace('."', '.')
                    content = content.replace(' 23"', ' 23')
                    content = content.replace(' 215"', ' 215')
                    
                    with open(txt_path, 'w', encoding='utf-8') as txt_file:
                        txt_file.write(content)
                
                # Convert back to CSV format
                with open(txt_path, 'r', encoding='utf-8') as txt_file:
                    reader = csv.reader(txt_file, delimiter='\t')
                    with open(file, 'w', encoding='utf-8', newline='') as csv_file:
                        writer = csv.writer(csv_file)
                        writer.writerows(reader)
                        
                # Clean up temporary file
                os.remove(txt_path)
                
            except Exception as e:
                print(f"Error processing {file}: {str(e)}")
                if os.path.exists(txt_path):
                    os.remove(txt_path)
                raise e


class UserInterface:
    """Methods for user interaction."""
    
    @staticmethod
    def display_bad_files(bad_files, numbered=False):
        """Display list of bad files."""
        for idx, file in enumerate(bad_files, 1):
            file_type = FileUtils.get_file_type(file)
            dir_path = FileUtils.get_last_two_dirs(os.path.dirname(file))
            file_name = os.path.basename(file)
            entries = QuoteDetector.count_quoted_rows(file)
            if numbered:
                print(f"{idx}) [{file_type}]  {dir_path}  {file_name} ({entries} ENTRIES)")
            else:
                print(f"[{file_type}]  {dir_path}  {file_name} ({entries} ENTRIES)")
    
    @staticmethod
    def display_file_entries(file_path, num_entries):
        """Display specified number of problematic entries from file."""
        quoted_rows = QuoteDetector.get_quoted_data(file_path)
        sorted_rows = sorted(quoted_rows.keys())
        
        if num_entries == 'ALL':
            display_rows = sorted_rows
        else:
            display_rows = sorted_rows[:int(num_entries)]
        
        for row_num in display_rows:
            print(f"[{row_num}] {quoted_rows[row_num]}")
    
    @staticmethod
    def examine_files(bad_files):
        """Interactive examination of problematic files."""
        while True:
            print("\n")
            UserInterface.display_bad_files(bad_files, numbered=True)
            print("\nWHICH FILE WOULD YOU LIKE TO EXAMINE? ENTER LIST NUMBER OR Z TO GO BACK")
            choice = input().upper()
            
            if choice == 'Z':
                return
            
            try:
                file_idx = int(choice)
                if 1 <= file_idx <= len(bad_files):
                    file_path = bad_files[file_idx - 1]
                    total_entries = QuoteDetector.count_quoted_rows(file_path)
                    
                    print("\nHOW MANY ENTRIES WOULD YOU LIKE DISPLAYED?")
                    entry_choice = input().upper()
                    
                    if entry_choice == 'NONE':
                        continue
                    elif entry_choice == 'ALL' or (entry_choice.isdigit() and 1 <= int(entry_choice) <= total_entries):
                        UserInterface.display_file_entries(file_path, entry_choice)
                        print("\nARE YOU READY TO PROCESS THE BAD FILES? Y/N")
                        return input().upper()
            except ValueError:
                continue


def main():
    """Main application flow."""
    # Find files that need processing
    bad_files = FileFinder.scan_directories()
    
    # Exit if no files found
    if not bad_files:
        print("\nNO BAD FILES DETECTED! PRESS ANY KEY TO EXIT")
        input()
        sys.exit()

    # Main interaction loop
    while True:
        print("\nTHE FOLLOWING FILES HAVE BEEN IDENTIFIED AS NEEDING CORRECTION")
        UserInterface.display_bad_files(bad_files)
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
                    process_choice = UserInterface.examine_files(bad_files)
                    if process_choice == 'Y':
                        choice = 'Y'
                        break
                    elif process_choice == 'N':
                        continue
            
        if choice == 'Y':
            try:
                FileProcessor.process_files(bad_files)
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