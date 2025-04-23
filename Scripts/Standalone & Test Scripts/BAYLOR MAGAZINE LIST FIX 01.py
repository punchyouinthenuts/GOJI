import pandas as pd
import re
import os

def get_file_path():
    return input("WHICH FILE NEEDS PROCESSING? ").strip('"')

def display_numbered_columns(df):
    print("\nCurrent columns:")
    for idx, col in enumerate(df.columns, 1):
        print(f"{idx}. {col}")

def get_columns_to_rename():
    response = input("\nDO ANY HEADERS NEED TO BE RENAMED? Y/N ").upper()
    if response != 'Y':
        return None
    
    cols = input("\nINPUT COLUMN NUMBERS: ").replace(" ", "").split(",")
    return [int(x) for x in cols]

def rename_columns(df):
    while True:
        display_numbered_columns(df)
        cols_to_rename = get_columns_to_rename()
        
        if not cols_to_rename:
            return df
            
        print("\nINPUT NEW COLUMN HEADER(S)")
        new_names = {}
        for col_num in cols_to_rename:
            old_name = df.columns[col_num - 1]
            new_name = input(f"New name for column {col_num}: ")
            new_names[old_name] = new_name
            
        print("\nIS THIS CORRECT? Y/N")
        for old, new in new_names.items():
            print(f"{old} -> {new}")
            
        if input().upper() == 'Y':
            return df.rename(columns=new_names)

def insert_new_column(df, column_name):
    display_numbered_columns(df)
    position = int(input(f"\nWHAT COLUMN SHOULD {column_name} PRECEDE? ")) - 1
    df.insert(position, column_name, '')
    return df

def get_multiple_columns(prompt):
    display_numbered_columns(df)
    cols = input(f"\n{prompt} ").replace(" ", "").split(",")
    return [int(x) - 1 for x in cols]

def get_single_column(prompt):
    display_numbered_columns(df)
    return int(input(f"\n{prompt} ")) - 1

def process_business_data(df, blank_cols, business_col):
    business_target_col = df.columns.get_loc('Business')
    
    mask = df.iloc[:, blank_cols].isna().all(axis=1)
    business_data = df.iloc[:, business_col]
    
    df.loc[mask, 'Business'] = df.loc[mask, df.columns[business_col]]
    df.loc[mask, df.columns[business_col]] = ''
    
    return df

def process_attn_data(df, attn_cols):
    title_col = df.columns.get_loc('Title')
    pattern = r'(?i)(ATTN:|ATTN|C/O:|C/O)\s*(.*)'
    
    for col in attn_cols:
        mask = df.iloc[:, col].str.contains(pattern, na=False, regex=True)
        matches = df.iloc[:, col][mask].str.extract(pattern)
        
        df.loc[mask, 'Title'] = matches[0] + ' ' + matches[1]
        df.loc[mask, df.columns[col]] = ''
    
    return df

# Main execution
if __name__ == "__main__":
    # Get and validate file path
    file_path = get_file_path()
    df = pd.read_excel(file_path)
    
    # Process header renaming
    df = rename_columns(df)
    
    # Add Title and Business columns
    df = insert_new_column(df, 'Title')
    df = insert_new_column(df, 'Business')
    
    # Get processing parameters
    blank_cols = get_multiple_columns("WHICH COLUMNS SHOULD BE BLANK FOR BUSINESS PROCESSING?")
    business_col = get_single_column("WHICH COLUMNS SHOULD CONTAIN BUSINESS NAMES?")
    attn_cols = get_multiple_columns("WHERE SHOULD I LOOK FOR ATTN DATA?")
    
    if input("\nPRESS Y TO PROCESS ").upper() == 'Y':
        # Process the data
        df = process_business_data(df, blank_cols, business_col)
        df = process_attn_data(df, attn_cols)
        
        # Save the processed file
        output_path = os.path.splitext(file_path)[0] + "_processed.csv"
        df.to_csv(output_path, index=False)
        print(f"\nProcessing complete. File saved as: {output_path}")
