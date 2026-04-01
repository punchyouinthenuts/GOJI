#!/usr/bin/env python3
import os, re, sys, shutil, zipfile
from datetime import datetime
import tempfile
import argparse

try:
    import pandas as pd
except ImportError:
    raise SystemExit("This script requires the 'pandas' package. Install with: pip install pandas")

# ---- Paths (GOJI) ----
BASE_DIR    = r"C:\Goji\TRACHMAR\FARMWORKERS"
DATA_DIR    = os.path.join(BASE_DIR, "DATA")
ARCHIVE_DIR = os.path.join(BASE_DIR, "ARCHIVE")

# Fallback basket: MOVE TO NETWORK DRIVE (no more BUSKRO)
MOVE_BASKET  = r"C:\Users\JCox\Desktop\MOVE TO NETWORK DRIVE"
MOVE_UPDATES = os.path.join(DATA_DIR, "MOVE UPDATES.csv")

# ---- Network drop ----
# \\NAS1069D9\AMPrintData\{YYYY}_SrcFiles\T\Trachmar\{job}_FARMWORKERS\HP Indigo\DATA
NAS_ROOT = r"\\NAS1069D9\AMPrintData"

# ---- Temp / rollback ----
TMP_DIR = tempfile.mkdtemp(prefix="tmfw_goji_")
BACKUPS = []

QUARTERS = {"1ST", "2ND", "3RD", "4TH"}


def parse_args():
    parser = argparse.ArgumentParser(description="FARMWORKERS two-phase processing")
    parser.add_argument("job", help="5-digit job number")
    parser.add_argument("quarter", help="Quarter: 1ST, 2ND, 3RD, 4TH")
    parser.add_argument("year", help="4-digit year")
    parser.add_argument(
        "--mode",
        choices=["prearchive", "archive"],
        required=True,
        help="Processing mode: prearchive (merge & copy) or archive (zip & clean)",
    )
    parser.add_argument("--work-dir", default=None, help="Override DATA directory path")
    parser.add_argument("--archive-root", default=None, help="Override ARCHIVE directory path")
    parser.add_argument("--backup-dir", default=None, help="Override backup directory path (accepted, not used)")
    parser.add_argument("--network-base", default=None, help="Override network base path")
    args = parser.parse_args()

    job = args.job.strip()
    qtr = args.quarter.strip().upper()
    yr  = args.year.strip()

    if not re.fullmatch(r"\d{5}", job):
        print("Job must be exactly 5 digits.")
        sys.exit(1)
    if qtr not in QUARTERS:
        print("Quarter must be one of: 1ST, 2ND, 3RD, 4TH")
        sys.exit(1)
    if not re.fullmatch(r"\d{4}", yr):
        print("Year must be 4 digits.")
        sys.exit(1)

    return job, qtr, yr, args


def backup(fp: str):
    if not os.path.exists(fp):
        return
    name = os.path.basename(fp)
    bk   = os.path.join(TMP_DIR, name)
    try:
        shutil.copy2(fp, bk)
        BACKUPS.append((bk, fp))
    except Exception as e:
        print(f"Backup failed for {fp}: {e}")


def rollback():
    print("\nERROR -> rolling back modified files...")
    for bk, dest in BACKUPS:
        if os.path.exists(bk):
            try:
                shutil.copy2(bk, dest)
                print(f"Restored: {dest}")
            except Exception as e:
                print(f"Failed to restore {dest}: {e}")
    try:
        shutil.rmtree(TMP_DIR)
    except Exception:
        pass
    sys.exit(1)


def cleanup_tmp():
    try:
        shutil.rmtree(TMP_DIR)
    except Exception:
        pass


# ---------- MATCHID-exact processing ----------
def process_mailing_status():
    """
    Build FARMWORKERS_MERGED.csv using exact MATCHID logic:
      - Read FARMWORKERS.csv + NOT MAILED.csv (+ optional MOVE UPDATES.csv)
      - Initialize MAILED=13; set to 14 if MATCHID is in NOT MAILED.csv
      - Apply MOVE UPDATES address fields (MATCHID join) only where MAILED=14
      - Drop MATCHID from final merged
    """
    input_file      = os.path.join(DATA_DIR, "FARMWORKERS.csv")
    not_mailed_file = os.path.join(DATA_DIR, "NOT MAILED.csv")
    output_file     = os.path.join(DATA_DIR, "FARMWORKERS_MERGED.csv")

    for fp in (input_file, not_mailed_file, output_file, MOVE_UPDATES):
        backup(fp)

    print("Reading input files...")
    try:
        df_fw = pd.read_csv(input_file, dtype=str, keep_default_na=False)
        df_nm = pd.read_csv(not_mailed_file, dtype=str, keep_default_na=False)
        if os.path.exists(MOVE_UPDATES):
            df_mu = pd.read_csv(MOVE_UPDATES, dtype=str, keep_default_na=False)
            print(f"Loaded MOVE UPDATES: {len(df_mu)} rows")
        else:
            df_mu = pd.DataFrame()
            print("MOVE UPDATES not found; continuing without address updates.")
    except Exception as e:
        print(f"Read error: {e}")
        rollback()

    # Validate required columns
    for need, df, label in [
        ("MATCHID", df_fw, "FARMWORKERS.csv"),
        ("MATCHID", df_nm, "NOT MAILED.csv"),
    ]:
        if need not in df.columns:
            print(f"ERROR: '{need}' missing from {label}")
            rollback()

    # Mailing status
    df_fw["MAILED"] = 13
    not_ids = set(df_nm["MATCHID"])
    df_fw.loc[df_fw["MATCHID"].isin(not_ids), "MAILED"] = 14

    # Ensure Current* columns exist
    for col in [
        "Current Address Line 1",
        "Current Address Line 2",
        "Current City",
        "Current State",
        "Current ZIP Code",
    ]:
        if col not in df_fw.columns:
            df_fw[col] = ""

    # Apply MOVE UPDATES where MAILED=14
    if not df_mu.empty:
        required_mu = ["MATCHID", "Address Line 1", "Address Line 2", "City", "State", "ZIP Code"]
        for c in required_mu:
            if c not in df_mu.columns:
                print(f"ERROR: '{c}' missing from MOVE UPDATES.csv")
                rollback()

        df_join = df_fw.merge(
            df_mu[required_mu],
            on="MATCHID",
            how="left",
        )
        has_update   = df_join["Address Line 1"].notna() & (df_join["Address Line 1"] != "")
        needs_update = has_update & (df_join["MAILED"] == 14)

        mapping = [
            ("Address Line 1", "Current Address Line 1"),
            ("Address Line 2", "Current Address Line 2"),
            ("City",           "Current City"),
            ("State",          "Current State"),
            ("ZIP Code",       "Current ZIP Code"),
        ]
        for src, dst in mapping:
            df_join.loc[needs_update, dst] = df_join.loc[needs_update, src]

        # Drop temp MU columns
        df_fw = df_join.drop(columns=["Address Line 1", "Address Line 2", "City", "State", "ZIP Code"])

    # Drop MATCHID in final merged
    if "MATCHID" in df_fw.columns:
        df_fw = df_fw.drop(columns=["MATCHID"])

    try:
        df_fw.to_csv(output_file, index=False)
        print(f"Wrote: {output_file}")
    except Exception as e:
        print(f"Write error: {e}")
        rollback()

    return output_file


def add_title_column(job_number: str):
    """
    Insert a 'Title' column (HEAD OF HOUSEHOLD) immediately after 'Endorsement Line'
    in 'FARMWORKERS (SORTED).csv', writing '{job} FARMWORKERS (SORTED).csv'.
    """
    sorted_in  = os.path.join(DATA_DIR, "FARMWORKERS (SORTED).csv")
    sorted_out = os.path.join(DATA_DIR, f"{job_number} FARMWORKERS (SORTED).csv")
    backup(sorted_in)
    try:
        df   = pd.read_csv(sorted_in, dtype=str, keep_default_na=False)
        cols = df.columns.tolist()
        try:
            i = cols.index("Endorsement Line")
        except ValueError:
            print("ERROR: 'Endorsement Line' column not found in sorted file.")
            rollback()
        new_cols = cols[: i + 1] + ["Title"] + cols[i + 1 :]
        df2 = pd.DataFrame(columns=new_cols)
        for c in cols:
            df2[c] = df[c]
        df2["Title"] = "HEAD OF HOUSEHOLD"
        df2.to_csv(sorted_out, index=False)
        os.remove(sorted_in)
        print(f"Added 'Title' and wrote: {sorted_out}")
        return sorted_out
    except Exception as e:
        print(f"Title insertion error: {e}")
        rollback()


def rename_merged(job_number: str):
    src = os.path.join(DATA_DIR, "FARMWORKERS_MERGED.csv")
    dst = os.path.join(DATA_DIR, f"{job_number} FARMWORKERS_MERGED.csv")
    backup(src)
    try:
        os.rename(src, dst)
        print(f"Renamed merged: {dst}")
        return dst
    except Exception as e:
        print(f"Rename error: {e}")
        rollback()


def copy_sorted_to_destination(sorted_path: str, job_number: str, year: str, network_base: str = None):
    """
    Copy '{job} FARMWORKERS (SORTED).csv' to NAS folder if the NAS root exists.
    If the NAS root is not reachable, copy to MOVE TO NETWORK DRIVE.
    """
    nas_root = network_base if network_base else NAS_ROOT

    try:
        if os.path.exists(nas_root):
            # NAS root is reachable: create year/T/Trachmar and job folders as needed
            nas_year   = os.path.join(nas_root, f"{year}_SrcFiles", "T", "Trachmar")
            job_folder = os.path.join(nas_year, f"{job_number}_FARMWORKERS")
            indigo_data = os.path.join(job_folder, "HP Indigo", "DATA")

            # Create expanded directory structure
            os.makedirs(job_folder, exist_ok=True)
            extra_subfolders = [
                os.path.join(job_folder, "PDF for Client"),
                os.path.join(job_folder, "Files for Ricoh"),
                os.path.join(job_folder, "Original Files"),
                os.path.join(job_folder, "HP Indigo", "DATA"),
                os.path.join(job_folder, "HP Indigo", "PRINT"),
                os.path.join(job_folder, "HP Indigo", "PROOF"),
            ]
            for folder in extra_subfolders:
                os.makedirs(folder, exist_ok=True)

            dst = os.path.join(indigo_data, os.path.basename(sorted_path))
            shutil.copy2(sorted_path, dst)

            print(f"Copied sorted file to NAS: {dst}")
            print("=== NAS_FOLDER_PATH ===")
            print(indigo_data)
            print("=== END_NAS_FOLDER_PATH ===")
            return indigo_data

        # Fallback: NAS root not reachable -> local MOVE TO NETWORK DRIVE
        os.makedirs(MOVE_BASKET, exist_ok=True)
        dst = os.path.join(MOVE_BASKET, os.path.basename(sorted_path))
        shutil.copy2(sorted_path, dst)
        print("NETWORK DRIVE UNAVAILABLE - saved to:", MOVE_BASKET)
        print("=== NAS_FOLDER_PATH ===")
        print(MOVE_BASKET)
        print("=== END_NAS_FOLDER_PATH ===")
        return MOVE_BASKET

    except Exception as e:
        print(f"Copy error: {e}")
        rollback()


def create_archive(job: str, quarter: str, year: str):
    """
    Archive entire DATA folder to ARCHIVE\\<job>_<quarter><year>.zip
    (Quarter-based name to align with GOJI/UI.)
    """
    os.makedirs(ARCHIVE_DIR, exist_ok=True)
    stamp = f"{job}_{quarter}{year}"
    zpath = os.path.join(ARCHIVE_DIR, f"{stamp}.zip")
    try:
        with zipfile.ZipFile(zpath, "w", compression=zipfile.ZIP_DEFLATED) as zf:
            for name in os.listdir(DATA_DIR):
                fp = os.path.join(DATA_DIR, name)
                if os.path.isfile(fp):
                    zf.write(fp, arcname=name)
        print(f"Archive created: {zpath}")
        return zpath
    except Exception as e:
        print(f"Archive error: {e}")
        rollback()


def clean_data_dir():
    try:
        for name in os.listdir(DATA_DIR):
            fp = os.path.join(DATA_DIR, name)
            if os.path.isfile(fp):
                os.remove(fp)
        print("DATA directory cleaned.")
    except Exception as e:
        print(f"Cleanup warning: {e}")


def main():
    job, qtr, yr, args = parse_args()

    # Use provided paths or defaults
    global DATA_DIR, ARCHIVE_DIR, MOVE_UPDATES
    if args.work_dir:
        DATA_DIR     = args.work_dir
        MOVE_UPDATES = os.path.join(DATA_DIR, "MOVE UPDATES.csv")
    if args.archive_root:
        ARCHIVE_DIR = args.archive_root

    print("=" * 60)
    print(f"TMFARMWORKERS FINAL (GOJI) - job={job} quarter={qtr} year={yr} mode={args.mode}")
    print("=" * 60)

    if args.mode == "prearchive":
        # Phase 1: Merge, copy, and pause for user review
        merged            = process_mailing_status()           # builds FARMWORKERS_MERGED.csv
        sorted_with_title = add_title_column(job)              # adds Title to sorted file
        renamed           = rename_merged(job)                 # job-prefixed merged
        drop_path         = copy_sorted_to_destination(
            sorted_with_title,
            job,
            yr,
            args.network_base,
        )

        print("\n=== PREARCHIVE PHASE COMPLETE ===")
        print("Please review the files in DATA folder before proceeding to archive.")
        cleanup_tmp()

    elif args.mode == "archive":
        # Phase 2: Archive and clean
        print("\n=== STARTING ARCHIVE PHASE ===")
        zpath = create_archive(job, qtr, yr)
        clean_data_dir()
        cleanup_tmp()

        print("\n=== ARCHIVE PHASE COMPLETE ===")
        print("\nSUCCESS")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nScript interrupted by user.")
        rollback()
    except Exception as e:
        print(f"\nUnexpected error: {e}")
        rollback()
