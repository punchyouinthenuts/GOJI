import pandas as pd
import os
from datetime import datetime

# Create the output and archive directories
output_folder = r"C:\Program Files\Goji\RAC\COUNTS"
archive_folder = os.path.join(output_folder, "ARCHIVE")
os.makedirs(output_folder, exist_ok=True)
os.makedirs(archive_folder, exist_ok=True)

output_path = os.path.join(output_folder, "COUNTS.csv")

# Check if COUNTS.csv exists and move it to archive with timestamp
if os.path.exists(output_path):
    timestamp = datetime.now().strftime("_%Y%m%d_%H%M")
    archived_filename = f"COUNTS{timestamp}.csv"
    archived_path = os.path.join(archive_folder, archived_filename)
    os.rename(output_path, archived_path)

# Define all file paths and configurations
inactive_folder = r"C:\Program Files\Goji\RAC\INACTIVE\JOB\OUTPUT"
ncwo_folder = r"C:\Program Files\Goji\RAC\NCWO\JOB\OUTPUT"
pif_folder = r"C:\Program Files\Goji\RAC\PREPIF\JOB\OUTPUT"

# Define value mappings and sort orders
header_mappings = {
    'CBC2WEEKLYREFORMAT.csv': 'CBC2',
    'CBC3WEEKLYREFORMAT.csv': 'CBC3',
    'EXC_OUTPUT.csv': 'EXC',
    'Merged Inactive Files': 'INACTIVE',
    'Merged NCWO Files': 'NCWO',
    'PRE_PIF.csv': 'PREPIF'
}

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
    'CBC2': ['CBC2 PR', 'CBC2 CANC', 'CBC2 A'],
    'CBC3': ['CBC3 PR', 'CBC3 CANC', 'CBC3 A'],
    'EXC': ['EXC PR', 'EXC US'],
    'INACTIVE': ['A-PO PR', 'A-PO US', 'A-PU PR', 'A-PU US'],
    'NCWO': ['1-A PR', '1-A US', '1-AP PR', '1-AP US', '2-A PR', '2-A US', '2-AP PR', '2-AP US'],
    'PREPIF': ['PREPIF PR', 'PREPIF US']
}

# Initialize results list
results = []

# Process CBC files
cbc_folder = r"C:\Program Files\Goji\RAC\CBC\JOB\OUTPUT"
for file in ['CBC2WEEKLYREFORMAT.csv', 'CBC3WEEKLYREFORMAT.csv']:
    file_path = os.path.join(cbc_folder, file)
    if os.path.exists(file_path):
        df = pd.read_csv(file_path, low_memory=False, encoding='latin1')
        counts = df.iloc[:, 14].value_counts()  # Column O
        counts_df = counts.reset_index()
        counts_df.columns = ['Value', 'Count']
        counts_df['Value'] = counts_df['Value'].map(value_mappings)
        header_type = header_mappings[file]
        
        # Sort according to predefined order
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
    counts = df.iloc[:, 13].value_counts()  # Column N
    counts_df = counts.reset_index()
    counts_df.columns = ['Value', 'Count']
    counts_df['Value'] = counts_df['Value'].map(value_mappings)
    
    # Sort according to predefined order
    counts_df['sort_order'] = counts_df['Value'].map({v: i for i, v in enumerate(sort_orders['EXC'])})
    counts_df = counts_df.sort_values('sort_order').drop('sort_order', axis=1)
    
    results.extend([
        pd.DataFrame([{'Value': '=== EXC Counts ===', 'Count': ''}]),
        counts_df,
        pd.DataFrame([{'Value': '', 'Count': ''}])
    ])

# Merge and process inactive files
inactive_dfs = []
for file in ['A-PO.csv', 'A-PU.csv', 'FZAPO.csv', 'FZAPU.csv', 'PR-PO.csv', 'PR-PU.csv']:
    file_path = os.path.join(inactive_folder, file)
    if os.path.exists(file_path):
        df = pd.read_csv(file_path, low_memory=False, encoding='latin1')
        inactive_dfs.append(df)

if inactive_dfs:
    merged_inactive = pd.concat(inactive_dfs)
    counts = merged_inactive.iloc[:, 2].value_counts()  # Column C
    counts_df = counts.reset_index()
    counts_df.columns = ['Value', 'Count']
    counts_df['Value'] = counts_df['Value'].map(value_mappings)
    
    # Sort according to predefined order
    counts_df['sort_order'] = counts_df['Value'].map({v: i for i, v in enumerate(sort_orders['INACTIVE'])})
    counts_df = counts_df.sort_values('sort_order').drop('sort_order', axis=1)
    
    results.extend([
        pd.DataFrame([{'Value': '=== INACTIVE Counts ===', 'Count': ''}]),
        counts_df,
        pd.DataFrame([{'Value': '', 'Count': ''}])
    ])

# Merge and process NCWO files
ncwo_dfs = []
for file in ['1-A_OUTPUT.csv', '1-AP_OUTPUT.csv', '2-A_OUTPUT.csv', '2-AP_OUTPUT.csv']:
    file_path = os.path.join(ncwo_folder, file)
    if os.path.exists(file_path):
        df = pd.read_csv(file_path, low_memory=False, encoding='latin1')
        ncwo_dfs.append(df)

if ncwo_dfs:
    merged_ncwo = pd.concat(ncwo_dfs)
    counts = merged_ncwo.iloc[:, 18].value_counts()  # Column S
    counts_df = counts.reset_index()
    counts_df.columns = ['Value', 'Count']
    counts_df['Value'] = counts_df['Value'].map(value_mappings)
    
    # Sort according to predefined order
    counts_df['sort_order'] = counts_df['Value'].map({v: i for i, v in enumerate(sort_orders['NCWO'])})
    counts_df = counts_df.sort_values('sort_order').drop('sort_order', axis=1)
    
    results.extend([
        pd.DataFrame([{'Value': '=== NCWO Counts ===', 'Count': ''}]),
        counts_df,
        pd.DataFrame([{'Value': '', 'Count': ''}])
    ])

# Process PRE_PIF file
pif_path = os.path.join(pif_folder, "PRE_PIF.csv")
if os.path.exists(pif_path):
    df = pd.read_csv(pif_path, low_memory=False, encoding='latin1')
    counts = df.iloc[:, 24].value_counts()  # Column Y
    counts_df = counts.reset_index()
    counts_df.columns = ['Value', 'Count']
    counts_df['Value'] = counts_df['Value'].map(value_mappings)
    
    # Sort according to predefined order
    counts_df['sort_order'] = counts_df['Value'].map({v: i for i, v in enumerate(sort_orders['PREPIF'])})
    counts_df = counts_df.sort_values('sort_order').drop('sort_order', axis=1)
    
    results.extend([
        pd.DataFrame([{'Value': '=== PREPIF Counts ===', 'Count': ''}]),
        counts_df
    ])

# Combine all results and write to file if we have any results
if results:
    final_df = pd.concat(results, ignore_index=True)
    final_df.to_csv(output_path, index=False)

print(f"Successfully created COUNTS.csv at {output_path}")
