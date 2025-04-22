import pandas as pd
import os
import sys

def clean_file_path(file_path):
    return file_path.strip().strip('"\'')

def read_file(file_path):
    file_ext = os.path.splitext(file_path)[1].lower()
    
    if file_ext in ['.txt', '.csv']:
        # Try different encodings in order
        encodings = ['utf-8', 'latin1', 'iso-8859-1', 'cp1252']
        for encoding in encodings:
            try:
                return pd.read_csv(file_path, encoding=encoding)
            except UnicodeDecodeError:
                continue
            except Exception as e:
                print(f"Error reading file: {str(e)}")
                return None
        print("Could not read file with any of the attempted encodings")
        return None
    elif file_ext in ['.xls', '.xlsx']:
        return pd.read_excel(file_path)
    else:
        print("Unsupported file format")
        return None

def save_file(df, file_path):
    file_ext = os.path.splitext(file_path)[1].lower()
    
    if file_ext in ['.txt', '.csv']:
        df.to_csv(file_path, index=False)
    elif file_ext in ['.xls', '.xlsx']:
        df.to_excel(file_path, index=False)
    print(f"\nChanges saved to {file_path}")

def display_headers(df):
    print("\nCurrent Headers:")
    for idx, header in enumerate(df.columns, 1):
        print(f"{idx}: {header}")

def get_valid_numbers(max_num):
    while True:
        user_input = input("\nWHICH HEADERS WOULD YOU LIKE TO CHANGE? ").strip()
        numbers = [num.strip() for num in user_input.split(',')]
        
        try:
            numbers = [int(num) for num in numbers]
            if all(1 <= num <= max_num for num in numbers):
                return numbers
            else:
                print("Numbers must be within the range of available headers")
        except ValueError:
            print("Please enter valid numbers separated by commas")

def main():
    while True:
        file_path = input("Enter the path to your file: ")
        file_path = clean_file_path(file_path)
        
        if not os.path.exists(file_path):
            print("File not found")
            continue
            
        df = read_file(file_path)
        if df is None:
            continue
            
        while True:
            display_headers(df)
            numbers = get_valid_numbers(len(df.columns))
            
            # Change headers
            old_headers = df.columns.tolist()
            for num in numbers:
                print(f"\n{num}.")
                new_name = input().strip()
                old_headers[num-1] = new_name
            
            df.columns = old_headers
            display_headers(df)
            save_file(df, file_path)  # Save changes after each modification
            
            while True:
                change_more = input("\nDO YOU WANT TO CHANGE ANY HEADERS? Y/N ").upper()
                if change_more in ['Y', 'N']:
                    break
                print("Please enter Y or N")
                
            if change_more == 'N':
                break
        
        while True:
            process_more = input("\nARE THERE ANY OTHER FILES YOU WANT TO PROCESS? Y/N ").upper()
            if process_more in ['Y', 'N']:
                break
            print("Please enter Y or N")
            
        if process_more == 'N':
            break
    
    while True:
        exit_input = input("\nPress X to terminate...").upper()
        if exit_input == 'X':
            sys.exit()

if __name__ == "__main__":
    main()
