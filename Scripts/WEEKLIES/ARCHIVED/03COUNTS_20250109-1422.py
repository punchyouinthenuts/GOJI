import os
import re
import pandas as pd
from datetime import datetime

def count_versions_in_input(filepath):
    version_counts = {}
    rac_pattern = r'RAC\d{4}-DM\d{2}.*'
    
    try:
        if filepath.lower().endswith('.csv'):
            df = pd.read_csv(filepath, low_memory=False)
            version_col = 'Creative_Version_Cd'
        else:
            df = pd.read_csv(filepath, delimiter='\t', encoding='utf-8')
            version_col = 'Creative_Version_Cd' if os.path.basename(filepath).upper() in ['FZAPO.TXT', 'FZAPU.TXT', 'PREPIF.TXT'] else 'creative_version'
        
        df.columns = df.columns.str.replace('"', '')
        
        if version_col in df.columns:
            counts = df[version_col].value_counts()
            for version, count in counts.items():
                if str(version).startswith('RAC') and re.match(rac_pattern, str(version)):
                    if version in version_counts:
                        version_counts[version] += count
                    else:
                        version_counts[version] = count
    except Exception as e:
        print(f"Error processing input file {filepath}: {str(e)}")
    
    return version_counts

def count_versions_in_output():
    output_counts = {}
    rac_pattern = r'RAC\d{4}-DM\d{2}.*'
    
    folders = {
        'CBC': r"C:\Program Files\Goji\RAC\CBC\JOB\OUTPUT",
        'EXC': r"C:\Program Files\Goji\RAC\EXC\JOB\OUTPUT",
        'INACTIVE': r"C:\Program Files\Goji\RAC\INACTIVE\JOB\OUTPUT",
        'NCWO': r"C:\Program Files\Goji\RAC\NCWO\JOB\OUTPUT",
        'PREPIF': r"C:\Program Files\Goji\RAC\PREPIF\JOB\OUTPUT"
    }
    
    try:
        for folder_type, folder_path in folders.items():
            if folder_type == 'CBC':
                for file in ['CBC2WEEKLYREFORMAT.csv', 'CBC3WEEKLYREFORMAT.csv']:
                    file_path = os.path.join(folder_path, file)
                    if os.path.exists(file_path):
                        df = pd.read_csv(file_path, low_memory=False, encoding='latin1')
                        version_col = df.columns[14]
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
                for file in ['A-PO.csv', 'A-PU.csv', 'FZAPO.csv', 'FZAPU.csv', 'PR-PO.csv', 'PR-PU.csv']:
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

def compare_counts(input_counts, output_counts):
    all_versions = sorted(set(list(input_counts.keys()) + list(output_counts.keys())))
    
    print("\nComparison of Counts:")
    print("-" * 70)
    print("Version Code                   Input       Output      Difference")
    print("-" * 70)
    
    for version in all_versions:
        input_count = input_counts.get(version, 0)
        output_count = output_counts.get(version, 0)
        diff = output_count - input_count
        error_flag = " [ERROR!]" if diff > 0 else ""
        
        print(f"{version:<30} {input_count:>7,d}     {output_count:>7,d}     {diff:>7,d}{error_flag}")
 
def main():
    # Part 1: Original comparison functionality
    input_folders = {
        'CBC': r"C:\Program Files\Goji\RAC\CBC\JOB\INPUT",
        'EXC': r"C:\Program Files\Goji\RAC\EXC\JOB\INPUT",
        'INACTIVE': r"C:\Program Files\Goji\RAC\INACTIVE\JOB\INPUT",
        'NCWO': r"C:\Program Files\Goji\RAC\NCWO\JOB\INPUT",
        'PREPIF': r"C:\Program Files\Goji\RAC\PREPIF\JOB\INPUT"
    }
    
    input_counts = {}
    for folder_type, folder_path in input_folders.items():
        for filename in os.listdir(folder_path):
            filepath = os.path.join(folder_path, filename)
            current_counts = count_versions_in_input(filepath)
            for version, count in current_counts.items():
                if folder_type == 'PREPIF':
                    input_counts[version] = input_counts.get(version, 0) + count
                else:
                    if version in input_counts:
                        input_counts[version] = max(input_counts.get(version, 0), count)
                    else:
                        input_counts[version] = count

    output_counts = count_versions_in_output()
    compare_counts(input_counts, output_counts)

    # Part 2: COUNTS.csv generation with backup system
    output_folder = r"C:\Program Files\Goji\RAC\COUNTS"
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
        'RAC2501-DM06-A-PO': 'A-PO US',
        'RAC2501-DM06-A-PU': 'A-PU US',
        'RAC2501-DM06-PR-PU': 'A-PU PR',
        'RAC2501-DM06-PR-PO': 'A-PO PR',
        '2-A': '2-A US',
        '1-A': '1-A US',
        '1-AP': '1-AP US',
        '2-AP': '2-AP US',
        '1-PR': '1-A PR',
        '2-PR': '2-A PR',
        '1-APPR': '1-AP PR',
        '2-APPR': '2-AP PR',
        'RAC2404-DM06-PPIF-A': 'PREPIF US',
        'RAC2404-DM06-PPIF-PR': 'PREPIF PR'
    }

    sort_orders = {
        'CBC2': ['CBC2 PR', 'CBC2 CANC', 'CBC2 US'],
        'CBC3': ['CBC3 PR', 'CBC3 CANC', 'CBC3 US'],
        'EXC': ['EXC PR', 'EXC US'],
        'INACTIVE': ['A-PO PR', 'A-PO US', 'A-PU PR', 'A-PU US'],
        'NCWO': ['1-A PR', '1-A US', '1-AP PR', '1-AP US', '2-A PR', '2-A US', '2-AP PR', '2-AP US'],
        'PREPIF': ['PREPIF PR', 'PREPIF US']
    }

    results = []

    # Process CBC files
    cbc_folder = r"C:\Program Files\Goji\RAC\CBC\JOB\OUTPUT"
    for file in ['CBC2WEEKLYREFORMAT.csv', 'CBC3WEEKLYREFORMAT.csv']:
        file_path = os.path.join(cbc_folder, file)
        if os.path.exists(file_path):
            df = pd.read_csv(file_path, low_memory=False, encoding='latin1')
            counts = df.iloc[:, 14].value_counts()
            counts_df = counts.reset_index()
            counts_df.columns = ['Value', 'Count']
            counts_df['Value'] = counts_df['Value'].map(value_mappings)
            header_type = 'CBC2' if 'CBC2' in file else 'CBC3'
            
            counts_df['sort_order'] = counts_df['Value'].map({v: i for i, v in enumerate(sort_orders[header_type])})
            counts_df = counts_df.sort_values('sort_order').drop('sort_order', axis=1)
            
            results.extend([
                pd.DataFrame([{'Value': f'=== {header_type} Counts ===', 'Count': ''}]),
                counts_df,
                pd.DataFrame([{'Value': '', 'Count': ''}])
            ])

    # Process EXC file
    exc_path = r"C:\Program Files\Goji\RAC\EXC\JOB\OUTPUT\EXC_OUTPUT.csv"
    if os.path.exists(exc_path):
        df = pd.read_csv(exc_path, low_memory=False, encoding='latin1')
        counts = df.iloc[:, 13].value_counts()
        counts_df = counts.reset_index()
        counts_df.columns = ['Value', 'Count']
        counts_df['Value'] = counts_df['Value'].map(value_mappings)
        
        counts_df['sort_order'] = counts_df['Value'].map({v: i for i, v in enumerate(sort_orders['EXC'])})
        counts_df = counts_df.sort_values('sort_order').drop('sort_order', axis=1)
        
        results.extend([
            pd.DataFrame([{'Value': '=== EXC Counts ===', 'Count': ''}]),
            counts_df,
            pd.DataFrame([{'Value': '', 'Count': ''}])
        ])

    # Process INACTIVE files
    inactive_folder = r"C:\Program Files\Goji\RAC\INACTIVE\JOB\OUTPUT"
    inactive_dfs = []
    for file in ['A-PO.csv', 'A-PU.csv', 'FZAPO.csv', 'FZAPU.csv', 'PR-PO.csv', 'PR-PU.csv']:
        file_path = os.path.join(inactive_folder, file)
        if os.path.exists(file_path):
            df = pd.read_csv(file_path, low_memory=False, encoding='latin1')
            inactive_dfs.append(df)

    if inactive_dfs:
        merged_inactive = pd.concat(inactive_dfs)
        counts = merged_inactive.iloc[:, 2].value_counts()
        counts_df = counts.reset_index()
        counts_df.columns = ['Value', 'Count']
        counts_df['Value'] = counts_df['Value'].map(value_mappings)
        
        counts_df['sort_order'] = counts_df['Value'].map({v: i for i, v in enumerate(sort_orders['INACTIVE'])})
        counts_df = counts_df.sort_values('sort_order').drop('sort_order', axis=1)
        
        results.extend([
            pd.DataFrame([{'Value': '=== INACTIVE Counts ===', 'Count': ''}]),
            counts_df,
            pd.DataFrame([{'Value': '', 'Count': ''}])
        ])

    # Process NCWO files
    ncwo_folder = r"C:\Program Files\Goji\RAC\NCWO\JOB\OUTPUT"
    ncwo_dfs = []
    for file in ['1-A_OUTPUT.csv', '1-AP_OUTPUT.csv', '2-A_OUTPUT.csv', '2-AP_OUTPUT.csv']:
        file_path = os.path.join(ncwo_folder, file)
        if os.path.exists(file_path):
            df = pd.read_csv(file_path, low_memory=False, encoding='latin1')
            ncwo_dfs.append(df)

    if ncwo_dfs:
        merged_ncwo = pd.concat(ncwo_dfs)
        counts = merged_ncwo.iloc[:, 18].value_counts()
        counts_df = counts.reset_index()
        counts_df.columns = ['Value', 'Count']
        counts_df['Value'] = counts_df['Value'].map(value_mappings)
        
        counts_df['sort_order'] = counts_df['Value'].map({v: i for i, v in enumerate(sort_orders['NCWO'])})
        counts_df = counts_df.sort_values('sort_order').drop('sort_order', axis=1)
        
        results.extend([
            pd.DataFrame([{'Value': '=== NCWO Counts ===', 'Count': ''}]),
            counts_df,
            pd.DataFrame([{'Value': '', 'Count': ''}])
        ])

    # Process PREPIF file
    pif_folder = r"C:\Program Files\Goji\RAC\PREPIF\JOB\OUTPUT"
    pif_path = os.path.join(pif_folder, "PRE_PIF.csv")
    if os.path.exists(pif_path):
        df = pd.read_csv(pif_path, low_memory=False, encoding='latin1')
        counts = df.iloc[:, 24].value_counts()
        counts_df = counts.reset_index()
        counts_df.columns = ['Value', 'Count']
        counts_df['Value'] = counts_df['Value'].map(value_mappings)
        
        counts_df['sort_order'] = counts_df['Value'].map({v: i for i, v in enumerate(sort_orders['PREPIF'])})
        counts_df = counts_df.sort_values('sort_order').drop('sort_order', axis=1)
        
        results.extend([
            pd.DataFrame([{'Value': '=== PREPIF Counts ===', 'Count': ''}]),
            counts_df
        ])

    if results:
        final_df = pd.concat(results, ignore_index=True)
        final_df.to_csv(output_path, index=False)
        print(f"\nSuccessfully created COUNTS.csv at {output_path}")

    while True:
        user_input = input("\nPRESS X TO CLOSE: ").lower()
        if user_input == 'x':
            break

if __name__ == "__main__":
    main()
