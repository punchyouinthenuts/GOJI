# BROKEN APPOINTMENTS – 01 INITIAL (HB-style, GOJI-friendly, headless)
# Save to: C:\Goji\scripts\TRACHMAR\BROKEN APPOINTMENTS\01 INITIAL.py
#
# Behavior (mirrors HB initial):
# - Scans "INPUT ZIP" for 1 CSV / many CSVs / one ZIP (extracts)
# - Adds a MATCHID column to each CSV (unique 2-letter prefix per run)
# - If >1 CSV, combines into DATA\INPUT\INPUT.csv; if 1 CSV, copies to INPUT.csv
# - Moves originals to DATA\ORIGINAL (timestamped backups if needed)
# - No interactive prompts; clean rollback on failure; exits 0/1

import os
import zipfile
import glob
import csv
import shutil
import sys
import time
import traceback
import random
import string

CANONICAL_TM_ROOT = r"C:\Goji\AUTOMATION\TRACHMAR"

def resolve_tm_root():
    if os.path.isdir(CANONICAL_TM_ROOT):
        return CANONICAL_TM_ROOT
    os.makedirs(CANONICAL_TM_ROOT, exist_ok=True)
    print("INFO: created canonical TRACHMAR root C:\\Goji\\AUTOMATION\\TRACHMAR.")
    return CANONICAL_TM_ROOT

def generate_match_id_prefix():
    return ''.join(random.choices(string.ascii_uppercase, k=2))

def add_match_id_column(csv_file_path, match_id_prefix, start_number):
    temp_file_path = csv_file_path + '.temp'
    with open(csv_file_path, 'r', newline='', encoding='utf-8-sig') as f_in:
        csv_reader = csv.reader(f_in)
        with open(temp_file_path, 'w', newline='', encoding='utf-8') as f_out:
            csv_writer = csv.writer(f_out)
            headers = next(csv_reader)
            new_headers = ['MATCHID'] + headers
            csv_writer.writerow(new_headers)
            current_number = start_number
            for row in csv_reader:
                if not any((cell or '').strip() for cell in row):
                    continue
                match_id = f"{match_id_prefix}{current_number:05d}"
                new_row = [match_id] + row
                csv_writer.writerow(new_row)
                current_number += 1
    os.replace(temp_file_path, csv_file_path)
    return current_number

def rollback(zip_path, moved_files, created_files):
    print("\nROLLING BACK CHANGES...")
    for file_path in list(created_files):
        if os.path.exists(file_path):
            try:
                if os.path.isdir(file_path):
                    try: os.rmdir(file_path)
                    except OSError: pass
                else:
                    os.remove(file_path)
            except Exception as e:
                print(f"Failed to remove {file_path}: {e}")
    for dest_path, orig_path in moved_files:
        if os.path.exists(dest_path):
            try:
                if os.path.exists(orig_path): os.remove(orig_path)
                shutil.move(dest_path, orig_path)
            except Exception as e:
                print(f"Failed to restore {orig_path}: {e}")
    if zip_path and not os.path.exists(zip_path):
        print(f"Note: original ZIP was removed earlier: {zip_path}")
    print("ROLLBACK COMPLETE")

def main():
    zip_path=None; moved_files=[]; created_files=[]
    try:
        match_id_prefix=generate_match_id_prefix()
        tm_root = resolve_tm_root()
        base_dir = os.path.join(tm_root, "BROKEN APPOINTMENTS")
        zip_input_dir = os.path.join(base_dir, "INPUT ZIP")
        data_input_dir = os.path.join(base_dir, "DATA", "INPUT")
        data_original_dir = os.path.join(base_dir, "DATA", "ORIGINAL")
        os.makedirs(data_input_dir,exist_ok=True)
        os.makedirs(data_original_dir,exist_ok=True)
        zip_files=glob.glob(os.path.join(zip_input_dir,'*.zip'))
        csv_files=glob.glob(os.path.join(zip_input_dir,'*.csv'))
        if len(zip_files)==0 and len(csv_files)==1:
            single_csv=csv_files[0]
            add_match_id_column(single_csv,match_id_prefix,1)
            output_path=os.path.join(data_input_dir,'INPUT.csv')
            if os.path.exists(output_path):
                shutil.move(output_path,output_path+f".backup_{int(time.time())}")
            shutil.copy2(single_csv,output_path); created_files.append(output_path)
            dest_path=os.path.join(data_original_dir,os.path.basename(single_csv))
            if os.path.exists(dest_path):
                shutil.move(dest_path,dest_path+f".backup_{int(time.time())}")
            shutil.move(single_csv,dest_path); moved_files.append((dest_path,single_csv))
        elif len(zip_files)==0 and len(csv_files)>1:
            csv_files.sort(); cur=1
            for csv_file in csv_files: cur=add_match_id_column(csv_file,match_id_prefix,cur)
            output_path=os.path.join(data_input_dir,'INPUT.csv')
            if os.path.exists(output_path):
                shutil.move(output_path,output_path+f".backup_{int(time.time())}")
            with open(csv_files[0],'r',newline='',encoding='utf-8') as f: headers=next(csv.reader(f))
            with open(output_path,'w',newline='',encoding='utf-8') as f_out:
                writer=csv.writer(f_out); writer.writerow(headers)
                for csv_file in csv_files:
                    with open(csv_file,'r',newline='',encoding='utf-8') as f_in:
                        reader=csv.reader(f_in); next(reader)
                        for row in reader:
                            if any((cell or '').strip() for cell in row[1:]): writer.writerow(row)
                    dest_path=os.path.join(data_original_dir,os.path.basename(csv_file))
                    if os.path.exists(dest_path):
                        shutil.move(dest_path,dest_path+f".backup_{int(time.time())}")
                    shutil.move(csv_file,dest_path); moved_files.append((dest_path,csv_file))
            created_files.append(output_path)
        elif len(zip_files)==1:
            zip_path=zip_files[0]; temp_extract_dir=os.path.join(os.path.dirname(zip_path),'temp_extract')
            os.makedirs(temp_extract_dir,exist_ok=True); created_files.append(temp_extract_dir)
            with zipfile.ZipFile(zip_path,'r') as z: z.extractall(temp_extract_dir)
            extracted_csv_files=[os.path.join(root,f) for root,_,files in os.walk(temp_extract_dir) for f in files if f.lower().endswith('.csv')]
            if not extracted_csv_files: raise Exception("No CSVs in ZIP")
            if len(extracted_csv_files)==1:
                csv_file=extracted_csv_files[0]; add_match_id_column(csv_file,match_id_prefix,1)
                output_path=os.path.join(data_input_dir,'INPUT.csv')
                if os.path.exists(output_path):
                    shutil.move(output_path,output_path+f".backup_{int(time.time())}")
                shutil.copy2(csv_file,output_path); created_files.append(output_path)
                dest=os.path.join(data_original_dir,os.path.basename(csv_file))
                if os.path.exists(dest): shutil.move(dest,dest+f".backup_{int(time.time())}")
                shutil.move(csv_file,dest); moved_files.append((dest,csv_file))
            else:
                extracted_csv_files.sort(); cur=1
                for csv_file in extracted_csv_files: cur=add_match_id_column(csv_file,match_id_prefix,cur)
                output_path=os.path.join(data_input_dir,'INPUT.csv')
                if os.path.exists(output_path):
                    shutil.move(output_path,output_path+f".backup_{int(time.time())}")
                with open(extracted_csv_files[0],'r',newline='',encoding='utf-8') as f: headers=next(csv.reader(f))
                with open(output_path,'w',newline='',encoding='utf-8') as f_out:
                    writer=csv.writer(f_out); writer.writerow(headers)
                    for csv_file in extracted_csv_files:
                        with open(csv_file,'r',newline='',encoding='utf-8') as f_in:
                            reader=csv.reader(f_in); next(reader)
                            for row in reader:
                                if any((cell or '').strip() for cell in row[1:]): writer.writerow(row)
                        dest=os.path.join(data_original_dir,os.path.basename(csv_file))
                        if os.path.exists(dest): shutil.move(dest,dest+f".backup_{int(time.time())}")
                        shutil.move(csv_file,dest); moved_files.append((dest,csv_file))
                created_files.append(output_path)
            shutil.rmtree(temp_extract_dir,ignore_errors=True)
            if temp_extract_dir in created_files: created_files.remove(temp_extract_dir)
            os.remove(zip_path)
        elif len(zip_files)>1: raise Exception("Multiple ZIP files found")
        else: raise Exception("No ZIP or CSV found")
        print("INITIAL PROCESSING COMPLETED SUCCESSFULLY!"); sys.exit(0)
    except Exception as e:
        print("ERROR:",e); traceback.print_exc(); rollback(zip_path,moved_files,created_files); sys.exit(1)

if __name__=="__main__": main()

