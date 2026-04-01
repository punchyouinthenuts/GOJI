#!/usr/bin/env python3
"""
TM TERM Final Step (Headless, corrected)

Requirements kept:
- Headless (no tkinter/popups)
- CLI args: job_number, month_abbrev, year, and options --work-dir, --archive-root, --backup-dir, --network-base
- First line JSON payload, followed by legacy NAS markers
- Exit code 0 on success, 1 on failure
- NAS job folder naming: "<job>_<MON>" (underscore, no year) into "<network_base>/<job>_<MON>/HP Indigo/DATA"
- Archive folder naming (TERM): "<archive_root>/<job> <MON> <YYYY>"
- Deterministic overwrite: timestamp suffix for delivered and archived files when destination exists
- Class-based structure: BackupManager, FileMover, TERMProcessor

Corrections implemented:
1) Back up *all three* inputs (FHK_TERM.xlsx, MOVE UPDATES.csv, PRESORTLIST.csv) with BackupManager.create().
   - cleanup() after outputs are written and *before* archiving/moves
   - rollback() on exception
2) Mailed status logic:
   - Default mailed=13 for all rows
   - Set mailed=14 when Excel key (col 7) exists in Presort key set (col 8)
   - Track mailed_status_14
3) Stats JSON now reports both presort sizes:
   - presort_records (raw input size of PRESORTLIST.csv)
   - presort_print_records (filtered printable subset length)
4) Optional multi-field address mapping:
   - If MOVE UPDATES.csv has cols for newadd..newzip (indices 3..7), apply them when present,
     ensure targets exist, and increment address_updates_applied once per matched row.
"""

import argparse
import json
import os
import shutil
import sys
from datetime import datetime

import pandas as pd


# ---------- Utilities ----------

def timestamp_suffix() -> str:
    return datetime.now().strftime('%Y%m%d%H%M%S')


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
        self.df_excel = pd.read_excel(self._p(self.excel_name))
        self.df_moves = pd.read_csv(self._p(self.move_updates_name))
        self.df_presort = pd.read_csv(self._p(self.presort_name))

    def process(self):
        """
        - Address updates using key match (excel col 7 vs moves col 0).
          Optional multi-field mapping if move updates has indices 3..7.
        - Mailed status:
          * Default mailed=13 for all rows
          * Set mailed=14 where excel_key (col 7) is in presort_key set (col 8)
        - presort_print := rows with non-null "Tray Number" (if column exists),
          else leave as full df_presort.
        Returns (df_excel_out, df_presort_print, stats_dict).
        """
        df_x = self.df_excel.copy()
        df_m = self.df_moves.copy()
        df_p = self.df_presort.copy()

        # Keys (consistent extraction style)
        try:
            excel_key = df_x.iloc[:, 7].astype(str).str.strip().str.upper()
        except Exception:
            excel_key = pd.Series([""] * len(df_x))
        try:
            move_key = df_m.iloc[:, 0].astype(str).str.strip().str.upper()
        except Exception:
            move_key = pd.Series([""] * len(df_m))

        # Optional multi-field address map (indices 3..7) if present
        # Fields correspond to: newadd, newadd2, newcity, newstate, newzip
        multi_field_available = (not df_m.empty and df_m.shape[1] > 7)
        simple_field_available = (not df_m.empty and df_m.shape[1] > 3)

        # Ensure target columns exist (create if missing)
        for colname in ["newadd", "newadd2", "newcity", "newstate", "newzip"]:
            if colname not in df_x.columns:
                df_x[colname] = pd.NA

        # Build a mapping dict for quick lookup
        address_map = {}
        if multi_field_available:
            for _, r in df_m.iterrows():
                mk = str(r.iloc[0]).strip().upper()
                address_map[mk] = (r.iloc[3], r.iloc[4], r.iloc[5], r.iloc[6], r.iloc[7])
        elif simple_field_available:
            for _, r in df_m.iterrows():
                mk = str(r.iloc[0]).strip().upper()
                address_map[mk] = (r.iloc[3], None, None, None, None)

        # Apply updates
        address_updates_applied = 0
        for i in range(len(df_x)):
            k = str(excel_key.iat[i]).strip().upper() if i < len(excel_key) else ""
            if k and k in address_map:
                vals = address_map[k]
                applied_any = False
                targets = ["newadd", "newadd2", "newcity", "newstate", "newzip"]
                for tcol, val in zip(targets, vals):
                    if val is not None:
                        df_x.at[i, tcol] = val
                        applied_any = True
                if applied_any:
                    address_updates_applied += 1

        # Presort print detection: "Tray Number" column preferred
        tray_col = None
        for c in df_p.columns:
            if str(c).strip().lower() == "tray number":
                tray_col = c
                break
        if tray_col is not None:
            df_presort_print = df_p[df_p[tray_col].notna()].copy()
        else:
            df_presort_print = df_p.copy()

        # ----- Mailed status logic (exact) -----
        # Default 13 for all rows
        df_x["mailed"] = 13
        mailed_status_14 = 0

        # Enhanced mailed logic: only mark 14 when Pallet Number (col 3) == -1
        try:
            presort_col8 = df_p.iloc[:, 8].astype(str).str.strip().str.upper()
        except Exception:
            presort_col8 = pd.Series([""] * len(df_p))

        for i, key in enumerate(excel_key):
            k = str(key).strip().upper()
            matching_rows = df_p[presort_col8 == k]
            if not matching_rows.empty:
                try:
                    col_d_value = matching_rows.iloc[0, 3]
                    if pd.notna(col_d_value) and col_d_value == -1:
                        df_x.at[i, "mailed"] = 14
                        mailed_status_14 += 1
                except Exception:
                    pass
# --------------------------------------

        # Drop unnamed/temp columns
        def _is_temp(colname: str) -> bool:
            s = str(colname).lower()
            return s.startswith("unnamed:") or s.startswith("temp_")
        df_x = df_x.loc[:, [c for c in df_x.columns if not _is_temp(c)]].copy()
        df_presort_print = df_presort_print.loc[:, [c for c in df_presort_print.columns if not _is_temp(c)]].copy()

        # Normalize integer-like columns in presort print (keep robust)
        for col in df_presort_print.columns:
            if df_presort_print[col].dtype == object:
                try:
                    df_presort_print[col] = pd.to_numeric(
                        df_presort_print[col].astype(str).str.replace(',', '')
                    )
                except Exception:
                    pass

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
