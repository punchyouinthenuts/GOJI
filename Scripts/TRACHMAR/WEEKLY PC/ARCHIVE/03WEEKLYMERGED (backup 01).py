import pandas as pd
import os

# Define file paths
input_file = r"C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\WEEKLY\JOB\INPUT\FHK_WEEKLY.csv"
pcexp_file = r"C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\WEEKLY\JOB\OUTPUT\TM WEEKLYPCEXP.csv"
move_updates_file = r"C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\WEEKLY\JOB\OUTPUT\MOVE UPDATES.csv"
output_file = r"C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\WEEKLY\JOB\OUTPUT\FHK Weekly_Merged.csv"

# Read the CSV files
df_main = pd.read_csv(input_file)
df_pcexp = pd.read_csv(pcexp_file)
df_move = pd.read_csv(move_updates_file)

# Convert matching columns to lowercase for case-insensitive comparison
df_main['hoh_guardian_name_lower'] = df_main['hoh_guardian_name'].str.lower()
df_main['member_address1_lower'] = df_main['member_address1'].str.lower()
df_move['Full Name_lower'] = df_move['Full Name'].str.lower()
df_move['Original Address Line 1_lower'] = df_move['Original Address Line 1'].str.lower()

# Add new columns
df_main['mailed'] = ''
df_main['new add'] = ''
df_main['newadd2'] = ''
df_main['City State ZIP Code'] = ''

# Process mailed/not mailed logic
df_main['mailed'] = df_main['recno'].map(
    lambda x: '13' if x in df_pcexp['recno'].values else '14'
)

# Print sample data to verify matching conditions
print("Sample from input file:")
print(df_main[['hoh_guardian_name_lower', 'member_address1_lower']].head())
print("\nSample from move updates file:")
print(df_move[['Full Name_lower', 'Original Address Line 1_lower']].head())

# Create the matches with explicit column mapping
matches = pd.merge(
    df_main,
    df_move[['Full Name_lower', 'Original Address Line 1_lower', 'Address Line 1', 'Address Line 2', 'City', 'State', 'ZIP Code']],
    left_on=['hoh_guardian_name_lower', 'member_address1_lower'],
    right_on=['Full Name_lower', 'Original Address Line 1_lower'],
    how='left',
    indicator=True
)

# Print matching statistics
print(f"\nTotal records in input file: {len(df_main)}")
print(f"Matched records: {len(matches[matches['_merge'] == 'both'])}")

# Update only the matching records
matching_mask = matches['_merge'] == 'both'
df_main.loc[matching_mask, 'new add'] = matches.loc[matching_mask, 'Address Line 1']
df_main.loc[matching_mask, 'newadd2'] = matches.loc[matching_mask, 'Address Line 2']
df_main.loc[matching_mask, 'City State ZIP Code'] = (
    matches.loc[matching_mask, 'City'].fillna('') + ' ' + 
    matches.loc[matching_mask, 'State'].fillna('') + ' ' + 
    matches.loc[matching_mask, 'ZIP Code'].fillna('')
).str.strip()

# Remove temporary lowercase columns
df_main = df_main.drop(['hoh_guardian_name_lower', 'member_address1_lower'], axis=1)

# Save with verification
df_main.to_csv(output_file, index=False)
print(f"\nOutput file saved with {len(df_main)} records")
