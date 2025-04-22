import os
import re
import pandas as pd
from datetime import datetime
import csv
import tempfile
import shutil

# --- Helper Functions ---

def detect_delimiter(file_path):
    """Detect the delimiter of the file by analyzing the first few lines."""
    with open(file_path, 'r', encoding='latin1') as file:
        lines = [file.readline() for _ in range(3)]  # Read first 3 lines
        for line in lines:
            if '\t' in line and ',' not in line:
                return '\t'
            elif ',' in line and '\t' not in line:
                return ','
        # Default to tab for known problematic files if ambiguous
        if os.path.basename(file_path).lower() in ['exc.txt', 'prepif.txt', 'apo.txt', 'apu.txt']:
            return '\t'
        return ','  # Fallback to comma if unclear

def create_quoted_temp_file(original_path):
    """Create a temporary file with appropriate handling for delimiters."""
    delimiter = detect_delimiter(original_path)
    temp_dir = tempfile.mkdtemp()
    temp_path = os.path.join(temp_dir, os.path.basename(original_path))
    
    with open(original_path, 'r', newline='', encoding='latin1') as infile, \
         open(temp_path, 'w', newline='', encoding='latin1') as outfile:
        reader = csv.reader(infile, delimiter=delimiter)
        quoting = csv.QUOTE_MINIMAL if delimiter == '\t' else csv.QUOTE_ALL
        writer = csv.writer(outfile, delimiter=delimiter, quoting=quoting)
        for row in reader:
            writer.writerow(row)
    
    return temp_path, temp_dir

# --- Processing Functions ---

def process_cbc_file(filepath):
    """Process CBC input files and count versions starting with 'RAC'."""
    version_counts = {}
    rac_pattern = r'RAC\d{4}-DM\d{2}.*'
    try:
        with open(filepath, 'r', encoding='latin1', newline='') as csvfile:
            reader = csv.reader(csvfile, delimiter=',', quotechar='"', quoting=csv.QUOTE_ALL)
            next(reader)  # Skip header
            for row in reader:
                if len(row) == 37:
                    version = row[6].strip().upper()  # Creative_Version_Cd at index 6
                    if str(version).startswith('RAC') and re.match(rac_pattern, version):
                        version_counts[version] = version_counts.get(version, 0) + 1
    except Exception as e:
        print(f"Error processing {filepath}: {str(e)}")
    return version_counts

def count_versions_in_input(filepath, folder_type):
    """Count RAC versions in input files based on folder type."""
    version_counts = {}
    rac_pattern = r'RAC\d{4}-DM\d{2}.*'
    temp_path, temp_dir = create_quoted_temp_file(filepath)
    filename = os.path.basename(filepath).lower()
    
    try:
        delimiter = detect_delimiter(filepath)
        
        if filename in ['exc.txt', 'prepif.txt', 'apo.txt', 'apu.txt']:
            df = pd.read_csv(temp_path, delimiter='\t', encoding='latin1', 
                            on_bad_lines='skip', quoting=csv.QUOTE_MINIMAL)
        else:
            df = pd.read_csv(temp_path, sep=delimiter, encoding='latin1', 
                            on_bad_lines='skip', low_memory=False)

        if folder_type == 'CBC' and filepath.lower().endswith('.csv'):
            version_col = 'creative_version'
        elif folder_type == 'INACTIVE' and filename in ['apo.txt', 'apu.txt']:
            version_col = 'Creative_Version_Cd'
        elif folder_type == 'NCWO' and filename == 'allinput.csv':
            version_col = 'Creative_Version_Cd'
        elif folder_type == 'PREPIF' and filename == 'prepif.txt':
            version_col = 'Creative_Version_Cd'
        elif folder_type == 'EXC' and filename == 'exc.txt':
            version_col = 'Creative_Version_Cd'
        else:
            return version_counts
        
        df.columns = df.columns.str.replace('"', '')
        if version_col in df.columns:
            counts = df[version_col].value_counts()
            for version, count in counts.items():
                if pd.notna(version) and str(version).startswith('RAC') and re.match(rac_pattern, str(version)):
                    version_counts[version] = count
    except Exception as e:
        print(f"Error processing input file {filepath}: {str(e)}")
    finally:
        shutil.rmtree(temp_dir)
    return version_counts

def count_versions_in_output():
    """Count RAC versions in output files across specified folders."""
    output_counts = {}
    rac_pattern = r'RAC\d{4}-DM\d{2}.*'
    
    folders = {
        'CBC': r"C:\Users\JCox\Desktop\AUTOMATION\RAC\CBC\JOB\OUTPUT",
        'EXC': r"C:\Users\JCox\Desktop\AUTOMATION\RAC\EXC\JOB\OUTPUT",
        'INACTIVE': r"C:\Users\JCox\Desktop\AUTOMATION\RAC\INACTIVE_2310-DM07\FOLDERS\OUTPUT",
        'NCWO': r"C:\Users\JCox\Desktop\AUTOMATION\RAC\NCWO_4TH\DM03\OUTPUT",
        'PREPIF': r"C:\Users\JCox\Desktop\AUTOMATION\RAC\PREPIF\FOLDERS\OUTPUT"
    }
    
    for folder_type, folder_path in folders.items():
        if folder_type == 'CBC':
            for file in ['CBC2WEEKLYREFORMAT.csv', 'CBC3WEEKLYREFORMAT.csv']:
                file_path = os.path.join(folder_path, file)
                if os.path.exists(file_path):
                    temp_path, temp_dir = create_quoted_temp_file(file_path)
                    try:
                        df = pd.read_csv(temp_path, low_memory=False, encoding='latin1')
                        version_col = 'Creative_Version_Cd'
                        if version_col in df.columns:
                            counts = df[version_col].value_counts()
                            for version, count in counts.items():
                                if str(version).startswith('RAC') and re.match(rac_pattern, str(version)):
                                    output_counts[version] = output_counts.get(version, 0) + count
                    finally:
                        shutil.rmtree(temp_dir)
        elif folder_type == 'EXC':
            file_path = os.path.join(folder_path, 'EXC_OUTPUT.csv')
            if os.path.exists(file_path):
                temp_path, temp_dir = create_quoted_temp_file(file_path)
                try:
                    df = pd.read_csv(temp_path, low_memory=False, encoding='latin1')
                    version_col = df.columns[13]
                    counts = df[version_col].value_counts()
                    for version, count in counts.items():
                        if str(version).startswith('RAC') and re.match(rac_pattern, str(version)):
                            output_counts[version] = count
                finally:
                    shutil.rmtree(temp_dir)
        elif folder_type == 'INACTIVE':
            for file in ['PR-PO.csv', 'PR-PU.csv', 'A-PO.csv', 'A-PU.csv', 'AT-PO.csv', 'AT-PU.csv']:
                file_path = os.path.join(folder_path, file)
                if os.path.exists(file_path):
                    temp_path, temp_dir = create_quoted_temp_file(file_path)
                    try:
                        df = pd.read_csv(temp_path, low_memory=False, encoding='latin1')
                        version_col = df.columns[2]
                        counts = df[version_col].value_counts()
                        for version, count in counts.items():
                            if str(version).startswith('RAC') and re.match(rac_pattern, str(version)):
                                output_counts[version] = count
                    finally:
                        shutil.rmtree(temp_dir)
        elif folder_type == 'NCWO':
            for file in ['1-A_OUTPUT.csv', '1-AP_OUTPUT.csv', '2-A_OUTPUT.csv', '2-AP_OUTPUT.csv']:
                file_path = os.path.join(folder_path, file)
                if os.path.exists(file_path):
                    temp_path, temp_dir = create_quoted_temp_file(file_path)
                    try:
                        df = pd.read_csv(temp_path, low_memory=False, encoding='latin1')
                        version_col = 'Creative_Version_Cd'
                        if version_col in df.columns:
                            counts = df[version_col].value_counts()
                            for version, count in counts.items():
                                if str(version).startswith('RAC') and re.match(rac_pattern, str(version)):
                                    output_counts[version] = output_counts.get(version, 0) + count
                    finally:
                        shutil.rmtree(temp_dir)
        elif folder_type == 'PREPIF':
            file_path = os.path.join(folder_path, 'PRE_PIF.csv')
            if os.path.exists(file_path):
                temp_path, temp_dir = create_quoted_temp_file(file_path)
                try:
                    df = pd.read_csv(temp_path, low_memory=False, encoding='latin1')
                    version_col = df.columns[24]
                    counts = df[version_col].value_counts()
                    for version, count in counts.items():
                        if str(version).startswith('RAC') and re.match(rac_pattern, str(version)):
                            output_counts[version] = count
                finally:
                    shutil.rmtree(temp_dir)
    
    return output_counts

def consolidate_counts(counts_dict):
    """Consolidate version counts into predefined groups with standardized uppercase comparison."""
    consolidated = {
        'CBC2': 0,
        'CBC3': 0,
        'EXC': 0,
        'INACTIVE A-PO': 0,
        'INACTIVE A-PU': 0,
        'INACTIVE AT-PO': 0,
        'INACTIVE AT-PU': 0,
        'NCWO 1-A': 0,
        'NCWO 1-AP': 0,
        'NCWO 2-A': 0,
        'NCWO 2-AP': 0,
        'PREPIF': 0
    }

    for version, count in counts_dict.items():
        version_upper = str(version).upper()
        if version_upper.startswith('RAC2404-DM07'):
            consolidated['CBC2'] += count
        elif version_upper.startswith('RAC2401-DM03'):
            consolidated['CBC3'] += count
        elif 'RACXW' in version_upper:
            consolidated['EXC'] += count
        elif 'PPIF' in version_upper:
            consolidated['PREPIF'] += count
        elif '-A-PO' in version_upper or '-PR-PO' in version_upper:
            consolidated['INACTIVE A-PO'] += count
        elif '-A-PU' in version_upper or '-PR-PU' in version_upper:
            consolidated['INACTIVE A-PU'] += count
        elif '-AT-PO' in version_upper:
            consolidated['INACTIVE AT-PO'] += count
        elif '-AT-PU' in version_upper:
            consolidated['INACTIVE AT-PU'] += count
        elif 'NCWO1-A' in version_upper or 'NCWO1-PR' in version_upper:
            consolidated['NCWO 1-A'] += count
        elif 'NCWO1-AP' in version_upper or 'NCWO1-APPR' in version_upper:
            consolidated['NCWO 1-AP'] += count
        elif 'NCWO2-A' in version_upper or 'NCWO2-PR' in version_upper:
            consolidated['NCWO 2-A'] += count
        elif 'NCWO2-AP' in version_upper or 'NCWO2-APPR' in version_upper:
            consolidated['NCWO 2-AP'] += count
    
    return consolidated

def compare_counts(input_counts, output_counts):
    """Compare input and output counts and display differences in original format."""
    consolidated_input = consolidate_counts(input_counts)
    consolidated_output = consolidate_counts(output_counts)
    
    # Main console output
    print("\nComparison of Counts:")
    print("-" * 70)
    print("Group                          Input       Output      Difference")
    print("-" * 70)
    
    for group in consolidated_input.keys():
        input_count = consolidated_input.get(group, 0)
        output_count = consolidated_output.get(group, 0)
        diff = output_count - input_count
        error_flag = " [ERROR!]" if diff > 0 else ""
        
        print(f"{group:<30} {input_count:>7,d}     {output_count:>7,d}     {diff:>7,d}{error_flag}")

def main():
    """Main function to execute the script."""
    input_folders = {
        'CBC': r"C:\Users\JCox\Desktop\AUTOMATION\RAC\CBC\JOB\INPUT",
        'EXC': r"C:\Users\JCox\Desktop\AUTOMATION\RAC\EXC\JOB\INPUT",
        'INACTIVE': r"C:\Users\JCox\Desktop\AUTOMATION\RAC\INACTIVE_2310-DM07\FOLDERS\INPUT",
        'NCWO': r"C:\Users\JCox\Desktop\AUTOMATION\RAC\NCWO_4TH\DM03\INPUT\ALLINPUT.csv",
        'PREPIF': r"C:\Users\JCox\Desktop\AUTOMATION\RAC\PREPIF\FOLDERS\INPUT"
    }
    
    input_counts = {}
    for folder_type, path in input_folders.items():
        if os.path.isfile(path):  # For NCWO ALLINPUT.csv
            current_counts = count_versions_in_input(path, folder_type)
            for version, count in current_counts.items():
                input_counts[version] = count
        else:  # For directory processing
            for filename in os.listdir(path):
                filepath = os.path.join(path, filename)
                if folder_type == 'CBC' and filepath.lower().endswith('.csv'):
                    current_counts = process_cbc_file(filepath)
                else:
                    current_counts = count_versions_in_input(filepath, folder_type)
                for version, count in current_counts.items():
                    input_counts[version] = input_counts.get(version, 0) + count

    output_counts = count_versions_in_output()
    compare_counts(input_counts, output_counts)

    # COUNTS.csv generation with backup system
    output_folder = r"C:\Users\JCox\Desktop\AUTOMATION\RAC\COUNTS"
    archive_folder = os.path.join(output_folder, "ARCHIVE")
    os.makedirs(output_folder, exist_ok=True)
    os.makedirs(archive_folder, exist_ok=True)

    output_path = os.path.join(output_folder, "COUNTS.csv")
    if os.path.exists(output_path):
        timestamp = datetime.now().strftime("_%Y%m%d_%H%M")
        archived_filename = f"COUNTS{timestamp}.csv"
        archived_path = os.path.join(archive_folder, archived_filename)
        os.rename(output_path, archived_path)

    value_mappings = {
        'RAC2404-DM07-CBC2-PR': 'CBC2 PR',
        'RAC2404-DM07-CBC2-CANC': 'CBC2 CANC',
        'RAC2404-DM07-CBC2-A': 'CBC2 US',
        'RAC2401-DM03-A': 'CBC3 US',
        'RAC2401-DM03-CANC': 'CBC3 CANC',
        'RAC2401-DM03-PR': 'CBC3 PR',
        'RAC2406-DM03-RACXW-A': 'EXC US',
        'RAC2406-DM03-RACXW-PR': 'EXC PR',
        'RAC2504-DM03-A-PU': 'INACTIVE A-PU US',
        'RAC2501-DM06-PR-PU': 'INACTIVE A-PU PR',
        'RAC2504-DM03-AT-PU': 'INACTIVE A-PU US',  # Consolidated with INACTIVE A-PU US
        'RAC2504-DM03-A-PO': 'INACTIVE A-PO US',
        'RAC2504-DM03-PR-PO': 'INACTIVE A-PO PR',
        'RAC2504-DM03-AT-PO': 'INACTIVE A-PO US',  # Consolidated with INACTIVE A-PO US
        'RAC2504-DM05-PPIF-A': 'PREPIF US',
        'RAC2504-DM05-PPIF-PR': 'PREPIF PR',
        'RAC2504-DM04-NCWO1-A': 'NCWO 1-A US',
        'RAC2504-DM04-NCWO1-PR': 'NCWO 1-A PR',
        'RAC2504-DM04-NCWO1-AP': 'NCWO 1-AP US',
        'RAC2504-DM04-NCWO1-APPR': 'NCWO 1-AP PR',
        'RAC2504-DM04-NCWO2-A': 'NCWO 2-A US',
        'RAC2504-DM04-NCWO2-PR': 'NCWO 2-A PR',
        'RAC2504-DM04-NCWO2-AP': 'NCWO 2-AP US',
        'RAC2504-DM04-NCWO2-APPR': 'NCWO 2-AP PR'
    }
    
    sort_orders = {
        'CBC2': ['CBC2 PR', 'CBC2 CANC', 'CBC2 US'],
        'CBC3': ['CBC3 PR', 'CBC3 CANC', 'CBC3 US'],
        'EXC': ['EXC PR', 'EXC US'],
        'INACTIVE': ['INACTIVE A-PO PR', 'INACTIVE A-PO US', 'INACTIVE A-PU PR', 'INACTIVE A-PU US'],  # Consolidated by removing INACTIVE AT-PO US and INACTIVE AT-PU US
        'NCWO': ['NCWO 1-A PR', 'NCWO 1-A US', 'NCWO 1-AP PR', 'NCWO 1-AP US', 'NCWO 2-A PR', 'NCWO 2-A US', 'NCWO 2-AP PR', 'NCWO 2-AP US'],
        'PREPIF': ['PREPIF PR', 'PREPIF US']
    }

    results = []

    # Process CBC files
    cbc_folder = r"C:\Users\JCox\Desktop\AUTOMATION\RAC\CBC\JOB\OUTPUT"
    for file in ['CBC2WEEKLYREFORMAT.csv', 'CBC3WEEKLYREFORMAT.csv']:
        file_path = os.path.join(cbc_folder, file)
        if os.path.exists(file_path):
            temp_path, temp_dir = create_quoted_temp_file(file_path)
            try:
                df = pd.read_csv(temp_path, low_memory=False, encoding='latin1')
                counts = df.iloc[:, 14].value_counts()
                counts_df = counts.reset_index()
                counts_df.columns = ['Value', 'Count']
                counts_df['Value'] = counts_df['Value'].map(value_mappings)
                header_type = 'CBC2' if 'CBC2' in file else 'CBC3'
                for value in sort_orders[header_type]:
                    count = counts_df[counts_df['Value'] == value]['Count'].sum() if value in counts_df['Value'].values else 0
                    results.append({'Value': value, 'Count': count})
            finally:
                shutil.rmtree(temp_dir)

    # Process EXC files
    exc_folder = r"C:\Users\JCox\Desktop\AUTOMATION\RAC\EXC\JOB\OUTPUT"
    file_path = os.path.join(exc_folder, 'EXC_OUTPUT.csv')
    if os.path.exists(file_path):
        temp_path, temp_dir = create_quoted_temp_file(file_path)
        try:
            df = pd.read_csv(temp_path, low_memory=False, encoding='latin1')
            counts = df.iloc[:, 13].value_counts()
            counts_df = counts.reset_index()
            counts_df.columns = ['Value', 'Count']
            counts_df['Value'] = counts_df['Value'].map(value_mappings)
            for value in sort_orders['EXC']:
                count = counts_df[counts_df['Value'] == value]['Count'].sum() if value in counts_df['Value'].values else 0
                results.append({'Value': value, 'Count': count})
        finally:
            shutil.rmtree(temp_dir)

    # Process INACTIVE files
    inactive_folder = r"C:\Users\JCox\Desktop\AUTOMATION\RAC\INACTIVE_2310-DM07\FOLDERS\OUTPUT"
    inactive_dfs = []
    for file in ['PR-PO.csv', 'PR-PU.csv', 'A-PO.csv', 'A-PU.csv', 'AT-PO.csv', 'AT-PU.csv']:
        file_path = os.path.join(inactive_folder, file)
        if os.path.exists(file_path):
            temp_path, temp_dir = create_quoted_temp_file(file_path)
            try:
                df = pd.read_csv(temp_path, low_memory=False, encoding='latin1')
                inactive_dfs.append(df)
            finally:
                shutil.rmtree(temp_dir)

    if inactive_dfs:
        merged_inactive = pd.concat(inactive_dfs)
        counts = merged_inactive['Creative_Version_Cd'].value_counts()
        counts_df = counts.reset_index()
        counts_df.columns = ['Value', 'Count']
        counts_df['Value'] = counts_df['Value'].map(value_mappings)
        
        for value in sort_orders['INACTIVE']:
            count = counts_df[counts_df['Value'] == value]['Count'].sum() if value in counts_df['Value'].values else 0
            results.append({'Value': value, 'Count': count})

    # Process NCWO files
    ncwo_folder = r"C:\Users\JCox\Desktop\AUTOMATION\RAC\NCWO_4TH\DM03\OUTPUT"
    for file in ['1-A_OUTPUT.csv', '1-AP_OUTPUT.csv', '2-A_OUTPUT.csv', '2-AP_OUTPUT.csv']:
        file_path = os.path.join(ncwo_folder, file)
        if os.path.exists(file_path):
            temp_path, temp_dir = create_quoted_temp_file(file_path)
            try:
                df = pd.read_csv(temp_path, low_memory=False, encoding='latin1')
                counts = df['Creative_Version_Cd'].value_counts()
                counts_df = counts.reset_index()
                counts_df.columns = ['Value', 'Count']
                counts_df['Value'] = counts_df['Value'].map(value_mappings)
                for value in sort_orders['NCWO']:
                    if value.split()[1:2][0] == file.split('_')[0]:
                        count = counts_df[counts_df['Value'] == value]['Count'].sum() if value in counts_df['Value'].values else 0
                        results.append({'Value': value, 'Count': count})
            finally:
                shutil.rmtree(temp_dir)

    # Process PREPIF files
    prepif_folder = r"C:\Users\JCox\Desktop\AUTOMATION\RAC\PREPIF\FOLDERS\OUTPUT"
    file_path = os.path.join(prepif_folder, 'PRE_PIF.csv')
    if os.path.exists(file_path):
        temp_path, temp_dir = create_quoted_temp_file(file_path)
        try:
            df = pd.read_csv(temp_path, low_memory=False, encoding='latin1')
            counts = df.iloc[:, 24].value_counts()
            counts_df = counts.reset_index()
            counts_df.columns = ['Value', 'Count']
            counts_df['Value'] = counts_df['Value'].map(value_mappings)
            for value in sort_orders['PREPIF']:
                count = counts_df[counts_df['Value'] == value]['Count'].sum() if value in counts_df['Value'].values else 0
                results.append({'Value': value, 'Count': count})
        finally:
            shutil.rmtree(temp_dir)

    # Create final DataFrame with sections and spacing
    final_results = []

    final_results.append({'Value': '=== CBC2 Counts ===', 'Count': ''})
    final_results.extend([{'Value': value, 'Count': count} for result in results 
                         if result['Value'].startswith('CBC2') 
                         for value, count in [result.values()]])
    final_results.append({'Value': '', 'Count': ''})

    final_results.append({'Value': '=== CBC3 Counts ===', 'Count': ''})
    final_results.extend([{'Value': value, 'Count': count} for result in results 
                         if result['Value'].startswith('CBC3') 
                         for value, count in [result.values()]])
    final_results.append({'Value': '', 'Count': ''})

    final_results.append({'Value': '=== EXC Counts ===', 'Count': ''})
    final_results.extend([{'Value': value, 'Count': count} for result in results 
                         if result['Value'].startswith('EXC') 
                         for value, count in [result.values()]])
    final_results.append({'Value': '', 'Count': ''})

    final_results.append({'Value': '=== INACTIVE Counts ===', 'Count': ''})
    final_results.extend([{'Value': value, 'Count': count} for result in results 
                         if result['Value'].startswith('INACTIVE') 
                         for value, count in [result.values()]])
    final_results.append({'Value': '', 'Count': ''})

    final_results.append({'Value': '=== NCWO Counts ===', 'Count': ''})
    final_results.extend([{'Value': value, 'Count': count} for result in results 
                         if result['Value'].startswith('NCWO') 
                         for value, count in [result.values()]])
    final_results.append({'Value': '', 'Count': ''})

    final_results.append({'Value': '=== PREPIF Counts ===', 'Count': ''})
    final_results.extend([{'Value': value, 'Count': count} for result in results 
                         if result['Value'].startswith('PREPIF') 
                         for value, count in [result.values()]])

    final_df = pd.DataFrame(final_results)
    final_df.to_csv(output_path, index=False)

if __name__ == "__main__":
    main()
    print("\nPRESS X TO CLOSE")
    while True:
        user_input = input().strip().upper()
        if user_input == 'X':
            break
