import pandas as pd
import os
import shutil
from datetime import datetime
import numpy as np
import traceback

try:
    # Define base file paths
    base_path = r"C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\DATA PROCESSING"
    input_file = os.path.join(base_path, "BM INPUT", "INPUT.csv")
    pcexp_file = os.path.join(base_path, "OUTPUT.csv")
    move_updates_file = os.path.join(base_path, "MOVE UPDATES.csv")

    # Get file number from user
    file_number = input("WHICH FILE WAS PROCESSED? ENTER NUMBER: ")

    # Find corresponding file in RAW FILES
    raw_folder = os.path.join(base_path, "RAW FILES")
    raw_files = os.listdir(raw_folder)
    target_file = None
    for file in raw_files:
        if file.startswith(file_number + " ") and (file.endswith(".xlsx") or file.endswith(".csv")):
            target_file = file
            break

    if not target_file:
        raise FileNotFoundError(f"No file found starting with {file_number}")

    # Generate output filename
    output_filename = target_file[3:].replace('.xlsx', '.csv').replace('.csv', '.csv')
    output_file = os.path.join(base_path, "PROCESSED", output_filename)

    # Read the RAW file first - IMPORTANT: We don't copy/convert it yet
    if target_file.endswith('.xlsx'):
        df_raw = pd.read_excel(os.path.join(raw_folder, target_file))
    else:
        df_raw = pd.read_csv(os.path.join(raw_folder, target_file))

    # Read the CSV files
    df_main = pd.read_csv(input_file)
    df_pcexp = pd.read_csv(pcexp_file)
    df_move = pd.read_csv(move_updates_file)

    # Convert recno to string and strip whitespace in both dataframes for consistent comparison
    df_main['recno'] = df_main['recno'].astype(str).str.strip()
    df_pcexp['recno'] = df_pcexp['recno'].astype(str).str.strip()

    # Debug statement to show unique values in 'User Text 3' before processing
    print("Unique values in 'User Text 3' before processing:", df_pcexp['User Text 3'].unique())

    # Use numeric comparison for 'User Text 3' to identify records equal to 14
    text14_recnos = set(df_pcexp[pd.to_numeric(df_pcexp['User Text 3'], errors='coerce') == 14]['recno'].values)
    print(f"Records with User Text 3 = 14: {text14_recnos}")

    # Add new columns to processed data
    df_raw['mailed'] = ''
    df_raw['new add'] = ''
    df_raw['newadd2'] = ''
    df_raw['City'] = ''
    df_raw['State'] = ''
    df_raw['ZIP Code'] = ''

    # Update the mailed column in the raw data based on recno matching
    for idx, row in df_raw.iterrows():
        recno = str(row['recno']).strip()
        if recno in text14_recnos:
            df_raw.at[idx, 'mailed'] = '14'
        else:
            df_raw.at[idx, 'mailed'] = '13'

    # Convert matching columns to lowercase with explicit string conversion
    df_main['hoh_guardian_name_lower'] = df_main['hoh_guardian_name'].astype(str).str.lower()
    df_main['member_address1_lower'] = df_main['member_address1'].astype(str).str.lower()
    df_move['Full Name_lower'] = df_move['Full Name'].astype(str).str.lower()
    df_move['Original Address Line 1_lower'] = df_move['Original Address Line 1'].astype(str).str.lower()

    # Create the matches with explicit column mapping
    matches = pd.merge(
        df_main,
        df_move[[
            'Full Name_lower', 
            'Original Address Line 1_lower', 
            'Address Line 1', 
            'Address Line 2', 
            'City', 
            'State', 
            'ZIP Code'
        ]],
        left_on=['hoh_guardian_name_lower', 'member_address1_lower'],
        right_on=['Full Name_lower', 'Original Address Line 1_lower'],
        how='left',
        indicator=True
    )

    # Update matching records
    matching_mask = matches['_merge'] == 'both'
    if matching_mask.any():
        df_main.loc[matching_mask, 'new add'] = matches.loc[matching_mask, 'Address Line 1']
        df_main.loc[matching_mask, 'newadd2'] = matches.loc[matching_mask, 'Address Line 2']
        df_main.loc[matching_mask, 'City'] = matches.loc[matching_mask, 'City']  # Fixed
        df_main.loc[matching_mask, 'State'] = matches.loc[matching_mask, 'State']  # Fixed
        df_main.loc[matching_mask, 'ZIP Code'] = matches.loc[matching_mask, 'ZIP Code']  # Fixed
    else:
        print("No address updates found in move update file. Processing continues with original addresses.")

    # Remove temporary lowercase columns
    df_main = df_main.drop(['hoh_guardian_name_lower', 'member_address1_lower'], axis=1)

    # Replace all NaN values with empty strings
    df_raw = df_raw.fillna('')

    # Function to clean decimal points from numbers in all columns
    def remove_decimals_from_dataframe(df):
        for column in df.columns:
            if df[column].astype(str).str.contains(r'\.0+$').any():
                df[column] = df[column].astype(str).apply(
                    lambda x: x.rstrip('0').rstrip('.') if x.endswith('.0') or x.endswith('.00') else x
                )
        return df

    # Apply the cleanup to df_raw
    df_raw = remove_decimals_from_dataframe(df_raw)

    # Save the processed file with the mailed column
    df_raw.to_csv(output_file, index=False)
    print(f"Processed file saved as: {output_filename} with mailed column added")
    print(f"  - Check for mailed column in: {output_file}")

    # Create PREFLIGHT CSV
    preflight_headers = [
        'member1_id', 'member1_effective_date', 'member1_grp_plan_name', 'member1_last_name', 
        'member1_first_name', 'member1_middle_initial', 'member2_id', 'member2_effective_date', 
        'member2_grp_plan_name', 'member2_last_name', 'member2_first_name', 'member2_middle_initial',
        'member3_id', 'member3_effective_date', 'member3_grp_plan_name', 'member3_last_name', 
        'member3_first_name', 'member3_middle_initial', 'member4_id', 'member4_effective_date', 
        'member4_grp_plan_name', 'member4_last_name', 'member4_first_name', 'member4_middle_initial',
        'hoh_guardian_name', 'member_address1', 'member_address2', 'member_city', 'member_state', 
        'member_zip', 'office_name', 'office_phone', 'office_address_1', 'office_address_2', 
        'office_city', 'office_state', 'office_zip', 'recno', 'mailed'
    ]

    # Merge with pcexp for preflight file creation only
    df_merged = pd.merge(df_main, 
                        df_pcexp[['Full Name', 'Address Line 1', 'Address Line 2', 'City', 'State', 
                                 'ZIP Code', 'recno', 'User Text 3']], 
                        on='recno', how='inner')

    # Filter out records with User Text 3 = 14
    df_processed_filtered = df_merged[pd.to_numeric(df_merged['User Text 3'], errors='coerce') != 14]

    # Create the preflight DataFrame
    df_preflight = pd.DataFrame(columns=preflight_headers)

    # Copy data from filtered records
    for column in preflight_headers:
        if column in df_processed_filtered.columns:
            df_preflight[column] = df_processed_filtered[column]

    # Explicitly map the address fields from OUTPUT.csv
    try:
        df_preflight['member_address1'] = df_processed_filtered['Address Line 1']
        df_preflight['member_address2'] = df_processed_filtered['Address Line 2']
        
        if 'City_y' in df_processed_filtered.columns:
            df_preflight['member_city'] = df_processed_filtered['City_y']
        else:
            df_preflight['member_city'] = df_processed_filtered['City']
            
        if 'State_y' in df_processed_filtered.columns:
            df_preflight['member_state'] = df_processed_filtered['State_y']
        else:
            df_preflight['member_state'] = df_processed_filtered['State']
            
        if 'ZIP Code_y' in df_processed_filtered.columns:
            df_preflight['member_zip'] = df_processed_filtered['ZIP Code_y']
        else:
            df_preflight['member_zip'] = df_processed_filtered['ZIP Code']
            
        df_preflight['hoh_guardian_name'] = df_processed_filtered['Full Name']
    except KeyError as e:
        print(f"WARNING: Could not find column {e}. Using original values where possible.")
        if 'member_city' in df_main.columns:
            df_preflight['member_city'] = df_main['member_city']
        if 'member_state' in df_main.columns:
            df_preflight['member_state'] = df_main['member_state']
        if 'member_zip' in df_main.columns:
            df_preflight['member_zip'] = df_main['member_zip']

    # Ensure recno is included
    df_preflight['recno'] = df_processed_filtered['recno']

    # Replace all NaN values with empty strings in preflight file
    df_preflight = df_preflight.fillna('')

    # Apply decimal cleanup to the preflight dataframe
    df_preflight = remove_decimals_from_dataframe(df_preflight)

    # Save PREFLIGHT CSV
    preflight_filename = output_filename.replace('.csv', '_PF.csv')
    preflight_path = os.path.join(base_path, "PREFLIGHT", preflight_filename)
    df_preflight.to_csv(preflight_path, index=False)
    print(f"Preflight file saved as: {preflight_filename}")

    # Move source file to processed folder
    source_path = os.path.join(raw_folder, target_file)
    dest_path = os.path.join(raw_folder, "PROCESSED", target_file)
    shutil.move(source_path, dest_path)
    print(f"Source file moved to: {os.path.join('RAW FILES', 'PROCESSED', target_file)}")

    # Generate timestamp and handle backups
    timestamp = datetime.now().strftime("_%y%m%d-%H%M")
    backup_folder = r"C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\DATA PROCESSING\BACKUP"
    move_backup = os.path.join(backup_folder, f"MOVE UPDATES{timestamp}.csv")
    shutil.move(move_updates_file, move_backup)
    output_backup = os.path.join(backup_folder, f"OUTPUT{timestamp}.csv")
    shutil.move(pcexp_file, output_backup)
    print(f"\nFiles moved and renamed:")
    print(f"MOVE UPDATES moved to: {os.path.basename(move_backup)}")
    print(f"OUTPUT moved to: {os.path.basename(output_backup)}")

except Exception as e:
    # Print the full error traceback
    print("\nAn error occurred:")
    traceback.print_exc()
    print("\nPRESS X TO ACKNOWLEDGE AND TERMINATE THE SCRIPT...")
    while True:
        user_input = input().strip()
        if user_input.lower() == 'x':
            break