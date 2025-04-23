import pandas as pd
import os
import shutil
import csv
from string import ascii_letters
from itertools import islice

def get_next_name(base_path):
    if not os.path.exists(base_path):
        return base_path
    
    directory, basename = os.path.split(base_path)
    return next(
        (os.path.join(directory, f"{basename}_{letter}")
         for letter in ascii_letters 
         if not os.path.exists(os.path.join(directory, f"{basename}_{letter}"))),
        None
    )

def handle_path_conflict(path):
    if os.path.exists(path):
        if input(f"{path} already exists. Overwrite? (Y/N): ").upper() == 'Y':
            return path
        new_path = get_next_name(path)
        if not new_path:
            raise Exception("No available names found")
        return new_path
    return path

def process_txt_to_df(txt_path):
    txt_file = next((f for f in os.listdir(txt_path) if f.endswith('.txt')), None)
    if not txt_file:
        raise FileNotFoundError("No TXT file found")
    
    txt_full_path = os.path.join(txt_path, txt_file)
    
    # First pass to analyze data structure
    with open(txt_full_path, 'r') as file:
        reader = csv.reader(file, delimiter=',', quotechar='"')
        sample_rows = list(islice(reader, 20))
    
    # Determine number of columns and analyze content
    num_columns = len(sample_rows[0]) if sample_rows else 0
    
    # Find columns with specific patterns
    columns_data = list(zip(*sample_rows))
    
    # Find Title column (C/O pattern)
    title_col_index = next((i for i, col in enumerate(columns_data) 
                          if any('C / O' in row for row in col)), None)
    
    # Find Business column (School/Institution pattern)
    business_keywords = ['SCHOOL', 'ACADEMY', 'HIGH', 'MIDDLE', 'ELEMENTARY', 'DISTRICT']
    business_col_index = next((i for i, col in enumerate(columns_data)
                             if any(any(keyword in row.upper() for keyword in business_keywords)
                                   for row in col)), None)
    
    # Create headers
    headers = ['BREAKMARK']
    for i in range(num_columns):
        if i == 0:
            headers.append('BARCODE')
        elif i == 1:
            headers.append('TRAY')
        elif i == 2:
            headers.append('ENDORSEMENT')
        elif i == business_col_index:
            headers.append('BUSINESS')
        elif i == title_col_index:
            headers.append('TITLE')
        elif i == num_columns - 1:
            headers.append('CSZ')
        else:
            headers.append('ADDRESS')
    
    # Process the file in chunks
    chunks = []
    chunk_size = 1000
    
    with open(txt_full_path, 'r') as file:
        reader = csv.reader(file, delimiter=',', quotechar='"')
        while True:
            chunk_data = list(islice(reader, chunk_size))
            if not chunk_data:
                break
            df_chunk = pd.DataFrame([[''] + row for row in chunk_data], columns=headers)
            chunks.append(df_chunk)
    
    df = pd.concat(chunks, ignore_index=True) if chunks else pd.DataFrame(columns=headers)
    
    # Remove final row if it contains header-like content
    if len(df) > 0:
        last_row = df.iloc[-1]
        if any(col.upper() in ['FIRST NAME', 'LAST', 'ADDRESS', 'CITY', 'STATE', 'ZIP'] 
               for col in last_row.values if isinstance(col, str)):
            df = df.iloc[:-1]
    
    return df, txt_file, txt_full_path

def main():
    txt_path = r"C:\Users\JCox\Downloads"
    df, txt_file, txt_full_path = process_txt_to_df(txt_path)
    
    # Process TRAY column efficiently with three # for final break
    mask = df['TRAY'].str.match(r'^\d+$', na=False)
    unique_trays = df.loc[mask, 'TRAY'].unique()
    
    for tray in unique_trays[:-1]:  # All except last tray
        df.loc[df[df['TRAY'] == tray].index[-1], 'BREAKMARK'] = '##'
    
    # Set last tray's break mark to three #
    if len(unique_trays) > 0:
        last_tray = unique_trays[-1]
        df.loc[df[df['TRAY'] == last_tray].index[-1], 'BREAKMARK'] = '###'
    
    job_number = input("ENTER JOB NUMBER: ")
    
    # Handle folder creation
    network_folder = fr'\\NAS1069D9\AMPrintData\2025_SrcFiles\D\DP Marketing\{job_number}'
    alternative_folder = r'C:\Users\JCox\Desktop\MOVE TO BUSKRO'
    
    try:
        new_folder = handle_path_conflict(network_folder)
        os.makedirs(new_folder, exist_ok=True)
    except OSError as e:
        print(f"Error accessing network drive: {e}")
        print(f"Saving files to alternative location: {alternative_folder}")
        new_folder = handle_path_conflict(os.path.join(alternative_folder, job_number))
        os.makedirs(new_folder, exist_ok=True)

    # Save files
    csv_filename = f"{job_number} DP Marketing_WBREAK.csv"
    csv_path = os.path.join(new_folder, csv_filename)
    df.to_csv(csv_path, index=False, quoting=csv.QUOTE_ALL)

    # Move and copy files
    try:
        shutil.move(txt_full_path, os.path.join(new_folder, txt_file))
        w_drive_dest = handle_path_conflict(os.path.join('W:\\', csv_filename))
        shutil.copy2(csv_path, w_drive_dest)
    except OSError as e:
        print(f"Error moving or copying files: {e}")
        print("Files will be saved in the alternative location.")

if __name__ == "__main__":
    main()