import pandas as pd
import os
import shutil
from datetime import datetime

# Define base file paths
base_path = r"C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\DATA PROCESSING"
input_file = os.path.join(base_path, "BM INPUT", "INPUT.csv")
pcexp_file = os.path.join(base_path, "OUTPUT.csv")
move_updates_file = os.path.join(base_path, "MOVE UPDATES.csv")

# Get file number from user
file_number = input("WHICH FILE WAS PROCESSED? ENTER NUMBER: ")

# Find corresponding file in RAW FILES
raw_folder = os.path.join(base_path, "RAW FILES")
raw_files = os.listdir(raw_folder)
target_file = None
for file in raw_files:
    if file.startswith(file_number + " ") and (file.endswith(".xlsx") or file.endswith(".csv")):
        target_file = file
        break

if not target_file:
    raise FileNotFoundError(f"No file found starting with {file_number}")

# Generate output filename
output_filename = target_file[3:].replace('.xlsx', '.csv').replace('.csv', '.csv')
output_file = os.path.join(base_path, "PROCESSED", output_filename)

# Handle file processing based on type
if target_file.endswith('.xlsx'):
    # Convert XLSX to CSV
    df_xlsx = pd.read_excel(os.path.join(raw_folder, target_file))
    df_xlsx.to_csv(output_file, index=False)
else:
    # Direct copy for CSV files
    shutil.copy2(os.path.join(raw_folder, target_file), output_file)

# Read the CSV files
df_main = pd.read_csv(input_file)
df_pcexp = pd.read_csv(pcexp_file)
df_move = pd.read_csv(move_updates_file)

# Remove '.0' from member ID columns to eliminate decimal values
for col in ['member1_id', 'member2_id', 'member3_id', 'member4_id']:
    if col in df_main.columns:
        df_main[col] = df_main[col].astype(str).str.replace(r'\.0$', '', regex=True)

# Convert matching columns to lowercase with explicit string conversion
df_main['hoh_guardian_name_lower'] = df_main['hoh_guardian_name'].astype(str).str.lower()
df_main['member_address1_lower'] = df_main['member_address1'].astype(str).str.lower()
df_move['Full Name_lower'] = df_move['Full Name'].astype(str).str.lower()
df_move['Original Address Line 1_lower'] = df_move['Original Address Line 1'].astype(str).str.lower()

# Add new columns
df_main['mailed'] = ''
df_main['new add'] = ''
df_main['newadd2'] = ''
df_main['City'] = ''
df_main['State'] = ''
df_main['ZIP Code'] = ''

# Update the mailing status logic based on User Text 3
df_main['mailed'] = df_main['recno'].map(
    lambda x: '14' if (x in df_pcexp[df_pcexp['User Text 3'].astype(str) == '14']['recno'].values) else 
              '13'
)

# Create the matches with explicit column mapping
matches = pd.merge(
    df_main,
    df_move[[
        'Full Name_lower', 
        'Original Address Line 1_lower', 
        'Address Line 1', 
        'Address Line 2', 
        'City', 
        'State', 
        'ZIP Code'
    ]],
    left_on=['hoh_guardian_name_lower', 'member_address1_lower'],
    right_on=['Full Name_lower', 'Original Address Line 1_lower'],
    how='left',
    indicator=True
)

# Update matching records
matching_mask = matches['_merge'] == 'both'
if matching_mask.any():
    df_main.loc[matching_mask, 'new add'] = matches.loc[matching_mask, 'Address Line 1']
    df_main.loc[matching_mask, 'newadd2'] = matches.loc[matching_mask, 'Address Line 2']
    df_main.loc[matching_mask, 'City'] = matches.loc[matching_mask, 'City_y']
    df_main.loc[matching_mask, 'State'] = matches.loc[matching_mask, 'State_y']
    df_main.loc[matching_mask, 'ZIP Code'] = matches.loc[matching_mask, 'ZIP Code_y']
else:
    print("No address updates found in move update file. Processing continues with original addresses.")

# Remove temporary lowercase columns
df_main = df_main.drop(['hoh_guardian_name_lower', 'member_address1_lower'], axis=1)

# Save output file
df_main.to_csv(output_file, index=False)

# Create PREFLIGHT CSV
preflight_headers = [
    'member1_id', 'member1_effective_date', 'member1_grp_plan_name', 'member1_last_name', 
    'member1_first_name', 'member1_middle_initial', 'member2_id', 'member2_effective_date', 
    'member2_grp_plan_name', 'member2_last_name', 'member2_first_name', 'member2_middle_initial',
    'member3_id', 'member3_effective_date', 'member3_grp_plan_name', 'member3_last_name', 
    'member3_first_name', 'member3_middle_initial', 'member4_id', 'member4_effective_date', 
    'member4_grp_plan_name', 'member4_last_name', 'member4_first_name', 'member4_middle_initial',
    'hoh_guardian_name', 'member_address1', 'member_address2', 'member_city', 'member_state', 
    'member_zip', 'office_name', 'office_phone', 'office_address_1', 'office_address_2', 
    'office_city', 'office_state', 'office_zip', 'recno'
]

# Read the processed CSV
df_processed = pd.read_csv(output_file)

# Perform INNER MERGE to keep only records present in both df_processed and df_pcexp
df_merged = pd.merge(df_processed, 
                     df_pcexp[['Full Name', 'Address Line 1', 'Address Line 2', 'City', 'State', 
                               'ZIP Code', 'recno', 'User Text 3']], 
                     on='recno', how='inner')

# Filter out records with User Text 3 = 14
df_processed_filtered = df_merged[pd.to_numeric(df_merged['User Text 3'], errors='coerce') != 14]

# Create the preflight DataFrame with the filtered data
df_preflight = pd.DataFrame(columns=preflight_headers)

# Copy data from filtered records
for column in df_preflight.columns:
    if column in df_processed_filtered.columns:
        df_preflight[column] = df_processed_filtered[column]

# Explicitly map the address fields from OUTPUT.csv
df_preflight['member_address1'] = df_processed_filtered['Address Line 1']
df_preflight['member_address2'] = df_processed_filtered['Address Line 2']
df_preflight['member_city'] = df_processed_filtered['City_y']
df_preflight['member_state'] = df_processed_filtered['State_y']
df_preflight['member_zip'] = df_processed_filtered['ZIP Code_y']
df_preflight['hoh_guardian_name'] = df_processed_filtered['Full Name']

# Ensure recno is included
df_preflight['recno'] = df_processed_filtered['recno']

# Save PREFLIGHT CSV
preflight_filename = output_filename.replace('.csv', '_PF.csv')
preflight_path = os.path.join(base_path, "PREFLIGHT", preflight_filename)
df_preflight.to_csv(preflight_path, index=False)

print(f"Preflight file saved as: {preflight_filename}")

# Move source file to processed folder
source_path = os.path.join(raw_folder, target_file)
dest_path = os.path.join(raw_folder, "PROCESSED", target_file)
shutil.move(source_path, dest_path)

print(f"\nProcessed file saved as: {output_filename}")
print(f"Source file moved to: {os.path.join('RAW FILES', 'PROCESSED', target_file)}")

# Generate timestamp and handle backups
timestamp = datetime.now().strftime("_%y%m%d-%H%M")
backup_folder = r"C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\DATA PROCESSING\BACKUP"

move_backup = os.path.join(backup_folder, f"MOVE UPDATES{timestamp}.csv")
shutil.move(move_updates_file, move_backup)

output_backup = os.path.join(backup_folder, f"OUTPUT{timestamp}.csv")
shutil.move(pcexp_file, output_backup)

print(f"\nFiles moved and renamed:")
print(f"MOVE UPDATES moved to: {os.path.basename(move_backup)}")
print(f"OUTPUT moved to: {os.path.basename(output_backup)}")
