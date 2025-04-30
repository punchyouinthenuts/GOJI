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

# Find corresponding XLSX file
xlsx_folder = os.path.join(base_path, "XLSX FILES")
xlsx_files = os.listdir(xlsx_folder)
target_file = None
for file in xlsx_files:
    if file.startswith(file_number + " ") and file.endswith(".xlsx"):
        target_file = file
        break

if not target_file:
    raise FileNotFoundError(f"No XLSX file found starting with {file_number}")

# Generate output filename (remove number and space from XLSX filename)
output_filename = target_file[3:].replace('.xlsx', '.csv')
output_file = os.path.join(base_path, "PROCESSED", output_filename)

# Read the CSV files
df_main = pd.read_csv(input_file)
df_pcexp = pd.read_csv(pcexp_file)
df_move = pd.read_csv(move_updates_file)

# Convert matching columns to lowercase
df_main['hoh_guardian_name_lower'] = df_main['hoh_guardian_name'].str.lower()
df_main['member_address1_lower'] = df_main['member_address1'].str.lower()
df_move['Full Name_lower'] = df_move['Full Name'].str.lower()
df_move['Original Address Line 1_lower'] = df_move['Original Address Line 1'].str.lower()

# Add new columns
df_main['mailed'] = ''
df_main['new add'] = ''
df_main['newadd2'] = ''
df_main['City'] = ''
df_main['State'] = ''
df_main['ZIP Code'] = ''

# Process mailed/not mailed logic
df_main['mailed'] = df_main['recno'].map(
    lambda x: '13' if x in df_pcexp['recno'].values else '14'
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
if matching_mask.any():  # If there are any matches
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

# Move XLSX file to processed folder
source_path = os.path.join(xlsx_folder, target_file)
dest_path = os.path.join(xlsx_folder, "PROCESSED", target_file)
shutil.move(source_path, dest_path)

print(f"\nProcessed file saved as: {output_filename}")
print(f"XLSX file moved to: {os.path.join('XLSX FILES', 'PROCESSED', target_file)}")

# Generate timestamp for backup files
timestamp = datetime.now().strftime("_%y%m%d-%H%M")

# Define backup folder
backup_folder = r"C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\DATA PROCESSING\BACKUP"

# Move and rename MOVE UPDATES.csv with timestamp
move_backup = os.path.join(backup_folder, f"MOVE UPDATES{timestamp}.csv")
shutil.move(move_updates_file, move_backup)

# Move and rename OUTPUT.csv with timestamp
output_backup = os.path.join(backup_folder, f"OUTPUT{timestamp}.csv")
shutil.move(pcexp_file, output_backup)

print(f"\nFiles moved and renamed:")
print(f"MOVE UPDATES moved to: {os.path.basename(move_backup)}")
print(f"OUTPUT moved to: {os.path.basename(output_backup)}")
