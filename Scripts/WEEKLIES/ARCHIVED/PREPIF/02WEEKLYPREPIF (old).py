import os
import pandas as pd
import random
from datetime import timedelta

# Function to format date field
def format_date(date_str):
    # If the month or day is a single digit, add a leading zero
    parts = date_str.split('/')
    parts = [part.zfill(2) for part in parts]
    return '/'.join(parts)

# Define your directories
INPUT_DIR = "C:\\Users\\JCox\\Desktop\\AUTOMATION\\RAC\\PREPIF\\FOLDERS\\OUTPUT"
PROOF_DIR = "C:\\Users\\JCox\\Desktop\\AUTOMATION\\RAC\\PREPIF\\FOLDERS\\PROOF"

# Loop through all files in the input directory
file_name = "PRE_PIF.csv"
     # Load the file into a pandas DataFrame with 'latin1' encoding
df = pd.read_csv(os.path.join(INPUT_DIR, file_name), encoding='latin1')
 # Preserve the format of the 'END DATE' field
df['END DATE'] = pd.to_datetime(df['BEGIN DATE'], format='%m/%d/%Y') + timedelta(days=54)
df['END DATE'] = df['END DATE'].dt.strftime('%m/%d/%Y')
 # Extract rows where 'Creative_Version_Cd' contains 'PR'
df_pr = df[df['Creative_Version_Cd'].str.contains('PR', na=False)].copy()  # Create a copy
 # Save these rows to a new file with '-PR' added to the file name
df_pr['END DATE'] = pd.to_datetime(df_pr['END DATE'], format='%m/%d/%Y').dt.strftime('%d/%m/%Y')
df_pr.to_csv(os.path.join(INPUT_DIR, file_name.replace('.csv', '-PR.csv')), index=False, encoding='latin1')
 # Save to the proof directory with '-PR-PD' added to the file name, regardless of the number of records
df_pr.to_csv(os.path.join(PROOF_DIR, file_name.replace('.csv', '-PR-PD.csv')), index=False, encoding='latin1')
 # If there are more than 15 rows, keep only the first 15 and overwrite the file in the proof directory
if len(df_pr) > 15:
    df_pr[:15].to_csv(os.path.join(PROOF_DIR, file_name.replace('.csv', '-PR-PD.csv')), index=False, encoding='latin1')
 # Remove these rows from the original DataFrame
df = df[~df.index.isin(df_pr.index)]
 # Save the file without PR records with '-US' added to the file name
df.to_csv(os.path.join(INPUT_DIR, file_name.replace('.csv', '-US.csv')), index=False, encoding='latin1')
 # Save to the proof directory with '-US-PD' added to the file name
df.to_csv(os.path.join(PROOF_DIR, file_name.replace('.csv', '-US-PD.csv')), index=False, encoding='latin1')


# Save the file without PR records with '-US' added to the file name
df.to_csv(os.path.join(INPUT_DIR, file_name.replace('.csv', '-US.csv')), index=False, encoding='latin1')

# Extract 15 records such that at least one has data in the Store_License field
subset_with_license = df[df['Store_License'].notna()].sample(min(1, len(df[df['Store_License'].notna()])))
remaining_records_needed = 15 - len(subset_with_license)
subset_without_license = df[df['Store_License'].isna()].sample(min(remaining_records_needed, len(df[df['Store_License'].isna()])))
final_subset = pd.concat([subset_with_license, subset_without_license])

# Save to the proof directory with '-US-PD' added to the file name
final_subset.to_csv(os.path.join(PROOF_DIR, file_name.replace('.csv', '-US-PD.csv')), index=False, encoding='latin1')
