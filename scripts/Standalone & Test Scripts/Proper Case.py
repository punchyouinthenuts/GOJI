import csv
import os
import shutil
import time
import re
from pathlib import Path

def to_proper_case(text):
    """Convert text to proper case with specific exceptions for certain words."""
    if not text or str(text).strip() == '' or str(text).lower() == 'nan':
        return text
    
    # Words that should remain lowercase unless they're the first word
    lowercase_exceptions = {'a', 'an', 'and', 'as', 'at', 'but', 'by', 'for', 'in', 'nor', 'of', 'on', 'or', 'the', 'up'}
    
    # Split the text into words
    words = str(text).split()
    if not words:
        return text
    
    result_words = []
    for i, word in enumerate(words):
        # Clean word of punctuation for comparison but keep original for processing
        clean_word = word.lower().strip('.,!?;:"()[]{}')
        
        if i == 0:
            # First word is always capitalized
            result_words.append(word.capitalize())
        elif clean_word in lowercase_exceptions:
            # Exception words stay lowercase (unless first word)
            result_words.append(word.lower())
        else:
            # All other words get proper case
            result_words.append(word.capitalize())
    
    return ' '.join(result_words)

def get_input_file():
    """Get input file path from user."""
    while True:
        file_path = input("Enter the input file path (quotes around path are acceptable): ").strip()
        
        # Remove quotes if present
        file_path = file_path.strip('"\'')
        
        file_path = Path(file_path)
        
        if file_path.exists() and file_path.suffix.lower() == '.csv':
            return file_path
        else:
            print(f"Error: File not found or not a CSV file: {file_path}")
            print("Please enter a valid CSV file path.")

def get_csv_headers(file_path):
    """Read and return CSV headers."""
    try:
        with file_path.open('r', newline='', encoding='utf-8-sig') as f:
            reader = csv.reader(f)
            headers = next(reader)
            return [h.strip() for h in headers]
    except Exception as e:
        raise ValueError(f"Failed to read CSV headers: {e}")

def display_headers(headers):
    """Display numbered list of headers."""
    print("\nColumn Headers:")
    for i, header in enumerate(headers, 1):
        print(f"{i:2d}. {header}")

def get_column_selection(headers):
    """Get user selection of columns to convert."""
    while True:
        try:
            selection = input("\nWHICH FIELDS NEED TO BE CONVERTED TO PROPER CASE?\nEnter column numbers separated by commas (e.g., 1,2,3 or 1, 2, 3): ").strip()
            
            if not selection:
                print("Please enter at least one column number.")
                continue
            
            # Parse the selection
            selected_numbers = []
            for num_str in selection.split(','):
                num_str = num_str.strip()
                if not num_str:
                    continue
                try:
                    num = int(num_str)
                    if 1 <= num <= len(headers):
                        selected_numbers.append(num)
                    else:
                        raise ValueError(f"Column number {num} is out of range (1-{len(headers)})")
                except ValueError as e:
                    print(f"Invalid input: {e}")
                    selected_numbers = []
                    break
            
            if not selected_numbers:
                continue
            
            # Remove duplicates and sort
            selected_numbers = sorted(list(set(selected_numbers)))
            
            # Show selected columns
            print("\nSelected columns:")
            for num in selected_numbers:
                print(f"  {num}. {headers[num-1]}")
            
            return selected_numbers
            
        except KeyboardInterrupt:
            print("\nOperation cancelled by user.")
            return None

def confirm_selection():
    """Ask user to confirm their selection."""
    while True:
        choice = input("\nConfirm selection? (Y/N): ").strip().upper()
        if choice in ['Y', 'YES']:
            return True
        elif choice in ['N', 'NO']:
            return False
        else:
            print("Please enter Y or N")

def backup_file(file_path):
    """Create a backup of the original file."""
    backup_path = file_path.with_suffix(f'{file_path.suffix}.backup')
    counter = 1
    while backup_path.exists():
        backup_path = file_path.with_suffix(f'{file_path.suffix}.backup{counter}')
        counter += 1
    
    shutil.copy2(file_path, backup_path)
    return backup_path

def restore_backup(original_path, backup_path):
    """Restore file from backup."""
    if backup_path.exists():
        shutil.copy2(backup_path, original_path)
        backup_path.unlink()  # Delete backup after restore

def process_csv(file_path, headers, selected_columns):
    """Process the CSV file to convert selected columns to proper case."""
    # Convert column numbers to indices
    column_indices = [num - 1 for num in selected_columns]
    column_names = [headers[i] for i in column_indices]
    
    # Create backup
    backup_path = backup_file(file_path)
    
    try:
        temp_file = file_path.with_suffix('.temp')
        rows_processed = 0
        conversions_made = 0
        
        with file_path.open('r', newline='', encoding='utf-8-sig') as f_in, \
             temp_file.open('w', newline='', encoding='utf-8') as f_out:
            
            reader = csv.reader(f_in)
            writer = csv.writer(f_out)
            
            # Write headers
            headers_row = next(reader)
            writer.writerow(headers_row)
            
            # Process data rows
            for row in reader:
                # Ensure row has enough columns
                while len(row) < len(headers):
                    row.append('')
                
                # Convert selected columns to proper case
                for col_idx in column_indices:
                    if col_idx < len(row):
                        original_value = row[col_idx]
                        proper_case_value = to_proper_case(original_value)
                        if original_value != proper_case_value:
                            conversions_made += 1
                        row[col_idx] = proper_case_value
                
                writer.writerow(row)
                rows_processed += 1
        
        # Replace original file with processed file
        file_path.unlink()
        temp_file.rename(file_path)
        
        # Clean up backup
        backup_path.unlink()
        
        return rows_processed, conversions_made, column_names
        
    except Exception as e:
        # Restore from backup on error
        if temp_file.exists():
            temp_file.unlink()
        restore_backup(file_path, backup_path)
        raise e

def main():
    try:
        print("=== PROPER CASE CONVERTER ===")
        
        # Get input file
        file_path = get_input_file()
        print(f"Selected file: {file_path}")
        
        # Get headers
        headers = get_csv_headers(file_path)
        
        while True:
            # Display headers
            display_headers(headers)
            
            # Get column selection
            selected_columns = get_column_selection(headers)
            if selected_columns is None:
                return
            
            # Confirm selection
            if confirm_selection():
                break
            else:
                print("Let's try again...")
                continue
        
        # Process the file
        print("\nProcessing file...")
        rows_processed, conversions_made, column_names = process_csv(file_path, headers, selected_columns)
        
        # Success message
        print(f"\n✅ SUCCESS!")
        print(f"Processed {rows_processed} rows")
        print(f"Made {conversions_made} conversions to proper case")
        print(f"Converted columns: {', '.join(column_names)}")
        print(f"Updated file: {file_path}")
        
        print("\nProgram will terminate in 3 seconds...")
        time.sleep(3)
        
    except Exception as e:
        print(f"\n❌ ERROR: {str(e)}")
        print("\nExplanation: The program encountered an error while processing the CSV file.")
        print("All changes have been rolled back to preserve your original data.")
        print("\nPRESS X TO TERMINATE")
        
        while True:
            user_input = input().strip().upper()
            if user_input == 'X':
                break

if __name__ == "__main__":
    main()
