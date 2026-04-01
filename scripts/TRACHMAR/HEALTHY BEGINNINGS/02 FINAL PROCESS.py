import sys
import csv
import datetime
import traceback
import re
import logging
import hashlib
import time
import shutil
import zipfile
import os
import argparse
from pathlib import Path
from typing import Optional, List, Dict, Any

def parse_mode():
    p = argparse.ArgumentParser()
    p.add_argument("job_number")
    p.add_argument("year")
    p.add_argument("--mode", choices=["prearchive","archive"], default="prearchive")
    return p.parse_args()

def get_base_paths(year: str) -> Dict[str, str]:
    """Get all required file paths based on correct directory structure."""
    # Script is in C:\\Goji\\scripts\\TRACHMAR\\HEALTHY BEGINNINGS\\02 FINAL PROCESS.py
    # Base directory should be C:\\Goji\\TRACHMAR\\HEALTHY BEGINNINGS
    script_path = Path(__file__).resolve()
    # Go up to C:\\Goji\\scripts, then navigate to C:\\Goji\\TRACHMAR\\HEALTHY BEGINNINGS
    goji_root = script_path.parents[3]  # Gets us to C:\\Goji
    base_dir = goji_root / "TRACHMAR" / "HEALTHY BEGINNINGS"
    
    print(f"[DEBUG] script_path: {script_path}")
    print(f"[DEBUG] goji_root: {goji_root}")
    print(f"[DEBUG] base_dir: {base_dir}")
    
    paths = {
        'input_csv': str(base_dir / "DATA" / "INPUT" / "INPUT.csv"),
        'output_dir': str(base_dir / "DATA" / "OUTPUT"),
        'original_dir': str(base_dir / "DATA" / "ORIGINAL"), 
        'merged_dir': str(base_dir / "DATA" / "MERGED"),
        'archive_dir': str(base_dir / "ARCHIVE"),
        'base_dir': str(base_dir),
        'tmhb_code_list': str(base_dir / "DATA" / "OUTPUT" / "TMHB14 CODE LIST.csv"),
        'network_base': f"\\\\NAS1069D9\\AMPrintData\\{year}_SrcFiles\\T\\Trachmar"
    }
    
    # Verify the paths exist
    print(f"[DEBUG] Checking paths:")
    for key, path in paths.items():
        if key != 'network_base':  # Skip network path check
            path_obj = Path(path)
            if key.endswith('_dir'):
                exists = path_obj.exists() and path_obj.is_dir()
                print(f"[DEBUG] {key}: {path} - {'EXISTS' if exists else 'MISSING'}")
            else:
                exists = path_obj.exists() and path_obj.is_file()
                print(f"[DEBUG] {key}: {path} - {'EXISTS' if exists else 'MISSING'}")
    
    return paths

def to_proper_case(text):
    """Convert text to proper case with specific exceptions for certain words."""
    if not text or str(text).strip() == '' or str(text).lower() == 'nan':
        return text
    
    # Words that should remain lowercase unless they're the first word
    lowercase_exceptions = {'a', 'an', 'and', 'as', 'at', 'but', 'by', 'for', 'in', 'nor', 'of', 'on', 'or', 'the', 'up'}
    
    # Split the text into words
    words = str(text).split()
    if not words:
        return text
    
    result_words = []
    for i, word in enumerate(words):
        # Clean word of punctuation for comparison but keep original for processing
        clean_word = word.lower().strip('.,!?;:\"()[]{}')
        
        if i == 0:
            # First word is always capitalized
            result_words.append(word.capitalize())
        elif clean_word in lowercase_exceptions:
            # Exception words stay lowercase (unless first word)
            result_words.append(word.lower())
        else:
            # All other words get proper case
            result_words.append(word.capitalize())
    
    return ' '.join(result_words)

def format_phone_number(phone_str):
    """Format phone number to (XXX) XXX-XXXX if exactly 10 digits."""
    if not phone_str or str(phone_str).strip() == '' or str(phone_str).lower() == 'nan':
        return phone_str
    
    digits = re.sub(r'\D', '', str(phone_str))
    
    if len(digits) == 10:
        return f"({digits[:3]}) {digits[3:6]}-{digits[6:]}"
    else:
        return phone_str

def validate_csv_headers(file_path: str, required_headers: List[str]) -> bool:
    """Validate CSV headers."""
    try:
        file_path_obj = Path(file_path)
        if not file_path_obj.exists():
            raise FileNotFoundError(f"CSV file not found: {file_path}")
        if file_path_obj.stat().st_size == 0:
            raise ValueError(f"CSV file is empty: {file_path}")
        
        with file_path_obj.open('r', encoding='utf-8-sig') as f:
            reader = csv.reader(f)
            headers = next(reader)
            headers = [h.strip() for h in headers]
        
        missing_headers = [h for h in required_headers if h not in headers]
        if missing_headers:
            raise ValueError(f"Missing required headers in {file_path}: {missing_headers}")
        
        print(f"INFO: Validated headers for {Path(file_path).name}")
        return True
        
    except Exception as e:
        print(f"ERROR: Header validation failed: {e}")
        raise

def verify_file_copy(src: str, dst: str):
    """Verify that the copied file matches the source using SHA256."""
    def get_file_hash(file_path):
        sha256 = hashlib.sha256()
        with open(file_path, 'rb') as f:
            for chunk in iter(lambda: f.read(4096), b''):
                sha256.update(chunk)
        return sha256.hexdigest()
    
    src_hash = get_file_hash(src)
    dst_hash = get_file_hash(dst)
    if src_hash != dst_hash:
        raise ValueError(f"File copy verification failed: {dst}")
    
    print(f"INFO: Verified file copy: {Path(dst).name}")

def find_existing_job_folder(base_path: str, job_number: str):
    """Find existing job folder that contains the job number."""
    try:
        base_path_obj = Path(base_path)
        if not base_path_obj.exists():
            return None
        
        for item in base_path_obj.iterdir():
            if item.is_dir() and job_number in item.name:
                print(f"INFO: Found existing job folder: {item.name}")
                return item
        
        return None
    except (FileNotFoundError, PermissionError) as e:
        print(f"WARNING: Error searching for existing job folder: {e}")
        return None

def get_next_original_number(directory: str, base_filename: str) -> str:
    """Find the next available (original XX) number for file renaming."""
    directory = Path(directory)
    base_name = Path(base_filename).stem
    extension = Path(base_filename).suffix
    
    for i in range(1, 100):
        number_str = f"{i:02d}"
        test_name = f"{base_name} (original {number_str}){extension}"
        if not (directory / test_name).exists():
            return test_name
    
    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    return f"{base_name} (original {timestamp}){extension}"

def create_and_process_job_copy(paths: Dict[str, str], job_number: str) -> Optional[str]:
    """Create job-numbered copy and process."""
    try:
        print("INFO: Starting job-numbered file processing...")
        
        original_file_path = paths['output_dir'] + "/TRACHMAR HEALTHY BEGINNINGS.csv"
        job_numbered_filename = f"{job_number} TRACHMAR HEALTHY BEGINNINGS.csv"
        job_numbered_path = paths['output_dir'] + "/" + job_numbered_filename
        
        print(f"[DEBUG] Looking for original file at: {original_file_path}")
        print(f"[DEBUG] Will create job-numbered file at: {job_numbered_path}")
        
        if not Path(original_file_path).exists():
            raise FileNotFoundError(f"Original file not found: {original_file_path}")
        
        if Path(job_numbered_path).exists():
            raise FileExistsError(f"Job-numbered file already exists: {job_numbered_filename}")
        
        print("INFO: Creating job-numbered copy...")
        shutil.copy2(original_file_path, job_numbered_path)
        
        validate_csv_headers(original_file_path, ["MATCHID"])
        validate_csv_headers(paths['input_csv'], 
                           ["MATCHID", "Location_Name", "Office_Address", "Office_Address_1", 
                            "Office_City", "Office_State", "Office_Zip", "Office_Phone_Number"])
        
        input_data = {}
        with open(paths['input_csv'], 'r', newline='', encoding='utf-8-sig') as input_file:
            input_reader = csv.DictReader(input_file, delimiter=',')
            if not input_reader.fieldnames:
                raise ValueError(f"No headers found in INPUT.csv")
            
            print(f"INFO: INPUT.csv headers: {list(input_reader.fieldnames)}")
            
            required_cols = ["MATCHID", "Location_Name", "Office_Address", "Office_Address_1", "Office_City", "Office_State", "Office_Zip", "Office_Phone_Number"]
            missing_cols = [col for col in required_cols if col not in input_reader.fieldnames]
            
            if missing_cols:
                raise ValueError(f"Input CSV missing columns: {missing_cols}")
            
            row_count = 0
            for row in input_reader:
                match_id = row.get('MATCHID', '').strip()
                if match_id:
                    input_data[match_id] = {
                        'Location_Name': row.get('Location_Name', ''),
                        'Office_Address': row.get('Office_Address', ''),
                        'Office_Address_1': row.get('Office_Address_1', ''),
                        'Office_City': row.get('Office_City', ''),
                        'Office_State': row.get('Office_State', ''),
                        'Office_Zip': row.get('Office_Zip', ''),
                        'Office_Phone_Number': row.get('Office_Phone_Number', '')
                    }
                row_count += 1
            
            print(f"INFO: Loaded {len(input_data)} unique MATCHID entries from {row_count} total rows")
            
            if not input_data:
                raise ValueError("No valid MATCHID data found in Input CSV")
        
        temp_file_path = job_numbered_path + '.temp'
        
        with open(job_numbered_path, 'r', newline='', encoding='utf-8-sig') as f_in, \
             open(temp_file_path, 'w', newline='', encoding='utf-8') as f_out:
            reader = csv.DictReader(f_in, delimiter=',')
            
            if not reader.fieldnames:
                raise ValueError(f"No headers found in {job_numbered_filename}")
            
            print(f"INFO: Original headers: {list(reader.fieldnames)}")
            
            if "MATCHID" not in reader.fieldnames:
                raise ValueError(f"MATCHID column not found in {job_numbered_filename}")
            
            original_fields = list(reader.fieldnames)
            matchid_idx = original_fields.index("MATCHID")
            
            new_columns = ["Location_Name", "Office_Address", "Office_Address_1", "Office_City", "Office_State", "Office_Zip", "Office_Phone_Number"]
            new_fieldnames = original_fields[:matchid_idx] + new_columns + original_fields[matchid_idx:]
            
            writer = csv.DictWriter(f_out, fieldnames=new_fieldnames)
            writer.writeheader()
            
            print(f"INFO: Updated headers: {new_fieldnames}")
            
            processed_count = 0
            matched_count = 0
            phone_formatted_count = 0
            location_name_updated_count = 0
            bad_rows_removed = 0
            all_rows = []
            
            for row in reader:
                match_id = row.get('MATCHID', '').strip()
                
                for col in new_columns:
                    row[col] = ''
                
                if match_id and match_id in input_data:
                    data = input_data[match_id]
                    formatted_phone = format_phone_number(data['Office_Phone_Number'])
                    if str(formatted_phone) != str(data['Office_Phone_Number']):
                        phone_formatted_count += 1
                    
                    row['Location_Name'] = to_proper_case(str(data['Location_Name']))
                    row['Office_Address'] = to_proper_case(str(data['Office_Address']))
                    row['Office_Address_1'] = to_proper_case(str(data['Office_Address_1']))
                    row['Office_City'] = to_proper_case(str(data['Office_City']))
                    row['Office_State'] = str(data['Office_State']).upper()
                    row['Office_Zip'] = data['Office_Zip']
                    row['Office_Phone_Number'] = formatted_phone
                    
                    matched_count += 1
                
                if not row['Location_Name'].strip():
                    row['Location_Name'] = "Please contact DentaQuest at the number below for more information."
                    location_name_updated_count += 1
                
                all_rows.append(row)
                processed_count += 1
            
            while all_rows:
                last_row = all_rows[-1]
                endorsement_empty = not str(last_row.get('Endorsement Line', '')).strip()
                im_numeric_empty = not str(last_row.get('IM Barcode Numeric', '')).strip()
                im_barcode_empty = not str(last_row.get('IM Barcode', '')).strip()
                
                if endorsement_empty and im_numeric_empty and im_barcode_empty:
                    all_rows.pop()
                    bad_rows_removed += 1
                    print("INFO: Removed bad row from end of file")
                else:
                    break
            
            for row in all_rows:
                writer.writerow(row)
        
        Path(job_numbered_path).unlink()
        Path(temp_file_path).rename(job_numbered_path)
        
        print(f"SUCCESS: Updated job-numbered file with {len(all_rows)} rows (removed {bad_rows_removed} bad rows)")
        print(f"INFO: Matched {matched_count} records with INPUT.csv data")
        print(f"INFO: Formatted {phone_formatted_count} phone numbers")
        print(f"INFO: Updated {location_name_updated_count} Location_Name records")
        print("INFO: Original TRACHMAR HEALTHY BEGINNINGS.csv remains untouched")
        
        return job_numbered_path
    
    except Exception as e:
        print(f"ERROR: {e}")
        traceback.print_exc()
        return None

def process_original_files(paths: Dict[str, str]) -> bool:
    """Process files in ORIGINAL folder to format phone numbers only."""
    try:
        input_csv_path = paths['input_csv']
        original_dir = paths['original_dir']
        
        input_data = {}
        with open(input_csv_path, 'r', newline='', encoding='utf-8-sig') as input_file:
            input_reader = csv.DictReader(input_file, delimiter=',')
            for row in input_reader:
                match_id = row['MATCHID']
                input_data[match_id] = row.get('Office_Phone_Number', '')
        
        original_files = list(Path(original_dir).glob("*.csv"))
        if not original_files:
            raise FileNotFoundError("No CSV files found in ORIGINAL folder")
        
        total_phone_updates = 0
        
        for orig_file in original_files:
            print(f"INFO: Processing ORIGINAL file: {orig_file.name}")
            
            validate_csv_headers(str(orig_file), ["MATCHID", "Office_Phone_Number"])
            
            temp_file = orig_file.with_suffix('.temp')
            phone_updates = 0
            
            with orig_file.open('r', newline='', encoding='utf-8-sig') as f_in, \
                 temp_file.open('w', newline='', encoding='utf-8') as f_out:
                reader = csv.DictReader(f_in, delimiter=',')
                writer = csv.DictWriter(f_out, fieldnames=reader.fieldnames)
                writer.writeheader()
                
                for row in reader:
                    match_id = row.get('MATCHID', '')
                    if match_id in input_data:
                        original_phone = row.get('Office_Phone_Number', '')
                        formatted_phone = format_phone_number(input_data[match_id])
                        if formatted_phone != input_data[match_id]:
                            phone_updates += 1
                        row['Office_Phone_Number'] = formatted_phone
                    writer.writerow(row)
            
            orig_file.unlink()
            temp_file.rename(orig_file)
            
            total_phone_updates += phone_updates
            print(f"INFO: Formatted {phone_updates} phone numbers")
        
        print(f"SUCCESS: Total phone numbers formatted across all ORIGINAL files: {total_phone_updates}")
        
        return True
    
    except Exception as e:
        print(f"ERROR: {e}")
        traceback.print_exc()
        return False

def copy_to_network_folder(paths: Dict[str, str], job_number: str, job_numbered_file_path: str) -> bool:
    """Copy to network with fallback to local directory."""
    max_attempts = 3
    wait_seconds = 2
    attempt = 1
    
    while attempt <= max_attempts:
        try:
            print("INFO: Preparing network copy operation...")
            
            job_numbered_path = Path(job_numbered_file_path)
            base_path = Path(f"{paths['network_base']}")
            
            existing_folder = find_existing_job_folder(str(base_path), job_number)
            
            if existing_folder:
                job_folder = existing_folder
                print(f"INFO: Using existing job folder: {job_folder.name}")
            else:
                job_folder = base_path / f"{job_number}_HealthyBeginnings"
                print(f"INFO: Creating new job folder: {job_folder.name}")
            
            data_folder = job_folder / "HP Indigo" / "DATA"
            
            if not data_folder.exists():
                data_folder.mkdir(parents=True)
                print(f"INFO: Created network folder structure: {data_folder}")
            
            dest_file = data_folder / job_numbered_path.name
            
            if dest_file.exists():
                print(f"WARNING: File already exists: {dest_file.name}")
                new_name = get_next_original_number(str(data_folder), dest_file.name)
                old_file_path = dest_file
                new_file_path = data_folder / new_name
                old_file_path.rename(new_file_path)
                print(f"INFO: Renamed existing file to: {new_name}")
            
            shutil.copy2(job_numbered_path, dest_file)
            verify_file_copy(str(job_numbered_path), str(dest_file))
            print(f"SUCCESS: Copied file to network location: {dest_file}")

            print("=== NAS_FOLDER_PATH ===")
            print(str(data_folder))
            print("=== END_NAS_FOLDER_PATH ===")
            
            return True
        
        except (FileNotFoundError, PermissionError, ValueError, OSError) as e:
            print(f"WARNING: Network copy attempt {attempt}/{max_attempts} failed: {e}")
            if attempt == max_attempts:
                fallback_dir = Path(r"C:\Users\JCox\Desktop\MOVE TO NETWORK DRIVE")
                fallback_dir.mkdir(exist_ok=True)
                dest_file = fallback_dir / job_numbered_path.name
                shutil.copy2(job_numbered_path, dest_file)
                verify_file_copy(str(job_numbered_path), str(dest_file))
                
                intended_path = Path(f"{paths['network_base']}\\{job_number}_HealthyBeginnings\\HP Indigo\\DATA")
                txt_file = dest_file.with_suffix('.txt')
                with txt_file.open('w', encoding='utf-8') as f:
                    f.write(f"Intended network location: {intended_path}")
                
                print(f"SUCCESS: Copied file to fallback location: {dest_file}")
                print(f"INFO: Created companion text file: {txt_file.name}")

                print("=== NAS_FOLDER_PATH ===")
                print(str(fallback_dir))
                print("=== END_NAS_FOLDER_PATH ===")
                
                return True
            attempt += 1
            time.sleep(wait_seconds)

def process_merged_files(paths: Dict[str, str], job_number: str) -> List[str]:
    """Process files in ORIGINAL folder, create MERGED copies, and mark them."""
    try:
        tmhb_path = paths['tmhb_code_list']
        original_dir = paths['original_dir']
        merged_dir = paths['merged_dir']
        
        merged_dir_path = Path(merged_dir)
        if not merged_dir_path.exists():
            merged_dir_path.mkdir(parents=True)
            print(f"INFO: Created directory: {merged_dir_path.name}")
        
        validate_csv_headers(tmhb_path, ["MATCHID"])
        
        tmhb_match_ids = set()
        with open(tmhb_path, 'r', newline='', encoding='utf-8-sig') as f:
            reader = csv.DictReader(f, delimiter=',')
            for row in reader:
                if 'MATCHID' in row:
                    tmhb_match_ids.add(row['MATCHID'])
        
        if not tmhb_match_ids:
            raise ValueError("No MATCHID values found in TMHB14 CODE LIST.csv")
        
        original_files = list(Path(original_dir).glob("*.csv"))
        if not original_files:
            raise FileNotFoundError("No CSV files found in ORIGINAL folder")
        
        merged_files = []
        marked_14_count = 0
        
        for orig_file in original_files:
            validate_csv_headers(str(orig_file), ["MATCHID"])
            
            filename = orig_file.name
            merged_filename = orig_file.stem + "_MERGED.csv"
            merged_file_path = merged_dir_path / merged_filename
            
            with orig_file.open('r', newline='', encoding='utf-8-sig') as f, \
                 merged_file_path.open('w', newline='', encoding='utf-8') as out_f:
                reader = csv.DictReader(f, delimiter=',')
                
                fieldnames = [field for field in reader.fieldnames if field != 'MATCHID']
                fieldnames.append("Mailed")
                
                writer = csv.DictWriter(out_f, fieldnames=fieldnames)
                writer.writeheader()
                
                for row in reader:
                    match_id = row.get('MATCHID', '')
                    del row['MATCHID']
                    
                    if match_id in tmhb_match_ids:
                        row['Mailed'] = "14"
                        marked_14_count += 1
                    else:
                        row['Mailed'] = "13"
                    
                    writer.writerow(row)
            
            merged_files.append(str(merged_file_path))
            print(f"INFO: Created merged file: {merged_filename}")
        
        if marked_14_count != len(tmhb_match_ids):
            print(f"WARNING: Number of records marked with 14 ({marked_14_count}) "
                  f"does not match the number of MATCHIDs in TMHB14 CODE LIST ({len(tmhb_match_ids)})")
        else:
            print(f"SUCCESS: Verification successful: {marked_14_count} records marked with 14 matches TMHB14 CODE LIST count")
        
        print(f"INFO: Total files processed: {len(original_files)}")
        print(f"INFO: Total records marked with 14: {marked_14_count}")
        
        return merged_files
    
    except Exception as e:
        print(f"ERROR: {e}")
        traceback.print_exc()
        return []

def zip_merged_files(job_number: str, merged_files: List[str]) -> Optional[str]:
    """Zip merged files if more than one exists."""
    if len(merged_files) <= 1:
        print("INFO: Only one or no merged files found, skipping ZIP function")
        return None
    
    try:
        merged_dir = Path(merged_files[0]).parent
        zip_name = f"{job_number} TM HEALTHY BEGINNINGS (MERGED).zip"
        zip_path = merged_dir / zip_name
        
        if zip_path.exists():
            timestamp = datetime.datetime.now().strftime("%Y%m%d-%H%M")
            zip_name = f"{job_number} TM HEALTHY BEGINNINGS (MERGED)_{timestamp}.zip"
            zip_path = merged_dir / zip_name
            print(f"INFO: Using timestamped filename: {zip_name}")
        
        with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED) as zipf:
            for file in merged_files:
                file_path = Path(file)
                zipf.write(file_path, file_path.name)
        
        with zipfile.ZipFile(zip_path, 'r') as zipf:
            if zipf.testzip() is not None:
                raise ValueError("ZIP file is corrupted")
        
        print(f"SUCCESS: Created ZIP file: {zip_path.name}")
        
        for file in merged_files:
            file_path = Path(file)
            file_path.unlink()
            print(f"INFO: Deleted original CSV after zipping: {file_path.name}")
        
        return str(zip_path)
    
    except Exception as e:
        print(f"ERROR: {e}")
        traceback.print_exc()
        return None

def zip_data_folders(paths: Dict[str, str], job_number: str) -> bool:
    """Zip all folders in DATA directory and save to ARCHIVE."""
    try:
        data_dir = Path(paths['base_dir']) / "DATA"
        archive_dir = Path(paths['archive_dir'])
        
        if not archive_dir.exists():
            archive_dir.mkdir(parents=True)
            print(f"INFO: Created archive directory: {archive_dir.name}")
        
        zip_name = f"{job_number} TM HEALTHY BEGINNINGS.zip"
        zip_path = archive_dir / zip_name
        if zip_path.exists():
            raise FileExistsError(f"Archive ZIP already exists: {zip_path}")
        
        with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED) as zipf:
            for root, dirs, files in os.walk(data_dir):
                for file in files:
                    file_path = Path(root) / file
                    rel_path = file_path.relative_to(data_dir.parent)
                    zipf.write(file_path, rel_path)
        
        with zipfile.ZipFile(zip_path, 'r') as zipf:
            if zipf.testzip() is not None:
                raise ValueError("ZIP file is corrupted")
        
        print(f"SUCCESS: Created archive ZIP: {zip_path.name}")
        
        for root, dirs, files in os.walk(data_dir):
            for file in files:
                file_path = Path(root) / file
                file_path.unlink()
                print(f"INFO: Removed file: {file_path.name}")
        
        print("SUCCESS: Cleared contents of DATA folders")
        return True
    
    except Exception as e:
        print(f"ERROR: {e}")
        traceback.print_exc()
        return False

if __name__ == "__main__":
    args = parse_mode()
    job_number, year = args.job_number, args.year
    mode = args.mode
    
    try:
        print("INFO: FINAL PROCESS script started")
        paths = get_base_paths(year)
        
        if mode == "prearchive":
            print("INFO: Running HEALTHY BEGINNINGS prearchive phase")
            
            print("INFO: Step 1: Creating job-numbered copy...")
            job_numbered_file_path = create_and_process_job_copy(paths, job_number)
            if not job_numbered_file_path:
                print("ERROR: Failed to create job-numbered copy")
                sys.exit(1)
            
            print("INFO: Step 2: Processing ORIGINAL files...")
            if not process_original_files(paths):
                print("ERROR: Failed to process ORIGINAL files")
                sys.exit(1)
            
            print("INFO: Step 3: Copying to network location...")
            if not copy_to_network_folder(paths, job_number, job_numbered_file_path):
                print("ERROR: Failed to copy to network location")
                sys.exit(1)
            
            print("INFO: Step 4: Processing MERGED files...")
            merged_files = process_merged_files(paths, job_number)
            if not merged_files:
                print("ERROR: Failed to process MERGED files")
                sys.exit(1)
            
            print("=== PAUSE_FOR_EMAIL ===")
            sys.exit(0)
        
        elif mode == "archive":
            print("INFO: Running HEALTHY BEGINNINGS archive phase")
            
            merged_dir_path = Path(paths['merged_dir'])
            merged_files = [str(f) for f in merged_dir_path.glob("*_MERGED.csv")]
            
            print("INFO: Step 5: Creating ZIP files...")
            zip_merged_files(job_number, merged_files)
            
            print("INFO: Step 6: Creating archive...")
            if not zip_data_folders(paths, job_number):
                print("ERROR: Failed to create archive")
                sys.exit(1)
            
            print("SUCCESS: ALL PROCESSES COMPLETED SUCCESSFULLY!")
        
    except Exception as e:
        print(f"CRITICAL ERROR: {e}")
        traceback.print_exc()
        sys.exit(1)
