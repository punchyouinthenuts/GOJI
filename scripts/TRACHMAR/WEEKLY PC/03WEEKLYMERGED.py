import pandas as pd
import os
import re
import sys
import subprocess
from decimal import Decimal, InvalidOperation
# NOTE: PyQt5 dialog components removed - GOJI now handles file manager window natively

# Get job parameters from command line arguments
if len(sys.argv) == 4:  # Changed from >= to ==
    job_number = sys.argv[1]
    month = sys.argv[2]
    week = sys.argv[3]
    print(f"Job parameters: {job_number} {month}.{week}")
else:
    print("Error: Invalid parameters. Usage: script.py <job_number> <month> <week>")
    sys.exit(1)

CANONICAL_TM_WEEKLY_BASE = r"C:\Goji\AUTOMATION\TRACHMAR\WEEKLY PC"
LEGACY_TM_WEEKLY_BASE = r"C:\Goji\TRACHMAR\WEEKLY PC"

def resolve_tm_weekly_base_path():
    """Resolve WEEKLY PC runtime path with canonical-first + legacy fallback behavior."""
    configured_tm_base = os.environ.get("GOJI_TM_BASE_PATH", "").strip()
    if configured_tm_base:
        configured_weekly_path = (
            configured_tm_base
            if configured_tm_base.replace("\\", "/").upper().endswith("/WEEKLY PC")
            else os.path.join(configured_tm_base, "WEEKLY PC")
        )
        if os.path.exists(configured_weekly_path):
            return configured_weekly_path
        print(f"WARNING: Configured GOJI_TM_BASE_PATH not found: {configured_weekly_path}")

    if os.path.exists(CANONICAL_TM_WEEKLY_BASE):
        return CANONICAL_TM_WEEKLY_BASE

    if os.path.exists(LEGACY_TM_WEEKLY_BASE):
        print(
            "WARNING: Using legacy WEEKLY PC runtime path C:\\Goji\\TRACHMAR\\WEEKLY PC. "
            "Migrate to C:\\Goji\\AUTOMATION\\TRACHMAR\\WEEKLY PC."
        )
        return LEGACY_TM_WEEKLY_BASE

    os.makedirs(CANONICAL_TM_WEEKLY_BASE, exist_ok=True)
    print(f"WARNING: Created canonical WEEKLY PC runtime path: {CANONICAL_TM_WEEKLY_BASE}")
    return CANONICAL_TM_WEEKLY_BASE

weekly_base_path = resolve_tm_weekly_base_path()
input_file = os.path.join(weekly_base_path, "JOB", "INPUT", "FHK_WEEKLY.csv")
pcexp_file = os.path.join(weekly_base_path, "JOB", "OUTPUT", "TM WEEKLYPCEXP.csv")
move_updates_file = os.path.join(weekly_base_path, "JOB", "OUTPUT", "MOVE UPDATES.csv")
output_file = os.path.join(weekly_base_path, "JOB", "OUTPUT", "FHK Weekly_Merged.csv")
proof_dir = os.path.join(weekly_base_path, "JOB", "PROOF")
output_dir = os.path.join(weekly_base_path, "JOB", "OUTPUT")

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

def normalize_pallet_header_name(header_name):
    """Normalize pallet header variants (spacing, underscores, #) for matching."""
    normalized = str(header_name).strip().lower()
    normalized = normalized.replace("_", " ")
    normalized = normalized.replace("#", " number ")
    normalized = re.sub(r"\s+", " ", normalized)
    normalized = re.sub(r"[^a-z0-9 ]", " ", normalized)
    normalized = re.sub(r"\s+", " ", normalized).strip()
    return normalized

def find_pallet_number_column(columns):
    for column in columns:
        normalized = normalize_pallet_header_name(column)
        if normalized in {"pallet number", "pallet num", "pallet no"}:
            return column

        tokens = set(normalized.split())
        if "pallet" in tokens and (("number" in tokens) or ("num" in tokens) or ("no" in tokens)):
            return column
    return None

def is_effectively_negative_one(value):
    if pd.isna(value):
        return False

    normalized = re.sub(r"\s+", "", str(value).strip())
    if not normalized:
        return False

    try:
        return Decimal(normalized) == Decimal("-1")
    except InvalidOperation:
        return False

# Function to rename PDF file in PROOF directory
def rename_proof_pdf():
    """Rename the PDF file in the PROOF directory with job information"""
    try:
        if not os.path.exists(proof_dir):
            print(f"Warning: PROOF directory does not exist: {proof_dir}")
            return False
        
        # Look for PDF files matching the pattern
        pdf_files = []
        for filename in os.listdir(proof_dir):
            if filename.upper().endswith('.PDF') and 'WEEKLY' in filename.upper() and 'PROOF' in filename.upper():
                if '(SORTED)' in filename.upper() or '(UNSORTED)' in filename.upper():
                    pdf_files.append(filename)
        
        if not pdf_files:
            print("Warning: No matching PDF files found in PROOF directory")
            return False
        
        if len(pdf_files) > 1:
            print(f"Warning: Multiple PDF files found: {pdf_files}")
            print("Using the first one found")
        
        original_filename = pdf_files[0]
        original_path = os.path.join(proof_dir, original_filename)
        
        # Create new filename: [jobNumber] [month].[week] WEEKLY [SORTED/UNSORTED] PROOF.pdf
        base_name = os.path.splitext(original_filename)[0]
        # Extract SORTED or UNSORTED part if present
        sort_status = "SORTED" if "(SORTED)" in base_name.upper() else "UNSORTED"
        new_filename = f"{job_number} {month}.{week} WEEKLY {sort_status} PROOF.pdf"
        new_path = os.path.join(proof_dir, new_filename)
        
        # Rename the file
        os.rename(original_path, new_path)
        print(f"Renamed PDF: {original_filename} -> {new_filename}")
        return True
        
    except Exception as e:
        print(f"Error renaming PDF file: {str(e)}")
        return False

# NOTE: DragDropListWidget class removed - GOJI now handles drag-and-drop natively

# NOTE: FileManagerDialog class removed - GOJI now handles file manager dialog natively

# NOTE: show_file_manager_dialog() function removed - GOJI now handles file manager dialog natively
def show_file_manager_dialog():
    """Placeholder function - GOJI will handle the file manager dialog"""
    print(f"File manager will be shown by GOJI for:")
    print(f"  PROOF directory: {proof_dir}")
    print(f"  OUTPUT directory: {output_dir}")
    return True

# Read the CSV files
print("Reading input files...")
try:
    df_main = pd.read_csv(input_file)
    print(f"Main file loaded: {len(df_main)} records")
except Exception as e:
    print(f"Error reading main file: {str(e)}")
    sys.exit(1)

try:
    df_pcexp = pd.read_csv(pcexp_file)
    print(f"PCEXP file loaded: {len(df_pcexp)} records")
except Exception as e:
    print(f"Error reading PCEXP file: {str(e)}")
    sys.exit(1)

# Remove PCEXP rows where pallet number normalizes to -1 before mailed assignment.
pcexp_pallet_column = find_pallet_number_column(df_pcexp.columns)
if pcexp_pallet_column is None:
    print("WARNING: Pallet Number column not found in PCEXP; skipping pre-mailed -1 pallet cleanup.")
else:
    pcexp_negative_one_mask = df_pcexp[pcexp_pallet_column].apply(is_effectively_negative_one)
    removed_pcexp_rows = int(pcexp_negative_one_mask.sum())
    if removed_pcexp_rows > 0:
        df_pcexp = df_pcexp.loc[~pcexp_negative_one_mask].copy()
    print(
        f"Removed {removed_pcexp_rows} PCEXP row(s) where "
        f"{pcexp_pallet_column} normalizes to -1 before mailed assignment."
    )

    if removed_pcexp_rows > 0:
        temp_pcexp_file = f"{pcexp_file}.tmp"
        try:
            df_pcexp.to_csv(temp_pcexp_file, index=False)
            os.replace(temp_pcexp_file, pcexp_file)
            print(f"Rewrote cleaned PCEXP file: {pcexp_file}")
        except Exception as e:
            if os.path.exists(temp_pcexp_file):
                os.remove(temp_pcexp_file)
            print(f"Error rewriting cleaned PCEXP file: {str(e)}")
            sys.exit(1)

try:
    df_move = pd.read_csv(move_updates_file)
    print(f"Move updates file loaded: {len(df_move)} records")
except Exception as e:
    print(f"Error reading move updates file: {str(e)}")
    sys.exit(1)

# Add new columns to main dataframe
df_main['mailed'] = ''
df_main['new add'] = ''
df_main['newadd2'] = ''
df_main['City State ZIP Code'] = ''

# Process mailed/not mailed logic using UNIQUE ID instead of recno
print("\nProcessing mailed/not mailed data...")
# Check if UNIQUE ID column exists in both files
if 'UNIQUE ID' not in df_main.columns:
    print("Error: UNIQUE ID column not found in main file")
    sys.exit(1)

if 'UNIQUE ID' not in df_pcexp.columns:
    print("Error: UNIQUE ID column not found in PCEXP file")
    sys.exit(1)

# --- Modified: Set mailed as integer directly using UNIQUE ID ---
df_main['mailed'] = df_main['UNIQUE ID'].apply(
    lambda x: 13 if x in df_pcexp['UNIQUE ID'].values else 14
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
            print(f"Main: '{main_row['hoh_guardian_name']}' -> Normalized: '{main_row['norm_name']}'")
            print(f"Move: '{move_row['Full Name']}' -> Normalized: '{move_row['norm_name']}'")
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
        
        print(f"Name: {match['main_name']} -> {match['move_name']}")
        print(f"  Old Address: {match['main_addr1']}, {match['main_addr2']}")
        print(f"  New Address: {match['move_new_addr1']}, {match['move_new_addr2']}, {city_state_zip}")
        print()
    
    print(f"Updated {len(matched_indices)} records with new address information")
else:
    print("No matching records found. No address updates were made.")

# Remove temporary columns
if 'norm_name' in df_main.columns:
    df_main = df_main.drop(['norm_name', 'norm_address'], axis=1)

# --- Fixed Bug: Convert all numeric columns to Int64 to prevent floats ---
numeric_cols = df_main.select_dtypes(include=['int64', 'float64']).columns
for col in numeric_cols:
    df_main[col] = df_main[col].astype('Int64')

# Remove UNIQUE ID column from the final merged output
if 'UNIQUE ID' in df_main.columns:
    df_main_final = df_main.drop('UNIQUE ID', axis=1)
    print(f"Removed UNIQUE ID column from final merged output")
else:
    df_main_final = df_main
    print(f"Warning: UNIQUE ID column not found in dataframe")

pallet_column = find_pallet_number_column(df_main_final.columns)
if pallet_column is None:
    print("WARNING: Pallet Number column not found in final dataset; skipping -1 pallet filtering.")
else:
    pallet_filter_mask = df_main_final[pallet_column].apply(is_effectively_negative_one)
    removed_rows = int(pallet_filter_mask.sum())
    if removed_rows > 0:
        df_main_final = df_main_final.loc[~pallet_filter_mask].copy()
    print(f"Removed {removed_rows} row(s) where {pallet_column} normalizes to -1.")

# Save the output file (without UNIQUE ID column)
try:
    df_main_final.to_csv(output_file, index=False)
    print(f"\nOutput file saved with {len(df_main_final)} records to: {output_file}")
except Exception as e:
    print(f"Error saving output file: {str(e)}")
    sys.exit(1)

# Rename PDF file in PROOF directory
print("\nRenaming PDF file in PROOF directory...")
pdf_renamed = rename_proof_pdf()

# REPLACED: Instead of opening Explorer windows, show Qt modal dialog
print("\nOpening file manager dialog...")
try:
    show_file_manager_dialog()
except Exception as e:
    print(f"Error opening file manager dialog: {str(e)}")
    # Fallback to original behavior if Qt fails
    print("Falling back to Explorer windows...")
    if pdf_renamed:
        subprocess.run(['explorer', proof_dir], check=True)
    subprocess.run(['explorer', output_dir], check=True)

print("\nProcessing completed successfully.")
