import os
import csv
import shutil
import sys
from datetime import datetime
from pathlib import Path
import argparse

class Config:
    """Configuration settings for the application."""
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
    def create_backup(file_path, base_dir, backup_dir):
        """Create a timestamped backup of the file."""
        timestamp = datetime.now().strftime("_%Y%m%d-%H%M")
        rel_path = os.path.relpath(file_path, base_dir)
        backup_path = os.path.join(backup_dir, rel_path)
        backup_dir_path = os.path.dirname(backup_path)
        filename, ext = os.path.splitext(os.path.basename(file_path))
        backup_path = os.path.join(backup_dir_path, f"{filename}{timestamp}{ext}")
        os.makedirs(backup_dir_path, exist_ok=True)
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
                print(f"Error reading file {csv_file}: {str(e)}", file=sys.stderr)
                return False
        return False

class FileFinder:
    """Methods for finding files that need processing."""
    
    @staticmethod
    def scan_directories(scan_dirs, exclude_dirs):
        """Scan specified directories for CSV files with quote issues."""
        bad_files = []
        for dir_path in scan_dirs:
            for root, dirs, files in os.walk(dir_path):
                dirs[:] = [d for d in dirs if d not in exclude_dirs]
                for file in files:
                    if file.lower().endswith('.csv'):
                        full_path = os.path.join(root, file)
                        if QuoteDetector.has_quotes(full_path):
                            bad_files.append(full_path)
        return bad_files

class FileProcessor:
    """Methods for processing and correcting CSV files."""
    
    @staticmethod
    def process_files(bad_files, base_dir, backup_dir):
        """Process all files in the bad_files list."""
        for file in bad_files:
            try:
                # Create backup
                backup_path = FileUtils.create_backup(file, base_dir, backup_dir)
                print(f"Created backup: {backup_path}")
                
                txt_path = file.replace('.csv', '.txt')
                
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
                print(f"Processed: {file}")
                
            except Exception as e:
                print(f"Error processing {file}: {str(e)}", file=sys.stderr)
                if os.path.exists(txt_path):
                    os.remove(txt_path)
                raise e

def main():
    """Main application flow."""
    # Parse command-line arguments
    parser = argparse.ArgumentParser(description="Correct CSV files with quote issues")
    parser.add_argument("--base_path", required=True, help="Base directory path (e.g., C:\\Goji\\RAC)")
    args = parser.parse_args()

    # Set dynamic directories
    base_dir = args.base_path
    backup_dir = os.path.join(base_dir, 'Backup')
    job_types = ['CBC', 'EXC', 'INACTIVE', 'NCWO', 'PREPIF']
    scan_dirs = [os.path.join(base_dir, job_type, 'JOB', subdir) for job_type in job_types for subdir in ['OUTPUT', 'PROOF']]

    # Find files with quote issues
    bad_files = FileFinder.scan_directories(scan_dirs, Config.EXCLUDE_DIRS)

    if not bad_files:
        print("No files with quote issues detected.")
        sys.exit(0)

    print(f"Found {len(bad_files)} files with quote issues.")

    # Process all bad files
    try:
        FileProcessor.process_files(bad_files, base_dir, backup_dir)
        print("All files processed successfully.")
    except Exception as e:
        print(f"Error during processing: {str(e)}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()