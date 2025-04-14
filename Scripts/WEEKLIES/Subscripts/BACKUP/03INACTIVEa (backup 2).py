import os
import sys
import pandas as pd
import random
import shutil
from datetime import datetime

# Define your directories
INPUT_DIR = "C:\\Users\\JCox\\Desktop\\AUTOMATION\\RAC\\INACTIVE_2310-DM07\\FOLDERS\\OUTPUT"
PROOF_DIR = "C:\\Users\\JCox\\Desktop\\AUTOMATION\\RAC\\INACTIVE_2310-DM07\\FOLDERS\\PROOF"

class ProcessingRollback:
    def __init__(self):
        self.backup_dir = os.path.join(os.path.dirname(INPUT_DIR), 'backup_' + datetime.now().strftime('%Y%m%d_%H%M%S'))
        self.modified_files = []
        
    def backup(self, directory):
        if os.path.exists(directory):
            shutil.copytree(directory, os.path.join(self.backup_dir, os.path.basename(directory)))
    
    def track_file(self, filepath):
        self.modified_files.append(filepath)
    
    def rollback(self):
        print("\nInitiating rollback due to critical error...")
        for file in self.modified_files:
            if os.path.exists(file):
                os.remove(file)
        
        for dir_name in ['OUTPUT', 'PROOF']:
            backup_path = os.path.join(self.backup_dir, dir_name)
            target_path = os.path.join(os.path.dirname(INPUT_DIR), 'FOLDERS', dir_name)
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
        return f'{float(str(value).replace(",", "")):,.2f}'
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

def save_version_rows(df, input_dir, proof_dir, file_type, rollback_manager):
    df_version = df[df['VERSION'].notna()].copy()
    output_file = os.path.join(input_dir, f"FZA{file_type}.csv")
    df_version.to_csv(output_file, index=False, encoding='latin1')
    rollback_manager.track_file(output_file)
    
    sample_df = get_random_sample(df_version, 15)
    sample_output_file = os.path.join(proof_dir, f"FZA{file_type}_PD.csv")
    sample_df.to_csv(sample_output_file, index=False, encoding='latin1')
    rollback_manager.track_file(sample_output_file)

    return df[~df.index.isin(df_version.index)]

def process_file(file_name, rollback_manager):
    input_file_path = os.path.join(INPUT_DIR, file_name)
    df = pd.read_csv(input_file_path, encoding='latin1')
    
    print(f"\nProcessing {file_name}")
    print(f"Total records in file: {len(df)}")
    
    # Display unique values in Creative_Version_Cd
    print("\nUnique values in Creative_Version_Cd:")
    print(df['Creative_Version_Cd'].unique())
    
    # Display a sample of records with their Creative_Version_Cd
    print("\nSample of records with Creative_Version_Cd:")
    print(df[['Creative_Version_Cd']].head(10))
    
    file_type = 'PU' if 'PU' in file_name else 'PO'

    # PU-specific processing
    if file_type == 'PU':
        df['First Name'] = df['First Name'].str.upper()
        df['CREDIT'] = df['CREDIT'].apply(format_credit)

    # Process PR records - add debugging
    print("\nChecking for PR records...")
    pr_mask = df['Creative_Version_Cd'].str.contains('PR', na=False)
    print(f"Records with PR in Creative_Version_Cd: {pr_mask.sum()}")
    
    df_pr = df[pr_mask].copy()
    print(f"PR DataFrame shape: {df_pr.shape}")
    
    if not df_pr.empty:
        print("Processing PR records...")
        df_pr['MESSAGE'] = "La oferta es válida hasta el "
        df_pr['EXPDATE'] = df_pr['EXPIRES'].str[29:]
        df_pr['EXPIRES'] = df_pr['MESSAGE'] + df_pr['EXPDATE']
        df_pr['SZIP'] = df_pr['SZIP'].apply(format_szip)

        # Save PR files
        pr_output_file = os.path.join(INPUT_DIR, file_name.replace('A', 'PR'))
        pr_proof_file = os.path.join(PROOF_DIR, file_name.replace('A', 'PD-PR'))
        
        print(f"Saving PR file to: {pr_output_file}")
        df_pr.to_csv(pr_output_file, index=False, encoding='latin1')
        rollback_manager.track_file(pr_output_file)
        
        print(f"Saving PR proof file to: {pr_proof_file}")
        pr_sample = get_random_sample(df_pr, 15)
        pr_sample.to_csv(pr_proof_file, index=False, encoding='latin1')
        rollback_manager.track_file(pr_proof_file)
    else:
        print("No PR records found in this file")

    # Remove PR rows and process VERSION rows
    df = df[~df.index.isin(df_pr.index)]
    df = save_version_rows(df, INPUT_DIR, PROOF_DIR, file_type, rollback_manager)

    # Create sample for remaining records
    if not df.empty:
        random_df = get_random_sample(df, 15)
        proof_file = os.path.join(PROOF_DIR, file_name.replace('.csv', '-PD.csv'))
        random_df.to_csv(proof_file, index=False, encoding='latin1')
        rollback_manager.track_file(proof_file)

    # Save final DataFrame
    final_output = os.path.join(INPUT_DIR, file_name)
    df.sort_values('Sort Position', ascending=True).to_csv(
        final_output, index=False, encoding='latin1')
    rollback_manager.track_file(final_output)

def main():
    rollback_manager = ProcessingRollback()
    
    try:
        # Create directories if they don't exist
        for directory in [INPUT_DIR, PROOF_DIR]:
            if not os.path.exists(directory):
                os.makedirs(directory)

        # Backup existing directories
        rollback_manager.backup(INPUT_DIR)
        rollback_manager.backup(PROOF_DIR)
        
        # Process each file
        for file_name in ["A-PO.csv", "A-PU.csv"]:
            process_file(file_name, rollback_manager)
        
        # Clean up backup after successful processing
        rollback_manager.cleanup()
            
    except Exception as e:
        error_message = f"""
CRITICAL ERROR DETECTED:
Type: {type(e).__name__}
Details: {str(e)}
Location: {e.__traceback__.tb_frame.f_code.co_name}
Line Number: {e.__traceback__.tb_lineno}
"""
        print(error_message)
        rollback_manager.rollback()
        sys.exit(1)

if __name__ == "__main__":
    main()
