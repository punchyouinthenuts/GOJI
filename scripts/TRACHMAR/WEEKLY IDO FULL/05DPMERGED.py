import pandas as pd
import os
import sys
import json
import shutil
from datetime import datetime
import numpy as np
import traceback
import re

# Define directories for GOJI
BASE_PATH = r"C:\Goji\TRACHMAR\WEEKLY IDO FULL"
RAW_FOLDER = os.path.join(BASE_PATH, "RAW FILES")
INPUT_FILE = os.path.join(BASE_PATH, "BM INPUT", "INPUT.csv")
PCEXP_FILE = os.path.join(BASE_PATH, "OUTPUT.csv")
MOVE_UPDATES_FILE = os.path.join(BASE_PATH, "MOVE UPDATES.csv")
PROCESSED_DIR = os.path.join(BASE_PATH, "PROCESSED")
PREFLIGHT_DIR = os.path.join(BASE_PATH, "PREFLIGHT")
BACKUP_DIR = os.path.join(BASE_PATH, "BACKUP")
TEMP_DIR = os.path.join(BASE_PATH, "TEMP")
ROLLBACK_LOG = os.path.join(TEMP_DIR, "rollback_05.log")

# Global rollback tracking
rollback_operations = []

def log_operation(operation_type, source_path, dest_path=None, created_file=None):
    """Log an operation for potential rollback"""
    operation = {
        "type": operation_type,  # "move", "create", "mkdir"
        "source": source_path,
        "destination": dest_path,
        "created_file": created_file,
        "timestamp": datetime.now().isoformat()
    }
    rollback_operations.append(operation)

def save_rollback_log():
    """Save rollback operations to log file"""
    try:
        os.makedirs(TEMP_DIR, exist_ok=True)
        with open(ROLLBACK_LOG, 'w') as f:
            json.dump(rollback_operations, f, indent=2)
        print(f"=== ROLLBACK_LOG_SAVED: {ROLLBACK_LOG} ===")
    except Exception as e:
        print(f"=== ERROR_SAVING_ROLLBACK_LOG: {str(e)} ===")

def perform_rollback():
    """Perform complete rollback of all operations"""
    print("=== STARTING_ROLLBACK ===")
    
    # Reverse the operations list to undo in reverse order
    for operation in reversed(rollback_operations):
        try:
            if operation["type"] == "move":
                # Move file back to original location
                if os.path.exists(operation["destination"]):
                    shutil.move(operation["destination"], operation["source"])
                    print(f"=== ROLLBACK_MOVED: {operation['destination']} -> {operation['source']} ===")
                    
            elif operation["type"] == "create":
                # Delete created file
                if operation["created_file"] and os.path.exists(operation["created_file"]):
                    os.remove(operation["created_file"])
                    print(f"=== ROLLBACK_DELETED: {operation['created_file']} ===")
                    
            elif operation["type"] == "mkdir":
                # Remove created directory if empty
                if operation["created_file"] and os.path.exists(operation["created_file"]):
                    try:
                        os.rmdir(operation["created_file"])
                        print(f"=== ROLLBACK_REMOVED_DIR: {operation['created_file']} ===")
                    except OSError:
                        # Directory not empty, leave it
                        pass
                        
        except Exception as e:
            print(f"=== ROLLBACK_ERROR: {str(e)} ===")
    
    # Clean up rollback log
    try:
        if os.path.exists(ROLLBACK_LOG):
            os.remove(ROLLBACK_LOG)
        if os.path.exists(TEMP_DIR) and not os.listdir(TEMP_DIR):
            os.rmdir(TEMP_DIR)
    except:
        pass
        
    print("=== ROLLBACK_COMPLETE ===")

def safe_makedirs(path):
    """Safely create directory with rollback logging"""
    if not os.path.exists(path):
        os.makedirs(path)
        log_operation("mkdir", path, created_file=path)
        print(f"=== CREATED_DIRECTORY: {path} ===")

def safe_move(source, destination):
    """Safely move file with rollback logging"""
    if os.path.exists(source):
        shutil.move(source, destination)
        log_operation("move", source, destination)
        print(f"=== MOVED_FILE: {os.path.basename(source)} -> {os.path.basename(destination)} ===")

def safe_create_file(file_path, dataframe):
    """Safely create CSV file with rollback logging"""
    dataframe.to_csv(file_path, index=False)
    log_operation("create", file_path, created_file=file_path)
    print(f"=== CREATED_FILE: {os.path.basename(file_path)} ===")

def find_target_file(file_number):
    """Find the target file in RAW FILES based on file number"""
    try:
        raw_files = os.listdir(RAW_FOLDER)
        file_number_padded = file_number.zfill(2)
        
        target_file = None
        for file in raw_files:
            if file.startswith(file_number_padded + " ") and (file.endswith(".xlsx") or file.endswith(".csv")):
                target_file = file
                break

        if not target_file:
            raise Exception(f"No file found starting with {file_number_padded}")

        return target_file
        
    except Exception as e:
        raise Exception(f"Error finding target file: {str(e)}")

def remove_decimals_from_dataframe(df):
    """Function to clean decimal points from numbers in all columns"""
    for column in df.columns:
        if df[column].astype(str).str.contains(r'\.0+$').any():
            df[column] = df[column].astype(str).apply(
                lambda x: x.rstrip('0').rstrip('.') if x.endswith('.0') or x.endswith('.00') else x
            )
    return df

def main():
    """Main processing function"""
    try:
        print("=== STARTING_DPMERGED_PROCESSING ===")
        
        # Check command line arguments
        if len(sys.argv) != 2:
            raise Exception("Usage: python 05DPMERGED.py <file_number>")
        
        file_number = sys.argv[1].strip()
        
        if not file_number:
            raise Exception("File number cannot be empty")
        
        print(f"=== PROCESSING_FILE_NUMBER: {file_number} ===")
        
        # Verify required files exist
        required_files = [INPUT_FILE, PCEXP_FILE, MOVE_UPDATES_FILE]
        for file_path in required_files:
            if not os.path.exists(file_path):
                raise Exception(f"Required file not found: {file_path}")
        
        # Find corresponding file in RAW FILES
        target_file = find_target_file(file_number)
        print(f"=== TARGET_FILE_FOUND: {target_file} ===")

        # Generate output filename
        output_filename = target_file[3:].replace('.xlsx', '.csv').replace('.csv', '.csv')
        
        # Create necessary directories
        safe_makedirs(PROCESSED_DIR)
        safe_makedirs(PREFLIGHT_DIR)
        safe_makedirs(BACKUP_DIR)
        
        output_file = os.path.join(PROCESSED_DIR, output_filename)

        # Read the RAW file first - IMPORTANT: We don't copy/convert it yet
        raw_file_path = os.path.join(RAW_FOLDER, target_file)
        if target_file.endswith('.xlsx'):
            df_raw = pd.read_excel(raw_file_path)
        else:
            df_raw = pd.read_csv(raw_file_path)

        print("=== RAW_FILE_LOADED ===")

        # Read the CSV files
        df_main = pd.read_csv(INPUT_FILE)
        df_pcexp = pd.read_csv(PCEXP_FILE)
        df_move = pd.read_csv(MOVE_UPDATES_FILE)

        print("=== INPUT_FILES_LOADED ===")

        # Convert recno to string and strip whitespace in both dataframes for consistent comparison
        df_main['recno'] = df_main['recno'].astype(str).str.strip()
        df_pcexp['recno'] = df_pcexp['recno'].astype(str).str.strip()

        # Debug statement to show unique values in 'User Text 3' before processing
        print("=== UNIQUE_USER_TEXT_3_VALUES:", df_pcexp['User Text 3'].unique(), "===")

        # Use numeric comparison for 'User Text 3' to identify records equal to 14
        text14_recnos = set(df_pcexp[pd.to_numeric(df_pcexp['User Text 3'], errors='coerce') == 14]['recno'].values)
        print(f"=== RECORDS_WITH_USER_TEXT_3_14: {len(text14_recnos)} ===")

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

        print("=== MAILED_COLUMN_UPDATED ===")

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
            df_main.loc[matching_mask, 'City'] = matches.loc[matching_mask, 'City']
            df_main.loc[matching_mask, 'State'] = matches.loc[matching_mask, 'State']
            df_main.loc[matching_mask, 'ZIP Code'] = matches.loc[matching_mask, 'ZIP Code']
            print("=== ADDRESS_UPDATES_APPLIED ===")
        else:
            print("=== NO_ADDRESS_UPDATES_FOUND ===")

        # Remove temporary lowercase columns
        df_main = df_main.drop(['hoh_guardian_name_lower', 'member_address1_lower'], axis=1)

        # Replace all NaN values with empty strings
        df_raw = df_raw.fillna('')

        # Apply the cleanup to df_raw
        df_raw = remove_decimals_from_dataframe(df_raw)

        # Save the processed file with the mailed column
        safe_create_file(output_file, df_raw)
        print(f"=== PROCESSED_FILE_SAVED: {output_filename} ===")

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
            print(f"=== WARNING: Could not find column {e}. Using original values where possible ===")
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
        base_no_ext = os.path.splitext(output_filename)[0]
        pre_base = re.sub(r'_(\d{8})(?:_.*)?$', '', base_no_ext)  # remove _YYYYMMDD and anything after
        preflight_filename = f"{pre_base}.csv"
        preflight_path = os.path.join(PREFLIGHT_DIR, preflight_filename)
        safe_create_file(preflight_path, df_preflight)
        print(f"=== PREFLIGHT_FILE_SAVED: {preflight_filename} ===")

        # Move source file to processed folder
        source_path = os.path.join(RAW_FOLDER, target_file)
        dest_path = os.path.join(RAW_FOLDER, "PROCESSED", target_file)
        safe_makedirs(os.path.join(RAW_FOLDER, "PROCESSED"))
        safe_move(source_path, dest_path)
        print(f"=== SOURCE_FILE_MOVED_TO_PROCESSED: {target_file} ===")

        # Generate timestamp and handle backups
        timestamp = datetime.now().strftime("_%y%m%d-%H%M")
        move_backup = os.path.join(BACKUP_DIR, f"MOVE UPDATES{timestamp}.csv")
        output_backup = os.path.join(BACKUP_DIR, f"OUTPUT{timestamp}.csv")
        
        safe_move(MOVE_UPDATES_FILE, move_backup)
        safe_move(PCEXP_FILE, output_backup)

        print(f"=== BACKUP_FILES_CREATED ===")
        print(f"=== MOVE_UPDATES_BACKUP: {os.path.basename(move_backup)} ===")
        print(f"=== OUTPUT_BACKUP: {os.path.basename(output_backup)} ===")

        # Save rollback log for coordination
        save_rollback_log()

        print("=== ALL_PROCESSING_COMPLETED_SUCCESSFULLY ===")
        print("=== SCRIPT_SUCCESS ===")
        return 0

    except Exception as e:
        print(f"=== SCRIPT_ERROR ===")
        print(f"=== ERROR_MESSAGE: {str(e)} ===")
        
        # Print the full error traceback for debugging
        print("=== FULL_ERROR_TRACEBACK ===")
        traceback.print_exc()
        
        # Perform rollback
        perform_rollback()
        
        return 1

if __name__ == "__main__":
    sys.exit(main())