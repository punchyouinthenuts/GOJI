import os
import re
import pandas as pd
import numpy as np

def count_versions_in_file(filepath):
    version_counts = {}
    rac_pattern = r'RAC\d{4}-DM\d{2}.*'
    filename = os.path.basename(filepath)
    
    try:
        if filepath.lower().endswith('.csv'):
            df = pd.read_csv(filepath, low_memory=False)
            version_col = 'Creative_Version_Cd'
        else:
            df = pd.read_csv(filepath, delimiter='\t', encoding='utf-8')
            version_col = 'Creative_Version_Cd' if filename.upper() in ['FZAPO.TXT', 'FZAPU.TXT', 'PREPIF.TXT'] else 'creative_version'
        
        df.columns = df.columns.str.replace('"', '')
        
        if version_col in df.columns:
            counts = df[version_col].value_counts()
            for version, count in counts.items():
                if str(version).startswith('RAC') and re.match(rac_pattern, str(version)):
                    version_counts[version] = count
    except Exception as e:
        print(f"Error processing {filepath}: {str(e)}")
    
    return version_counts

def process_folder_counts(folder_path, folder_type):
    version_counts = {}
    
    for filename in os.listdir(folder_path):
        filepath = os.path.join(folder_path, filename)
        current_counts = count_versions_in_file(filepath)
        
        if folder_type == 'CBC':
            for version, count in current_counts.items():
                version_counts[version] = max(version_counts.get(version, 0), count)
        elif folder_type in ['EXC', 'NCWO']:
            original_counts = read_original_csv()
            for version, count in current_counts.items():
                version_counts[version] = original_counts.get(version, count)
        elif folder_type == 'PREPIF':
            if filename.upper() == 'PREPIF.TXT':
                version_counts.update(current_counts)
        elif folder_type == 'INACTIVE':
            if filename.upper() in ['FZAPO.TXT', 'FZAPU.TXT']:
                version_counts.update(current_counts)
    
    return version_counts

def read_original_csv():
    original_counts = {}
    df = pd.read_csv(r'C:\Users\JCox\Downloads\original.csv', encoding='utf-8-sig')
    
    for _, row in df.iterrows():
        code = row.iloc[0].strip()
        count_str = str(row.iloc[1]).replace(',', '').replace('Â', '').replace('"', '').strip()
        try:
            count = int(float(count_str))
            if code in original_counts:
                original_counts[code] = max(original_counts[code], count)                
            else:
                original_counts[code] = count
        except ValueError:
            continue
    
    return original_counts

def main():
    folders = {
        'CBC': r'C:\Users\JCox\Desktop\AUTOMATION\RAC\CBC\JOB\INPUT',
        'EXC': r'C:\Users\JCox\Desktop\AUTOMATION\RAC\EXC\JOB\INPUT',
        'INACTIVE': r'C:\Users\JCox\Desktop\AUTOMATION\RAC\INACTIVE_2310-DM07\FOLDERS\INPUT',
        'NCWO': r'C:\Users\JCox\Desktop\AUTOMATION\RAC\NCWO_4TH\DM03\INPUT',
        'PREPIF': r'C:\Users\JCox\Desktop\AUTOMATION\RAC\PREPIF\FOLDERS\INPUT'
    }
    
    all_scanned_counts = {}
    for folder_type, folder_path in folders.items():
        folder_counts = process_folder_counts(folder_path, folder_type)
        for version, count in folder_counts.items():
            all_scanned_counts[version] = max(all_scanned_counts.get(version, 0), count)
    
    original_counts = read_original_csv()
    
    print("\nComparison of Counts:")
    print("-" * 70)
    print("Version Code                   Scanned      Original     Match?  ")
    print("-" * 70)
    
    all_versions = sorted(set(list(all_scanned_counts.keys()) + list(original_counts.keys())))
    for version in all_versions:
        scanned = all_scanned_counts.get(version, 0)
        original = original_counts.get(version, 0)
        match = "✓" if scanned == original else "✗"
        print(f"{version:<30} {scanned:>7,d}        {original:>7,d}        {match}       ")

if __name__ == "__main__":
    main()
