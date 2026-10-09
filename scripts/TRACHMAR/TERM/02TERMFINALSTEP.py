#!/usr/bin/env python3
"""
TM TERM Final Step (Headless, corrected)

Requirements kept:
- Headless (no tkinter/popups)
- CLI args: job_number, month_abbrev, year, and options --work-dir, --archive-root, --backup-dir, --network-base
- First line JSON payload, followed by legacy NAS markers
- Exit code 0 on success, 1 on failure
- NAS job folder naming: "<job>_<MON>" (underscore, no year) into "<network_base>/<job>_<MON>/HP Indigo/DATA"
- Archive folder naming (TERM): "<archive_root>/<job> <MON>"
- Deterministic overwrite: timestamp suffix for delivered and archived files when destination exists
- Class-based structure: BackupManager, FileMover, TERMProcessor

Corrections implemented:
1) Back up *all three* inputs (FHK_TERM.xlsx, MOVE UPDATES.csv, PRESORTLIST.csv) with BackupManager.create().
   - cleanup() after outputs are written and *before* archiving/moves
   - rollback() on exception
2) Reconcile complete households through member IDs in existing ID1..IDn fields.
   - Assign mailed=13 only for a verified printable household, 14 for pallet -1.
   - Fail before writing/delivery on missing, duplicate or mixed household records.
3) Stats JSON now reports both presort sizes:
   - presort_records (raw input size of PRESORTLIST.csv)
   - presort_print_records (filtered printable subset length)
4) Apply move updates to the original household identified by the existing ID fields.
   - Preserve full address mapping and the single-address-field export variant.
"""

import argparse
import json
import os
import shutil
import sys
import re
from datetime import datetime

import pandas as pd


# ---------- Utilities ----------

def timestamp_suffix() -> str:
    return datetime.now().strftime('%Y%m%d%H%M%S')

def text_value(value):
    return '' if pd.isna(value) else str(value).strip()


def dependent_ids(row, columns, location):
    """Read the member number at the end of each existing dependent display field."""
    members = []
    for column in columns:
        value = text_value(row[column])
        if not value:
            continue
        match = re.search(r'(?:^|[\s,])([0-9]+)$', value)
        if not match:
            raise ValueError(f"{location}: invalid member ID in {column}")
        members.append(match.group(1))
    if not members:
        raise ValueError(f"{location}: no member IDs in ID fields")
    if len(members) != len(set(members)):
        raise ValueError(f"{location}: duplicate member IDs")
    return members


# ---------- Backup Manager ----------

class BackupManager:
    """
    Create .bak copies of inputs we touch; provides rollback and cleanup.
    """
    def __init__(self):
        self._created = []  # list[(original, backup)]

    def create(self, path: str) -> None:
        if not os.path.exists(path):
            return  # nothing to backup
        bak = f"{path}.bak"
        if os.path.exists(bak):
            root, ext = os.path.splitext(bak)
            bak = f"{root}_{timestamp_suffix()}{ext}"
        shutil.copy2(path, bak)
        self._created.append((path, bak))

    def rollback(self) -> None:
        # Restore originals from .bak (best-effort)
        for original, bak in self._created:
            try:
                if os.path.exists(bak):
                    shutil.copy2(bak, original)
            except Exception:
                pass

    def cleanup(self) -> None:
        # Remove created .bak files (best-effort)
        for _, bak in self._created:
            try:
                if os.path.exists(bak):
                    os.remove(bak)
            except Exception:
                pass
        self._created.clear()


# ---------- File Mover ----------

class FileMover:
    """
    Delivery to NAS/backup and archive moves with unique-dest semantics.
    """
    def __init__(self, job_number: str, month_abbrev: str, year: str,
                 work_dir: str, archive_root: str, backup_dir: str, network_base: str):
        self.job_number = job_number
        self.month_abbrev = month_abbrev.upper()
        self.year = year
        self.work_dir = work_dir
        self.archive_root = archive_root  # already expanded to include "<job> <MON> <YYYY>"
        self.backup_dir = backup_dir
        self.network_base = network_base

    def _unique_dest(self, path: str) -> str:
        if not os.path.exists(path):
            return path
        base, ext = os.path.splitext(path)
        return f"{base}_{timestamp_suffix()}{ext}"

    def deliver_presort_print(self, local_src_file: str):
        """
        Deliver presort print to:
            <network_base>/<job>_<MON>/HP Indigo/DATA
        If NAS unavailable, copy to --backup-dir.
        Returns: (net_ok: bool, final_folder: str, delivered_path: str)
        """
        job_folder_name = f"{self.job_number}_{self.month_abbrev}"  # underscore, no year
        job_folder_path = os.path.join(self.network_base, job_folder_name)
        final_network_path = os.path.join(job_folder_path, 'HP Indigo', 'DATA')

        # Try NAS first
        try:
            os.makedirs(job_folder_path, exist_ok=True)
            extra_subfolders = [
                os.path.join(job_folder_path, "PDF for Client"),
                os.path.join(job_folder_path, "Files for Ricoh"),
                os.path.join(job_folder_path, "Original Files"),
                os.path.join(job_folder_path, "HP Indigo", "DATA"),
                os.path.join(job_folder_path, "HP Indigo", "PRINT"),
                os.path.join(job_folder_path, "HP Indigo", "PROOF"),
            ]
            for folder in extra_subfolders:
                os.makedirs(folder, exist_ok=True)

            dest_file = os.path.join(final_network_path, os.path.basename(local_src_file))
            dest_file = self._unique_dest(dest_file)
            shutil.copy2(local_src_file, dest_file)
            return True, final_network_path, dest_file
        except Exception:
            # Fallback to backup
            os.makedirs(self.backup_dir, exist_ok=True)
            dest_file = os.path.join(self.backup_dir, os.path.basename(local_src_file))
            dest_file = self._unique_dest(dest_file)
            shutil.copy2(local_src_file, dest_file)
            return False, self.backup_dir, dest_file

    def archive_folder_move_keep(self, src_dir: str, dst_dir: str, keep_filename: str):
        """
        Move all files from src_dir to dst_dir EXCEPT keep_filename (copy it).
        Use unique-dest for collisions.
        """
        os.makedirs(dst_dir, exist_ok=True)
        for name in os.listdir(src_dir):
            src_path = os.path.join(src_dir, name)
            dst_path = os.path.join(dst_dir, name)
            if not os.path.isfile(src_path):
                continue
            if name == keep_filename:
                # copy (keep in src)
                dst_path = self._unique_dest(dst_path) if os.path.exists(dst_path) else dst_path
                shutil.copy2(src_path, dst_path)
            else:
                # move
                dst_path = self._unique_dest(dst_path) if os.path.exists(dst_path) else dst_path
                shutil.move(src_path, dst_path)


# ---------- TERM Processor ----------

class TERMProcessor:
    """
    Load sources, apply address/mailing updates, and write outputs.
    """
    def __init__(self, job_number: str, month_abbrev: str, year: str, work_dir: str):
        self.job_number = job_number
        self.month_abbrev = month_abbrev.upper()
        self.year = year
        self.work_dir = work_dir
        self.excel_name = "FHK_TERM.xlsx"
        self.move_updates_name = "MOVE UPDATES.csv"
        self.presort_name = "PRESORTLIST.csv"

        self.df_excel = None
        self.df_moves = None
        self.df_presort = None

    def _p(self, name: str) -> str:
        return os.path.join(self.work_dir, name)

    def verify_inputs_exist(self) -> None:
        for nm in (self.excel_name, self.move_updates_name, self.presort_name):
            pth = self._p(nm)
            if not os.path.exists(pth):
                raise FileNotFoundError(f"Required file not found: {pth}")

    def read_data(self) -> None:
        self.df_excel = pd.read_excel(
            self._p(self.excel_name),
            dtype={'Member ID': str, 'Enrollee ID': str, 'ZIP': str}, keep_default_na=False
        )
        self.df_moves = pd.read_csv(self._p(self.move_updates_name), dtype=str, keep_default_na=False)
        self.df_presort = pd.read_csv(self._p(self.presort_name), dtype=str, keep_default_na=False)

    def process(self):
        """
        Verify one complete presort record per original household before assigning
        statuses or applying moves. Names/addresses formatted by Bulk Mailer are
        display values; identity comes from the existing dependent member IDs.
        Returns (df_excel_out, df_presort_print, stats_dict).
        """
        df_x = self.df_excel.copy().reset_index(drop=True)
        df_m = self.df_moves.copy()
        df_p = self.df_presort.copy()

        household_columns = ['Guardian Name', 'ADDRESS1', 'ADDRESS2', 'CITY', 'STATE', 'ZIP']
        for column in household_columns + ['Member ID']:
            if list(df_x.columns).count(column) != 1:
                raise ValueError(f"Expected exactly one '{column}' source column")
        df_x['Guardian Name'] = df_x['Guardian Name'].map(
            lambda value: 'HEAD OF HOUSEHOLD' if not text_value(value) else value
        )
        df_x['Member ID'] = df_x['Member ID'].map(
            lambda value: re.sub(r'^([0-9]+)\.0$', r'\1', text_value(value))
        )
        invalid = ~df_x['Member ID'].map(lambda value: bool(re.fullmatch(r'[0-9]+', value)))
        if invalid.any():
            raise ValueError(f"Missing or invalid Member ID at source rows {(df_x.index[invalid] + 2).tolist()[:10]}")
        duplicates = df_x['Member ID'].duplicated(keep=False)
        if duplicates.any():
            raise ValueError(f"Duplicate source Member ID(s): {df_x.loc[duplicates, 'Member ID'].tolist()[:10]}")

        # Same key rules as prepare_term_data in the first step; never group by city alone.
        normalized = df_x[household_columns].apply(
            lambda col: col.map(lambda value: ' '.join(text_value(value).split()).upper())
        )
        keys = list(normalized.itertuples(index=False, name=None))
        member_household = dict(zip(df_x['Member ID'], keys))
        household_members = {}
        for member, key in member_household.items():
            household_members.setdefault(key, set()).add(member)

        def id_columns(frame):
            return [c for c in frame.columns if re.fullmatch(r'ID[1-9][0-9]*', str(c))]

        def identify_household(row, columns, location):
            members = dependent_ids(row, columns, location)
            unknown = set(members) - member_household.keys()
            if unknown:
                raise ValueError(f"{location}: unknown member ID(s): {sorted(unknown)[:10]}")
            households = {member_household[member] for member in members}
            if len(households) != 1:
                raise ValueError(f"{location}: dependents from different household addresses are combined")
            return households.pop(), set(members)

        presort_columns = id_columns(df_p)
        tray_columns = [c for c in df_p if str(c).strip().lower() == 'tray number']
        pallet_columns = [c for c in df_p if str(c).strip().lower() == 'pallet number']
        if not presort_columns or len(tray_columns) != 1 or len(pallet_columns) != 1:
            raise ValueError("PRESORTLIST.csv requires ID fields, Tray Number and Pallet Number")
        printable = df_p[tray_columns[0]].map(lambda value: bool(text_value(value)))
        pallets = pd.to_numeric(df_p[pallet_columns[0]], errors='coerce')
        status_by_household = {}
        for position, (_, row) in enumerate(df_p.iterrows()):
            location = f"PRESORTLIST.csv row {position + 2}"
            key, members = identify_household(row, presort_columns, location)
            if key in status_by_household:
                raise ValueError(f"{location}: duplicate household record")
            missing = household_members[key] - members
            if missing:
                raise ValueError(f"{location}: missing dependent member ID(s): {sorted(missing)[:10]}")
            pallet = pallets.iloc[position]
            if pd.isna(pallet):
                raise ValueError(f"{location}: invalid Pallet Number")
            if pallet == -1:
                if printable.iloc[position]:
                    raise ValueError(f"{location}: pallet -1 record also has a Tray Number")
                status_by_household[key] = 14
            elif printable.iloc[position]:
                status_by_household[key] = 13
            else:
                raise ValueError(f"{location}: no printable tray or pallet -1 rejection")

        missing = set(household_members) - status_by_household.keys()
        if missing:
            rows = [i + 2 for i, key in enumerate(keys) if key in missing]
            raise ValueError(f"Missing presort household(s): {len(missing)}; source rows {rows[:10]}")
        df_x['mailed'] = [status_by_household[key] for key in keys]
        mailed_status_14 = int((df_x['mailed'] == 14).sum())
        df_presort_print = df_p.loc[printable].copy()

        # Preserve original household identity when applying a new mailing address.
        targets = ['newadd', 'newadd2', 'newcity', 'newstate', 'newzip']
        for column in targets:
            if column not in df_x:
                df_x[column] = ''
        move_fields = ['Address Line 1', 'Address Line 2', 'City', 'State', 'ZIP Code']
        move_columns = id_columns(df_m)
        address_map = {}
        if not df_m.empty:
            if not move_columns or move_fields[0] not in df_m:
                raise ValueError("MOVE UPDATES.csv requires ID fields and Address Line 1")
            full_address = all(column in df_m for column in move_fields)
            for position, (_, row) in enumerate(df_m.iterrows()):
                location = f"MOVE UPDATES.csv row {position + 2}"
                key, _ = identify_household(row, move_columns, location)
                values = tuple(text_value(row[column]) for column in move_fields) if full_address else (
                    text_value(row[move_fields[0]]), None, None, None, None
                )
                if key in address_map and address_map[key] != values:
                    raise ValueError(f"{location}: conflicting moves for the same household")
                address_map[key] = values
        address_updates_applied = 0
        for i, key in enumerate(keys):
            if key in address_map:
                for column, value in zip(targets, address_map[key]):
                    if value is not None:
                        df_x.at[i, column] = value
                address_updates_applied += 1

        # Keep the established return schema: new address fields followed by mailed.
        mailed = df_x.pop('mailed')
        df_x['mailed'] = mailed

        # Drop unnamed/temp columns
        def _is_temp(colname: str) -> bool:
            s = str(colname).lower()
            return s.startswith("unnamed:") or s.startswith("temp_")
        df_x = df_x.loc[:, [c for c in df_x.columns if not _is_temp(c)]].copy()
        df_presort_print = df_presort_print.loc[:, [c for c in df_presort_print.columns if not _is_temp(c)]].copy()

        # Presort identifiers (including barcodes with leading zeros) remain text.

        stats = {
            "excel_records": int(len(df_x)),
            "move_updates": int(len(df_m)),
            "presort_records": int(len(df_p)),                 # original input size
            "presort_print_records": int(len(df_presort_print)),  # filtered printable subset
            "address_updates_applied": int(address_updates_applied),
            "mailed_status_14": int(mailed_status_14),
        }
        return df_x, df_presort_print, stats

    def write_outputs(self, df_excel_out, df_presort_print):
        df_excel_out = df_excel_out.fillna("")
        df_presort_print = df_presort_print.fillna("")
        updated_excel = os.path.join(self.work_dir, "FHK_TERM_UPDATED.xlsx")
        presort_print_filename = f"{self.job_number} {self.month_abbrev} PRESORTLIST_PRINT.csv"
        presort_print_path = os.path.join(self.work_dir, presort_print_filename)

        # Deterministic overwrite handling for outputs in work_dir
        if os.path.exists(updated_excel):
            os.replace(updated_excel, f"{updated_excel}.{timestamp_suffix()}.replaced")
        if os.path.exists(presort_print_path):
            os.replace(presort_print_path, f"{presort_print_path}.{timestamp_suffix()}.replaced")

        df_excel_out = df_excel_out.fillna("").replace("nan", "")
        df_presort_print = df_presort_print.fillna("").replace("nan", "")
        df_excel_out.to_excel(updated_excel, index=False)
        df_presort_print.to_csv(presort_print_path, index=False, encoding="utf-8-sig")

        return updated_excel, presort_print_path


# ---------- CLI ----------

def parse_args():
    p = argparse.ArgumentParser(description="TM TERM Final Step Script (Headless)")
    p.add_argument("job_number")
    p.add_argument("month_abbrev")
    p.add_argument("year")
    p.add_argument("--work-dir", required=True)
    p.add_argument("--archive-root", required=True)
    p.add_argument("--backup-dir", required=True)
    p.add_argument("--network-base", required=True)
    p.add_argument("--mode", choices=["prearchive", "archive"], default="prearchive",
                    help="Specifies whether to run prearchive or archive phase")
    return p.parse_args()


# ---------- Main ----------

def main():
    args = parse_args()
    MODE = args.mode.lower()

    job = args.job_number
    mon = args.month_abbrev.upper()
    year = args.year

    # Validations
    if not (job.isdigit() and len(job) == 5):
        print(json.dumps({"status": "ERROR", "message": f"Job number must be exactly 5 digits, got {job}"}))
        print("=== NAS_FOLDER_PATH ==="); print(""); print("=== END_NAS_FOLDER_PATH ===")
        sys.exit(1)
    if mon not in ["JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"]:
        print(json.dumps({"status": "ERROR", "message": f"Invalid month abbreviation: {mon}"}))
        print("=== NAS_FOLDER_PATH ==="); print(""); print("=== END_NAS_FOLDER_PATH ===")
        sys.exit(1)
    if not (year.isdigit() and len(year) == 4):
        print(json.dumps({"status": "ERROR", "message": f"Year must be 4 digits, got {year}"}))
        print("=== NAS_FOLDER_PATH ==="); print(""); print("=== END_NAS_FOLDER_PATH ===")
        sys.exit(1)

    work_dir = args.work_dir
    # year is parsed for consistency with prearchive phase; not used in archive path
    archive_dir = os.path.join(args.archive_root, f"{job} {mon}")  # TERM archive naming
    backup_dir = args.backup_dir
    network_base = args.network_base

    os.makedirs(work_dir, exist_ok=True)
    os.makedirs(archive_dir, exist_ok=True)

    backups = BackupManager()
    mover = FileMover(job, mon, year, work_dir, archive_dir, backup_dir, network_base)
    proc = TERMProcessor(job, mon, year, work_dir)

    try:
        if MODE == "prearchive":
            # Back up all three inputs *before* any reads/writes
            for nm in ("FHK_TERM.xlsx", "MOVE UPDATES.csv", "PRESORTLIST.csv"):
                backups.create(os.path.join(work_dir, nm))
            # Read + process
            proc.verify_inputs_exist()
            proc.read_data()
            df_x_out, df_print, stats = proc.process()

            # Write outputs (in work_dir)
            updated_excel, presort_print_path = proc.write_outputs(df_x_out, df_print)

            # Per requirements: cleanup backups after outputs written and *before* archiving/moves
            backups.cleanup()

            # Deliver presort print to NAS (or backup fallback)
            net_ok, final_folder, delivered_file = mover.deliver_presort_print(presort_print_path)

            # Success payload
            payload = {
                "status": "OK",
                "job_number": job,
                "month": mon,
                "year": year,
                "archive_dir": archive_dir,
                "work_dir": work_dir,
                "presort_print_file": presort_print_path,
                "network_deliver_success": bool(net_ok),
                "final_output_folder": final_folder,
                "delivered_file": delivered_file,
                "stats": stats,
            }
            print(json.dumps(payload))  # first line JSON

            # Legacy NAS markers
            print("=== NAS_FOLDER_PATH ===")
            print(final_folder)
            print("=== END_NAS_FOLDER_PATH ===")
            print("=== DISPLAY_FILE_PATH ===")
            print(os.path.join(work_dir, "FHK_TERM_UPDATED.xlsx"))
            print("=== END_DISPLAY_FILE_PATH ===")

            sys.exit(0)

        elif MODE == "archive":
            # Archive-only logic
            import time
            print("Archive phase starting...")
            for file in os.listdir(work_dir):
                src_file = os.path.join(work_dir, file)
                if src_file.endswith('.bak'):
                    continue
                dst_file = os.path.join(archive_dir, file)
                if os.path.exists(dst_file):
                    base, ext = os.path.splitext(dst_file)
                    dst_file = f"{base}_copy{ext}"
                max_retries = 5
                for attempt in range(max_retries):
                    try:
                        shutil.move(src_file, dst_file)
                        print(f"Archived and removed from DATA: {file}")
                        break
                    except PermissionError as e:
                        if attempt < max_retries - 1:
                            print(f"File in use ({file}), retrying in 2s...")
                            time.sleep(2)
                        else:
                            raise e

            remaining = os.listdir(work_dir)
            if remaining:
                print(f"[WARNING] Some files were not moved from DATA: {remaining}")
            else:
                print("DATA folder successfully cleared after archiving.")
            sys.exit(0)

    except Exception as e:
        # Rollback and emit error
        try:
            backups.rollback()
        except Exception:
            pass
        print(json.dumps({"status": "ERROR", "message": str(e)}))
        print("=== NAS_FOLDER_PATH ===")
        print("")
        print("=== END_NAS_FOLDER_PATH ===")
        sys.exit(1)


if __name__ == "__main__":
    main()
