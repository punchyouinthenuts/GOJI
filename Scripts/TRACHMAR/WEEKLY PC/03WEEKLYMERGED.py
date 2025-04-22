import pandas as pd
import os
import re

# Define file paths
input_file = r"C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\WEEKLY\JOB\INPUT\FHK_WEEKLY.csv"
pcexp_file = r"C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\WEEKLY\JOB\OUTPUT\TM WEEKLYPCEXP.csv"
move_updates_file = r"C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\WEEKLY\JOB\OUTPUT\MOVE UPDATES.csv"
output_file = r"C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\WEEKLY\JOB\OUTPUT\FHK Weekly_Merged.csv"

# Function to normalize text (for both names and addresses)
def normalize_text(text):
    """Normalize text by removing extra spaces, punctuation, and converting to lowercase"""
    if pd.isna(text):
        return ""
    text = str(text).lower()
    text = re.sub(r'[^\w\s]', ' ', text)
    text = re.sub(r'\s+', ' ', text).strip()
    return text

# Function to normalize an address for comparison
def normalize_address(address1, address2=""):
    """Normalize address by combining lines and normalizing"""
    combined = normalize_text(str(address1) + " " + str(address2))
    return " ".join(sorted(combined.split()))

# Function to normalize a name
def normalize_name(name):
    """Normalize name by removing extra spaces and standardizing case"""
    return normalize_text(name)

# Read the CSV files
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
# --- Modified: Set mailed as integer directly ---
df_main['mailed'] = df_main['recno'].apply(
    lambda x: 13 if x in df_pcexp['recno'].values else 14
)
print(f"Mailed count: {len(df_main[df_main['mailed'] == 13])}")
print(f"Not mailed count: {len(df_main[df_main['mailed'] == 14])}")

# Process move updates with intelligent address matching
print("\nProcessing move updates with intelligent address matching...")

# Normalize names and addresses for all records
df_main['norm_name'] = df_main['hoh_guardian_name'].apply(normalize_name)
df_main['norm_address'] = df_main.apply(
    lambda row: normalize_address(row['member_address1'], row.get('member_address2', '')),
    axis=1
)

df_move['norm_name'] = df_move['Full Name'].apply(normalize_name)
df_move['norm_address_new'] = df_move.apply(
    lambda row: normalize_address(row.get('Address Line 1', ''), row.get('Address Line 2', '')),
    axis=1
)

# For each main record, look for potential matches in move updates
matched_indices = []
match_details = []

for idx, main_row in df_main.iterrows():
    for move_idx, move_row in df_move.iterrows():
        name_match = main_row['norm_name'] == move_row['norm_name']
        if not name_match:
            name_match = (main_row['norm_name'] in move_row['norm_name'] or
                          move_row['norm_name'] in main_row['norm_name'])
        if name_match:
            print(f"\nPotential match found:")
            print(f"Main: '{main_row['hoh_guardian_name']}' → Normalized: '{main_row['norm_name']}'")
            print(f"Move: '{move_row['Full Name']}' → Normalized: '{move_row['norm_name']}'")
            print(f"Name match: {name_match}")
            
            main_norm_addr = main_row['norm_address']
            move_norm_addr = move_row['norm_address_new']
            
            print(f"Main address: '{main_row['member_address1']} {main_row.get('member_address2', '')}'")
            print(f"Main normalized: '{main_norm_addr}'")
            print(f"Move address: '{move_row.get('Address Line 1', '')} {move_row.get('Address Line 2', '')}'")
            print(f"Move normalized: '{move_norm_addr}'")
            
            addr_match = (main_norm_addr == move_norm_addr or
                          main_norm_addr in move_norm_addr or
                          move_norm_addr in main_norm_addr)
            
            print(f"Address match: {addr_match}")
            
            if addr_match:
                matched_indices.append(idx)
                match_details.append({
                    'main_idx': idx,
                    'move_idx': move_idx,
                    'main_name': main_row['hoh_guardian_name'],
                    'move_name': move_row['Full Name'],
                    'main_addr1': main_row['member_address1'],
                    'main_addr2': main_row.get('member_address2', ''),
                    'move_new_addr1': move_row.get('Address Line 1', ''),
                    'move_new_addr2': move_row.get('Address Line 2', ''),
                    'move_city': move_row.get('City', ''),
                    'move_state': move_row.get('State', ''),
                    'move_zip': move_row.get('ZIP Code', '')
                })
                break

# Print matching statistics
print(f"\nTotal records in input file: {len(df_main)}")
print(f"Intelligently matched records: {len(matched_indices)}")

# Update the matched records
if matched_indices:
    print("\nMatched records with intelligent address comparison:")
    for match in match_details:
        idx = match['main_idx']
        df_main.loc[idx, 'new add'] = match['move_new_addr1']
        df_main.loc[idx, 'newadd2'] = match['move_new_addr2']
        city_state_zip = (
            str(match['move_city']) + ' ' +
            str(match['move_state']) + ' ' +
            str(match['move_zip'])
        ).strip()
        df_main.loc[idx, 'City State ZIP Code'] = city_state_zip
        
        print(f"Name: {match['main_name']} → {match['move_name']}")
        print(f"  Old Address: {match['main_addr1']}, {match['main_addr2']}")
        print(f"  New Address: {match['move_new_addr1']}, {match['move_new_addr2']}, {city_state_zip}")
        print()
    
    print(f"Updated {len(matched_indices)} records with new address information")
else:
    print("No matching records found. No address updates were made.")

# Remove temporary columns
if 'norm_name' in df_main.columns:
    df_main = df_main.drop(['norm_name', 'norm_address'], axis=1)

# --- Added Fix: Convert all numeric columns to Int64 to prevent floats ---
numeric_cols = df_main.select_dtypes(include=['int64', 'float64']).columns
for col in numeric_cols:
    df_main[col] = df_main[col].astype('Int64')

# Save the output file
df_main.to_csv(output_file, index=False)
print(f"\nOutput file saved with {len(df_main)} records to: {output_file}")
print("\nProcessing completed successfully.")