import pandas as pd
import os

def get_file_path():
    return input("WHICH FILE NEEDS PROCESSING? ").strip('"')

def display_numbered_columns(df):
    print("\nCurrent columns:")
    for idx, col in enumerate(df.columns, 1):
        print(f"{idx}. {col}")

def get_address_columns():
    cols = input("\nWHICH COLUMNS ARE ADDRESS LINES? ").replace(" ", "").split(",")
    return [int(x) - 1 for x in cols]

def process_manual_moves(df, address_cols):
    while True:
        # Find rows where all address columns have data
        mask = df.iloc[:, address_cols].notna().all(axis=1)
        if not mask.any():
            print("\nNo rows found with data in all selected columns.")
            return df
            
        # Display address columns for selection
        print("\nAddress columns:")
        for idx, col_idx in enumerate(address_cols, 1):
            print(f"{idx}. {df.columns[col_idx]}")
            
        source_idx = int(input("\nWHICH COLUMN'S DATA DO YOU WANT TO MOVE? ")) - 1
        source_col = address_cols[source_idx]
        
        display_numbered_columns(df)
        target_col = int(input("\nWHICH COLUMN DO YOU WANT TO MOVE IT TO? ")) - 1
        
        print(f"\nIS THIS CORRECT? {df.columns[source_col]} -> {df.columns[target_col]} Y/N")
        if input().upper() != 'Y':
            continue
            
        # Move the data
        df.loc[mask, df.columns[target_col]] = df.loc[mask, df.columns[source_col]]
        df.loc[mask, df.columns[source_col]] = ''
        
        if input("\nDO YOU NEED TO REPEAT THIS PROCESS? Y/N ").upper() != 'Y':
            break
            
    return df

def process_automatic_moves(df, address_cols):
    first_col = address_cols[0]
    last_col = address_cols[-1]
    
    # Find rows where last column has data but first doesn't
    mask = (df.iloc[:, last_col].notna()) & (df.iloc[:, first_col].isna())
    
    # Move data from last to first column for matching rows
    df.loc[mask, df.columns[first_col]] = df.loc[mask, df.columns[last_col]]
    df.loc[mask, df.columns[last_col]] = ''
    
    return df

if __name__ == "__main__":
    # Get and read input file
    file_path = get_file_path()
    df = pd.read_csv(file_path) if file_path.lower().endswith('.csv') else pd.read_excel(file_path)
    
    # Display columns and get address columns
    display_numbered_columns(df)
    address_cols = get_address_columns()
    
    # Process manual moves
    df = process_manual_moves(df, address_cols)
    
    # Process automatic moves
    df = process_automatic_moves(df, address_cols)
    
    # Save processed file
    output_path = os.path.splitext(file_path)[0] + "_v2.csv"
    df.to_csv(output_path, index=False)
    
    print("\nADDRESS LINES PROCESSED! PRESS X TO TERMINATE...")
    while input().upper() != 'X':
        print("Please press X to terminate...")
