import os
import sys
import pandas as pd
import random
import shutil
from datetime import datetime

# Define directories
INPUT_DIR = "C:\\Users\\JCox\\Desktop\\AUTOMATION\\RAC\\NCWO_4TH\\DM03\\OUTPUT"
PROOF_DIR = "C:\\Users\\JCox\\Desktop\\AUTOMATION\\RAC\\NCWO_4TH\\DM03\\PROOF"

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

def fix_spanish_chars(text):
    if not isinstance(text, str):
        return text
        
    replacements = {
        'Á': 'á', 'É': 'é', 'Í': 'í', 'Ó': 'ó', 'Ú': 'ú', 'Ñ': 'ñ'
    }
    
    words = text.split()
    fixed_words = []
    
    for word in words:
        for char, replacement in replacements.items():
            if char in word[1:]:
                word = word.replace(char, replacement)
        fixed_words.append(word)
    
    return ' '.join(fixed_words)

def clean_text_encoding(text):
    if isinstance(text, str):
        return text.encode('latin1', errors='ignore').decode('latin1')
    return text

def clean_dataframe_encoding(df):
    for column in df.select_dtypes(include=['object']).columns:
        df[column] = df[column].apply(clean_text_encoding)
        df[column] = df[column].apply(fix_spanish_chars)
    return df

def sample_records(df, sample_size, condition_column=None):
    if len(df) <= sample_size:
        return df
    elif condition_column:
        condition_df = df[df[condition_column].notna()]
        remaining_df = df[df[condition_column].isna()]
        
        if not condition_df.empty:
            if not remaining_df.empty:
                condition_sample_size = min(1, len(condition_df))
                condition_sample = condition_df.sample(condition_sample_size)
                remaining_sample_size = sample_size - condition_sample_size
                remaining_sample = remaining_df.sample(min(remaining_sample_size, len(remaining_df)))
                return pd.concat([condition_sample, remaining_sample])
            else:
                return condition_df.sample(min(sample_size, len(condition_df)))
        else:
            if not remaining_df.empty:
                return remaining_df.sample(min(sample_size, len(remaining_df)))
            else:
                return pd.DataFrame()
    else:
        return df.sample(sample_size)

def format_currency(df):
    if 'TOTAL' in df.columns:
        df['TOTAL'] = df['TOTAL'].replace('[$]', '', regex=True).astype(str)
        df['TOTAL'] = df['TOTAL'].replace('[,]', '', regex=True).astype(float)
        df['TOTAL'] = df['TOTAL'].apply(lambda x: f"{x:,.2f}")
    if 'WEEKLY' in df.columns:
        df['WEEKLY'] = df['WEEKLY'].replace('[$]', '', regex=True).astype(str)
        df['WEEKLY'] = df['WEEKLY'].replace('[,]', '', regex=True).astype(float)
        df['WEEKLY'] = df['WEEKLY'].apply(lambda x: f"{x:,.2f}")
    return df

def replace_date_slashes(df):
    if 'START_DATE' in df.columns and 'END_DATE' in df.columns:
        df['START_DATE'] = df['START_DATE'].str.replace('/', ' de ')
        df['END_DATE'] = df['END_DATE'].str.replace('/', ' de ')
    return df

def prepare_base_dataframe(file_path):
    df = pd.read_csv(
        file_path,
        quotechar='"',
        encoding='utf-8'
    )
    df['FIRST_NAME'] = df['First Name'].str.upper()
    df = clean_dataframe_encoding(df)
    return df

def process_a_type(file_prefix, df, rollback_manager):
    df = clean_dataframe_encoding(df)
    df = df.replace(['nan', 'NAN', '$nan'], pd.NA)
    df_a = df[df['VERSION'] == f'{file_prefix}-A'].sort_values(by='Sort Position')
    df_pr = df[df['VERSION'] == f'{file_prefix}-PR'].sort_values(by='Sort Position')
    
    df_pr = replace_date_slashes(df_pr)
    
    a_output = os.path.join(INPUT_DIR, f'{file_prefix}-A.csv')
    pr_output = os.path.join(INPUT_DIR, f'{file_prefix}-PR.csv')
    df_a.to_csv(a_output, index=False, quotechar='"', encoding='utf-8')
    df_pr.to_csv(pr_output, index=False, quotechar='"', encoding='utf-8')
    
    rollback_manager.track_file(a_output)
    rollback_manager.track_file(pr_output)
    
    # Define critical columns
    critical_columns = [
        'Full Name', 'Title', 'Address Line 1', 'City State ZIP Code',
        'Store_AddressLine_1', 'Store_City', 'Store_Phone_Number',
        'VERSION', 'START_DATE', 'END_DATE', 'Creative_Version_Cd',
        'Individual_Id', 'CUSTOM_05', 'First Name', 'FIRST_NAME'
    ]
    
    # Filter critical columns to only those present in the dataframe
    critical_columns = [col for col in critical_columns if col in df.columns]
    
    # Sampling logic for PD files
    if len(df_a) < 15:
        sample_a = df_a  # Include all entries, even with blank critical fields
    else:
        df_a_complete = df_a.dropna(subset=critical_columns)
        sample_a = sample_records(df_a_complete, 15, 'Store_License')
    
    if len(df_pr) < 15:
        sample_pr = df_pr  # Include all entries, even with blank critical fields
    else:
        df_pr_complete = df_pr.dropna(subset=critical_columns)
        sample_pr = sample_records(df_pr_complete, 15)
    
    # Replace NaN with blank strings
    sample_a = sample_a.fillna('')
    sample_pr = sample_pr.fillna('')
    
    a_proof = os.path.join(PROOF_DIR, f'{file_prefix}-A-PD.csv')
    pr_proof = os.path.join(PROOF_DIR, f'{file_prefix}-PR-PD.csv')
    sample_a.to_csv(a_proof, index=False, quotechar='"', encoding='utf-8')
    sample_pr.to_csv(pr_proof, index=False, quotechar='"', encoding='utf-8')
    
    rollback_manager.track_file(a_proof)
    rollback_manager.track_file(pr_proof)

def process_ap_type(file_prefix, df, rollback_manager):
    df = clean_dataframe_encoding(df)
    df = format_currency(df)
    df = df.replace(['nan', 'NAN', '$nan'], pd.NA)
    
    # Define critical columns
    critical_columns = [
        'Full Name', 'Title', 'Address Line 1', 'City State ZIP Code',
        'Store_AddressLine_1', 'Store_City', 'Store_Phone_Number',
        'VERSION', 'START_DATE', 'END_DATE', 'Creative_Version_Cd',
        'Individual_Id', 'TOTAL', 'WEEKLY', 'First Name'
    ]
    # Filter to columns present in the dataframe
    critical_columns = [col for col in critical_columns if col in df.columns]
    
    df_ap = df[df['VERSION'] == f'{file_prefix}-AP'].sort_values(by='Sort Position')
    df_appr = df[df['VERSION'] == f'{file_prefix}-APPR'].sort_values(by='Sort Position')
    
    df_appr = replace_date_slashes(df_appr)
    
    ap_output = os.path.join(INPUT_DIR, f'{file_prefix}-AP.csv')
    appr_output = os.path.join(INPUT_DIR, f'{file_prefix}-APPR.csv')
    df_ap.to_csv(ap_output, index=False, quotechar='"', encoding='utf-8')
    df_appr.to_csv(appr_output, index=False, quotechar='"', encoding='utf-8')
    
    rollback_manager.track_file(ap_output)
    rollback_manager.track_file(appr_output)
    
    # Sampling logic for PD files
    if len(df_ap) < 15:
        sample_ap = df_ap  # Include all entries, even with blank critical fields
    else:
        df_ap_complete = df_ap.dropna(subset=critical_columns)
        sample_ap = sample_records(df_ap_complete, 15, 'Store_License')
    
    if len(df_appr) < 15:
        sample_appr = df_appr  # Include all entries, even with blank critical fields
    else:
        df_appr_complete = df_appr.dropna(subset=critical_columns)
        sample_appr = sample_records(df_appr_complete, 15)
    
    # Replace NaN with blank strings
    sample_ap = sample_ap.fillna('')
    sample_appr = sample_appr.fillna('')
    
    ap_proof = os.path.join(PROOF_DIR, f'{file_prefix}-AP-PD.csv')
    appr_proof = os.path.join(PROOF_DIR, f'{file_prefix}-APPR-PD.csv')
    sample_ap.to_csv(ap_proof, index=False, quotechar='"', encoding='utf-8')
    sample_appr.to_csv(appr_proof, index=False, quotechar='"', encoding='utf-8')
    
    rollback_manager.track_file(ap_proof)
    rollback_manager.track_file(appr_proof)
    
def process_all_files(rollback_manager):
    file_configs = [
        ('1-A_OUTPUT.csv', '1', 'a'),
        ('1-AP_OUTPUT.csv', '1', 'ap'),
        ('2-A_OUTPUT.csv', '2', 'a'),
        ('2-AP_OUTPUT.csv', '2', 'ap')
    ]
    
    for file_name, prefix, file_type in file_configs:
        input_file_path = os.path.join(INPUT_DIR, file_name)
        if not os.path.exists(input_file_path):
            raise FileNotFoundError(f"Input file not found: {input_file_path}")
            
        df = prepare_base_dataframe(input_file_path)
        
        if file_type == 'a':
            process_a_type(prefix, df, rollback_manager)
        else:
            process_ap_type(prefix, df, rollback_manager)

def main():
    rollback_manager = ProcessingRollback()
    
    try:
        for directory in [INPUT_DIR, PROOF_DIR]:
            if not os.path.exists(directory):
                os.makedirs(directory)

        rollback_manager.backup(INPUT_DIR)
        rollback_manager.backup(PROOF_DIR)
        
        process_all_files(rollback_manager)
        
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
