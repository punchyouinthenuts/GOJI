import pandas as pd
import os

def validate_file(file_path):
    """Validate if the file exists and is an XLS, XLSX, or CSV."""
    file_path = file_path.strip('"')  # Remove quotes from Windows "Copy as path"
    if not os.path.exists(file_path):
        return False, "File does not exist."
    ext = os.path.splitext(file_path)[1].lower()
    if ext not in ['.xls', '.xlsx', '.csv']:
        return False, "File must be XLS, XLSX, or CSV."
    return True, file_path

def read_spreadsheet(file_path):
    """Read the first worksheet and return DataFrame and record count."""
    ext = os.path.splitext(file_path)[1].lower()
    try:
        if ext in ['.xls', '.xlsx']:
            df = pd.read_excel(file_path, sheet_name=0)
        else:  # .csv
            df = pd.read_csv(file_path)
        record_count = len(df)  # Excludes header
        return df, record_count
    except Exception as e:
        return None, f"Error reading file: {str(e)}"

def split_dataframe(df, split_option):
    """Split DataFrame into 2, 3, or 4 parts based on user choice."""
    total_records = len(df)
    if split_option == '1':  # ½
        parts = 2
    elif split_option == '2':  # ⅓
        parts = 3
    else:  # ¼
        parts = 4
    
    # Calculate records per part
    base_size = total_records // parts
    remainder = total_records % parts
    sizes = [base_size + 1 if i < remainder else base_size for i in range(parts)]
    
    # Split DataFrame
    splits = []
    start = 0
    for size in sizes:
        splits.append(df.iloc[start:start + size])
        start += size
    return splits, sizes

def save_splits(splits, sizes, header, base_name, output_dir):
    """Save each split as a CSV and return file names with record counts."""
    file_info = []
    for i, (split_df, size) in enumerate(zip(splits, sizes), 1):
        output_file = os.path.join(output_dir, f"{base_name} {i:02d}.csv")
        split_df.to_csv(output_file, index=False)
        file_info.append((output_file, size))
    return file_info

def main():
    while True:
        # Prompt for file
        file_path = input("ENTER LIST TO SCAN: ")
        is_valid, result = validate_file(file_path)
        if not is_valid:
            print(result)
            continue
        
        # Read spreadsheet
        df, record_count = read_spreadsheet(result)
        if isinstance(record_count, str):
            print(record_count)
            continue
        
        # Display record count
        print(f"TOTAL RECORD COUNT: {record_count}")
        
        # Prompt for split option
        print("HOW DO YOU WANT TO SPLIT THE LIST?")
        print("1) ½")
        print("2) ⅓")
        print("3) ¼")
        split_option = input("ENTER NUMBER: ")
        if split_option not in ['1', '2', '3']:
            print("Invalid option. Please enter 1, 2, or 3.")
            continue
        
        # Split DataFrame
        splits, sizes = split_dataframe(df, split_option)
        
        # Prompt for base file name
        base_name = input("ENTER FILE NAME: ")
        
        # Save splits as CSVs
        output_dir = os.path.dirname(result)
        file_info = save_splits(splits, sizes, df.columns, base_name, output_dir)
        
        # Display file names and record counts
        for file_path, count in file_info:
            print(f"{os.path.basename(file_path)}: {count} records")
        
        # Prompt to process another list
        again = input("DO YOU WANT TO PROCESS ANOTHER LIST? Y/N: ").strip().upper()
        if again != 'Y':
            break

if __name__ == "__main__":
    main()