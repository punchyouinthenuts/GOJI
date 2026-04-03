import os
import glob
import shutil
import random
import string
from typing import List

import pandas as pd

CANONICAL_TM_ROOT = r"C:\Goji\AUTOMATION\TRACHMAR"
LEGACY_TM_ROOT = r"C:\Goji\TRACHMAR"

def resolve_tm_root():
    if os.path.isdir(CANONICAL_TM_ROOT):
        return CANONICAL_TM_ROOT
    if os.path.isdir(LEGACY_TM_ROOT):
        print("[WARNING] Using legacy TRACHMAR root C:\\Goji\\TRACHMAR; migrate to C:\\Goji\\AUTOMATION\\TRACHMAR.")
        return LEGACY_TM_ROOT
    os.makedirs(CANONICAL_TM_ROOT, exist_ok=True)
    print("[INFO] Created canonical TRACHMAR root C:\\Goji\\AUTOMATION\\TRACHMAR.")
    return CANONICAL_TM_ROOT

# -----------------------------
# Configuration (GOJI-specific)
# -----------------------------
DOWNLOADS_PATH = r"C:\Users\JCox\Downloads"
TARGET_DIR = os.path.join(resolve_tm_root(), "FARMWORKERS", "DATA")
TARGET_FILENAME = "FARMWORKERS.csv"
DEST_PATH = os.path.join(TARGET_DIR, TARGET_FILENAME)

MAX_PER_PREFIX = 9999
TOTAL_CAPACITY = 26 * 26 * MAX_PER_PREFIX  # 6,759,624


# -----------------------------
# Utility Functions
# -----------------------------
def _ensure_dir(path: str) -> None:
    if not os.path.exists(path):
        os.makedirs(path, exist_ok=True)


def _find_latest_fwc_csv(downloads_path: str) -> str:
    pattern = os.path.join(downloads_path, "*FWC*.csv")
    matches = glob.glob(pattern)
    if not matches:
        raise FileNotFoundError(f"No '*FWC*.csv' files found in: {downloads_path}")
    return max(matches, key=os.path.getmtime)


def _generate_match_ids(n_rows: int) -> List[str]:
    if n_rows > TOTAL_CAPACITY:
        raise ValueError(f"Too many rows: {n_rows}. Max is {TOTAL_CAPACITY}")

    used_prefixes = set()

    def new_prefix():
        while True:
            p = ''.join(random.choices(string.ascii_uppercase, k=2))
            if p not in used_prefixes:
                used_prefixes.add(p)
                return p

    ids = []
    prefix = new_prefix()
    counter = 1

    for _ in range(n_rows):
        if counter > MAX_PER_PREFIX:
            prefix = new_prefix()
            counter = 1
        ids.append(f"{prefix}{counter:04d}")
        counter += 1

    return ids


def _add_matchid_column_inplace(csv_path: str) -> None:
    df = pd.read_csv(csv_path, dtype=str, keep_default_na=False)
    match_ids = _generate_match_ids(len(df))
    df.insert(0, "MATCHID", match_ids)
    df.to_csv(csv_path, index=False)
    print(f"[MATCHID] Inserted {len(match_ids)} IDs into '{csv_path}'.")


# -----------------------------
# Main Process
# -----------------------------
def process_fwc_file_for_goji():
    print("[GOJI] Starting FWC CSV processing...")
    _ensure_dir(TARGET_DIR)

    try:
        source_csv = _find_latest_fwc_csv(DOWNLOADS_PATH)
    except FileNotFoundError as e:
        print(f"[ERROR] {e}")
        return

    try:
        shutil.copy2(source_csv, DEST_PATH)
        print(f"[COPY] {source_csv} -> {DEST_PATH}")
        _add_matchid_column_inplace(DEST_PATH)

        try:
            os.remove(source_csv)
            print(f"[CLEANUP] Deleted original: {source_csv}")
        except Exception as del_err:
            print(f"[WARNING] Could not delete original: {del_err}")

        print("[SUCCESS] File processed successfully.")
    except Exception as e:
        print(f"[ERROR] Processing failed: {e}")


if __name__ == "__main__":
    process_fwc_file_for_goji()
