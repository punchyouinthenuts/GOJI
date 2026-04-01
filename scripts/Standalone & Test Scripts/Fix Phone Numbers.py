import pandas as pd
import re
import os
import shutil

def get_unique_backup_path(original_path):
    """Generate a unique backup file path by appending _backup, _backup02, etc."""
    base, ext = os.path.splitext(original_path)
    backup_path = f"{base}_backup{ext}"
    counter = 2
    while os.path.exists(backup_path):
        backup_path = f"{base}_backup{counter:02d}{ext}"
        counter += 1
    return backup_path

def is_phone_number(value):
    """Check if a value resembles a phone number (10 digits, possibly with (), -, or spaces)."""
    if not isinstance(value, str):
        return False
    # Remove all non-digits and count
    digits = re.sub(r'\D', '', value)
    return len(digits) == 10

def reformat_phone_number(value):
    """Reformat a 10-digit phone number to (XXX) XXX-XXXX."""
    if not isinstance(value, str):
        return value, False
    # Extract digits
    digits = re.sub(r'\D', '', value)
    if len(digits) != 10:
        return value, False
    # Reformat to (XXX) XXX-XXXX
    formatted = f"({digits[:3]}) {digits[3:6]}-{digits[6:]}"
    return formatted, value != formatted

def main():
    # Prompt for file path
    file_path = input("ENTER INPUT FILE LOCATION: ").strip('"')
    
    # Validate file existence
    if not os.path.isfile(file_path) or not file_path.lower().endswith('.csv'):
        print("Error: Invalid or non-existent CSV file.")
        print("PRESS X TO TERMINATE")
        while input().strip().lower() != 'x':
            pass
        return
    
    try:
        # Read CSV
        df = pd.read_csv(file_path)
        
        # Check if file is empty or has no headers
        if df.empty or df.columns.empty:
            print("Error: File is empty or has no headers.")
            print("PRESS X TO TERMINATE")
            while input().strip().lower() != 'x':
                pass
            return
        
        # Create backup
        backup_path = get_unique_backup_path(file_path)
        shutil.copyfile(file_path, backup_path)
        
        # Display column headers
        print("\nColumn Headers:")
        for i, col in enumerate(df.columns, 1):
            print(f"{i}. {col}")
        
        # Prompt for column number
        try:
            col_num = int(input("\nWHICH COLUMN NEEDS TO BE SCANNED? "))
            if col_num < 1 or col_num > len(df.columns):
                raise ValueError
        except ValueError:
            print("Error: Invalid column number.")
            print("PRESS X TO TERMINATE")
            while input().strip().lower() != 'x':
                pass
            return
        
        # Get selected column
        col_name = df.columns[col_num - 1]
        
        # Validate column contains phone numbers
        phone_count = sum(df[col_name].apply(is_phone_number))
        if phone_count == 0:
            print("Error: Selected column contains no valid phone numbers.")
            print("PRESS X TO TERMINATE")
            while input().strip().lower() != 'x':
                pass
            return
        
        # Process phone numbers
        corrected_count = 0
        bad_rows = []
        
        for idx, value in df[col_name].items():
            new_value, was_corrected = reformat_phone_number(value)
            if was_corrected:
                corrected_count += 1
                df.at[idx, col_name] = new_value
            elif not is_phone_number(value) and pd.notna(value):
                bad_rows.append(idx + 2)  # +2 to account for header and 1-based indexing
        
        # Save changes to original file
        df.to_csv(file_path, index=False)
        
        # Display results
        print(f"\n{corrected_count} RECORDS WERE CORRECTED")
        if bad_rows:
            print("THE FOLLOWING ROWS HAVE BAD DATA:")
            for row in bad_rows:
                print(row)
        
        # Prompt to terminate
        print("PRESS X TO TERMINATE")
        while input().strip().lower() != 'x':
            pass
    
    except Exception as e:
        print(f"Error processing file: {e}")
        print("PRESS X TO TERMINATE")
        while input().strip().lower() != 'x':
            pass

if __name__ == "__main__":
    main()