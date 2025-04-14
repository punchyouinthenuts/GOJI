import os
import sys
import zipfile
import shutil
import glob
import pandas as pd
import re
from pathlib import Path
from datetime import datetime
from contextlib import contextmanager
import chardet  # Added for encoding detection

class TransactionManager:
    def __init__(self):
        self.operations = []
        self.log_dir = "transaction_logs"
        os.makedirs(self.log_dir, exist_ok=True)
        self.log_file = os.path.join(self.log_dir, f"transaction_log_{datetime.now().strftime('%Y%m%d_%H%M%S')}.txt")
        self.backup_dir = os.path.join(self.log_dir, "backups")
        os.makedirs(self.backup_dir, exist_ok=True)

    def create_backup(self, file_path):
        if os.path.exists(file_path):
            backup_path = os.path.join(self.backup_dir, f"{os.path.basename(file_path)}.bak")
            shutil.copy2(file_path, backup_path)
            return backup_path
        return None

    def log_operation(self, op_type, original_state, new_state=None):
        backup_path = None
        if op_type in ['modify', 'delete']:
            backup_path = self.create_backup(original_state)
        
        operation = {
            'type': op_type,
            'original': original_state,
            'new': new_state,
            'backup': backup_path,
            'timestamp': datetime.now()
        }
        self.operations.append(operation)
        self._write_to_log(operation)

    def _write_to_log(self, operation):
        with open(self.log_file, 'a') as f:
            f.write(f"{operation['timestamp']}: {operation['type']} - {operation['original']} -> {operation['new']}\n")

    def rollback(self):
        print("\nInitiating rollback of all operations...")
        
        for operation in reversed(self.operations):
            try:
                if operation['type'] == 'rename':
                    if os.path.exists(operation['new']):
                        os.rename(operation['new'], operation['original'])
                elif operation['type'] == 'move':
                    if os.path.exists(operation['new']):
                        os.makedirs(os.path.dirname(operation['original']), exist_ok=True)
                        shutil.move(operation['new'], operation['original'])
                elif operation['type'] == 'create':
                    if os.path.exists(operation['original']):
                        os.remove(operation['original'])
                elif operation['type'] == 'create_dir':
                    if os.path.exists(operation['original']):
                        shutil.rmtree(operation['original'])
                elif operation['type'] == 'modify':
                    if operation['backup'] and os.path.exists(operation['backup']):
                        shutil.copy2(operation['backup'], operation['original'])
                elif operation['type'] == 'delete':
                    if operation['backup'] and os.path.exists(operation['backup']):
                        os.makedirs(os.path.dirname(operation['original']), exist_ok=True)
                        shutil.copy2(operation['backup'], operation['original'])
                
                print(f"Reverted: {operation['type']} operation on {os.path.basename(operation['original'])}")
                
            except Exception as e:
                print(f"Warning: Rollback operation failed - {str(e)}")
        
        print("Rollback completed")
        if os.path.exists(self.backup_dir):
            shutil.rmtree(self.backup_dir)

@contextmanager
def transaction_scope():
    transaction = TransactionManager()
    try:
        yield transaction
    except Exception as e:
        print(f"\nError occurred: {str(e)}")
        transaction.rollback()
        raise
    finally:
        if os.path.exists(transaction.log_file):
            os.rename(transaction.log_file, transaction.log_file.replace('.txt', '_completed.txt'))

# Function to standardize encoding to UTF-8
def standardize_encoding(file_path):
    """Detects the encoding of a file and converts it to UTF-8 if necessary."""
    with open(file_path, 'rb') as f:
        raw_data = f.read()
        result = chardet.detect(raw_data)
        encoding = result['encoding']
        print(f"Detected encoding for {file_path}: {encoding}, confidence: {result['confidence']}")
    
    # Skip conversion if encoding is None, ascii, or utf-8 (all UTF-8 compatible)
    if not encoding or encoding.lower() in ('ascii', 'utf-8'):
        print(f"No conversion needed for {file_path} (compatible with UTF-8)")
        return
    
    try:
        content = raw_data.decode(encoding, errors='replace')
        with open(file_path, 'w', encoding='utf-8', newline='\n') as f:
            f.write(content)
        print(f"Converted {file_path} from {encoding} to UTF-8")
    except Exception as e:
        print(f"Error converting {file_path}: {str(e)}")

def validate_and_fix_extensions(zip_source_path, transaction):
    for filename in os.listdir(zip_source_path):
        file_path = os.path.join(zip_source_path, filename)
        if os.path.isfile(file_path):
            _, ext = os.path.splitext(filename)
            if not ext:
                new_path = file_path + '.txt'
                transaction.log_operation('rename', file_path, new_path)
                os.rename(file_path, new_path)
                print(f"Added .txt extension to: {filename}")
            elif ext.lower() != '.txt':
                print(f"FILE {filename} IS NOT A TXT FILE! SCRIPT WILL TERMINATE, PRESS ANY KEY TO CONTINUE...")
                input()
                sys.exit(1)

def process_zip_files(transaction):
    zip_source_path = r'C:\Users\JCox\Desktop\AUTOMATION\RAC\WEEKLY\INPUTZIP'
    base_path = r'C:\Users\JCox\Desktop\AUTOMATION\RAC\WEEKLY'
    filebox_path = os.path.join(base_path, 'FILEBOX')
    macosx_path = os.path.join(filebox_path, '__MACOSX')

    destinations = {
        '202404': r'C:\Users\JCox\Desktop\AUTOMATION\RAC\CBC\JOB\INPUT',
        '201209': r'C:\Users\JCox\Desktop\AUTOMATION\RAC\CBC\JOB\INPUT',
        '202406': r'C:\Users\JCox\Desktop\AUTOMATION\RAC\EXC\JOB\INPUT',
        '201504': r'C:\Users\JCox\Desktop\AUTOMATION\RAC\INACTIVE_2310-DM07\FOLDERS\INPUT',
        '201903': r'C:\Users\JCox\Desktop\AUTOMATION\RAC\NCWO_4TH\DM03\INPUT',
        '202303': r'C:\Users\JCox\Desktop\AUTOMATION\RAC\PREPIF\FOLDERS\INPUT'
    }
    
    os.makedirs(filebox_path, exist_ok=True)
    transaction.log_operation('create_dir', filebox_path)

    zip_files = glob.glob(os.path.join(zip_source_path, '*.zip'))
    if not zip_files:
        raise Exception("No ZIP files found in source directory")

    for zip_file_path in zip_files:
        with zipfile.ZipFile(zip_file_path, 'r') as zip_ref:
            zip_ref.extractall(filebox_path)
            transaction.log_operation('extract', zip_file_path, filebox_path)
            print(f"Extracted files from {zip_file_path}: {zip_ref.namelist()}")

    if os.path.exists(macosx_path):
        shutil.rmtree(macosx_path)

    # Debug and process only valid TXT files
    for root, _, files in os.walk(filebox_path):
        for file in files:
            if file.endswith('.txt') and '__MACOSX' not in root:
                file_path = os.path.join(root, file)
                try:
                    with open(file_path, 'r', encoding='utf-8') as f:
                        lines = f.readlines()
                        print(f"Post-extraction {file}: {len(lines)} lines, first data row = {repr(lines[1]) if len(lines) > 1 else 'N/A'}")
                except UnicodeDecodeError as e:
                    print(f"Skipping {file} due to decode error: {str(e)}")
                    continue

    # Standardize encoding with error handling and post-check
    for root, _, files in os.walk(filebox_path):
        for file in files:
            if file.endswith('.txt') and '__MACOSX' not in root:
                file_path = os.path.join(root, file)
                try:
                    standardize_encoding(file_path)
                    with open(file_path, 'r', encoding='utf-8') as f:
                        lines = f.readlines()
                        print(f"Post-standardization {file}: {len(lines)} lines, first data row = {repr(lines[1]) if len(lines) > 1 else 'N/A'}")
                except Exception as e:
                    print(f"Error processing {file} in standardize_encoding: {str(e)}")
                    continue

    # Move files to destinations
    for root, _, files in os.walk(filebox_path):
        for file in files:
            if file.endswith('.txt') and '__MACOSX' not in root:
                file_path = os.path.join(root, file)
                for pattern, dest in destinations.items():
                    if pattern in file:
                        os.makedirs(dest, exist_ok=True)
                        new_path = os.path.join(dest, file)
                        shutil.move(file_path, new_path)
                        transaction.log_operation('move', file_path, new_path)

    shutil.rmtree(filebox_path)
    transaction.log_operation('delete_dir', filebox_path)
    
    for zip_file in zip_files:
        os.remove(zip_file)
        transaction.log_operation('delete', zip_file)

def process_cbc_input(transaction):
    input_directory = r'C:\Users\JCox\Desktop\AUTOMATION\RAC\CBC\JOB\INPUT'
    versions = ['RAC2401-DM03-A', 'RAC2401-DM03-CANC', 'RAC2401-DM03-PR', 
                'RAC2404-DM07-CBC2-A', 'RAC2404-DM07-CBC2-PR', 'RAC2404-DM07-CBC2-CANC']

    for file_name in os.listdir(input_directory):
        if file_name.endswith('.txt'):
            file_path = os.path.join(input_directory, file_name)
            transaction.log_operation('modify', file_path)
            
            df = pd.read_csv(file_path, sep='\t', encoding='utf-8')
            
            for version in versions:
                filtered_df = df[df.iloc[:, 6] == version]
                output_file = os.path.join(input_directory, f"{version}.csv")
                file_exists = os.path.isfile(output_file)
                
                if not file_exists:
                    transaction.log_operation('create', output_file)
                else:
                    transaction.log_operation('modify', output_file)
                
                filtered_df.to_csv(output_file, mode='a', index=False, header=not file_exists, encoding='utf-8')

def process_exc_input(transaction):
    input_path = r'C:\Users\JCox\Desktop\AUTOMATION\RAC\EXC\JOB\INPUT'
    output_file = os.path.join(input_path, 'EXC.txt')
    
    txt_files = glob.glob(os.path.join(input_path, '*.txt'))
    headers = None
    all_data = []

    for file in txt_files:
        transaction.log_operation('modify', file)
        with open(file, 'r', encoding='utf-8') as f:
            lines = f.readlines()
            if not lines:
                continue
            if headers is None:
                headers = lines[0]
                all_data.extend(lines[1:])
            else:
                all_data.extend(lines[1:])

    transaction.log_operation('create', output_file)
    with open(output_file, 'w', encoding='utf-8') as f:
        f.write(headers)
        f.writelines(all_data)

    for file in txt_files:
        if os.path.basename(file) != 'EXC.txt':
            transaction.log_operation('delete', file)
            os.remove(file)

def process_inactive_input(transaction):
    input_dir = r'C:\Users\JCox\Desktop\AUTOMATION\RAC\INACTIVE_2310-DM07\FOLDERS\INPUT'
    files = [f for f in os.listdir(input_dir) if f.endswith('.txt')]

    if len(files) != 2:
        raise Exception(f"Expected exactly 2 TXT files in {input_dir}, found {len(files)}")

    expected_columns = 36  # Based on typical header

    for file in files:
        file_path = os.path.join(input_dir, file)
        with open(file_path, 'r', encoding='utf-8') as f:
            header = [col.strip('"') for col in f.readline().strip().split('\t')]  # Strip quotes
            raw_line = f.readline()
            first_row = raw_line.strip().split('\t')
        
        print(f"Processing {file}: Header length = {len(header)}")
        print(f"Header = {header}")
        print(f"Raw first row = {repr(raw_line)}")
        print(f"Split first row length = {len(first_row)}, values = {first_row}")
        
        try:
            index = header.index('Creative_Version_Cd')
        except ValueError:
            raise Exception(f"'Creative_Version_Cd' not found in header of {file}")
        
        if len(first_row) < index + 1:
            raise Exception(f"Invalid data in {file}: First row has {len(first_row)} columns, need at least {index + 1} to reach 'Creative_Version_Cd' at index {index}")
        if len(first_row) != expected_columns:
            print(f"Warning: {file} first row has {len(first_row)} columns, expected {expected_columns}")
        
        creative_version = first_row[index].strip('"')  # Strip quotes from value
        if creative_version.endswith('-PU'):
            new_name = 'APU.txt'
        elif creative_version.endswith('-PO'):
            new_name = 'APO.txt'
        else:
            raise Exception(f"Unexpected suffix in {file}: {creative_version}")
        
        new_path = os.path.join(input_dir, new_name)
        if os.path.exists(new_path):
            os.remove(new_path)
        transaction.log_operation('rename', file_path, new_path)
        os.rename(file_path, new_path)

def process_prepif_input(transaction):
    input_dir = r'C:\Users\JCox\Desktop\AUTOMATION\RAC\PREPIF\FOLDERS\INPUT'
    rename_rules = {
        'Pre-PIF_Prod_ALL_Files_HHG': 'PREPIF.txt',
        'DM001': 'PIF.txt'
    }

    for filename in os.listdir(input_dir):
        if filename.endswith('.txt'):
            file_path = os.path.join(input_dir, filename)
            for pattern, new_name in rename_rules.items():
                if pattern in filename:
                    new_path = os.path.join(input_dir, new_name)
                    if os.path.exists(new_path):
                        os.remove(new_path)
                    transaction.log_operation('rename', file_path, new_path)
                    os.rename(file_path, new_path)
                    break

def process_ncwo_input(transaction):
    input_dir = r'C:\Users\JCox\Desktop\AUTOMATION\RAC\NCWO_4TH\DM03\INPUT'
    output_file = os.path.join(input_dir, 'ALLINPUT.csv')
    
    column_names = [
        "Campaign_Cd", "Campaign_Name", "Campaign_Type_Cd", "Cell_Cd", "Cell_Name",
        "Channel_Cd", "Creative_Version_Cd", "Campaign_Deployment_Dt", "Individual_Id",
        "First_Name", "Last_Name", "OCR", "AddressLine_1", "AddressLIne_2", "City",
        "State_Cd", "Postal_Cd", "Zip4", "Store_Id", "Store_AddressLine_1", "Store_AddressLine_2",
        "Store_City", "Store_State_Cd", "Store_Postal_Cd", "Store_Phone_Number", "Store_License",
        "DMA_Name", "CUSTOM_01", "CUSTOM_02", "CUSTOM_03", "CUSTOM_04", "CUSTOM_05",
        "CUSTOM_06", "CUSTOM_07", "CUSTOM_08", "CUSTOM_09", "CUSTOM_10"
    ]

    all_data = []
    for file in os.listdir(input_dir):
        if file.endswith(".txt"):
            file_path = os.path.join(input_dir, file)
            transaction.log_operation('modify', file_path)
            df = pd.read_csv(file_path, sep='\t', names=column_names, header=0, low_memory=False, encoding='utf-8')
            all_data.append(df)
    
    if not all_data:
        raise Exception("No input files found for NCWO processing")
        
    combined_df = pd.concat(all_data, ignore_index=True)
    
    def process_custom_04(date_range_str):
        try:
            start_date, end_date = [date.strip() for date in date_range_str.split('-')]
            return start_date, end_date
        except (ValueError, AttributeError):
            return None, None

    combined_df['START_DATE'], combined_df['END_DATE'] = zip(*combined_df['CUSTOM_04'].apply(process_custom_04))
    combined_df['VERSION'] = ''
    
    transaction.log_operation('create', output_file)
    combined_df.to_csv(output_file, index=False, encoding='utf-8')
    
    version_codes = {
        "RAC2504-DM04-NCWO2-APPR": "2-APPR.csv",
        "RAC2504-DM04-NCWO2-PR": "2-PR.csv",
        "RAC2504-DM04-NCWO2-AP": "2-AP.csv",
        "RAC2504-DM04-NCWO2-A": "2-A.csv",
        "RAC2504-DM04-NCWO1-PR": "1-PR.csv",
        "RAC2504-DM04-NCWO1-A": "1-A.csv",
        "RAC2504-DM04-NCWO1-APPR": "1-APPR.csv",
        "RAC2504-DM04-NCWO1-AP": "1-AP.csv"
    }
    
    for version_code, file_name in version_codes.items():
        version_df = combined_df[combined_df['Creative_Version_Cd'] == version_code].copy()
        if not version_df.empty:
            version_df['VERSION'] = file_name.replace('.csv', '')
            output_path = os.path.join(input_dir, file_name)
            transaction.log_operation('create', output_path)
            version_df.to_csv(output_path, index=False, encoding='utf-8')

def main():
    try:
        with transaction_scope() as transaction:
            print("Starting input processing sequence...")
            
            process_zip_files(transaction)
            print("\nProcessing CBC input...")
            process_cbc_input(transaction)
            print("\nProcessing EXC input...")
            process_exc_input(transaction)
            print("\nProcessing INACTIVE input...")
            process_inactive_input(transaction)
            print("\nProcessing NCWO input...")
            process_ncwo_input(transaction)
            print("\nProcessing PREPIF input...")
            process_prepif_input(transaction)
            
            print("\nAll processing completed successfully!")
            
    except Exception as e:
        print("\nScript terminated due to error.")
        input("Press any key to continue...")
        sys.exit(1)

if __name__ == "__main__":
    main()
