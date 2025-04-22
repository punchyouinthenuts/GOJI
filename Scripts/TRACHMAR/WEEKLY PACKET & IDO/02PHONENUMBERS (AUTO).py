import pandas as pd
import os
from pathlib import Path

def format_phone(number):
    try:
        num_str = ''.join(filter(str.isdigit, str(number).split('.')[0]))  # Remove decimal part
        if len(num_str) != 10:
            return None
        return f"{num_str[:3]}-{num_str[3:6]}-{num_str[6:]}"
    except:
        return None

def is_valid_phone_column(df, column):
    # Only consider non-null values
    non_null_phones = df[column].dropna()
    
    # Count digits before decimal point
    def count_digits(x):
        return sum(c.isdigit() for c in str(x).split('.')[0]) == 10
    
    valid_numbers = non_null_phones.apply(count_digits)
    valid_count = valid_numbers.sum()
    total_non_null = len(non_null_phones)
    
    validation_ratio = valid_count / total_non_null if total_non_null > 0 else 0
    
    print(f"\nTotal non-null phones: {total_non_null}")
    print(f"Valid numbers: {valid_count}")
    print(f"Validation ratio: {validation_ratio:.2f}")
    
    return validation_ratio >= 0.8

def process_files():
    target_dir = r"C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\DATA PROCESSING\RAW FILES"
    file_patterns = ['*.csv', '*.xls', '*.xlsx']
    all_files = []
    
    for pattern in file_patterns:
        all_files.extend(Path(target_dir).glob(pattern))
    
    if not all_files:
        print("No files found in the specified directory!")
        return False
    
    success = True
    for file_path in all_files:
        try:
            if file_path.suffix.lower() == '.csv':
                df = pd.read_csv(file_path)
            else:
                df = pd.read_excel(file_path)
                
            if 'office_phone' not in df.columns:
                print(f"Error: Column 'office_phone' not found in {file_path.name}")
                success = False
                continue
                
            if not is_valid_phone_column(df, 'office_phone'):
                print(f"Error: Invalid phone number data in {file_path.name}")
                success = False
                continue
                
            df['office_phone'] = df['office_phone'].apply(format_phone)
            
            if file_path.suffix.lower() == '.csv':
                df.to_csv(file_path, index=False)
            else:
                df.to_excel(file_path, index=False)
                
            print(f"Successfully processed: {file_path.name}")
            
        except Exception as e:
            print(f"Error processing {file_path.name}: {str(e)}")
            success = False
            
    return success

def main():
    if process_files():
        print("\nALL FILES SUCCESSFULLY PROCESSED! PRESS X TO TERMINATE...")
    else:
        print("\nProcessing completed with errors. PRESS X TO TERMINATE...")
        
    while input().upper() != 'X':
        print("PRESS X TO TERMINATE...")

if __name__ == "__main__":
    main()
