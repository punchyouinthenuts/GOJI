import pandas as pd
import os
import numpy as np

# Define file paths
input_file = r"C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\WEEKLY\JOB\INPUT\FHK_WEEKLY.csv"
pcexp_file = r"C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\WEEKLY\JOB\OUTPUT\TM WEEKLYPCEXP.csv"
move_updates_file = r"C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\WEEKLY\JOB\OUTPUT\MOVE UPDATES.csv"
output_file = r"C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\WEEKLY\JOB\OUTPUT\FHK Weekly_Merged.csv"

# Read the CSV files with error handling
print("Reading input files...")
df_main = pd.read_csv(input_file)
print(f"Main file loaded: {len(df_main)} records")

df_pcexp = pd.read_csv(pcexp_file)
print(f"PCEXP file loaded: {len(df_pcexp)} records")

df_move = pd.read_csv(move_updates_file)
print(f"Move updates file loaded: {len(df_move)} records")

# Add new columns to main dataframe
df_main['mailed'] = ''
df_main['new add'] = ''
df_main['newadd2'] = ''
df_main['City State ZIP Code'] = ''

# Process mailed/not mailed logic
print("\nProcessing mailed/not mailed data...")
# Convert to int to ensure type compatibility if needed
if 'recno' in df_main.columns and 'recno' in df_pcexp.columns:
    df_main['mailed'] = df_main['recno'].apply(
        lambda x: '13' if x in df_pcexp['recno'].values else '14'
    )
    print(f"Mailed count: {len(df_main[df_main['mailed'] == '13'])}")
    print(f"Not mailed count: {len(df_main[df_main['mailed'] == '14'])}")
else:
    print("Warning: 'recno' column not found in one of the files")

# Process move updates with safer approach
print("\nProcessing move updates data...")
if len(df_move) > 0:
    # First create lowercase versions of name fields for matching
    # Handle both string and non-string fields safely
    if 'hoh_guardian_name' in df_main.columns:
        df_main['name_lower'] = df_main['hoh_guardian_name'].fillna('').astype(str).str.lower()
    else:
        print("Warning: 'hoh_guardian_name' column not found in main file")
        df_main['name_lower'] = ''
    
    if 'Full Name' in df_move.columns:
        df_move['name_lower'] = df_move['Full Name'].fillna('').astype(str).str.lower()
    else:
        print("Warning: 'Full Name' column not found in move updates file")
        df_move['name_lower'] = ''
    
    # For address matching, we need to handle the numeric address case
    if 'member_address1' in df_main.columns:
        df_main['address_lower'] = df_main['member_address1'].fillna('').astype(str).str.lower()
    else:
        print("Warning: 'member_address1' column not found in main file")
        df_main['address_lower'] = ''
    
    # For the move updates file, we need to check which address column to use
    # Since 'Original Address Line 1' has NaN values, we should use 'Address Line 1' 
    if 'Address Line 1' in df_move.columns:
        df_move['address_lower'] = df_move['Address Line 1'].fillna('').astype(str).str.lower()
    elif 'Original Address Line 1' in df_move.columns:
        df_move['address_lower'] = df_move['Original Address Line 1'].fillna('').astype(str).str.lower()
    else:
        print("Warning: No address column found in move updates file")
        df_move['address_lower'] = ''
    
    # Print sample data for debugging
    print("\nSample matching data:")
    print("Main file:")
    print(df_main[['name_lower', 'address_lower']].head(2))
    print("Move updates file:")
    print(df_move[['name_lower', 'address_lower']].head(2))
    
    # Merge the data, being careful about which fields we use
    matches = pd.merge(
        df_main,
        df_move[['name_lower', 'address_lower', 'Address Line 1', 'Address Line 2', 
                'City', 'State', 'ZIP Code']].fillna(''),
        on=['name_lower', 'address_lower'],
        how='left',
        indicator=True
    )
    
    # Print matching statistics
    print(f"\nTotal records in input file: {len(df_main)}")
    print(f"Matched records: {len(matches[matches['_merge'] == 'both'])}")
    
    # Update only the matching records
    matching_mask = matches['_merge'] == 'both'
    
    if matching_mask.any():
        df_main.loc[matching_mask, 'new add'] = matches.loc[matching_mask, 'Address Line 1']
        df_main.loc[matching_mask, 'newadd2'] = matches.loc[matching_mask, 'Address Line 2']
        
        # Create city state zip with proper handling
        city_state_zip = (
            matches.loc[matching_mask, 'City'].fillna('').astype(str) + ' ' + 
            matches.loc[matching_mask, 'State'].fillna('').astype(str) + ' ' + 
            matches.loc[matching_mask, 'ZIP Code'].fillna('').astype(str)
        ).str.strip()
        
        df_main.loc[matching_mask, 'City State ZIP Code'] = city_state_zip
        print(f"Updated {matching_mask.sum()} records with new address information")
    else:
        print("No matching records found. No address updates were made.")
else:
    print("Move updates file is empty. No address updates were made.")

# Remove temporary columns
if 'name_lower' in df_main.columns:
    df_main = df_main.drop('name_lower', axis=1)
if 'address_lower' in df_main.columns:
    df_main = df_main.drop('address_lower', axis=1)

# Save the output file
df_main.to_csv(output_file, index=False)
print(f"\nOutput file saved with {len(df_main)} records to: {output_file}")
print("\nProcessing completed successfully.")