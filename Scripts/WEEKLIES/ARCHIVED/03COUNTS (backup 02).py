import pandas as pd
import os
from datetime import datetime

# Create the output and archive directories
output_folder = r"C:\Users\JCox\Desktop\AUTOMATION\RAC\COUNTS"
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
inactive_folder = r"C:\Users\JCox\Desktop\AUTOMATION\RAC\INACTIVE_2310-DM07\FOLDERS\OUTPUT"
ncwo_folder = r"C:\Users\JCox\Desktop\AUTOMATION\RAC\NCWO_4TH\DM03\OUTPUT"
pif_folder = r"C:\Users\JCox\Desktop\AUTOMATION\RAC\PREPIF\FOLDERS\OUTPUT"
inactive_files = ['A-PO.csv', 'A-PU.csv', 'FZAPO.csv', 'FZAPU.csv', 'PR-PO.csv', 'PR-PU.csv']
ncwo_files = ['1-A_OUTPUT.csv', '1-AP_OUTPUT.csv', '2-A_OUTPUT.csv', '2-AP_OUTPUT.csv']

# Initialize results list
results = []

# Process CBC files
cbc_folder = r"C:\Users\JCox\Desktop\AUTOMATION\RAC\CBC\JOB\OUTPUT"
for file in ['CBC2WEEKLYREFORMAT.csv', 'CBC3WEEKLYREFORMAT.csv']:
    file_path = os.path.join(cbc_folder, file)
    if os.path.exists(file_path):
        df = pd.read_csv(file_path, low_memory=False, encoding='latin1')
        counts = df.iloc[:, 14].value_counts()  # Column O
        counts_df = counts.reset_index()
        counts_df.columns = ['Value', 'Count']
        results.extend([
            pd.DataFrame([{'Value': f'=== {file} Counts ===', 'Count': ''}]),
            counts_df,
            pd.DataFrame([{'Value': '', 'Count': ''}])
        ])

# Process EXC file
exc_path = r"C:\Users\JCox\Desktop\AUTOMATION\RAC\EXC\JOB\OUTPUT\EXC_OUTPUT.csv"
if os.path.exists(exc_path):
    df = pd.read_csv(exc_path, low_memory=False, encoding='latin1')
    counts = df.iloc[:, 13].value_counts()  # Column N
    counts_df = counts.reset_index()
    counts_df.columns = ['Value', 'Count']
    results.extend([
        pd.DataFrame([{'Value': '=== EXC_OUTPUT.csv Counts ===', 'Count': ''}]),
        counts_df,
        pd.DataFrame([{'Value': '', 'Count': ''}])
    ])

# Merge and process inactive files
inactive_dfs = []
for file in inactive_files:
    file_path = os.path.join(inactive_folder, file)
    if os.path.exists(file_path):
        df = pd.read_csv(file_path, low_memory=False, encoding='latin1')
        inactive_dfs.append(df)

if inactive_dfs:
    merged_inactive = pd.concat(inactive_dfs)
    merged_inactive_path = os.path.join(inactive_folder, "MERGEDTEMP_INACTIVE.csv")
    merged_inactive.to_csv(merged_inactive_path, index=False)
    counts = merged_inactive.iloc[:, 2].value_counts()  # Column C
    counts_df = counts.reset_index()
    counts_df.columns = ['Value', 'Count']
    results.extend([
        pd.DataFrame([{'Value': '=== Merged Inactive Files Counts ===', 'Count': ''}]),
        counts_df,
        pd.DataFrame([{'Value': '', 'Count': ''}])
    ])

# Merge and process NCWO files
ncwo_dfs = []
for file in ncwo_files:
    file_path = os.path.join(ncwo_folder, file)
    if os.path.exists(file_path):
        df = pd.read_csv(file_path, low_memory=False, encoding='latin1')
        ncwo_dfs.append(df)

if ncwo_dfs:
    merged_ncwo = pd.concat(ncwo_dfs)
    merged_ncwo_path = os.path.join(ncwo_folder, "MERGEDTEMP_NCWO.csv")
    merged_ncwo.to_csv(merged_ncwo_path, index=False)
    counts = merged_ncwo.iloc[:, 18].value_counts()  # Column S
    counts_df = counts.reset_index()
    counts_df.columns = ['Value', 'Count']
    results.extend([
        pd.DataFrame([{'Value': '=== Merged NCWO Files Counts ===', 'Count': ''}]),
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
    results.extend([
        pd.DataFrame([{'Value': '=== PRE_PIF.csv Counts ===', 'Count': ''}]),
        counts_df
    ])

# Combine all results and write to file if we have any results
if results:
    final_df = pd.concat(results, ignore_index=True)
    final_df.to_csv(output_path, index=False)

    # Clean up temporary files if they exist
    if 'merged_inactive_path' in locals() and os.path.exists(merged_inactive_path):
        os.remove(merged_inactive_path)
    if 'merged_ncwo_path' in locals() and os.path.exists(merged_ncwo_path):
        os.remove(merged_ncwo_path)

# Update the success message
print(f"Successfully created COUNTS.csv at {output_path}")
