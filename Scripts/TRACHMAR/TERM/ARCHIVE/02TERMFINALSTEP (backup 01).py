import pandas as pd
import os

# Set the working directory
work_dir = r'C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\TERM\DATA'

# Read all files
excel_file = os.path.join(work_dir, 'FHK_TERM.xlsx')
csv_file = os.path.join(work_dir, 'MOVE UPDATES.csv')
presort_file = os.path.join(work_dir, 'PRESORTLIST.csv')

df_excel = pd.read_excel(excel_file)
df_csv = pd.read_csv(csv_file)
df_presort = pd.read_csv(presort_file)

# Convert lookup columns to string and clean them
df_excel['lookup_col'] = df_excel.iloc[:, 7].astype(str).str.strip().str.upper()
df_csv['match_col'] = df_csv.iloc[:, 0].astype(str).str.strip().str.upper()
df_presort['presort_col'] = df_presort.iloc[:, 8].astype(str).str.strip().str.upper()

# First VLOOKUP operations
column_mapping = {
    'newadd': 3,    # CSV Column D
    'newadd2': 4,   # CSV Column E
    'newcity': 5,   # CSV Column F
    'newstate': 6,  # CSV Column G
    'newzip': 7     # CSV Column H
}

# Add new columns
for new_col in column_mapping.keys():
    df_excel[new_col] = ''

# Perform the first VLOOKUP operation
matches = 0
for index, row in df_excel.iterrows():
    lookup_value = row['lookup_col']
    match = df_csv[df_csv['match_col'] == lookup_value]
    
    if not match.empty:
        matches += 1
        for new_col, csv_col_index in column_mapping.items():
            df_excel.at[index, new_col] = match.iloc[0, csv_col_index]

# Enhanced mailed column logic - 14 for -1 in column D, 13 for others
df_excel['mailed'] = 13  # Default value is now 13
matches_found = 0

for index, row in df_excel.iterrows():
    lookup_value = row['lookup_col']
    matching_rows = df_presort[df_presort['presort_col'] == lookup_value]
    
    if not matching_rows.empty:
        col_d_value = matching_rows.iloc[0, 3]
        if pd.notna(col_d_value) and col_d_value == -1:
            df_excel.at[index, 'mailed'] = 14
            matches_found += 1
            print(f"Match found - Value: {lookup_value}, Col D value: {col_d_value}")

# Remove temporary columns
df_excel = df_excel.drop(['lookup_col'], axis=1)

# Remove columns with 'unnamed' in header
unnamed_cols = [col for col in df_excel.columns if 'unnamed' in col.lower()]
df_excel = df_excel.drop(columns=unnamed_cols)

# Save the updated file
output_file = os.path.join(work_dir, 'FHK_TERM_UPDATED.xlsx')
df_excel.to_excel(output_file, index=False)

print(f"\nProcessing complete! Found {matches_found} matches with Column D value of -1")
print(f"Removed {len(unnamed_cols)} unnamed columns")

# Process PRESORTLIST.csv to create PRESORTLIST_PRINT.csv
df_presort_print = df_presort[df_presort.iloc[:, 3] != -1]
presort_print_file = os.path.join(work_dir, 'PRESORTLIST_PRINT.csv')
df_presort_print.to_csv(presort_print_file, index=False)

print("Created PRESORTLIST_PRINT.csv with filtered rows")

print(f"\nProcessing complete! Found {matches_found} matches with Column D value of -1")
print(f"Removed {len(unnamed_cols)} unnamed columns")
