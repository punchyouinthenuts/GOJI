import glob
import pandas as pd
import os
import time
import sys
import threading
import shutil
from tqdm import tqdm

def loading_animation(description="Processing"):
    with tqdm(total=100, desc=description, bar_format='{l_bar}{bar}| {n_fmt}/{total_fmt}') as pbar:
        while not loading_complete:
            pbar.update(1)
            time.sleep(0.1)
            if pbar.n >= 99:
                pbar.n = 0

def find_header_row(df):
    for idx, row in df.iterrows():
        if "Member ID" in row.values and "Guardian Name" in row.values:
            return idx
    return None

def process_term_file():
    global loading_complete
    loading_complete = False
    
    file_path = r'C:\Users\JCox\Downloads'
    output_dir = r'C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\TERM\DATA'
    
    # Create directory if it doesn't exist
    os.makedirs(output_dir, exist_ok=True)
    
    input_files = glob.glob(os.path.join(file_path, 'FHK_Term*.xlsx'))
    while True:
        if not input_files:
            response = input("No FHK_Term*.xlsx files found in Downloads folder. Enter 1 to retry or 0 to terminate: ")
            
            if response == '0':
                print("Script terminated.")
                return
            elif response == '1':
                input_files = glob.glob(os.path.join(file_path, 'FHK_Term*.xlsx'))
                continue
            else:
                print("Invalid input. Please enter 0 or 1.")
                continue

        input_file = max(input_files, key=os.path.getctime)
        try:
            # Copy and rename the file to the destination folder
            destination_file = os.path.join(output_dir, 'FHK_TERM.xlsx')
            shutil.copy2(input_file, destination_file)

            animation_thread = threading.Thread(target=loading_animation)
            animation_thread.start()
            
            # Read the Excel file without headers
            df = pd.read_excel(destination_file, header=None)
            
            # Find the header row
            header_row_idx = find_header_row(df)
            
            if header_row_idx is None:
                raise ValueError("Could not find header row with 'Member ID' and 'Guardian Name'")
            
            # Set the header row as column names and remove previous rows
            df.columns = df.iloc[header_row_idx]
            df = df.iloc[header_row_idx + 1:].reset_index(drop=True)
            
            # Save the cleaned file
            df.to_excel(destination_file, index=False)

            guardian_col = df.columns[7]  # Column H
            address_col = df.columns[12]  # Column M
            result = df.copy()
            grouped = df.groupby([guardian_col, address_col])

            for name, group in grouped:
                member_data = group[df.columns[[4, 5, 6]]].apply(
                    lambda x: f"{x[df.columns[6]]} {x[df.columns[5]]}, {x[df.columns[4]]}", 
                    axis=1
                ).tolist()
                
                for i, data in enumerate(member_data, 1):
                    col_name = f'ID{i}'
                    mask = (result[guardian_col] == name[0]) & (result[address_col] == name[1])
                    if col_name not in result.columns:
                        result[col_name] = ''
                    result.loc[mask, col_name] = data

            output_path = os.path.join(output_dir, 'processed_data.xlsx')
            result.to_excel(output_path, index=False)

            processed_df = pd.read_excel(output_path)
            deduped_df = processed_df.drop_duplicates(subset=[guardian_col])
            deduped_df = deduped_df.loc[:, ~deduped_df.columns.str.contains('Unnamed', case=False)]
            
            csv_path = os.path.join(output_dir, 'FHK_TERM.csv')
            deduped_df.to_csv(csv_path, index=False)
            
            os.remove(output_path)
            
            # Delete the original file from Downloads folder
            os.remove(input_file)
            
            loading_complete = True
            animation_thread.join()
            print("\nPROCESSING COMPLETE!")
            time.sleep(3)
            break

        except Exception as e:
            loading_complete = True
            animation_thread.join()
            print(f"\nError processing file: {str(e)}")
            break

if __name__ == "__main__":
    process_term_file()
