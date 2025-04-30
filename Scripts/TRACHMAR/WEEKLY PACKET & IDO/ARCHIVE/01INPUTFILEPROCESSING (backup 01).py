import os
import csv
import shutil

# Define directories
RAW_FILES_DIR = r"C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\DATA PROCESSING\RAW FILES"
PROCESSED_DIR = os.path.join(RAW_FILES_DIR, "PROCESSED")

# Ensure PROCESSED directory exists
if not os.path.exists(PROCESSED_DIR):
    os.makedirs(PROCESSED_DIR)

def process_full_file(file_path):
    """
    Process a FULL file by splitting it into English and Spanish files based on language_indicator,
    then further splitting those based on member3_id.
    """
    file_name = os.path.basename(file_path)
    date_part = file_name.split('_')[-1].replace('.csv', '')

    # Read the FULL file
    with open(file_path, 'r', newline='') as f:
        reader = csv.DictReader(f)
        rows = list(reader)
        header = reader.fieldnames

    # Split into English and Spanish rows
    english_rows = [row for row in rows if row['language_indicator'].strip().lower() == 'english']
    spanish_rows = [row for row in rows if row['language_indicator'].strip().lower() == 'spanish']

    # Write English file
    english_file = os.path.join(RAW_FILES_DIR, f"FHK_Full_English_{date_part}.csv")
    with open(english_file, 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=header)
        writer.writeheader()
        writer.writerows(english_rows)

    # Write Spanish file (Note: Query specifies '02 ' prefix for Spanish)
    spanish_file = os.path.join(RAW_FILES_DIR, f"02 FHK_Full_Spanish_{date_part}.csv")
    with open(spanish_file, 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=header)
        writer.writeheader()
        writer.writerows(spanish_rows)

    # Process language-specific files
    process_language_file(english_file, 'English', date_part)
    process_language_file(spanish_file, 'Spanish', date_part)

    # Move the original FULL file to PROCESSED
    shutil.move(file_path, os.path.join(PROCESSED_DIR, file_name))

def process_language_file(lang_file, lang, date_part):
    """
    Process a language-specific file by splitting it based on member3_id into 1-2 and 3-4 files.
    """
    with open(lang_file, 'r', newline='') as f:
        reader = csv.DictReader(f)
        rows = list(reader)
        header = reader.fieldnames

    # Split based on member3_id
    has_member3_rows = [row for row in rows if row['member3_id'].strip()]
    no_member3_rows = [row for row in rows if not row['member3_id'].strip()]

    # Write 1-2 file (rows without member3_id data)
    one_two_file = os.path.join(RAW_FILES_DIR, f"FHK_Full_{lang}-1-2_{date_part}.csv")
    with open(one_two_file, 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=header)
        writer.writeheader()
        writer.writerows(no_member3_rows)

    # Write 3-4 file if there are rows with member3_id data
    if has_member3_rows:
        three_four_file = os.path.join(RAW_FILES_DIR, f"FHK_Full_{lang}-3-4_{date_part}.csv")
        with open(three_four_file, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=header)
            writer.writeheader()
            writer.writerows(has_member3_rows)

    # Move the language-specific file to PROCESSED
    shutil.move(lang_file, os.path.join(PROCESSED_DIR, os.path.basename(lang_file)))

def process_ido_file(file_path):
    """
    Process an IDO file by splitting it based on member3_id into 1-2 and 3-4 files.
    """
    file_name = os.path.basename(file_path)
    date_part = file_name.split('_')[-1].replace('.csv', '')

    # Read the IDO file
    with open(file_path, 'r', newline='') as f:
        reader = csv.DictReader(f)
        rows = list(reader)
        header = reader.fieldnames

    # Split based on member3_id
    has_member3_rows = [row for row in rows if row['member3_id'].strip()]
    no_member3_rows = [row for row in rows if not row['member3_id'].strip()]

    # Write 1-2 file (rows without member3_id data)
    one_two_file = os.path.join(RAW_FILES_DIR, f"FHK_IDO-1-2_{date_part}.csv")
    with open(one_two_file, 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=header)
        writer.writeheader()
        writer.writerows(no_member3_rows)

    # Write 3-4 file if there are rows with member3_id data
    if has_member3_rows:
        three_four_file = os.path.join(RAW_FILES_DIR, f"FHK_IDO-3-4_{date_part}.csv")
        with open(three_four_file, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=header)
            writer.writeheader()
            writer.writerows(has_member3_rows)

    # Move the original IDO file to PROCESSED
    shutil.move(file_path, os.path.join(PROCESSED_DIR, file_name))

# Scan directory and process files
files = [f for f in os.listdir(RAW_FILES_DIR) if f.endswith('.csv') and ("FHK_Full" in f or "FHK_IDO" in f)]

for file in files:
    file_path = os.path.join(RAW_FILES_DIR, file)
    if "FHK_Full" in file:
        process_full_file(file_path)
    elif "FHK_IDO" in file:
        process_ido_file(file_path)