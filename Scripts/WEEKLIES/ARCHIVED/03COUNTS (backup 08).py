import os
import re
import pandas as pd
from datetime import datetime
import io

def process_cbc_file(filepath):
    version_counts = {'CBC2': 0, 'CBC3': 0}
    
    try:
        # First try reading with comma delimiter
        df = pd.read_csv(filepath, encoding='latin1', sep=',', on_bad_lines='skip')
        
        # If no valid version column found, try tab delimiter
        if len(df.columns) <= 6:
            df = pd.read_csv(filepath, encoding='latin1', sep='\t', on_bad_lines='skip')
        
        # Get version column (7th column, index 6)
        if len(df.columns) > 6:
            version_col = df.columns[6]
            
            for version in df[version_col].dropna():
                version = str(version).strip().strip('"')
                if 'CBC2' in version or 'DM07-CBC2' in version:
                    version_counts['CBC2'] += 1
                elif 'CBC3' in version or 'DM03' in version:
                    version_counts['CBC3'] += 1
                    
    except Exception as e:
        print(f"Error processing CBC file {filepath}: {str(e)}")
    
    return version_counts

def count_versions_in_input(filepath):
    version_counts = {}
    rac_pattern = r'RAC\d{4}-DM\d{2}.*'
    
    try:
        if 'CBC' in filepath:
            if not filepath.lower().endswith('.csv'):
                return version_counts
            df = pd.read_csv(filepath, low_memory=False)
            version_col = 'creative_version'
        elif 'INACTIVE' in filepath:
            if os.path.basename(filepath).lower() in ['apo.txt', 'apu.txt']:
                df = pd.read_csv(filepath, delimiter='\t', encoding='utf-8')
                version_col = 'Creative_Version_Cd'
            else:
                return version_counts
        elif 'NCWO' in filepath and os.path.basename(filepath).lower() == 'allinput.csv':
            df = pd.read_csv(filepath, low_memory=False)
            version_col = 'Creative_Version_Cd'
        elif 'PREPIF' in filepath:
            if os.path.basename(filepath).lower() == 'prepif.txt':
                df = pd.read_csv(filepath, delimiter='\t', encoding='utf-8')
                version_col = 'Creative_Version_Cd'
            else:
                return version_counts
        else:
            if filepath.lower().endswith('.csv'):
                df = pd.read_csv(filepath, low_memory=False)
                version_col = 'Creative_Version_Cd'
            else:
                df = pd.read_csv(filepath, delimiter='\t', encoding='utf-8')
                version_col = 'Creative_Version_Cd'
        
        df.columns = df.columns.str.replace('"', '')
        
        if version_col in df.columns:
            counts = df[version_col].value_counts()
            for version, count in counts.items():
                if str(version).startswith('RAC') and re.match(rac_pattern, str(version)):
                    version_counts[version] = count
                    
    except Exception as e:
        print(f"Error processing input file {filepath}: {str(e)}")
    
    return version_counts

def count_versions_in_output():
    output_counts = {}
    rac_pattern = r'RAC\d{4}-DM\d{2}.*'
    
    folders = {
        'CBC': r"C:\Users\JCox\Desktop\AUTOMATION\RAC\CBC\JOB\OUTPUT",
        'EXC': r"C:\Users\JCox\Desktop\AUTOMATION\RAC\EXC\JOB\OUTPUT",
        'INACTIVE': r"C:\Users\JCox\Desktop\AUTOMATION\RAC\INACTIVE_2310-DM07\FOLDERS\OUTPUT",
        'NCWO': r"C:\Users\JCox\Desktop\AUTOMATION\RAC\NCWO_4TH\DM03\OUTPUT",
        'PREPIF': r"C:\Users\JCox\Desktop\AUTOMATION\RAC\PREPIF\FOLDERS\OUTPUT"
    }
    
    try:
        for folder_type, folder_path in folders.items():
            if folder_type == 'CBC':
                for file in ['CBC2WEEKLYREFORMAT.csv', 'CBC3WEEKLYREFORMAT.csv']:
                    file_path = os.path.join(folder_path, file)
                    if os.path.exists(file_path):
                        df = pd.read_csv(file_path, low_memory=False, encoding='latin1')
                        version_col = 'Creative_Version_Cd'
                        if version_col in df.columns:
                            counts = df[version_col].value_counts()
                            for version, count in counts.items():
                                if str(version).startswith('RAC') and re.match(rac_pattern, str(version)):
                                    output_counts[version] = count
            elif folder_type == 'EXC':
                file_path = os.path.join(folder_path, 'EXC_OUTPUT.csv')
                if os.path.exists(file_path):
                    df = pd.read_csv(file_path, low_memory=False, encoding='latin1')
                    version_col = df.columns[13]
                    counts = df[version_col].value_counts()
                    for version, count in counts.items():
                        if str(version).startswith('RAC') and re.match(rac_pattern, str(version)):
                            output_counts[version] = count
            elif folder_type == 'INACTIVE':
                for file in ['PR-PO.csv', 'PR-PU.csv', 'A-PO.csv', 'A-PU.csv', 'AT-PO.csv', 'AT-PU.csv']:
                    file_path = os.path.join(folder_path, file)
                    if os.path.exists(file_path):
                        df = pd.read_csv(file_path, low_memory=False, encoding='latin1')
                        version_col = df.columns[2]
                        counts = df[version_col].value_counts()
                        for version, count in counts.items():
                            if str(version).startswith('RAC') and re.match(rac_pattern, str(version)):
                                output_counts[version] = count
            elif folder_type == 'NCWO':
                for file in ['1-A_OUTPUT.csv', '1-AP_OUTPUT.csv', '2-A_OUTPUT.csv', '2-AP_OUTPUT.csv']:
                    file_path = os.path.join(folder_path, file)
                    if os.path.exists(file_path):
                        df = pd.read_csv(file_path, low_memory=False, encoding='latin1')
                        version_col = 'Creative_Version_Cd'
                        if version_col in df.columns:
                            counts = df[version_col].value_counts()
                            for version, count in counts.items():
                                if str(version).startswith('RAC') and re.match(rac_pattern, str(version)):
                                    if version in output_counts:
                                        output_counts[version] += count
                                    else:
                                        output_counts[version] = count
            elif folder_type == 'PREPIF':
                file_path = os.path.join(folder_path, 'PRE_PIF.csv')
                if os.path.exists(file_path):
                    df = pd.read_csv(file_path, low_memory=False, encoding='latin1')
                    version_col = df.columns[24]
                    counts = df[version_col].value_counts()
                    for version, count in counts.items():
                        if str(version).startswith('RAC') and re.match(rac_pattern, str(version)):
                            output_counts[version] = count
    except Exception as e:
        print(f"Error processing output folders: {str(e)}")
    
    return output_counts

def create_diagnostic_log():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    log_path = os.path.join(script_dir, 'cbc_diagnostic.log')
    
    cbc_input_folder = r"C:\Users\JCox\Desktop\AUTOMATION\RAC\CBC\JOB\INPUT"
    
    with open(log_path, 'w') as log:
        log.write("CBC Input Files Diagnostic Log\n")
        log.write("=" * 50 + "\n\n")
        
        for filename in os.listdir(cbc_input_folder):
            filepath = os.path.join(cbc_input_folder, filename)
            if os.path.isfile(filepath):
                log.write(f"\nFile: {filename}\n")
                log.write(f"Size: {os.path.getsize(filepath):,} bytes\n")
                
                try:
                    df = pd.read_csv(filepath, 
                                   sep=None,
                                   engine='python',
                                   encoding='latin1',
                                   quoting=3,
                                   nrows=5)
                    
                    log.write("\nColumn Names:\n")
                    log.write(", ".join(df.columns) + "\n")
                    
                    version_col = df.columns[6]  # creative_version column
                    log.write(f"\nVersion Column ({version_col}) First 5 Values:\n")
                    for val in df[version_col].values:
                        log.write(f"{val}\n")
                    
                except Exception as e:
                    log.write(f"\nError reading file: {str(e)}\n")
                
                log.write("\n" + "="*50 + "\n")
    
    return log_path

def consolidate_counts(counts_dict):
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
        if 'CBC2' in version:
            consolidated['CBC2'] += count
        elif 'DM03-A' in version or 'DM03-CANC' in version or 'DM03-PR' in version:
            consolidated['CBC3'] += count
        elif 'RACXW' in version:
            consolidated['EXC'] += count
        elif 'PPIF' in version:
            consolidated['PREPIF'] += count
        elif '-A-PO' in version or '-PR-PO' in version:
            consolidated['INACTIVE A-PO'] += count
        elif '-A-PU' in version or '-PR-PU' in version:
            consolidated['INACTIVE A-PU'] += count
        elif '-AT-PO' in version:
            consolidated['INACTIVE AT-PO'] += count
        elif '-AT-PU' in version:
            consolidated['INACTIVE AT-PU'] += count
        elif 'NCWO1-A' in version or 'NCWO1-PR' in version:
            consolidated['NCWO 1-A'] += count
        elif 'NCWO1-AP' in version or 'NCWO1-APPR' in version:
            consolidated['NCWO 1-AP'] += count
        elif 'NCWO2-A' in version or 'NCWO2-PR' in version:
            consolidated['NCWO 2-A'] += count
        elif 'NCWO2-AP' in version or 'NCWO2-APPR' in version:
            consolidated['NCWO 2-AP'] += count
    
    return consolidated

def compare_counts(input_counts, output_counts):
    consolidated_input = consolidate_counts(input_counts)
    consolidated_output = consolidate_counts(output_counts)
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
    input_folders = {
        'CBC': r"C:\Users\JCox\Desktop\AUTOMATION\RAC\CBC\JOB\INPUT",
        'EXC': r"C:\Users\JCox\Desktop\AUTOMATION\RAC\EXC\JOB\INPUT",
        'INACTIVE': r"C:\Users\JCox\Desktop\AUTOMATION\RAC\INACTIVE_2310-DM07\FOLDERS\INPUT",
        'NCWO': r"C:\Users\JCox\Desktop\AUTOMATION\RAC\NCWO_4TH\DM03\INPUT\ALLINPUT.csv",
        'PREPIF': r"C:\Users\JCox\Desktop\AUTOMATION\RAC\PREPIF\FOLDERS\INPUT"
    }
    
    log_path = create_diagnostic_log()
    print(f"\nCBC diagnostic log created at: {log_path}")
    
    input_counts = {}
    for folder_type, path in input_folders.items():
        if os.path.isfile(path):  # For NCWO ALLINPUT.csv
            current_counts = count_versions_in_input(path)
            for version, count in current_counts.items():
                input_counts[version] = count
        else:  # For directory processing
            for filename in os.listdir(path):
                filepath = os.path.join(path, filename)
                current_counts = count_versions_in_input(filepath)
                for version, count in current_counts.items():
                    if folder_type == 'PREPIF':
                        input_counts[version] = input_counts.get(version, 0) + count
                    else:
                        input_counts[version] = max(input_counts.get(version, 0), count)

    output_counts = count_versions_in_output()
    compare_counts(input_counts, output_counts)

    # Part 2: COUNTS.csv generation with backup system
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
        'RAC2504-DM03-AT-PU': 'INACTIVE AT-PU US',
        'RAC2504-DM03-A-PO': 'INACTIVE A-PO US',
        'RAC2504-DM03-PR-PO': 'INACTIVE A-PO PR',
        'RAC2504-DM03-AT-PO': 'INACTIVE AT-PO US',
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
        'INACTIVE': ['INACTIVE A-PO PR', 'INACTIVE A-PO US', 'INACTIVE A-PU PR', 'INACTIVE A-PU US', 'INACTIVE AT-PO US', 'INACTIVE AT-PU US'],
        'NCWO': ['NCWO 1-A PR', 'NCWO 1-A US', 'NCWO 1-AP PR', 'NCWO 1-AP US', 'NCWO 2-A PR', 'NCWO 2-A US', 'NCWO 2-AP PR', 'NCWO 2-AP US'],
        'PREPIF': ['PREPIF PR', 'PREPIF US']
    }

    results = []

    # Process CBC files
    cbc_folder = r"C:\Users\JCox\Desktop\AUTOMATION\RAC\CBC\JOB\OUTPUT"
    for file in ['CBC2WEEKLYREFORMAT.csv', 'CBC3WEEKLYREFORMAT.csv']:
        file_path = os.path.join(cbc_folder, file)
        if os.path.exists(file_path):
            df = pd.read_csv(file_path, low_memory=False, encoding='latin1')
            counts = df.iloc[:, 14].value_counts()
            counts_df = counts.reset_index()
            counts_df.columns = ['Value', 'Count']
            counts_df['Value'] = counts_df['Value'].map(value_mappings)
            header_type = 'CBC2' if 'CBC2' in file else 'CBC3'
            for value in sort_orders[header_type]:
                count = counts_df[counts_df['Value'] == value]['Count'].sum() if value in counts_df['Value'].values else 0
                results.append({'Value': value, 'Count': count})

    # Process EXC files
    exc_folder = r"C:\Users\JCox\Desktop\AUTOMATION\RAC\EXC\JOB\OUTPUT"
    file_path = os.path.join(exc_folder, 'EXC_OUTPUT.csv')
    if os.path.exists(file_path):
        df = pd.read_csv(file_path, low_memory=False, encoding='latin1')
        counts = df.iloc[:, 13].value_counts()
        counts_df = counts.reset_index()
        counts_df.columns = ['Value', 'Count']
        counts_df['Value'] = counts_df['Value'].map(value_mappings)
        for value in sort_orders['EXC']:
            count = counts_df[counts_df['Value'] == value]['Count'].sum() if value in counts_df['Value'].values else 0
            results.append({'Value': value, 'Count': count})

    # Process INACTIVE files
    inactive_folder = r"C:\Users\JCox\Desktop\AUTOMATION\RAC\INACTIVE_2310-DM07\FOLDERS\OUTPUT"
    inactive_dfs = []
    for file in ['PR-PO.csv', 'PR-PU.csv', 'A-PO.csv', 'A-PU.csv', 'AT-PO.csv', 'AT-PU.csv']:
        file_path = os.path.join(inactive_folder, file)
        if os.path.exists(file_path):
            df = pd.read_csv(file_path, low_memory=False, encoding='latin1')
            inactive_dfs.append(df)

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
            df = pd.read_csv(file_path, low_memory=False, encoding='latin1')
            counts = df['Creative_Version_Cd'].value_counts()
            counts_df = counts.reset_index()
            counts_df.columns = ['Value', 'Count']
            counts_df['Value'] = counts_df['Value'].map(value_mappings)
            for value in sort_orders['NCWO']:
                if value.split()[1:2][0] == file.split('_')[0]:
                    count = counts_df[counts_df['Value'] == value]['Count'].sum() if value in counts_df['Value'].values else 0
                    results.append({'Value': value, 'Count': count})

    # Process PREPIF files
    prepif_folder = r"C:\Users\JCox\Desktop\AUTOMATION\RAC\PREPIF\FOLDERS\OUTPUT"
    file_path = os.path.join(prepif_folder, 'PRE_PIF.csv')
    if os.path.exists(file_path):
        df = pd.read_csv(file_path, low_memory=False, encoding='latin1')
        counts = df.iloc[:, 24].value_counts()
        counts_df = counts.reset_index()
        counts_df.columns = ['Value', 'Count']
        counts_df['Value'] = counts_df['Value'].map(value_mappings)
        for value in sort_orders['PREPIF']:
            count = counts_df[counts_df['Value'] == value]['Count'].sum() if value in counts_df['Value'].values else 0
            results.append({'Value': value, 'Count': count})

    # Create final DataFrame with sections and spacing
    final_results = []

    # Add CBC2 section
    final_results.append({'Value': '=== CBC2 Counts ===', 'Count': ''})
    final_results.extend([{'Value': value, 'Count': count} for result in results 
                         if result['Value'].startswith('CBC2') 
                         for value, count in [result.values()]])
    final_results.append({'Value': '', 'Count': ''})

    # Add CBC3 section
    final_results.append({'Value': '=== CBC3 Counts ===', 'Count': ''})
    final_results.extend([{'Value': value, 'Count': count} for result in results 
                         if result['Value'].startswith('CBC3') 
                         for value, count in [result.values()]])
    final_results.append({'Value': '', 'Count': ''})

    # Add EXC section
    final_results.append({'Value': '=== EXC Counts ===', 'Count': ''})
    final_results.extend([{'Value': value, 'Count': count} for result in results 
                         if result['Value'].startswith('EXC') 
                         for value, count in [result.values()]])
    final_results.append({'Value': '', 'Count': ''})

    # Add INACTIVE section
    final_results.append({'Value': '=== INACTIVE Counts ===', 'Count': ''})
    final_results.extend([{'Value': value, 'Count': count} for result in results 
                         if result['Value'].startswith('INACTIVE') 
                         for value, count in [result.values()]])
    final_results.append({'Value': '', 'Count': ''})

    # Add NCWO section
    final_results.append({'Value': '=== NCWO Counts ===', 'Count': ''})
    final_results.extend([{'Value': value, 'Count': count} for result in results 
                         if result['Value'].startswith('NCWO') 
                         for value, count in [result.values()]])
    final_results.append({'Value': '', 'Count': ''})

    # Add PREPIF section
    final_results.append({'Value': '=== PREPIF Counts ===', 'Count': ''})
    final_results.extend([{'Value': value, 'Count': count} for result in results 
                         if result['Value'].startswith('PREPIF') 
                         for value, count in [result.values()]])

    # Create final DataFrame and save to CSV
    final_df = pd.DataFrame(final_results)
    final_df.to_csv(output_path, index=False)

if __name__ == "__main__":
    main()
    print("\nPRESS X TO CLOSE")
    while True:
        user_input = input().strip().upper()
        if user_input == 'X':
            break