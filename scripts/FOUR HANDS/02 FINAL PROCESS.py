#!/usr/bin/env python3
r"""
GOJI - FOUR HANDS
02 FINAL PROCESS.py (GOJI)

GOJI executes this script when finalStepFH is clicked.

Expected arguments (REQUIRED):
  job_number drop_number year month count version

Where:
  job_number : job id (string)
  drop_number: numeric string (GOJI should pass digits only)
  year, month: kept for logging / compatibility (not used for archive timestamp)
  count      : end Sort Position / record count computed by GOJI
  version    : RESIDENTIAL or HOSPITALITY (case-insensitive; also accepts R/H)

Output locations (fixed):
  C:\Goji\AUTOMATION\FOUR HANDS\RESIDENTIAL\OUTPUT\FOUR HANDS (R).csv
  C:\Goji\AUTOMATION\FOUR HANDS\HOSPITALITY\OUTPUT\FOUR HANDS (H).csv

Behavior:
- Fails if the expected version output CSV does not exist.
- Creates a renamed COPY of the version output CSV in that same OUTPUT folder:
    [job_number] FOUR HANDS (R) D[drop_number]_[count].csv
    [job_number] FOUR HANDS (H) D[drop_number]_[count].csv
- Copies the renamed CSV to W:\ if available, else to fallback folder.
- Creates a ZIP archive to:
    C:\Goji\AUTOMATION\FOUR HANDS\ARCHIVE
  with name:
    [job_number] FOUR HANDS RD[drop_number]_YYYMMDD-HHMM.zip   (RESIDENTIAL)
    [job_number] FOUR HANDS HD[drop_number]_YYYMMDD-HHMM.zip   (HOSPITALITY)
- Clears ORIGINAL and the selected version's INPUT/OUTPUT folders 100% after success
  (directories are preserved; their contents are removed).
- Rollback removes created files if anything fails (source CSV is never modified).

Notes:
- Console output is ASCII-only (Windows cp1252 safe).
"""

import os
import sys
import zipfile
import shutil
import time
import traceback
import json
from pathlib import Path

print("=== INITIALIZING ===")

BASE_DIR = Path(r"C:\Goji\AUTOMATION\FOUR HANDS")
ARCHIVE_DIR = BASE_DIR / "ARCHIVE"
MANIFEST_FILE = BASE_DIR / ".goji_fourhands_state.json"

RES_DIR = BASE_DIR / "RESIDENTIAL"
HOSP_DIR = BASE_DIR / "HOSPITALITY"

RES_OUTPUT_DIR = RES_DIR / "OUTPUT"
HOSP_OUTPUT_DIR = HOSP_DIR / "OUTPUT"

RES_INPUT_DIR = RES_DIR / "INPUT"
HOSP_INPUT_DIR = HOSP_DIR / "INPUT"

RES_SOURCE = RES_OUTPUT_DIR / "FOUR HANDS (R).csv"
HOSP_SOURCE = HOSP_OUTPUT_DIR / "FOUR HANDS (H).csv"

PRIMARY_DEST = Path(r"C:\Users\JCox\Desktop\PPWK Temp")
FALLBACK_DEST = Path(r"C:\Users\JCox\Desktop\MOVE TO BUSKRO")

rollback_records = {
    "created": [],   # files created by this script to remove on rollback
}

VERSION_ORDER = ["RESIDENTIAL", "HOSPITALITY"]

def rollback():
    print("=== ROLLBACK INITIATED ===")
    try:
        for p in rollback_records["created"]:
            p = Path(p)
            if p.exists():
                try:
                    p.unlink(missing_ok=True)
                except TypeError:
                    # Python < 3.8 compatibility
                    if p.exists():
                        p.unlink()
                print(f"Removed created file: {p}")
    except Exception as e:
        print("WARNING: rollback incomplete:", e)

def _normalize_version(raw: str) -> str:
    v = str(raw).strip().upper()
    if v in ("R", "RES", "RESIDENTIAL"):
        return "RESIDENTIAL"
    if v in ("H", "HOSP", "HOSPITALITY"):
        return "HOSPITALITY"
    return ""

def _now_string() -> str:
    return time.strftime("%Y-%m-%dT%H:%M:%S")

def _normalize_version_list(values) -> list[str]:
    versions = []
    if not isinstance(values, list):
        return versions
    for value in values:
        version = _normalize_version(value)
        if version and version not in versions:
            versions.append(version)
    return [version for version in VERSION_ORDER if version in versions]

def _load_manifest() -> dict:
    if not MANIFEST_FILE.exists():
        raise FileNotFoundError(f"FOUR HANDS manifest was not found: {MANIFEST_FILE}")
    try:
        manifest = json.loads(MANIFEST_FILE.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise ValueError(f"FOUR HANDS manifest is invalid JSON: {exc}") from exc

    if not isinstance(manifest, dict):
        raise ValueError("FOUR HANDS manifest must contain a JSON object.")

    versions_present = _normalize_version_list(manifest.get("versions_present", []))
    if not versions_present:
        raise ValueError("FOUR HANDS manifest does not list any generated versions.")

    versions_complete = [
        version for version in _normalize_version_list(manifest.get("versions_complete", []))
        if version in versions_present
    ]
    outputs = manifest.get("outputs", {})
    if not isinstance(outputs, dict):
        outputs = {}

    manifest["versions_present"] = versions_present
    manifest["versions_complete"] = versions_complete
    manifest["outputs"] = outputs
    manifest.setdefault("created_at", _now_string())
    manifest["updated_at"] = _now_string()
    return manifest

def _save_manifest(manifest: dict):
    manifest["updated_at"] = _now_string()
    MANIFEST_FILE.write_text(json.dumps(manifest, indent=2), encoding="utf-8")

def _version_root(version: str) -> Path:
    if version == "RESIDENTIAL":
        return RES_DIR
    if version == "HOSPITALITY":
        return HOSP_DIR
    raise ValueError(f"Unsupported FOUR HANDS version: {version}")

def _archive_marker(versions_present: list[str]) -> str:
    letters = "".join(version[0] for version in VERSION_ORDER if version in versions_present)
    return letters or "X"

def _require_dir(path: Path):
    path.mkdir(parents=True, exist_ok=True)

def _safe_zip_add(zf: zipfile.ZipFile, root: Path, arc_prefix: str):
    """
    Add all files under 'root' into the zip under 'arc_prefix', preserving structure.
    """
    if not root.exists():
        return
    for p in root.rglob("*"):
        if p.is_file():
            rel = p.relative_to(root)
            zf.write(p, str(Path(arc_prefix) / rel))

def _clean_folder_contents(folder: Path):
    """
    Delete all files/subfolders under 'folder' (folder itself is preserved).
    """
    _require_dir(folder)
    for child in folder.iterdir():
        try:
            if child.is_dir():
                shutil.rmtree(child, ignore_errors=False)
            else:
                child.unlink()
        except Exception:
            # last resort: try again with ignore_errors
            try:
                if child.is_dir():
                    shutil.rmtree(child, ignore_errors=True)
                else:
                    if child.exists():
                        child.unlink()
            except Exception:
                pass

def _clear_version_tree_preserve_io(version_root: Path):
    """
    Clears everything under version_root, but PRESERVES INPUT and OUTPUT directories
    (their contents are cleared, not the directories themselves).
    """
    _require_dir(version_root)

    input_dir = version_root / "INPUT"
    output_dir = version_root / "OUTPUT"

    # Ensure INPUT/OUTPUT exist (even if prior steps didn't create them yet)
    _require_dir(input_dir)
    _require_dir(output_dir)

    for child in list(version_root.iterdir()):
        # Preserve INPUT/OUTPUT dirs, but clear their contents
        if child.is_dir() and child.name.upper() in ("INPUT", "OUTPUT"):
            _clean_folder_contents(child)
            continue

        # Anything else gets removed entirely
        try:
            if child.is_dir():
                shutil.rmtree(child, ignore_errors=False)
            else:
                child.unlink()
        except Exception:
            try:
                if child.is_dir():
                    shutil.rmtree(child, ignore_errors=True)
                else:
                    if child.exists():
                        child.unlink()
            except Exception:
                pass

def _pick_destination_root() -> Path:
    r"""
    Prefer W:\ if it's an accessible directory; otherwise use fallback.
    Using is_dir() is generally more reliable than exists() for mapped drives.
    """
    try:
        if PRIMARY_DEST.is_dir():
            return PRIMARY_DEST
    except Exception:
        pass
    return FALLBACK_DEST

def main() -> int:
    try:
        if len(sys.argv) != 7:
            print("FAILURE: Wrong number of arguments.")
            print("USAGE: 02 FINAL PROCESS.py job_number drop_number year month count version")
            return 2

        job_number = str(sys.argv[1]).strip()
        drop_number = str(sys.argv[2]).strip()
        year = str(sys.argv[3]).strip()
        month = str(sys.argv[4]).strip()
        count = str(sys.argv[5]).strip()
        version_raw = sys.argv[6]

        version = _normalize_version(version_raw)
        if not version:
            print(f"FAILURE: Invalid version argument: {version_raw!r}. Expected RESIDENTIAL or HOSPITALITY.")
            return 2

        if not drop_number.isdigit():
            print(f"FAILURE: drop_number must be numeric digits only. Got: {drop_number!r}")
            return 2

        manifest = _load_manifest()
        versions_present = manifest["versions_present"]
        if version not in versions_present:
            print("FAILURE: Selected version was not generated by initial processing.")
            print(f"Selected version : {version}")
            print(f"Generated versions: {', '.join(versions_present)}")
            return 1

        manifest_job = str(manifest.get("job_number", "")).strip()
        manifest_drop = str(manifest.get("drop_number", "")).strip()
        if manifest_job and manifest_job != job_number:
            print(f"WARNING: Manifest job number {manifest_job!r} does not match argument {job_number!r}.")
        if manifest_drop and manifest_drop != drop_number:
            print(f"WARNING: Manifest drop number {manifest_drop!r} does not match argument {drop_number!r}.")

        marker_paren = "(R)" if version == "RESIDENTIAL" else "(H)"

        if version == "RESIDENTIAL":
            src = RES_SOURCE
            out_dir = RES_OUTPUT_DIR
            version_root = RES_DIR
        else:
            src = HOSP_SOURCE
            out_dir = HOSP_OUTPUT_DIR
            version_root = HOSP_DIR

        # Ensure core dirs exist
        _require_dir(ARCHIVE_DIR)
        _require_dir(version_root / "INPUT")
        _require_dir(version_root / "OUTPUT")
        _require_dir(BASE_DIR / "ORIGINAL")

        if not src.exists():
            print("FAILURE: Expected output file for selected version does not exist.")
            print(f"Version   : {version}")
            print(f"Expected  : {src}")
            print("Reason    : FINAL PROCESS cannot continue without this file. Run prior steps and try again.")
            return 1

        # 1) Create renamed copy in the same OUTPUT folder
        renamed_name = f"{job_number} FOUR HANDS {marker_paren} D{drop_number}_{count}.csv"
        renamed_path = out_dir / renamed_name

        if renamed_path.exists():
            # Back up and replace (avoid accidental overwrite)
            backup = renamed_path.with_suffix(renamed_path.suffix + f".bak_{int(time.time())}")
            shutil.move(str(renamed_path), str(backup))
            rollback_records["created"].append(backup)
            print(f"NOTICE: Existing renamed output was backed up: {backup}")

        shutil.copy2(src, renamed_path)
        rollback_records["created"].append(renamed_path)
        print(f"Created renamed output: {renamed_path}")

        # 2) Copy to destination (W:\ preferred)
        dest_root = _pick_destination_root()
        _require_dir(dest_root)
        dest_path = dest_root / renamed_name
        shutil.copy2(renamed_path, dest_path)
        rollback_records["created"].append(dest_path)
        print(f"Copied to destination: {dest_path}")

        if version not in manifest["versions_complete"]:
            manifest["versions_complete"].append(version)
        manifest["versions_complete"] = [
            complete_version for complete_version in VERSION_ORDER
            if complete_version in manifest["versions_complete"] and complete_version in versions_present
        ]
        manifest["outputs"][version] = str(renamed_path)
        _save_manifest(manifest)
        print(f"Marked version complete: {version}")

        incomplete_versions = [
            present_version for present_version in versions_present
            if present_version not in manifest["versions_complete"]
        ]
        if incomplete_versions:
            print("Remaining FOUR HANDS versions are not complete; skipping archive and cleanup.")
            print(f"Remaining versions: {', '.join(incomplete_versions)}")
            print("=== SUCCESS ===")
            return 0

        # 3) Create archive zip (timestamp is NOW, not the passed year/month)
        ts = time.strftime("%Y%m%d-%H%M")
        marker_zip = _archive_marker(versions_present)
        zip_name = f"{job_number} FOUR HANDS {marker_zip}D{drop_number}_{ts}.zip"
        zip_path = ARCHIVE_DIR / zip_name

        if zip_path.exists():
            backup_zip = zip_path.with_suffix(zip_path.suffix + f".bak_{int(time.time())}")
            shutil.move(str(zip_path), str(backup_zip))
            rollback_records["created"].append(backup_zip)
            print(f"NOTICE: Existing archive zip was backed up: {backup_zip}")

        with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
            # Archive ORIGINAL plus every generated version tree (INPUT/OUTPUT included).
            _safe_zip_add(zf, BASE_DIR / "ORIGINAL", "ORIGINAL")
            for present_version in versions_present:
                present_root = _version_root(present_version)
                _safe_zip_add(zf, present_root, present_root.name)
            if MANIFEST_FILE.exists():
                zf.write(MANIFEST_FILE, MANIFEST_FILE.name)

        rollback_records["created"].append(zip_path)
        print(f"Archive created: {zip_path}")

        # 4) Cleanup (CLEAR 100% AFTER ARCHIVE)
        # Keep directory structure, wipe contents
        _clean_folder_contents(BASE_DIR / "ORIGINAL")
        for present_version in versions_present:
            _clear_version_tree_preserve_io(_version_root(present_version))
        if MANIFEST_FILE.exists():
            MANIFEST_FILE.unlink()
            print(f"Manifest removed: {MANIFEST_FILE}")

        print("=== SUCCESS ===")
        return 0

    except Exception as e:
        print("FAILURE: Exception occurred.")
        print("Reason:", str(e))
        print(traceback.format_exc())
        rollback()
        return 1

if __name__ == "__main__":
    sys.exit(main())
