import pandas as pd
import os
import shutil
from datetime import datetime
import re
import sys

# Valid month abbreviations
VALID_MONTHS = ['JAN', 'FEB', 'MAR', 'APR', 'MAY', 'JUN', 
                'JUL', 'AUG', 'SEP', 'OCT', 'NOV', 'DEC']

# Backup tracking
backup_files = []

def create_backup(file_path):
    backup_path = file_path + '.bak'
    shutil.copy2(file_path, backup_path)
    backup_files.append(backup_path)
    return backup_path

def cleanup_backups():
    for backup in backup_files:
        if os.path.exists(backup):
            os.remove(backup)
    backup_files.clear()

def rollback():
    print("Error occurred. Rolling back changes...")
    for backup in backup_files:
        if os.path.exists(backup):
            original = backup[:-4]  # Remove .bak
            if os.path.exists(original):
                os.remove(original)
            shutil.move(backup, original)
    backup_files.clear()

def check_network_path(path):
    return os.path.exists(path)

def get_user_input():
    while True:
        jnm = input("ENTER JOB NUMBER AND MONTH: ").strip().upper()
        parts = jnm.split()
        if len(parts) != 2:
            print("Please enter both job number and month separated by space.")
            continue
        
        job_num, month = parts
        if not (job_num.isdigit() and len(job_num) == 5):
            print("Job number must be exactly 5 digits.")
            continue
            
        if month not in VALID_MONTHS:
            print("Month must be a valid 3-letter abbreviation (JAN, FEB, etc).")
            continue
            
        return job_num, month, f"{job_num} {month}"

try:
    # Get user input
    job_number, month, jnm = get_user_input()

    # Set directories
    work_dir = r'C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\TERM\DATA'
    archive_dir = os.path.join(r'C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\TERM\ARCHIVE', jnm)
    os.makedirs(archive_dir, exist_ok=True)

    # Read all files
    excel_file = os.path.join(work_dir, 'FHK_TERM.xlsx')
    csv_file = os.path.join(work_dir, 'MOVE UPDATES.csv')
    presort_file = os.path.join(work_dir, 'PRESORTLIST.csv')

    # Create backups
    for file in [excel_file, csv_file, presort_file]:
        create_backup(file)

    # Read files
    df_excel = pd.read_excel(excel_file, index_col=None)
    df_csv = pd.read_csv(csv_file)
    df_presort = pd.read_csv(presort_file)

    # Convert lookup columns to string and clean them
    df_excel['lookup_col'] = df_excel.iloc[:, 7].astype(str).str.strip().str.upper()
    df_csv['match_col'] = df_csv.iloc[:, 0].astype(str).str.strip().str.upper()
    df_presort['presort_col'] = df_presort.iloc[:, 8].astype(str).str.strip().str.upper()

    # First VLOOKUP operations
    column_mapping = {
        'newadd': 3,    # CSV Column D
        'newadd2': 4,   # CSV Column E
        'newcity': 5,   # CSV Column F
        'newstate': 6,  # CSV Column G
        'newzip': 7     # CSV Column H
    }

    # Add new columns
    for new_col in column_mapping.keys():
        df_excel[new_col] = ''

    # Perform the VLOOKUP operation
    matches = 0
    for index, row in df_excel.iterrows():
        lookup_value = row['lookup_col']
        match = df_csv[df_csv['match_col'] == lookup_value]
        
        if not match.empty:
            matches += 1
            for new_col, csv_col_index in column_mapping.items():
                df_excel.at[index, new_col] = match.iloc[0, csv_col_index]

    # Enhanced mailed column logic
    df_excel['mailed'] = 13
    matches_found = 0

    for index, row in df_excel.iterrows():
        lookup_value = row['lookup_col']
        matching_rows = df_presort[df_presort['presort_col'] == lookup_value]
        
        if not matching_rows.empty:
            col_d_value = matching_rows.iloc[0, 3]
            if pd.notna(col_d_value) and col_d_value == -1:
                df_excel.at[index, 'mailed'] = 14
                matches_found += 1

    # Remove temporary columns
    df_excel = df_excel.drop(['lookup_col'], axis=1)

    # Remove unnamed columns
    unnamed_cols = [col for col in df_excel.columns if 'unnamed' in col.lower()]
    df_excel = df_excel.drop(columns=unnamed_cols)

    # Save FHK_TERM_UPDATED.xlsx
    output_file = os.path.join(work_dir, 'FHK_TERM_UPDATED.xlsx')
    df_excel.to_excel(output_file, index=False)

    # Create PRESORTLIST_PRINT.csv - keep records with Tray Numbers
    df_presort_print = df_presort[df_presort['Tray Number'].notna()]
    print_filename = f"{jnm} PRESORTLIST_PRINT.csv"
    presort_print_file = os.path.join(work_dir, print_filename)
    df_presort_print.to_csv(presort_print_file, index=False)

    # Network path operations
    current_year = str(datetime.now().year)
    network_base = rf'\\NAS1069D9\AMPrintData\{current_year}_SrcFiles\T\Trachmar'
    
    if check_network_path(network_base):
        # Find job number folder
        job_folders = [f for f in os.listdir(network_base) if job_number in f]
        if job_folders:
            dest_path = os.path.join(network_base, job_folders[0], 'HP Indigo', 'DATA')
            dest_file = os.path.join(dest_path, print_filename)
            
            if os.path.exists(dest_file):
                response = input("FILES ALREADY EXIST, OVERWRITE? Y/N: ").upper()
                if response == 'Y':
                    shutil.copy2(presort_print_file, dest_file)
                else:
                    base, ext = os.path.splitext(dest_file)
                    dest_file = f"{base}_copy{ext}"
                    shutil.copy2(presort_print_file, dest_file)
            else:
                shutil.copy2(presort_print_file, dest_file)
    else:
        print("Network drive is offline. Skipping network copy operation.")

    # Move files to archive
    for file in os.listdir(work_dir):
        src_file = os.path.join(work_dir, file)
        dst_file = os.path.join(archive_dir, file)
        if os.path.isfile(src_file):
            if os.path.exists(dst_file):
                base, ext = os.path.splitext(dst_file)
                dst_file = f"{base}_copy{ext}"
            shutil.move(src_file, dst_file)

    # Cleanup successful execution
    cleanup_backups()
    print("Processing completed successfully!")
    input("Press Enter to exit...")

except Exception as e:
    print(f"An error occurred: {str(e)}")
    rollback()
    input("Press Enter to exit...")
    sys.exit(1)
