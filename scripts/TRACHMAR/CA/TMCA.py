# TMCA.py  (Unified TM CA BA + TM CA EDR)  -- GOJI-integrated, non-interactive, 2-phase
# -----------------------------------------------------------------------------------
# Key guarantees:
# - Single job type per run: BA xor EDR (never both)
# - Phase 1 (--phase process): validate, MERGED, OUTPUT, deliverables, copies; emits JSON result block
# - Phase 2 (--phase archive): zip INPUT+OUTPUT+MERGED then transactional clear
# - No prompts, no waits, no os.startfile, no opening DAILYLOG.xlsx
# - Collision detection: abort before overwriting anywhere
# - Rollback on fatal error: removes artifacts created/copied by this run; restores backups if any
#
# Default folder layout (GOJI home base):
#   C:\Goji\AUTOMATION\TRACHMAR\CA\BA\INPUT|OUTPUT|MERGED|ARCHIVE
#   C:\Goji\AUTOMATION\TRACHMAR\CA\EDR\INPUT|OUTPUT|MERGED|ARCHIVE
#
# Invocation examples (GOJI):
#   python TMCA.py --phase process --job 46655 --ba-input "C:\Goji\AUTOMATION\TRACHMAR\CA\BA\INPUT" --edr-input "C:\Goji\AUTOMATION\TRACHMAR\CA\EDR\INPUT" --w-dest "W:/" --nas-base "\\NAS1069D9\AMPrintData" --year 2026
#   python TMCA.py --phase archive --job 46655 --ba-input "C:\Goji\AUTOMATION\TRACHMAR\CA\BA\INPUT" --edr-input "C:\Goji\AUTOMATION\TRACHMAR\CA\EDR\INPUT"
#
from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import sys
import time
import zipfile
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional, Sequence, Tuple

import pandas as pd

ALLOWED_EXTS = {".csv", ".xls", ".xlsx"}

# Language order (both BA + EDR)
LANG_ORDER = [
    "English",
    "Arabic",
    "Farsi",
    "Russian",
    "Spanish",
    "Vietnamese",
    "Chinese",
    "Tagalog",
    "Armenian",
    "Korean",
    "Cambodian",
    "Hmong",
]
_LANG_CANON = {k.lower(): k for k in LANG_ORDER}
LANG_ALLOWED_BY_GROUP = {
    "LA": {
        "Arabic",
        "Armenian",
        "Cambodian",
        "Chinese",
        "English",
        "Farsi",
        "Korean",
        "Russian",
        "Spanish",
        "Tagalog",
        "Vietnamese",
    },
    "SA": {
        "Arabic",
        "Chinese",
        "English",
        "Farsi",
        "Hmong",
        "Russian",
        "Spanish",
        "Vietnamese",
    },
}


# -----------------------------
# Utilities
# -----------------------------
def ensure_dir(p: Path) -> None:
    p.mkdir(parents=True, exist_ok=True)


def is_dir_writable(p: Path) -> bool:
    """Best-effort check that we can create a file in the directory."""
    try:
        ensure_dir(p)
        test = p / f"__writetest_{os.getpid()}_{int(time.time()*1000)}.tmp"
        test.write_text("x", encoding="utf-8")
        test.unlink(missing_ok=True)
        return True
    except Exception:
        return False


def normalize_cell(x) -> str:
    if x is None:
        return ""
    if isinstance(x, float) and pd.isna(x):
        return ""
    if isinstance(x, (pd.Timestamp, datetime)):
        return str(x)
    s = str(x).replace("\ufeff", "").strip()
    return "" if s.lower() == "nan" else s


def read_csv_with_fallback(path: Path) -> pd.DataFrame:
    for enc in ("utf-8-sig", "utf-8", "cp1252"):
        try:
            return pd.read_csv(path, dtype=str, encoding=enc, keep_default_na=False)
        except Exception:
            pass
    return pd.read_csv(path, dtype=str, keep_default_na=False)


def read_input_file(path: Path) -> pd.DataFrame:
    ext = path.suffix.lower()
    if ext == ".csv":
        return read_csv_with_fallback(path)
    if ext in (".xls", ".xlsx"):
        return pd.read_excel(path, dtype=str, sheet_name=0, keep_default_na=False)
    raise RuntimeError(f"Unsupported file type: {path.name}")


def language_sort_key(series: pd.Series) -> pd.Categorical:
    return pd.Categorical(series, categories=LANG_ORDER, ordered=True)


def normalize_language_common(x) -> str:
    """
    Common normalization rules used by both scripts:
      - blank -> English
      - startswith 'Spanish;' (spacing/case tolerant) -> Spanish
      - startswith 'Chinese;' (spacing/case tolerant) -> Chinese
      - else: canonical known language or English
    """
    s = normalize_cell(x)
    s = re.sub(r"\s+", " ", s).strip()
    if s == "":
        return "English"
    low = s.lower()
    if re.match(r"^spanish\s*;", low):
        return "Spanish"
    if re.match(r"^chinese\s*;", low):
        return "Chinese"
    # keep canonical if allowed, else English
    return _LANG_CANON.get(low, "English")


def normalize_output_language(x, group: str) -> str:
    lang = normalize_language_common(x)
    allowed = LANG_ALLOWED_BY_GROUP.get(normalize_cell(group).upper())
    if not allowed:
        return "English"
    return lang if lang in allowed else "English"


def write_csv(path: Path, df: pd.DataFrame, encoding: str = "utf-8-sig") -> None:
    ensure_dir(path.parent)
    df.to_csv(path, index=False, encoding=encoding, lineterminator="\n")


def json_result_print(obj: dict) -> None:
    print("=== TMCA_RESULT_BEGIN ===")
    print(json.dumps(obj, ensure_ascii=False))
    print("=== TMCA_RESULT_END ===")


# -----------------------------
# Rollback Manager
# -----------------------------
class RollbackManager:
    """
    Tracks files created and files that might be overwritten (backups).
    Rollback deletes created files (reverse order) and restores backups if needed.
    """

    def __init__(self, rollback_root: Path):
        ensure_dir(rollback_root)
        stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        self.root = rollback_root / f"__ROLLBACK_{stamp}_{os.getpid()}"
        ensure_dir(self.root)
        self._backups: Dict[Path, Tuple[Path, bool]] = {}  # original -> (backup_path, existed)
        self._created: List[Path] = []
        self._zip_created: Optional[Path] = None

    def backup_file(self, path: Path) -> None:
        if path in self._backups:
            return
        existed = path.exists()
        backup_path = self.root / f"BK_{len(self._backups)+1:04d}_{path.name}"
        if existed:
            ensure_dir(backup_path.parent)
            shutil.copy2(path, backup_path)
        self._backups[path] = (backup_path, existed)

    def record_created(self, path: Path) -> None:
        if path not in self._created:
            self._created.append(path)

    def record_zip(self, path: Path) -> None:
        self._zip_created = path

    def rollback(self) -> None:
        # 1) delete created files
        for p in reversed(self._created):
            try:
                if p.exists():
                    if p.is_file():
                        p.unlink()
                    else:
                        shutil.rmtree(p, ignore_errors=True)
            except Exception:
                pass

        # 2) restore backups
        for original, (backup_path, existed) in self._backups.items():
            try:
                if existed:
                    if backup_path.exists():
                        ensure_dir(original.parent)
                        shutil.copy2(backup_path, original)
                else:
                    if original.exists():
                        if original.is_file():
                            original.unlink()
                        else:
                            shutil.rmtree(original, ignore_errors=True)
            except Exception:
                pass

        # 3) delete zip created in this run (if tracked separately)
        if self._zip_created:
            try:
                if self._zip_created.exists():
                    self._zip_created.unlink()
            except Exception:
                pass

        # 4) leave rollback folder for diagnostics? For GOJI, better to remove to avoid clutter.
        try:
            shutil.rmtree(self.root, ignore_errors=True)
        except Exception:
            pass

    def commit(self) -> None:
        try:
            shutil.rmtree(self.root, ignore_errors=True)
        except Exception:
            pass


# -----------------------------
# Path model
# -----------------------------
@dataclass(frozen=True)
class JobPaths:
    job_type: str  # "BA" or "EDR"
    base: Path
    input_dir: Path
    output_dir: Path
    merged_dir: Path
    archive_dir: Path


def infer_base_from_input(input_dir: Path) -> Path:
    # input_dir should be ...\<TYPE>\INPUT
    return input_dir.parent


def make_job_paths(job_type: str, input_dir: Path) -> JobPaths:
    base = infer_base_from_input(input_dir)
    return JobPaths(
        job_type=job_type,
        base=base,
        input_dir=input_dir,
        output_dir=base / "OUTPUT",
        merged_dir=base / "MERGED",
        archive_dir=base / "ARCHIVE",
    )


# -----------------------------
# Discovery / eligibility
# -----------------------------
def eligible_ba_files(ba_input: Path) -> List[Path]:
    if not ba_input.exists():
        return []
    out: List[Path] = []
    for p in sorted(ba_input.iterdir()):
        if not (p.is_file() and p.suffix.lower() in ALLOWED_EXTS):
            continue
        name = p.name.upper()
        if "LA_BA" in name or "SA_BA" in name:
            out.append(p)
    return out


def eligible_edr_files(edr_input: Path) -> List[Path]:
    if not edr_input.exists():
        return []
    out: List[Path] = []
    for p in sorted(edr_input.iterdir()):
        if not (p.is_file() and p.suffix.lower() in ALLOWED_EXTS):
            continue
        name = p.name.upper()
        if "LA_EDR" in name or "SA_EDR" in name:
            out.append(p)
    return out


def detect_job_type(ba_input: Path, edr_input: Path) -> Tuple[str, List[Path]]:
    ba = eligible_ba_files(ba_input)
    edr = eligible_edr_files(edr_input)
    if ba and edr:
        raise RuntimeError("Both BA and EDR input folders contain eligible files. Aborting (single job type only).")
    if not ba and not edr:
        raise RuntimeError("No eligible input files found in BA or EDR input folders.")
    return ("BA", ba) if ba else ("EDR", edr)


# -----------------------------
# BA processing
# -----------------------------
BA_REQUIRED_COLUMNS = [
    "Member First Name",
    "Member Last Name",
    "Address_1",
    "Address_2",
    "City",
    "State",
    "Zip",
    "Language Indicator",
]
BA_ADDR_FIELDS_FOR_BLANK = ["Address_1", "Address_2"]

BA_WIN_IJ_KEEP = [
    "ID",
    "Member First Name",
    "Member Last Name",
    "Address_1",
    "Address_2",
    "City",
    "State",
    "Zip",
]
BA_NAS_KEEP = ["ID", "Language Indicator"]


def ba_classify_prefix(filename: str) -> Optional[str]:
    name = filename.upper()
    if "LA_BA" in name:
        return "LA"
    if "SA_BA" in name:
        return "SA"
    return None


def assert_required_columns(df: pd.DataFrame, required: Sequence[str], filename: str) -> None:
    missing = [c for c in required if c not in df.columns]
    if missing:
        raise RuntimeError(
            "Required columns missing.\n"
            f"File: {filename}\n"
            "Missing columns:\n - " + "\n - ".join(missing)
        )


def ba_blank_address_mask(df: pd.DataFrame) -> pd.Series:
    tmp = df.copy()
    for c in BA_ADDR_FIELDS_FOR_BLANK:
        if c not in tmp.columns:
            tmp[c] = ""
        tmp[c] = tmp[c].map(normalize_cell)
        tmp[c] = tmp[c].apply(lambda v: "" if v == "0" else v)
    mask = pd.Series(True, index=tmp.index)
    for c in BA_ADDR_FIELDS_FOR_BLANK:
        mask &= tmp[c].eq("")
    return mask


def write_ba_merged(paths: JobPaths, rm: RollbackManager, original_path: Path, df_raw: pd.DataFrame, mask_blank: pd.Series) -> Path:
    # Write merged into MERGED folder (GOJI requirement)
    out_name = f"{original_path.stem}_MERGED{original_path.suffix}"
    out_path = paths.merged_dir / out_name
    rm.backup_file(out_path)
    merged = df_raw.copy()
    if "MAILED" in merged.columns:
        merged = merged.drop(columns=["MAILED"])
    mailed = pd.Series("13", index=merged.index, dtype=str)
    mailed.loc[mask_blank] = "14"
    merged["MAILED"] = mailed
    for _c in ["Address_2"]:
        if _c in merged.columns:
            merged[_c] = merged[_c].map(normalize_cell).apply(lambda v: "" if v == "0" else v)
    write_csv(out_path, merged, encoding="utf-8-sig")
    if not rm._backups[out_path][1]:
        rm.record_created(out_path)
    return out_path


def ba_process_file(paths: JobPaths, rm: RollbackManager, file_path: Path, prefix: str) -> Tuple[Path, pd.DataFrame, int, int]:
    df_raw = read_input_file(file_path)
    assert_required_columns(df_raw, BA_REQUIRED_COLUMNS, file_path.name)

    mask_blank = ba_blank_address_mask(df_raw)
    blank_count = int(mask_blank.sum())
    total = int(len(df_raw))
    merged_path = write_ba_merged(paths, rm, file_path, df_raw, mask_blank)

    if total > 0 and blank_count == total:
        return merged_path, pd.DataFrame(columns=BA_REQUIRED_COLUMNS), 0, blank_count

    df = df_raw.loc[~mask_blank].copy()

    # Normalize all kept columns
    for c in BA_REQUIRED_COLUMNS:
        df[c] = df[c].map(normalize_cell)

    # Strip "0" from Address_2 (blank placeholder in source data)
    if "Address_2" in df.columns:
        df["Address_2"] = df["Address_2"].apply(lambda v: "" if v == "0" else v)

    # Production/output language normalization + LA/SA allowed-language enforcement
    df["Language Indicator"] = df["Language Indicator"].apply(lambda v: normalize_output_language(v, prefix))

    df_valid = df[BA_REQUIRED_COLUMNS].copy()
    return merged_path, df_valid, len(df_valid), blank_count


def ba_write_combined_output(paths: JobPaths, rm: RollbackManager, prefix: str, dfs: List[pd.DataFrame]) -> Optional[Path]:
    if not dfs:
        return None

    df_final = pd.concat(dfs, ignore_index=True)
    if df_final.empty:
        return None

    df_final = df_final.sort_values(by=["Language Indicator"], key=language_sort_key, kind="mergesort").reset_index(drop=True)

    n = len(df_final)
    pad = 2 if n <= 99 else 3
    id_prefix = f"BA{prefix}"
    df_final.insert(0, "ID", [f"{id_prefix}{str(i).zfill(pad)}" for i in range(1, n + 1)])

    ensure_dir(paths.output_dir)
    out_path = paths.output_dir / f"{prefix}_{n}.csv"
    rm.backup_file(out_path)
    write_csv(out_path, df_final, encoding="utf-8-sig")
    if not rm._backups[out_path][1]:
        rm.record_created(out_path)

    return out_path

def ba_build_filenames(job: str, original_name: str) -> Tuple[str, str]:
    # W drive gets IJ
    win_name = f"{job} TM CA BA IJ {original_name}"
    nas_name = f"{job} TM CA BA {original_name}"
    return win_name, nas_name


# -----------------------------
# EDR processing
# -----------------------------
EDR_REQUIRED_DIRECT = [
    "Member_Language",
    "Member_Address_1",
    "Member_Address_2",
    "Member_City",
    "Member_State",
    "Member_Zip",
    "DH_Location_Name",
    "DH_Address_1",
    "DH_Address_2",
    "DH_City",
    "DH_State",
    "DH_Zip",
    "DH_Phone_Number",
]
EDR_FIRST_NAME_VARIANTS = ["First_Name", "Member_First_Name"]
EDR_LAST_NAME_VARIANTS = ["Last_Name", "Member_Last_Name"]
EDR_BLANK_ADDR_FIELDS = ["Member_Address_1", "Member_City", "Member_State", "Member_Zip"]

EDR_FINAL_COLUMNS_ORDER = [
    "ID",
    "Member_Language",
    "First_Name",
    "Last_Name",
    "Member_Address_1",
    "Member_Address_2",
    "Member_City",
    "Member_State",
    "Member_Zip",
    "DH_Location_Name",
    "DH_Address_1",
    "DH_Address_2",
    "DH_City",
    "DH_State",
    "DH_Zip",
    "DH_Phone_Number",
]

EDR_DH_COLS = [
    "DH_Location_Name",
    "DH_Address_1",
    "DH_Address_2",
    "DH_City",
    "DH_State",
    "DH_Zip",
    "DH_Phone_Number",
]

EDR_WIN_IJ_KEEP = [
    "ID",
    "First_Name",
    "Last_Name",
    "Member_Address_1",
    "Member_Address_2",
    "Member_City",
    "Member_State",
    "Member_Zip",
]
EDR_NAS_KEEP = ["ID", "Member_Language", *EDR_DH_COLS]


def edr_classify_prefix(filename: str) -> Optional[str]:
    name = filename.upper()
    if "LA_EDR" in name:
        return "LA"
    if "SA_EDR" in name:
        return "SA"
    return None


def edr_validate_required_columns(df: pd.DataFrame) -> List[str]:
    cols = set(df.columns.tolist())
    missing: List[str] = []
    for c in EDR_REQUIRED_DIRECT:
        if c not in cols:
            missing.append(c)
    if not any(c in cols for c in EDR_FIRST_NAME_VARIANTS):
        missing.append("First_Name or Member_First_Name")
    if not any(c in cols for c in EDR_LAST_NAME_VARIANTS):
        missing.append("Last_Name or Member_Last_Name")
    return missing


def edr_blank_address_mask(df: pd.DataFrame) -> pd.Series:
    tmp = df.copy()
    for c in EDR_BLANK_ADDR_FIELDS:
        if c not in tmp.columns:
            tmp[c] = ""
        tmp[c] = tmp[c].map(normalize_cell)
        tmp[c] = tmp[c].apply(lambda v: "" if v == "0" else v)
    mask = pd.Series(True, index=tmp.index)
    for c in EDR_BLANK_ADDR_FIELDS:
        mask &= tmp[c].eq("")
    return mask


def edr_resolve_name_columns(df: pd.DataFrame) -> pd.DataFrame:
    for c in EDR_FIRST_NAME_VARIANTS + EDR_LAST_NAME_VARIANTS:
        if c not in df.columns:
            df[c] = ""
        df[c] = df[c].map(normalize_cell)

    def pick(member_val: str, non_member_val: str) -> str:
        return member_val if member_val else non_member_val

    # Ensure both keys exist
    if "Member_First_Name" not in df.columns:
        df["Member_First_Name"] = ""
    if "Member_Last_Name" not in df.columns:
        df["Member_Last_Name"] = ""
    if "First_Name" not in df.columns:
        df["First_Name"] = ""
    if "Last_Name" not in df.columns:
        df["Last_Name"] = ""

    df["First_Name"] = [pick(mv, nv) for mv, nv in zip(df["Member_First_Name"], df["First_Name"])]
    df["Last_Name"] = [pick(mv, nv) for mv, nv in zip(df["Member_Last_Name"], df["Last_Name"])]
    return df


def write_edr_merged(paths: JobPaths, rm: RollbackManager, original_path: Path, df_raw: pd.DataFrame, mask_blank: pd.Series) -> Path:
    out_name = f"{original_path.stem}_MERGED{original_path.suffix}"
    out_path = paths.merged_dir / out_name
    rm.backup_file(out_path)
    merged = df_raw.copy()
    if "MAILED" in merged.columns:
        merged = merged.drop(columns=["MAILED"])
    mailed = pd.Series("13", index=merged.index, dtype=str)
    mailed.loc[mask_blank] = "14"
    merged["MAILED"] = mailed
    for _c in ["Member_Address_2", "DH_Address_2"]:
        if _c in merged.columns:
            merged[_c] = merged[_c].map(normalize_cell).apply(lambda v: "" if v == "0" else v)
    write_csv(out_path, merged, encoding="utf-8-sig")
    if not rm._backups[out_path][1]:
        rm.record_created(out_path)
    return out_path


def edr_process_file(paths: JobPaths, rm: RollbackManager, file_path: Path, prefix: str) -> Tuple[Path, pd.DataFrame, int, int]:
    df_raw = read_input_file(file_path)
    missing = edr_validate_required_columns(df_raw)
    if missing:
        raise RuntimeError(f"{file_path.name} is missing required columns: {', '.join(missing)}")

    mask_blank = edr_blank_address_mask(df_raw)
    blank_count = int(mask_blank.sum())
    total = int(len(df_raw))
    merged_path = write_edr_merged(paths, rm, file_path, df_raw, mask_blank)

    if total > 0 and blank_count == total:
        return merged_path, pd.DataFrame(columns=[c for c in EDR_FINAL_COLUMNS_ORDER if c != "ID"]), 0, blank_count

    df = df_raw.loc[~mask_blank].copy()
    df = edr_resolve_name_columns(df)

    # Normalize required direct
    for c in EDR_REQUIRED_DIRECT:
        if c not in df.columns:
            df[c] = ""
        df[c] = df[c].map(normalize_cell)

    # Strip "0" from address-2 fields (blank placeholder in source data)
    for _c in ["Member_Address_2", "DH_Address_2"]:
        if _c in df.columns:
            df[_c] = df[_c].apply(lambda v: "" if v == "0" else v)

    # Production/output language normalization + LA/SA allowed-language enforcement
    df["Member_Language"] = df["Member_Language"].apply(lambda v: normalize_output_language(v, prefix))

    df_valid = df[[c for c in EDR_FINAL_COLUMNS_ORDER if c != "ID"]].copy()
    return merged_path, df_valid, len(df_valid), blank_count


def edr_write_combined_output(paths: JobPaths, rm: RollbackManager, prefix: str, dfs: List[pd.DataFrame]) -> Optional[Path]:
    if not dfs:
        return None

    df_final = pd.concat(dfs, ignore_index=True)
    if df_final.empty:
        return None

    df_final["__lang_order"] = pd.Categorical(df_final["Member_Language"], categories=LANG_ORDER, ordered=True)
    df_final = (
        df_final.sort_values(by="__lang_order", kind="mergesort")
        .drop(columns=["__lang_order"])
        .reset_index(drop=True)
    )

    n = len(df_final)
    pad = 2 if n <= 99 else 3
    id_prefix = f"EDR{prefix}"
    df_final.insert(0, "ID", [f"{id_prefix}{str(i).zfill(pad)}" for i in range(1, n + 1)])

    ensure_dir(paths.output_dir)
    out_path = paths.output_dir / f"{prefix}_{n}.csv"
    rm.backup_file(out_path)
    write_csv(out_path, df_final, encoding="utf-8-sig")
    if not rm._backups[out_path][1]:
        rm.record_created(out_path)

    return out_path

def edr_build_filenames(job: str, original_name: str) -> Tuple[str, str]:
    win_name = f"{job} TM CA EDR IJ {original_name}"
    nas_name = f"{job} TM CA EDR {original_name}"
    return win_name, nas_name


# -----------------------------
# Destinations + Deliverables
# -----------------------------
def compute_nas_dest(nas_base: Path, year: str, job: str, job_type: str) -> Path:
    if job_type == "BA":
        return nas_base / f"{year}_SrcFiles" / "T" / "Trachmar" / f"{job}_BrokenAppointments" / "HP Indigo" / "DATA"
    if job_type == "EDR":
        return nas_base / f"{year}_SrcFiles" / "T" / "Trachmar" / f"{job}_CA-EDR" / "HP Indigo" / "DATA"
    raise RuntimeError(f"Unknown job_type for NAS path: {job_type}")


def plan_destinations(
    w_pref: Path,
    w_fallback: Path,
    nas_pref: Path,
    nas_fallback: Path,
) -> Tuple[Path, Path]:
    w_dest = w_pref if is_dir_writable(w_pref) else w_fallback
    if w_dest == w_fallback:
        ensure_dir(w_fallback)
    if not is_dir_writable(w_dest):
        raise RuntimeError(f"Neither W: nor fallback is writable: {w_fallback}")

    nas_dest = nas_pref if is_dir_writable(nas_pref) else nas_fallback
    if nas_dest == nas_fallback:
        ensure_dir(nas_fallback)
    if not is_dir_writable(nas_dest):
        raise RuntimeError(f"Neither NAS destination nor fallback is writable: {nas_fallback}")

    return w_dest, nas_dest


def preflight_collision_check(staged_paths: List[Path], destination_paths: List[Path]) -> None:
    existing: List[str] = []
    for p in staged_paths:
        if p.exists():
            existing.append(f"Already exists in OUTPUT staging: {p}")
    for p in destination_paths:
        if p.exists():
            existing.append(f"Already exists in destination: {p}")
    if existing:
        raise RuntimeError("One or more target files already exist.\n" + "\n".join(existing))


def safe_copy(rm: RollbackManager, src: Path, dst: Path) -> None:
    ensure_dir(dst.parent)
    shutil.copy2(src, dst)
    rm.record_created(dst)


def create_and_copy_deliverables(
    paths: JobPaths,
    rm: RollbackManager,
    job: str,
    output_csvs: List[Path],
    w_dest: Path,
    nas_dest: Path,
) -> Dict[str, List[str]]:
    staged_paths: List[Path] = []
    destination_paths: List[Path] = []

    for src in output_csvs:
        if paths.job_type == "BA":
            win_name, nas_name = ba_build_filenames(job, src.name)
        else:
            win_name, nas_name = edr_build_filenames(job, src.name)

        staged_win = paths.output_dir / win_name
        staged_nas = paths.output_dir / nas_name

        staged_paths.extend([staged_win, staged_nas])

        # Copy targets:
        # - W: gets IJ (win_name)
        # - NAS gets: (1) job-type file (nas_name) and (2) IJ file (win_name) also copied to NAS
        destination_paths.extend([w_dest / win_name, nas_dest / nas_name, nas_dest / win_name])

    preflight_collision_check(staged_paths, destination_paths)

    placements: Dict[str, List[str]] = {
        "STAGED_IN_OUTPUT": [],
        f"COPIED_TO_{str(w_dest)}": [],
        f"COPIED_TO_{str(nas_dest)}": [],
    }

    for src in output_csvs:
        df = read_csv_with_fallback(src)
        # normalize
        for c in df.columns:
            df[c] = df[c].map(normalize_cell)

        if paths.job_type == "BA":
            assert_required_columns(df, BA_WIN_IJ_KEEP + ["Language Indicator"], src.name)
            assert_required_columns(df, BA_NAS_KEEP, src.name)
            win_name, nas_name = ba_build_filenames(job, src.name)

            staged_win = paths.output_dir / win_name
            staged_nas = paths.output_dir / nas_name

            rm.backup_file(staged_win)
            rm.backup_file(staged_nas)

            # IJ excludes Language Indicator
            win_df = df[BA_WIN_IJ_KEEP].copy()
            write_csv(staged_win, win_df, encoding="utf-8-sig")
            if not rm._backups[staged_win][1]:
                rm.record_created(staged_win)
            placements["STAGED_IN_OUTPUT"].append(staged_win.name)

            # NAS contains only ID + Language Indicator
            nas_df = df[BA_NAS_KEEP].copy()
            write_csv(staged_nas, nas_df, encoding="utf-8")
            if not rm._backups[staged_nas][1]:
                rm.record_created(staged_nas)
            placements["STAGED_IN_OUTPUT"].append(staged_nas.name)

        else:
            # EDR
            # validate minimal columns for both outputs
            assert_required_columns(df, EDR_WIN_IJ_KEEP, src.name)
            assert_required_columns(df, EDR_NAS_KEEP, src.name)

            win_name, nas_name = edr_build_filenames(job, src.name)

            staged_win = paths.output_dir / win_name
            staged_nas = paths.output_dir / nas_name

            rm.backup_file(staged_win)
            rm.backup_file(staged_nas)

            # IJ: member address block fields (no language, no DH)
            win_df = df[EDR_WIN_IJ_KEEP].copy()
            write_csv(staged_win, win_df, encoding="utf-8-sig")
            if not rm._backups[staged_win][1]:
                rm.record_created(staged_win)
            placements["STAGED_IN_OUTPUT"].append(staged_win.name)

            # "Mac"/NAS: DH fields + language (no member address block)
            nas_df = df[EDR_NAS_KEEP].copy()
            write_csv(staged_nas, nas_df, encoding="utf-8")
            if not rm._backups[staged_nas][1]:
                rm.record_created(staged_nas)
            placements["STAGED_IN_OUTPUT"].append(staged_nas.name)

        # Copy staged files to destinations
        win_dst = w_dest / staged_win.name
        safe_copy(rm, staged_win, win_dst)
        placements[f"COPIED_TO_{str(w_dest)}"].append(win_dst.name)

        # Copy IJ also to NAS
        win_dst_nas = nas_dest / staged_win.name
        safe_copy(rm, staged_win, win_dst_nas)
        placements[f"COPIED_TO_{str(nas_dest)}"].append(win_dst_nas.name)

        # Copy job-type file to NAS
        nas_dst = nas_dest / staged_nas.name
        safe_copy(rm, staged_nas, nas_dst)
        placements[f"COPIED_TO_{str(nas_dest)}"].append(nas_dst.name)

    return placements


# -----------------------------
# Archive + transactional clear
# -----------------------------
def zip_folder_contents(zipf: zipfile.ZipFile, folder: Path, arc_root: str) -> None:
    if not folder.exists():
        return
    # Ensure the root dir exists in zip (helps some zip viewers)
    dir_arc = arc_root.rstrip("/") + "/"
    zipf.writestr(zipfile.ZipInfo(dir_arc), "")
    for p in sorted(folder.rglob("*")):
        if p.is_file():
            rel = p.relative_to(folder).as_posix()
            arcname = f"{arc_root}/{rel}".replace("//", "/")
            zipf.write(p, arcname)


def transactional_clear_folders(folders: List[Path], temp_root: Path) -> Tuple[bool, List[Tuple[Path, Path]]]:
    move_map: List[Tuple[Path, Path]] = []
    try:
        ensure_dir(temp_root)
        for folder in folders:
            if not folder.exists():
                continue
            ensure_dir(temp_root / folder.name)
            for item in folder.iterdir():
                dst = temp_root / folder.name / item.name
                ensure_dir(dst.parent)
                shutil.move(str(item), str(dst))
                move_map.append((folder / item.name, dst))
        return True, move_map
    except Exception:
        return False, move_map


def rollback_moved_items(move_map: List[Tuple[Path, Path]]) -> None:
    for original, temp in reversed(move_map):
        try:
            ensure_dir(original.parent)
            if temp.exists():
                shutil.move(str(temp), str(original))
        except Exception:
            pass


def delete_tree(path: Path) -> None:
    if not path.exists():
        return
    if path.is_file():
        path.unlink(missing_ok=True)
    else:
        shutil.rmtree(path, ignore_errors=True)


def archive_and_clear(paths: JobPaths, rm: RollbackManager, job: str) -> Path:
    ensure_dir(paths.archive_dir)
    stamp = datetime.now().strftime("%Y%m%d")
    zip_name = f"{job} TM CA {paths.job_type}-{stamp}.zip"
    zip_path = paths.archive_dir / zip_name

    rm.backup_file(zip_path)
    if zip_path.exists():
        raise RuntimeError(f"Archive zip already exists: {zip_path}")

    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        zip_folder_contents(zf, paths.input_dir, "INPUT")
        zip_folder_contents(zf, paths.output_dir, "OUTPUT")
        zip_folder_contents(zf, paths.merged_dir, "MERGED")

    with zipfile.ZipFile(zip_path, "r") as zf:
        if len(zf.namelist()) == 0:
            raise RuntimeError("Archive ZIP appears empty or malformed.")

    rm.record_zip(zip_path)
    rm.record_created(zip_path)

    # Transactional clear: move contents of INPUT/OUTPUT/MERGED into temp hold under ARCHIVE
    temp_hold = paths.archive_dir / f"__TMP_CLEANUP_{job}_{stamp}_{os.getpid()}"
    ok, move_map = transactional_clear_folders([paths.input_dir, paths.output_dir, paths.merged_dir], temp_hold)
    if not ok:
        rollback_moved_items(move_map)
        delete_tree(temp_hold)
        raise RuntimeError("Failed while clearing INPUT/OUTPUT/MERGED contents (rollback performed).")

    delete_tree(temp_hold)
    return zip_path


# -----------------------------
# Main phases
# -----------------------------
def phase_process(args: argparse.Namespace) -> int:
    job: str = args.job
    ba_input = Path(args.ba_input)
    edr_input = Path(args.edr_input)

    job_type, files = detect_job_type(ba_input, edr_input)

    # Build job paths for detected type
    if job_type == "BA":
        paths = make_job_paths("BA", ba_input)
    else:
        paths = make_job_paths("EDR", edr_input)

    ensure_dir(paths.output_dir)
    ensure_dir(paths.merged_dir)
    ensure_dir(paths.archive_dir)

    rm = RollbackManager(paths.archive_dir)

    # Precompute destinations (only needed if we produce deliverables)
    w_pref = Path(args.w_dest)
    w_fallback = Path(args.w_fallback)
    nas_base = Path(args.nas_base)
    nas_fallback = Path(args.nas_fallback)
    year = str(args.year)

    try:
        # Defensive single-side enforcement inside script
        if eligible_ba_files(ba_input) and eligible_edr_files(edr_input):
            raise RuntimeError("Both BA and EDR input folders contain eligible files. Aborting (single job type only).")

        la_valid = sa_valid = 0
        la_blank = sa_blank = 0

        merged_files: List[str] = []
        la_frames: List[pd.DataFrame] = []
        sa_frames: List[pd.DataFrame] = []

        # Process all compliant files, but only combine production-side outputs by like prefix
        for f in files:
            if job_type == "BA":
                pref = ba_classify_prefix(f.name)
                if pref is None:
                    continue
                merged_path, df_valid, valid_count, blank_count = ba_process_file(paths, rm, f, pref)
            else:
                pref = edr_classify_prefix(f.name)
                if pref is None:
                    continue
                merged_path, df_valid, valid_count, blank_count = edr_process_file(paths, rm, f, pref)

            merged_files.append(str(merged_path.resolve()))

            if pref == "LA":
                la_valid += valid_count
                la_blank += blank_count
                if not df_valid.empty:
                    la_frames.append(df_valid)
            else:
                sa_valid += valid_count
                sa_blank += blank_count
                if not df_valid.empty:
                    sa_frames.append(df_valid)

        # If no compliant files after filtering, error
        if not merged_files:
            raise RuntimeError(f"No compliant {job_type} files were processed (expected LA_{job_type}/SA_{job_type} tokens).")

        # Build one combined production output per like-group
        out_csvs: List[Path] = []
        if job_type == "BA":
            la_out = ba_write_combined_output(paths, rm, "LA", la_frames)
            sa_out = ba_write_combined_output(paths, rm, "SA", sa_frames)
        else:
            la_out = edr_write_combined_output(paths, rm, "LA", la_frames)
            sa_out = edr_write_combined_output(paths, rm, "SA", sa_frames)

        if la_out is not None:
            out_csvs.append(la_out)
        if sa_out is not None:
            out_csvs.append(sa_out)

        # Build deliverables using exact placement keys derived from actual destinations
        w_dest_str:    str       = ""
        nas_dest_str:  str       = ""
        w_drive_files: List[str] = []
        nas_files:     List[str] = []

        if out_csvs:
            nas_pref = compute_nas_dest(nas_base, year, job, job_type)
            w_dest, nas_dest = plan_destinations(w_pref, w_fallback, nas_pref, nas_fallback)

            placements = create_and_copy_deliverables(paths, rm, job, out_csvs, w_dest, nas_dest)

            w_dest_str   = str(w_dest)
            nas_dest_str = str(nas_dest)

            w_key = f"COPIED_TO_{w_dest_str}"
            n_key = f"COPIED_TO_{nas_dest_str}"
            w_drive_files = placements.get(w_key, [])
            nas_files     = placements.get(n_key, [])

        else:
            # All input rows were blank-address; no deliverables to copy.
            # Still resolve destinations so nas_dest/w_dest strings are valid for the UI.
            nas_dest     = compute_nas_dest(nas_base, year, job, job_type)
            w_dest_str   = str(w_pref)
            nas_dest_str = str(nas_dest)

        result = {
            "job_type":       job_type,
            "la_valid_count": int(la_valid),
            "sa_valid_count": int(sa_valid),
            "la_blank_count": int(la_blank),
            "sa_blank_count": int(sa_blank),
            "merged_files":   merged_files,
            "deliverables": {
                "w_drive": w_drive_files,
                "nas":     nas_files,
            },
            "nas_dest": nas_dest_str,
            "w_dest":   w_dest_str,
        }

        json_result_print(result)
        rm.commit()
        return 0

    except Exception as e:
        # Print a concise error, rollback, and return non-zero.
        print(f"ERROR: {e}", file=sys.stderr)
        try:
            rm.rollback()
        except Exception:
            pass
        return 1

def phase_archive(args: argparse.Namespace) -> int:
    job: str = args.job
    ba_input = Path(args.ba_input)
    edr_input = Path(args.edr_input)

    # Determine what to archive by which side currently has content.
    # Enforce single-side: if both have eligible -> abort.
    try:
        job_type, _files = detect_job_type(ba_input, edr_input)
    except Exception as e:
        # In archive phase, folders may have been cleared already; fall back to "has any files" test.
        ba_any = ba_input.exists() and any(p.is_file() for p in ba_input.iterdir())
        edr_any = edr_input.exists() and any(p.is_file() for p in edr_input.iterdir())
        if ba_any and edr_any:
            print(f"ERROR: Both BA and EDR folders contain files during archive: {e}", file=sys.stderr)
            return 1
        if not ba_any and not edr_any:
            print("ERROR: Nothing to archive (both BA and EDR INPUT folders appear empty).", file=sys.stderr)
            return 1
        job_type = "BA" if ba_any else "EDR"

    paths = make_job_paths(job_type, ba_input if job_type == "BA" else edr_input)
    ensure_dir(paths.archive_dir)
    rm = RollbackManager(paths.archive_dir)

    try:
        zip_path = archive_and_clear(paths, rm, job)
        # Success message to stdout (GOJI terminal can show it)
        print(f"ARCHIVE_OK: {zip_path}")
        rm.commit()
        return 0
    except Exception as e:
        print(f"ERROR: {e}", file=sys.stderr)
        try:
            rm.rollback()
        except Exception:
            pass
        return 1


# -----------------------------
# CLI
# -----------------------------
def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Unified TMCA (BA + EDR) GOJI-integrated script")
    p.add_argument("--phase", required=True, choices=["process", "archive"], help="process or archive")
    p.add_argument("--job", required=True, help="5-digit job number")
    p.add_argument("--ba-input", required=True, help="Path to BA/INPUT")
    p.add_argument("--edr-input", required=True, help="Path to EDR/INPUT")

    # process-phase only args (safe defaults; GOJI should pass)
    p.add_argument("--w-dest", default="C:/Users/JCox/Desktop/PPWK Temp", help="Preferred W: destination")
    p.add_argument("--w-fallback", default=r"C:\Users\JCox\Desktop\MOVE TO BUSKRO", help="Fallback destination when W: unavailable")
    p.add_argument("--nas-base", default=r"\\NAS1069D9\AMPrintData", help="NAS base")
    p.add_argument("--nas-fallback", default=r"C:\Users\JCox\Desktop\MOVE TO NETWORK DRIVE", help="NAS fallback")
    p.add_argument("--year", default=datetime.now().strftime("%Y"), help="Year for NAS path (YYYY)")
    args = p.parse_args(argv)

    if not re.fullmatch(r"\d{5}", args.job.strip()):
        raise SystemExit("ERROR: --job must be exactly 5 digits (e.g., 46655).")

    return args


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    if args.phase == "process":
        return phase_process(args)
    return phase_archive(args)


if __name__ == "__main__":
    raise SystemExit(main())
