import os
import sys
import pandas as pd
import random
import shutil
import csv
from datetime import timedelta, datetime

# Define directories
INPUT_DIR = "C:\\Users\\JCox\\Desktop\\AUTOMATION\\RAC\\PREPIF\\FOLDERS\\OUTPUT"
PROOF_DIR = "C:\\Users\\JCox\\Desktop\\AUTOMATION\\RAC\\PREPIF\\FOLDERS\\PROOF"

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

def format_large_numbers(x):
    if isinstance(x, (int, float)):
        return f"{x:.0f}"
    return x

def main():
    rollback_manager = ProcessingRollback()
    file_name = "PRE_PIF.csv"
    
    try:
        print("Starting PREPIF processing...")
        
        # Set pandas display options
        pd.set_option('display.float_format', lambda x: '%.0f' % x)
        
        # Create directories if they don't exist
        for directory in [INPUT_DIR, PROOF_DIR]:
            if not os.path.exists(directory):
                os.makedirs(directory)

        # Backup existing directories
        rollback_manager.backup(INPUT_DIR)
        rollback_manager.backup(PROOF_DIR)
        
        # Load and process data
        print(f"Processing {file_name}...")
        df = pd.read_csv(os.path.join(INPUT_DIR, file_name), encoding='utf-8')
        
        # Date processing
        df['BEGIN DATE'] = pd.to_datetime(df['BEGIN DATE']).dt.strftime('%m/%d/%Y')
        df['END DATE'] = pd.to_datetime(df['BEGIN DATE'], format='%m/%d/%Y') + timedelta(days=54)
        df['END DATE'] = df['END DATE'].dt.strftime('%m/%d/%Y')
        
        # Process PR records
        print("Processing PR records...")
        df_pr = df[df['Creative_Version_Cd'].str.contains('PR', na=False)].copy()
        df_pr['END DATE'] = pd.to_datetime(df_pr['END DATE'], format='%m/%d/%Y').dt.strftime('%d/%m/%Y')
        
        # Save PR files
        pr_output = os.path.join(INPUT_DIR, file_name.replace('.csv', '-PR.csv'))
        pr_proof = os.path.join(PROOF_DIR, file_name.replace('.csv', '-PR-PD.csv'))
        
        df_pr.to_csv(pr_output, index=False, encoding='utf-8', float_format='%.0f', quoting=csv.QUOTE_ALL)
        df_pr_proof = df_pr[:15] if len(df_pr) > 15 else df_pr
        df_pr_proof.to_csv(pr_proof, index=False, encoding='utf-8', float_format='%.0f', quoting=csv.QUOTE_ALL)
        
        rollback_manager.track_file(pr_output)
        rollback_manager.track_file(pr_proof)
        
        # Process US records
        print("Processing US records...")
        df = df[~df.index.isin(df_pr.index)]
        us_output = os.path.join(INPUT_DIR, file_name.replace('.csv', '-US.csv'))
        df.to_csv(us_output, index=False, encoding='utf-8', float_format='%.0f', quoting=csv.QUOTE_ALL)
        rollback_manager.track_file(us_output)
        
        # Create proof subset with license requirements
        subset_with_license = df[df['Store_License'].notna()].sample(min(1, len(df[df['Store_License'].notna()])))
        remaining_records_needed = 15 - len(subset_with_license)
        subset_without_license = df[df['Store_License'].isna()].sample(min(remaining_records_needed, len(df[df['Store_License'].isna()])))
        final_subset = pd.concat([subset_with_license, subset_without_license])
        
        us_proof = os.path.join(PROOF_DIR, file_name.replace('.csv', '-US-PD.csv'))
        final_subset.to_csv(us_proof, index=False, encoding='utf-8', float_format='%.0f', quoting=csv.QUOTE_ALL)
        rollback_manager.track_file(us_proof)
        
        # Clean up backup after successful processing
        rollback_manager.cleanup()
        print("PREPIF processing completed successfully!")
            
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