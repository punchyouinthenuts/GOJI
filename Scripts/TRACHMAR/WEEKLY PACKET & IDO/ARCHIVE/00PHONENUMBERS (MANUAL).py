import pandas as pd
import os

def format_phone(number):
    try:
        num_str = ''.join(filter(str.isdigit, str(number)))
        if len(num_str) != 10:
            return None
        return f"{num_str[:3]}-{num_str[3:6]}-{num_str[6:]}"
    except:
        return None

def is_valid_phone_column(df, column):
    valid_count = 0
    total_count = len(df[column].dropna())
    if total_count == 0:
        return False
    
    for value in df[column].dropna():
        num_str = ''.join(filter(str.isdigit, str(value)))
        if len(num_str) == 10:
            valid_count += 1
    
    return (valid_count / total_count) >= 0.8

def process_file():
    while True:
        print("\nWHICH FILE DO YOU NEED TO PROCESS?")
        file_path = input().strip().strip('"')
        
        if not os.path.exists(file_path):
            print("File not found!")
            continue
            
        try:
            if file_path.lower().endswith('.csv'):
                df = pd.read_csv(file_path)
            elif file_path.lower().endswith('.xlsx'):
                df = pd.read_excel(file_path)
            else:
                print("File must be CSV or XLSX!")
                continue
            break
        except Exception as e:
            print(f"Error reading file: {e}")
            continue

    while True:
        print("\nAvailable columns:")
        for idx, column in enumerate(df.columns, 1):
            print(f"{idx}. {column}")
            
        print("\nWHICH COLUMN NEEDS PROCESSING?")
        try:
            column_choice = int(input())
            if column_choice < 1 or column_choice > len(df.columns):
                print("Invalid column number!")
                continue
                
            selected_column = df.columns[column_choice - 1]
            
            print(f"\nProcess column '{selected_column}'? (Y/N)")
            if input().upper() != 'Y':
                continue
                
            if not is_valid_phone_column(df, selected_column):
                print("\nError: Selected column does not appear to contain valid phone numbers!")
                print("PRESS X TO ACKNOWLEDGE")
                while input().upper() != 'X':
                    print("PRESS X TO ACKNOWLEDGE")
                continue
                
            df[selected_column] = df[selected_column].apply(format_phone)
            
            if file_path.lower().endswith('.csv'):
                df.to_csv(file_path, index=False)
            else:
                df.to_excel(file_path, index=False)
                
            print("\nPHONE NUMBERS SUCCESSFULLY PROCESSED!")
            return True
                
        except ValueError:
            print("Invalid input! Please enter a number.")
            continue

def main():
    while True:
        if process_file():
            print("\nARE THERE ANY OTHER FILES THAT NEED PROCESSING? Y/N")
            if input().upper() != 'Y':
                print("\nPRESS X TO TERMINATE...")
                while input().upper() != 'X':
                    print("PRESS X TO TERMINATE...")
                break

if __name__ == "__main__":
    main()
