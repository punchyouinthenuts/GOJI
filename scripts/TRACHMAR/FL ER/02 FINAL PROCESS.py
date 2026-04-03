# 02 FINAL PROCESS.py (GOJI)
# Fixes applied:
# 1) Preserve leading zeros in UNIQUE ID by forcing dtype='string' and stripping whitespace on all relevant pd.read_csv calls (code list and ORIGINAL files).
# 2) Add validation guard: if no UNIQUE ID matches are found (all MAILED would be 13), raise and abort instead of writing incorrect _MERGED.csv files.

import sys
import os
import shutil
import pandas as pd
import zipfile
import time
from datetime import datetime
import argparse

CANONICAL_TM_ROOT = r"C:\Goji\AUTOMATION\TRACHMAR"
LEGACY_TM_ROOT = r"C:\Goji\TRACHMAR"

def resolve_tm_root():
    if os.path.isdir(CANONICAL_TM_ROOT):
        return CANONICAL_TM_ROOT
    if os.path.isdir(LEGACY_TM_ROOT):
        print("WARNING: using legacy TRACHMAR root C:\\Goji\\TRACHMAR; migrate to C:\\Goji\\AUTOMATION\\TRACHMAR.")
        return LEGACY_TM_ROOT
    os.makedirs(CANONICAL_TM_ROOT, exist_ok=True)
    print("INFO: created canonical TRACHMAR root C:\\Goji\\AUTOMATION\\TRACHMAR.")
    return CANONICAL_TM_ROOT

def parse_mode():
    parser = argparse.ArgumentParser()
    parser.add_argument("job_number")
    parser.add_argument("year")
    parser.add_argument("month")
    parser.add_argument("--mode", choices=["prearchive","archive"], default="prearchive")
    return parser.parse_args()

class CSVMergerProcessor:
    def __init__(self):
        # ✅ GOJI paths
        base_dir = os.path.join(resolve_tm_root(), "FL ER")
        self.data_dir     = os.path.join(base_dir, "DATA")
        self.original_dir = os.path.join(self.data_dir, "ORIGINAL")  # ✅ Correct: originals come from DATA\ORIGINAL
        self.archive_dir  = os.path.join(base_dir, "ARCHIVE")
        self.w_drive      = r"C:\Users\JCox\Desktop\PPWK Temp"
        self.fallback_dir = r"C:\Users\JCox\Desktop\MOVE TO BUSKRO"

        self.rollback_data = {
            'created_files': [],
            'copied_files': [],
            'renamed_files': [],
            'deleted_files': [],
            'backup_files': [],
            'original_data_contents': []
        }

    # -------------------------
    # Backup & Rollback methods
    # -------------------------
    def create_backup(self, file_path, backup_suffix="backup"):
        try:
            if os.path.exists(file_path):
                timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
                backup_path = f"{file_path}.{backup_suffix}_{timestamp}"
                shutil.copy2(file_path, backup_path)
                self.rollback_data['backup_files'].append(backup_path)
                return backup_path
        except Exception as e:
            raise Exception(f"Error creating backup for {file_path}: {str(e)}")

    def backup_directory_contents(self):
        try:
            if os.path.exists(self.data_dir):
                for file_name in os.listdir(self.data_dir):
                    file_path = os.path.join(self.data_dir, file_name)
                    if os.path.isfile(file_path):
                        self.rollback_data['original_data_contents'].append({
                            'name': file_name,
                            'path': file_path,
                            'backup_path': self.create_backup(file_path, "original")
                        })
        except Exception as e:
            raise Exception(f"Error backing up directory contents: {str(e)}")

    def rollback(self):
        print("\n" + "="*60)
        print("ERROR DETECTED - INITIATING ROLLBACK")
        print("="*60)

        try:
            for file_path in self.rollback_data['created_files']:
                if os.path.exists(file_path):
                    os.remove(file_path)
                    print(f"[OK] Removed created file: {os.path.basename(file_path)}")

            for copy_info in self.rollback_data['copied_files']:
                if os.path.exists(copy_info['destination']):
                    os.remove(copy_info['destination'])
                    print(f"[OK] Removed copied file: {copy_info['destination']}")

            for rename_info in self.rollback_data['renamed_files']:
                current_path = rename_info['new_path']
                original_path = rename_info['original_path']
                if os.path.exists(current_path) and not os.path.exists(original_path):
                    shutil.move(current_path, original_path)
                    print(f"[OK] Restored renamed file: {os.path.basename(original_path)}")

            for file_info in self.rollback_data['original_data_contents']:
                original_path = file_info['path']
                backup_path = file_info['backup_path']
                if os.path.exists(backup_path):
                    os.makedirs(os.path.dirname(original_path), exist_ok=True)
                    shutil.copy2(backup_path, original_path)
                    print(f"[OK] Restored file: {file_info['name']}")

            for backup_file in self.rollback_data['backup_files']:
                if os.path.exists(backup_file):
                    os.remove(backup_file)

            print("[OK] Rollback completed successfully - All changes reverted")

        except Exception as e:
            print(f"ERROR during rollback: {str(e)}")
            print("Manual cleanup may be required")

    def cleanup_backup_files(self):
        for backup_file in self.rollback_data['backup_files']:
            if os.path.exists(backup_file):
                os.remove(backup_file)

    # -------------------------
    # Core file operations
    # -------------------------
    def find_original_files(self):
        """Look for originals in DATA\ORIGINAL."""
        try:
            if not os.path.exists(self.original_dir):
                raise FileNotFoundError(f"ORIGINAL directory not found: {self.original_dir}")

            csv_files = []
            for file_name in os.listdir(self.original_dir):
                if file_name.endswith('.csv') and not file_name.endswith('_MERGED.csv'):
                    csv_files.append(os.path.join(self.original_dir, file_name))

            if not csv_files:
                raise FileNotFoundError("No original CSV files found in ORIGINAL folder")

            return csv_files

        except Exception as e:
            raise Exception(f"Error finding original files: {str(e)}")

    def create_merged_files(self, original_files):
        """Create _MERGED files for all original files with MAILED column populated
           — adapted from the working non-GOJI version, with correct GOJI paths."""
        try:
            code_list_path = os.path.join(self.data_dir, "TMFLER14 CODE LIST.csv")
            if not os.path.exists(code_list_path):
                raise FileNotFoundError("TMFLER14 CODE LIST.csv not found")

            df_code_list = pd.read_csv(code_list_path, encoding='utf-8', dtype={'UNIQUE ID': 'string'})
            df_code_list['UNIQUE ID'] = df_code_list['UNIQUE ID'].astype('string').str.strip()
            code_list_unique_ids = set(df_code_list['UNIQUE ID'])
            code_list_count = len(df_code_list)

            merged_files = []
            matched_unique_ids = set()

            for original_file in original_files:
                df_original = pd.read_csv(original_file, encoding='cp1252', dtype={'UNIQUE ID': 'string'})

                df_original['UNIQUE ID'] = df_original['UNIQUE ID'].astype('string').str.strip()

                mailed_values = []
                file_matches = 0
                for unique_id in df_original['UNIQUE ID']:
                    if unique_id in code_list_unique_ids:
                        mailed_values.append(14)
                        file_matches += 1
                        matched_unique_ids.add(unique_id)
                    else:
                        mailed_values.append(13)

                df_original['MAILED'] = mailed_values

                if 'UNIQUE ID' in df_original.columns:
                    df_original = df_original.drop('UNIQUE ID', axis=1)

                base_name = os.path.splitext(os.path.basename(original_file))[0]
                merged_file_name = f"{base_name}_MERGED.csv"
                merged_file_path = os.path.join(self.data_dir, merged_file_name)

                df_original.to_csv(merged_file_path, index=False, encoding='cp1252')

                self.rollback_data['created_files'].append(merged_file_path)
                merged_files.append(merged_file_path)

                print(f"[OK] Created merged file: {merged_file_name}")
                print(f"[OK] File contains {file_matches} non-mailed matches from code list")

            total_unique_matches = len(matched_unique_ids)
            print(f"[INFO] Code list total: {code_list_count}")
            print(f"[INFO] Unique non-mailed IDs matched in job data: {total_unique_matches}")

            if total_unique_matches == 0:
                raise Exception("Validation failed: No UNIQUE ID values from the code list matched the job data. Aborting to prevent all-13 MERGED output.")
            elif total_unique_matches < code_list_count:
                diff = code_list_count - total_unique_matches
                print(f"[WARNING] {diff} non-mailed IDs in code list not found in job data — continuing.")
            else:
                print("[OK] All non-mailed IDs from code list found in job data.")

            print(f"[OK] MERGED file(s) successfully written to {self.data_dir}")
            return merged_files

        except Exception as e:
            raise Exception(f"Error creating merged files: {str(e)}")

    def rename_trachmar_file(self, job_number):
        """Rename TRACHMAR FL ER.csv with job number and max sort position."""
        try:
            trachmar_path = os.path.join(self.data_dir, "TRACHMAR FL ER.csv")
            if not os.path.exists(trachmar_path):
                raise FileNotFoundError("TRACHMAR FL ER.csv not found")

            self.create_backup(trachmar_path, "trachmar_original")

            df = pd.read_csv(trachmar_path, encoding='utf-8')
            max_sort_position = df['Sort Position'].max()

            new_filename = f"{job_number} TRACHMAR FL ER_{max_sort_position}.csv"
            new_path = os.path.join(self.data_dir, new_filename)

            shutil.move(trachmar_path, new_path)

            self.rollback_data['renamed_files'].append({
                'original_path': trachmar_path,
                'new_path': new_path
            })

            print(f"[OK] Renamed TRACHMAR FL ER.csv to: {new_filename}")
            return new_path

        except Exception as e:
            raise Exception(f"Error renaming TRACHMAR file: {str(e)}")

    def copy_to_destination(self, file_path):
        """Copy renamed file to W: drive or fallback location."""
        try:
            filename = os.path.basename(file_path)
            if os.path.exists(self.w_drive):
                destination = os.path.join(self.w_drive, filename)
                destination_name = "W:\\"
            else:
                os.makedirs(self.fallback_dir, exist_ok=True)
                destination = os.path.join(self.fallback_dir, filename)
                destination_name = "MOVE TO BUSKRO folder"

            shutil.copy2(file_path, destination)
            self.rollback_data['copied_files'].append({'source': file_path, 'destination': destination})

            print(f"[OK] Copied file to: {destination_name}")
            return destination

        except Exception as e:
            raise Exception(f"Error copying file to destination: {str(e)}")

    def create_archive(self, job_number, merged_files):
        """Create ZIP archive of entire DATA folder, including subfolders like ORIGINAL."""
        try:
            os.makedirs(self.archive_dir, exist_ok=True)

            if len(merged_files) > 1:
                zip_name = f"{job_number} TM FL ER (MERGED).zip"
                zip_path = os.path.join(self.archive_dir, zip_name)
                with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
                    for f in merged_files:
                        zf.write(f, os.path.basename(f))
                print(f"[OK] Created MERGED ZIP: {zip_name}")
                for f in merged_files:
                    os.remove(f)
                print(f"[OK] Removed {len(merged_files)} individual merged files after zipping")
            else:
                print("Single merged file — no ZIP created.")

            timestamp = datetime.now().strftime('%Y%m%d-%H%M')
            zip_filename = f"{job_number} TM FL ER {timestamp}.zip"
            zip_path = os.path.join(self.archive_dir, zip_filename)

            with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED) as zipf:
                for root, _, files in os.walk(self.data_dir):
                    for file in files:
                        file_path = os.path.join(root, file)
                        arcname = os.path.relpath(file_path, self.data_dir)
                        zipf.write(file_path, arcname)

            self.rollback_data['created_files'].append(zip_path)
            print(f"[OK] Created archive: {zip_filename}")
            return zip_path

        except Exception as e:
            raise Exception(f"Error creating archive: {str(e)}")

    def delete_data_files(self):
        """Recursively delete all files and folders from DATA directory."""
        try:
            deleted_count = 0
            for root, dirs, files in os.walk(self.data_dir, topdown=False):
                for file in files:
                    file_path = os.path.join(root, file)
                    os.remove(file_path)
                    deleted_count += 1
                for dir in dirs:
                    dir_path = os.path.join(root, dir)
                    os.rmdir(dir_path)
            print(f"[OK] Deleted {deleted_count} files and all subfolders from DATA directory")
        except Exception as e:
            raise Exception(f"Error deleting DATA files: {str(e)}")

    def run_prearchive(self, job_number, year, month):
        """Phase 1: Create merged files and pause for email"""
        try:
            print("CSV MERGER PROCESSOR - PREARCHIVE PHASE")
            print("="*50)
            print(f"INFO: Job={job_number}, Year={year}, Month={month}")

            print("Creating backups...")
            self.backup_directory_contents()

            print("\nFinding original CSV files...")
            original_files = self.find_original_files()
            print(f"[OK] Found {len(original_files)} original file(s):")
            for file_path in original_files:
                print(f"  - {os.path.basename(file_path)}")

            print("\nCreating MERGED files...")
            merged_files = self.create_merged_files(original_files)

            print("\n" + "="*50)
            print(f"[OK] {len(merged_files)} MERGED file(s) created and validated")
            print("=== NAS_FOLDER_PATH ===")
            print(self.data_dir)
            print("=== END_NAS_FOLDER_PATH ===")
            print("=== PAUSE_FOR_EMAIL ===")
            sys.exit(0)

        except Exception as e:
            print(f"\nFATAL ERROR: {str(e)}")
            self.rollback()
            sys.exit(1)

    def run_archive(self, job_number, year, month):
        """Phase 2: Archive and cleanup after email sent"""
        try:
            print("CSV MERGER PROCESSOR - ARCHIVE PHASE")
            print("="*50)
            print(f"INFO: Job={job_number}, Year={year}, Month={month}")

            merged_files = []
            for file_name in os.listdir(self.data_dir):
                if file_name.endswith('_MERGED.csv'):
                    merged_files.append(os.path.join(self.data_dir, file_name))

            print("\nRenaming TRACHMAR FL ER.csv...")
            renamed_file = self.rename_trachmar_file(job_number)

            print("\nCopying to destination...")
            self.copy_to_destination(renamed_file)

            print("\nCreating archive...")
            self.create_archive(job_number, merged_files)

            print("\nDeleting files from DATA directory...")
            self.delete_data_files()

            print("\n" + "="*50)
            print("SUCCESS! All operations completed successfully")
            print("="*50)
            print("[OK] TRACHMAR file renamed and copied")
            print("[OK] Archive created")
            print("[OK] DATA directory cleaned")

            self.cleanup_backup_files()
            print("\nPROCESS COMPLETE! TERMINATING...")
            time.sleep(2)

        except Exception as e:
            print(f"\nFATAL ERROR: {str(e)}")
            self.rollback()
            print("\nScript terminated due to error.")
            print("TERMINATING...")
            time.sleep(2)
            sys.exit(1)

if __name__ == "__main__":
    args = parse_mode()
    job_number, year, month = args.job_number, args.year, args.month
    mode = args.mode

    processor = CSVMergerProcessor()

    if mode == "prearchive":
        processor.run_prearchive(job_number, year, month)
    elif mode == "archive":
        processor.run_archive(job_number, year, month)
