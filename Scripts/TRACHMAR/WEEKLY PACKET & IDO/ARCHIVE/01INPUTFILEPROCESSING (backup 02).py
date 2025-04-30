import os
import csv
import shutil
import re

# Define directories
RAW_FILES_DIR = r"C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\DATA PROCESSING\RAW FILES"
PROCESSED_DIR = os.path.join(RAW_FILES_DIR, "PROCESSED")

# Ensure PROCESSED directory exists
if not os.path.exists(PROCESSED_DIR):
    os.makedirs(PROCESSED_DIR)

def is_number(s):
    """
    Check if a string represents a number (integer or float).
    """
    return re.match(r'^-?\d+\.?\d*$', s) is not None

def convert_to_int_if_number(s):
    """
    Convert a string to an integer if it represents a number, otherwise return unchanged.
    """
    if is_number(s):
        try:
            return str(int(float(s)))
        except ValueError:
            return s
    return s

def process_full_file(file_path):
    """
    Process a FULL file, splitting by language and member3_id, ensuring no decimals.
    """
    file_name = os.path.basename(file_path)
    date_part = file_name.split('_')[-1].replace('.csv', '')

    # Read the FULL file
    with open(file_path, 'r', newline='') as f:
        reader = csv.DictReader(f)
        rows = list(reader)
        header = reader.fieldnames

    # Convert all numeric values to integers
    rows = [{k: convert_to_int_if_number(v) for k, v in row.items()} for row in rows]

    # Split into English and Spanish rows
    english_rows = [row for row in rows if row['language_indicator'].strip().lower() == 'english']
    spanish_rows = [row for row in rows if row['language_indicator'].strip().lower() == 'spanish']

    # Write English file
    english_file = os.path.join(RAW_FILES_DIR, f"FHK_Full_English_{date_part}.csv")
    with open(english_file, 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=header)
        writer.writeheader()
        writer.writerows(english_rows)

    # Write Spanish file
    spanish_file = os.path.join(RAW_FILES_DIR, f"02 FHK_Full_Spanish_{date_part}.csv")
    with open(spanish_file, 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=header)
        writer.writeheader()
        writer.writerows(spanish_rows)

    # Process language-specific files further
    process_language_file(english_file, 'English', date_part)
    process_language_file(spanish_file, 'Spanish', date_part)

    # Move original file to PROCESSED
    shutil.move(file_path, os.path.join(PROCESSED_DIR, file_name))

def process_language_file(lang_file, lang, date_part):
    """
    Split language-specific file by member3_id, ensuring no decimals.
    """
    with open(lang_file, 'r', newline='') as f:
        reader = csv.DictReader(f)
        rows = list(reader)
        header = reader.fieldnames

    # Split based on member3_id
    has_member3_rows = [row for row in rows if row['member3_id'].strip()]
    no_member3_rows = [row for row in rows if not row['member3_id'].strip()]

    # Write 1-2 file (no member3_id)
    one_two_file = os.path.join(RAW_FILES_DIR, f"FHK_Full_{lang}-1-2_{date_part}.csv")
    with open(one_two_file, 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=header)
        writer.writeheader()
        writer.writerows(no_member3_rows)

    # Write 3-4 file (has member3_id), if applicable
    if has_member3_rows:
        three_four_file = os.path.join(RAW_FILES_DIR, f"FHK_Full_{lang}-3-4_{date_part}.csv")
        with open(three_four_file, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=header)
            writer.writeheader()
            writer.writerows(has_member3_rows)

    # Move language file to PROCESSED
    shutil.move(lang_file, os.path.join(PROCESSED_DIR, os.path.basename(lang_file)))

def process_ido_file(file_path):
    """
    Process an IDO file, splitting by member3_id, ensuring no decimals.
    """
    file_name = os.path.basename(file_path)
    date_part = file_name.split('_')[-1].replace('.csv', '')

    # Read the IDO file
    with open(file_path, 'r', newline='') as f:
        reader = csv.DictReader(f)
        rows = list(reader)
        header = reader.fieldnames

    # Convert all numeric values to integers
    rows = [{k: convert_to_int_if_number(v) for k, v in row.items()} for row in rows]

    # Split based on member3_id
    has_member3_rows = [row for row in rows if row['member3_id'].strip()]
    no_member3_rows = [row for row in rows if not row['member3_id'].strip()]

    # Write 1-2 file (no member3_id)
    one_two_file = os.path.join(RAW_FILES_DIR, f"FHK_IDO-1-2_{date_part}.csv")
    with open(one_two_file, 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=header)
        writer.writeheader()
        writer.writerows(no_member3_rows)

    # Write 3-4 file (has member3_id), if applicable
    if has_member3_rows:
        three_four_file = os.path.join(RAW_FILES_DIR, f"FHK_IDO-3-4_{date_part}.csv")
        with open(three_four_file, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=header)
            writer.writeheader()
            writer.writerows(has_member3_rows)

    # Move original file to PROCESSED
    shutil.move(file_path, os.path.join(PROCESSED_DIR, file_name))

# Scan and process files
files = [f for f in os.listdir(RAW_FILES_DIR) if f.endswith('.csv') and ("FHK_Full" in f or "FHK_IDO" in f)]
for file in files:
    file_path = os.path.join(RAW_FILES_DIR, file)
    if "FHK_Full" in file:
        process_full_file(file_path)
    elif "FHK_IDO" in file:
        process_ido_file(file_path)