import os
import sys
import random
import shutil
import csv
from datetime import datetime
import pandas as pd

# Define directories
INPUT_DIR = "C:\\Users\\JCox\\Desktop\\AUTOMATION\\RAC\\INACTIVE_2310-DM07\\FOLDERS\\OUTPUT"
PROOF_DIR = "C:\\Users\\JCox\\Desktop\\AUTOMATION\\RAC\\INACTIVE_2310-DM07\\FOLDERS\\PROOF"

class ProcessingRollback:
    def __init__(self):
        self.backup_dir = os.path.join(os.path.dirname(INPUT_DIR), 'backup_' + datetime.now().strftime('%Y%m%d_%H%M%S'))
        self.modified_files = []
        self.txt_files = []
        
    def backup(self, directory):
        if os.path.exists(directory):
            shutil.copytree(directory, os.path.join(self.backup_dir, os.path.basename(directory)))
        
    def track_file(self, filepath):
        self.modified_files.append(filepath)
        if filepath.endswith('.txt'):
            self.txt_files.append(filepath)
        
    def rollback(self):
        print("\nInitiating rollback due to critical error...")
        for file in self.modified_files:
            if os.path.exists(file):
                os.remove(file)
                
        for dir_name in ['OUTPUT', 'PROOF']:
            backup_path = os.path.join(self.backup_dir, dir_name)
            target_path = os.path.join(os.path.dirname(INPUT_DIR), dir_name)
            if os.path.exists(backup_path):
                if os.path.exists(target_path):
                    shutil.rmtree(target_path)
                shutil.copytree(backup_path, target_path)
                
        print("Rollback completed successfully - all files restored to original state")
        
    def cleanup(self):
        if os.path.exists(self.backup_dir):
            shutil.rmtree(self.backup_dir)
            print("Backup folder removed - processing completed successfully")

def format_szip(szip):
    if pd.isna(szip):
        return '00000'
    szip = str(szip).strip()
    if len(szip) < 5:
        return szip.zfill(5)
    return szip

def format_credit(value):
    if pd.isna(value):
        return value
    try:
        # This will ensure 2 decimal places are always shown
        return '{:,.2f}'.format(float(str(value).replace(",", "")))
    except ValueError:
        return value

def get_random_sample(df, n=15):
    if len(df) <= n:
        return df
        
    sample_df = pd.DataFrame()
    if df['Store_License'].notna().any():
        license_record = df[df['Store_License'].notna()].sample(n=1)
        remaining = df[~df.index.isin(license_record.index)].sample(n=n-1)
        sample_df = pd.concat([license_record, remaining])
    else:
        sample_df = df.sample(n=n)
        
    return sample_df

def save_txt_file(df, output_path, rollback_manager):
    df.to_csv(output_path, index=False, encoding='utf-8')
    rollback_manager.track_file(output_path)

def convert_txt_to_csv(txt_path):
    csv_path = txt_path.replace('.txt', '.csv')
    df = pd.read_csv(txt_path, encoding='utf-8')
    df.to_csv(csv_path, index=False, encoding='utf-8-sig')
    return csv_path

def process_file(file_name, rollback_manager):
    input_file_path = os.path.join(INPUT_DIR, file_name)
    df = pd.read_csv(input_file_path, encoding='utf-8')
    
    file_type = 'PU' if 'PU' in file_name else 'PO'
    
    if file_type == 'PU':
        df['First Name'] = df['First Name'].str.upper()
        df['CREDIT'] = pd.to_numeric(df['CREDIT'], errors='coerce')
        df['CREDIT'] = df['CREDIT'].apply(format_credit)
        df['CREDIT'] = df['CREDIT'].astype(str)
    
    # Process PR records
    pr_mask = df['Creative_Version_Cd'].str.contains('-PR-' + file_type + '$', regex=True, na=False)
    df_pr = df[pr_mask].copy()
    
    if not df_pr.empty:
        df_pr['MESSAGE'] = "La oferta es válida hasta el "
        df_pr['EXPDATE'] = df_pr['EXPIRES'].str[29:]
        df_pr['EXPIRES'] = df_pr['MESSAGE'] + df_pr['EXPDATE']
        df_pr['SZIP'] = df_pr['SZIP'].apply(format_szip)
        
        pr_output_file = os.path.join(INPUT_DIR, f"PR-{file_type}.txt")
        pr_proof_file = os.path.join(PROOF_DIR, f"PR-{file_type}-PD.txt")
                
        save_txt_file(df_pr, pr_output_file, rollback_manager)
        pr_sample = get_random_sample(df_pr, 15)
        save_txt_file(pr_sample, pr_proof_file, rollback_manager)
    
    # Remove PR records from main dataframe
    df = df[~df.index.isin(df_pr.index)]
    
    # Process AT records
    at_mask = df['Creative_Version_Cd'].str.contains('-AT-' + file_type + '$', regex=True, na=False)
    df_at = df[at_mask].copy()
    
    if not df_at.empty:
        at_output_file = os.path.join(INPUT_DIR, f"AT-{file_type}.txt")
        at_proof_file = os.path.join(PROOF_DIR, f"AT-{file_type}-PD.txt")
        
        save_txt_file(df_at, at_output_file, rollback_manager)
        at_sample = get_random_sample(df_at, 15)
        save_txt_file(at_sample, at_proof_file, rollback_manager)
    
    # Remove AT records from main dataframe
    df = df[~df.index.isin(df_at.index)]
    
    # Process A records (remaining)
    a_mask = df['Creative_Version_Cd'].str.contains('-A-' + file_type + '$', regex=True, na=False)
    df_a = df[a_mask].copy()
    
    if not df_a.empty:
        a_output_file = os.path.join(INPUT_DIR, f"A-{file_type}.txt")
        a_proof_file = os.path.join(PROOF_DIR, f"A-{file_type}-PD.txt")
        
        save_txt_file(df_a, a_output_file, rollback_manager)
        a_sample = get_random_sample(df_a, 15)
        save_txt_file(a_sample, a_proof_file, rollback_manager)
    
    # Handle any remaining records that don't match the patterns
    remaining_df = df[~df.index.isin(df_a.index)]
    if not remaining_df.empty:
        print(f"Warning: {len(remaining_df)} records did not match any version pattern in {file_name}")
        remaining_output_file = os.path.join(INPUT_DIR, f"UNMATCHED-{file_type}.txt")
        save_txt_file(remaining_df, remaining_output_file, rollback_manager)

def convert_all_txt_to_csv(rollback_manager):
    print("\nConverting TXT files to CSV...")
    for txt_file in rollback_manager.txt_files:
        if os.path.exists(txt_file):
            csv_path = convert_txt_to_csv(txt_file)
            print(f"Converted: {txt_file} -> {csv_path}")

def cleanup_txt_files(rollback_manager):
    print("\nCleaning up TXT files...")
    for txt_file in rollback_manager.txt_files:
        if os.path.exists(txt_file):
            os.remove(txt_file)
            print(f"Removed: {txt_file}")

def main():
    rollback_manager = ProcessingRollback()
    
    try:
        if not os.path.exists(INPUT_DIR) or not os.path.exists(PROOF_DIR):
            raise ValueError(f"Required directories not found")
        
        rollback_manager.backup(INPUT_DIR)
        rollback_manager.backup(PROOF_DIR)
                
        for file_name in ["A-PO.txt", "A-PU.txt"]:
            if not os.path.exists(os.path.join(INPUT_DIR, file_name)):
                raise FileNotFoundError(f"Input file not found: {file_name}")
            
            process_file(file_name, rollback_manager)
                
        convert_all_txt_to_csv(rollback_manager)
        cleanup_txt_files(rollback_manager)
        rollback_manager.cleanup()
                
    except Exception as e:
        error_message = f"""CRITICAL ERROR DETECTED:
Type: {type(e).__name__}
Details: {str(e)}
Location: {e.__traceback__.tb_frame.f_code.co_name}
Line Number: {e.__traceback__.tb_lineno}"""
        print(error_message)
        rollback_manager.rollback()
        sys.exit(1)

if __name__ == "__main__":
    main()
