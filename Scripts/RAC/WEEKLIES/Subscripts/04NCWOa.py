import os
import sys
import pandas as pd
import random
import argparse

def fix_spanish_chars(text):
    """Fix Spanish characters in text by replacing uppercase accented letters with lowercase equivalents."""
    if not isinstance(text, str):
        return text
    replacements = {'Á': 'á', 'É': 'é', 'Í': 'í', 'Ó': 'ó', 'Ú': 'ú', 'Ñ': 'ñ'}
    words = text.split()
    fixed_words = []
    for word in words:
        for char, replacement in replacements.items():
            if char in word[1:]:
                word = word.replace(char, replacement)
        fixed_words.append(word)
    return ' '.join(fixed_words)

def clean_text_encoding(text):
    """Clean text encoding by converting to latin1 and back, ignoring errors."""
    if isinstance(text, str):
        return text.encode('latin1', errors='ignore').decode('latin1')
    return text

def clean_dataframe_encoding(df):
    """Apply text encoding cleanup and Spanish character fixes to all object columns in the dataframe."""
    for column in df.select_dtypes(include=['object']).columns:
        df[column] = df[column].apply(clean_text_encoding)
        df[column] = df[column].apply(fix_spanish_chars)
    return df

def sample_records(df, sample_size, condition_column=None):
    """Sample records from the dataframe, prioritizing rows with non-null condition_column if provided."""
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
    """Format 'TOTAL' and 'WEEKLY' columns as currency with commas and two decimal places."""
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
    """Replace slashes in 'START_DATE' and 'END_DATE' columns with ' de '."""
    if 'START_DATE' in df.columns and 'END_DATE' in df.columns:
        df['START_DATE'] = df['START_DATE'].str.replace('/', ' de ')
        df['END_DATE'] = df['END_DATE'].str.replace('/', ' de ')
    return df

def prepare_base_dataframe(file_path):
    """Read CSV file and prepare dataframe by uppercasing 'First Name' and cleaning encoding."""
    df = pd.read_csv(file_path, quotechar='"', encoding='utf-8')
    df['FIRST_NAME'] = df['First Name'].str.upper()
    df = clean_dataframe_encoding(df)
    return df

def get_encoding(file_path):
    """Determine encoding: 'cp1252' for PR/APPR files, 'utf-8' otherwise."""
    base_name = os.path.basename(file_path)
    if 'PR' in base_name or 'APPR' in base_name:
        return 'cp1252'
    return 'utf-8'

def process_a_type(file_prefix, df, input_dir, proof_dir):
    """Process A-type files, generating output and proof CSVs."""
    df = clean_dataframe_encoding(df)
    df = df.replace(['nan', 'NAN', '$nan'], pd.NA)
    df_a = df[df['VERSION'] == f'{file_prefix}-A'].sort_values(by='Sort Position')
    df_pr = df[df['VERSION'] == f'{file_prefix}-PR'].sort_values(by='Sort Position')
    
    df_pr = replace_date_slashes(df_pr)
    
    a_output = os.path.join(input_dir, f'{file_prefix}-A.csv')
    pr_output = os.path.join(input_dir, f'{file_prefix}-PR.csv')
    df_a.to_csv(a_output, index=False, quotechar='"', encoding=get_encoding(a_output))
    df_pr.to_csv(pr_output, index=False, quotechar='"', encoding=get_encoding(pr_output))
    
    critical_columns = [
        'Full Name', 'Title', 'Address Line 1', 'City State ZIP Code',
        'Store_AddressLine_1', 'Store_City', 'Store_Phone_Number',
        'VERSION', 'START_DATE', 'END_DATE', 'Creative_Version_Cd',
        'Individual_Id', 'CUSTOM_05', 'First Name', 'FIRST_NAME'
    ]
    critical_columns = [col for col in critical_columns if col in df.columns]
    
    if len(df_a) < 15:
        sample_a = df_a
    else:
        df_a_complete = df_a.dropna(subset=critical_columns)
        sample_a = sample_records(df_a_complete, 15, 'Store_License')
    
    if len(df_pr) < 15:
        sample_pr = df_pr
    else:
        df_pr_complete = df_pr.dropna(subset=critical_columns)
        sample_pr = sample_records(df_pr_complete, 15)
    
    sample_a = sample_a.fillna('')
    sample_pr = sample_pr.fillna('')
    
    a_proof = os.path.join(proof_dir, f'{file_prefix}-A-PD.csv')
    pr_proof = os.path.join(proof_dir, f'{file_prefix}-PR-PD.csv')
    sample_a.to_csv(a_proof, index=False, quotechar='"', encoding=get_encoding(a_proof))
    sample_pr.to_csv(pr_proof, index=False, quotechar='"', encoding=get_encoding(pr_proof))

def process_ap_type(file_prefix, df, input_dir, proof_dir):
    """Process AP-type files, generating output and proof CSVs with currency formatting."""
    df = clean_dataframe_encoding(df)
    df = format_currency(df)
    df = df.replace(['nan', 'NAN', '$nan'], pd.NA)
    
    critical_columns = [
        'Full Name', 'Title', 'Address Line 1', 'City State ZIP Code',
        'Store_AddressLine_1', 'Store_City', 'Store_Phone_Number',
        'VERSION', 'START_DATE', 'END_DATE', 'Creative_Version_Cd',
        'Individual_Id', 'TOTAL', 'WEEKLY', 'First Name'
    ]
    critical_columns = [col for col in critical_columns if col in df.columns]
    
    df_ap = df[df['VERSION'] == f'{file_prefix}-AP'].sort_values(by='Sort Position')
    df_appr = df[df['VERSION'] == f'{file_prefix}-APPR'].sort_values(by='Sort Position')
    
    df_appr = replace_date_slashes(df_appr)
    
    ap_output = os.path.join(input_dir, f'{file_prefix}-AP.csv')
    appr_output = os.path.join(input_dir, f'{file_prefix}-APPR.csv')
    df_ap.to_csv(ap_output, index=False, quotechar='"', encoding=get_encoding(ap_output))
    df_appr.to_csv(appr_output, index=False, quotechar='"', encoding=get_encoding(appr_output))
    
    if len(df_ap) < 15:
        sample_ap = df_ap
    else:
        df_ap_complete = df_ap.dropna(subset=critical_columns)
        sample_ap = sample_records(df_ap_complete, 15, 'Store_License')
    
    if len(df_appr) < 15:
        sample_appr = df_appr
    else:
        df_appr_complete = df_appr.dropna(subset=critical_columns)
        sample_appr = sample_records(df_appr_complete, 15)
    
    sample_ap = sample_ap.fillna('')
    sample_appr = sample_appr.fillna('')
    
    ap_proof = os.path.join(proof_dir, f'{file_prefix}-AP-PD.csv')
    appr_proof = os.path.join(proof_dir, f'{file_prefix}-APPR-PD.csv')
    sample_ap.to_csv(ap_proof, index=False, quotechar='"', encoding=get_encoding(ap_proof))
    sample_appr.to_csv(appr_proof, index=False, quotechar='"', encoding=get_encoding(appr_proof))

def process_all_files(input_dir, proof_dir):
    """Process all specified input files, calling appropriate processing functions."""
    file_configs = [
        ('1-A_OUTPUT.csv', '1', 'a'),
        ('1-AP_OUTPUT.csv', '1', 'ap'),
        ('2-A_OUTPUT.csv', '2', 'a'),
        ('2-AP_OUTPUT.csv', '2', 'ap')
    ]
    
    for file_name, prefix, file_type in file_configs:
        input_file_path = os.path.join(input_dir, file_name)
        if not os.path.exists(input_file_path):
            print(f"Input file not found: {input_file_path}", file=sys.stderr)
            sys.exit(1)
        
        df = prepare_base_dataframe(input_file_path)
        
        if file_type == 'a':
            process_a_type(prefix, df, input_dir, proof_dir)
        else:
            process_ap_type(prefix, df, input_dir, proof_dir)

def main():
    """Main function to parse arguments and execute file processing."""
    parser = argparse.ArgumentParser(description="Process NCWO job files")
    parser.add_argument("--base_path", required=True, help="Base directory path (e.g., C:\\Goji\\RAC)")
    parser.add_argument("--job_num", required=True, help="Job number")
    parser.add_argument("--week", required=True, help="Week in MM.DD format")
    args = parser.parse_args()

    input_dir = os.path.join(args.base_path, "NCWO", "JOB", "OUTPUT")
    proof_dir = os.path.join(args.base_path, "NCWO", "JOB", "PROOF")

    if not os.path.exists(input_dir):
        print(f"Input directory not found: {input_dir}", file=sys.stderr)
        sys.exit(1)
    if not os.path.exists(proof_dir):
        os.makedirs(proof_dir)

    process_all_files(input_dir, proof_dir)
    print("Processing completed successfully")

if __name__ == "__main__":
    main()