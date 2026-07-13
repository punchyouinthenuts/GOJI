#!/usr/bin/env python3
r"""
GOJI - FOUR HANDS
02 FINAL PROCESS.py (GOJI)

GOJI executes this script when finalStepFH is clicked.

Expected arguments (REQUIRED):
  job_number drop_number year month count version
"""

import hashlib
import json
import os
import re
import shutil
import sys
import time
import traceback
import zipfile
from pathlib import Path

print("=== INITIALIZING ===")

BASE_DIR = Path(r"C:\Goji\AUTOMATION\FOUR HANDS")
ARCHIVE_DIR = BASE_DIR / "ARCHIVE"
MANIFEST_FILE = BASE_DIR / ".goji_fourhands_state.json"

RES_DIR = BASE_DIR / "RESIDENTIAL"
HOSP_DIR = BASE_DIR / "HOSPITALITY"

RES_OUTPUT_DIR = RES_DIR / "OUTPUT"
HOSP_OUTPUT_DIR = HOSP_DIR / "OUTPUT"

RES_SOURCE = RES_OUTPUT_DIR / "FOUR HANDS (R).csv"
HOSP_SOURCE = HOSP_OUTPUT_DIR / "FOUR HANDS (H).csv"

PRIMARY_DEST = Path(r"C:\Users\JCox\Desktop\PPWK Temp")
FALLBACK_DEST = Path(r"C:\Users\JCox\Desktop\MOVE TO BUSKRO")
FOUR_HANDS_NAS_BASE = Path(r"\\NAS1069D9\AMPrintData")

VERSION_ORDER = ["RESIDENTIAL", "HOSPITALITY"]
LOOKBOOK_FOLDER_RE = re.compile(r"^([0-9]+)_LookBookLabels$")
REQUIRED_LOOKBOOK_DIRS = [
    Path("."),
    Path("Files for Ricoh"),
    Path("HP Indigo"),
    Path("HP Indigo") / "DATA",
    Path("HP Indigo") / "PRINT",
    Path("HP Indigo") / "PROOF",
    Path("Original Files"),
    Path("PDF for Client"),
]

temp_files_created = []


def _normalize_version(raw: str) -> str:
    value = str(raw).strip().upper()
    if value in ("R", "RES", "RESIDENTIAL"):
        return "RESIDENTIAL"
    if value in ("H", "HOSP", "HOSPITALITY"):
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


def _save_manifest_atomic(manifest: dict):
    manifest["updated_at"] = _now_string()
    MANIFEST_FILE.parent.mkdir(parents=True, exist_ok=True)
    temp_path = MANIFEST_FILE.with_name(MANIFEST_FILE.name + ".tmp")
    payload = json.dumps(manifest, indent=2)
    with temp_path.open("w", encoding="utf-8") as handle:
        handle.write(payload)
        handle.flush()
        os.fsync(handle.fileno())
    os.replace(temp_path, MANIFEST_FILE)


def _require_dir(path: Path):
    if path.exists() and not path.is_dir():
        raise NotADirectoryError(f"Required directory path exists but is not a directory: {path}")
    path.mkdir(parents=True, exist_ok=True)


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _same_file_content(left: Path, right: Path) -> bool:
    if not left.is_file() or not right.is_file():
        return False
    if left.stat().st_size != right.stat().st_size:
        return False
    return _sha256(left) == _sha256(right)


def _verify_regular_readable_nonzero(path: Path, label: str):
    if not path.exists():
        raise FileNotFoundError(f"{label} does not exist: {path}")
    if not path.is_file():
        raise ValueError(f"{label} is not a regular file: {path}")
    if path.stat().st_size <= 0:
        raise ValueError(f"{label} is empty: {path}")
    with path.open("rb") as handle:
        handle.read(1)


def _goji_temp_path(final_path: Path) -> Path:
    return final_path.with_name(final_path.name + ".goji_tmp")


def _cleanup_temp(path: Path):
    try:
        if path.exists() and path.is_file():
            path.unlink()
            print(f"Removed temporary file: {path}")
    except Exception as exc:
        print(f"WARNING: Could not remove temporary file {path}: {exc}")


def _verified_copy_to_temp(source: Path, temp_path: Path):
    _cleanup_temp(temp_path)
    shutil.copy2(source, temp_path)
    temp_files_created.append(temp_path)
    if not temp_path.is_file():
        raise IOError(f"Temporary copy was not created: {temp_path}")
    if source.stat().st_size != temp_path.stat().st_size:
        raise IOError(f"Temporary copy size mismatch: {temp_path}")
    if _sha256(source) != _sha256(temp_path):
        raise IOError(f"Temporary copy hash mismatch: {temp_path}")


def _finalize_temp_without_overwrite(source: Path, temp_path: Path, final_path: Path):
    if final_path.exists():
        if final_path.is_file() and _same_file_content(source, final_path):
            _cleanup_temp(temp_path)
            return
        _cleanup_temp(temp_path)
        raise FileExistsError(f"Destination already exists and differs: {final_path}")
    os.rename(temp_path, final_path)
    if source.stat().st_size != final_path.stat().st_size:
        raise IOError(f"Final file size mismatch: {final_path}")
    if _sha256(source) != _sha256(final_path):
        raise IOError(f"Final file hash mismatch: {final_path}")


def _ensure_local_final_csv(source: Path, final_path: Path) -> Path:
    _verify_regular_readable_nonzero(source, "Bulk Mailer output CSV")
    _require_dir(final_path.parent)
    temp_path = _goji_temp_path(final_path)

    if final_path.exists():
        if not final_path.is_file():
            raise ValueError(f"Local finalized CSV path is not a file: {final_path}")
        if _same_file_content(source, final_path):
            print(f"Reusing existing finalized output: {final_path}")
            return final_path

    _verified_copy_to_temp(source, temp_path)
    os.replace(temp_path, final_path)
    if source.stat().st_size != final_path.stat().st_size:
        raise IOError(f"Local finalized CSV size mismatch: {final_path}")
    if _sha256(source) != _sha256(final_path):
        raise IOError(f"Local finalized CSV hash mismatch: {final_path}")
    print(f"Created finalized output: {final_path}")
    return final_path


def _copy_required_csv_to_destination(local_csv: Path, final_path: Path):
    _verify_regular_readable_nonzero(local_csv, "Finalized Residential CSV")
    if not final_path.parent.exists() or not final_path.parent.is_dir():
        raise NotADirectoryError(f"Residential DATA folder is unavailable: {final_path.parent}")

    if final_path.exists():
        if not final_path.is_file():
            raise ValueError(f"Residential CSV destination exists but is not a file: {final_path}")
        if _same_file_content(local_csv, final_path):
            print(f"Residential CSV already delivered and verified: {final_path}")
            return final_path
        raise FileExistsError(
            f"Residential CSV destination already exists and differs. Source: {local_csv} Destination: {final_path}"
        )

    temp_path = _goji_temp_path(final_path)
    _verified_copy_to_temp(local_csv, temp_path)
    try:
        _finalize_temp_without_overwrite(local_csv, temp_path, final_path)
    except Exception:
        _cleanup_temp(temp_path)
        raise
    print(f"Residential CSV delivered and verified: {final_path}")
    return final_path


def _copy_optional_indd(source: Path, final_path: Path):
    try:
        if final_path.exists():
            if final_path.is_file() and _same_file_content(source, final_path):
                print(f"INDD already exists and is identical: {final_path}")
                return
            print(f"WARNING: Destination INDD exists and differs; leaving untouched: {final_path}")
            return

        temp_path = _goji_temp_path(final_path)
        _verified_copy_to_temp(source, temp_path)
        _finalize_temp_without_overwrite(source, temp_path, final_path)
        print(f"Copied previous INDD for current job: {final_path}")
    except Exception as exc:
        print(f"WARNING: Optional INDD copy skipped: {exc}")
        try:
            _cleanup_temp(_goji_temp_path(final_path))
        except Exception:
            pass


def _lookbook_root(year: str, job_number: str) -> Path:
    return FOUR_HANDS_NAS_BASE / f"{year}_SrcFiles" / "F" / "Four Hands" / f"{job_number}_LookBookLabels"


def _inspect_lookbook_tree(root: Path):
    missing = []
    for relative in REQUIRED_LOOKBOOK_DIRS:
        path = root if str(relative) == "." else root / relative
        if path.exists() and not path.is_dir():
            raise NotADirectoryError(f"Required LookBookLabels directory path is not a directory: {path}")
        if not path.exists():
            missing.append(relative)
    return not missing, missing


def _create_missing_lookbook_dirs(root: Path):
    for relative in REQUIRED_LOOKBOOK_DIRS:
        path = root if str(relative) == "." else root / relative
        _require_dir(path)
    print(f"Created/verified LookBookLabels folder tree: {root}")


def _find_previous_indd(current_year: int, current_job: int):
    candidates = []
    for year in (current_year, current_year - 1):
        base = FOUR_HANDS_NAS_BASE / f"{year}_SrcFiles" / "F" / "Four Hands"
        try:
            if not base.exists() or not base.is_dir():
                print(f"WARNING: Previous INDD search root unavailable: {base}")
                continue
            for child in base.iterdir():
                if not child.is_dir():
                    continue
                match = LOOKBOOK_FOLDER_RE.match(child.name)
                if not match:
                    continue
                job = int(match.group(1))
                if job >= current_job:
                    continue
                candidates.append((year, job, child))
        except Exception as exc:
            print(f"WARNING: Previous INDD folder scan failed for {base}: {exc}")
            continue

    candidates.sort(key=lambda item: (item[0], item[1]), reverse=True)
    for _year, job, folder in candidates:
        expected = folder / f"{job}_FH_LookBook_Label.indd"
        if expected.is_file():
            print(f"Selected previous INDD: {expected}")
            return expected
        print(f"WARNING: Expected INDD missing in previous folder: {expected}")
    print("WARNING: No exact previous LookBookLabels INDD found; continuing without INDD copy.")
    return None


def _deliver_residential(local_csv: Path, year: str, job_number: str) -> Path:
    if not year.isdigit():
        raise ValueError(f"Residential network year must be numeric: {year!r}")
    current_job = int(job_number)
    root = _lookbook_root(year, job_number)
    was_complete, missing = _inspect_lookbook_tree(root)

    if was_complete:
        print(f"Existing complete LookBookLabels tree found; skipping INDD work: {root}")
    else:
        if missing:
            print("LookBookLabels tree is incomplete; missing: " + ", ".join(str(item) for item in missing))
        previous_indd = _find_previous_indd(int(year), current_job)
        _create_missing_lookbook_dirs(root)
        if previous_indd:
            target_indd = root / f"{job_number}_FH_LookBook_Label.indd"
            _copy_optional_indd(previous_indd, target_indd)

    data_dir = root / "HP Indigo" / "DATA"
    dest_path = data_dir / local_csv.name
    _copy_required_csv_to_destination(local_csv, dest_path)
    return data_dir


def _pick_hospitality_destination_root() -> Path:
    try:
        if PRIMARY_DEST.is_dir():
            return PRIMARY_DEST
    except Exception:
        pass
    return FALLBACK_DEST


def _deliver_hospitality(local_csv: Path) -> Path:
    dest_root = _pick_hospitality_destination_root()
    _require_dir(dest_root)
    dest_path = dest_root / local_csv.name
    shutil.copy2(local_csv, dest_path)
    print(f"Copied to destination: {dest_path}")
    return dest_path


def _version_root(version: str) -> Path:
    if version == "RESIDENTIAL":
        return RES_DIR
    if version == "HOSPITALITY":
        return HOSP_DIR
    raise ValueError(f"Unsupported FOUR HANDS version: {version}")


def _archive_marker(versions_present: list[str]) -> str:
    letters = "".join(version[0] for version in VERSION_ORDER if version in versions_present)
    return letters or "X"


def _safe_zip_add(zf: zipfile.ZipFile, root: Path, arc_prefix: str):
    if not root.exists():
        return
    for path in root.rglob("*"):
        if path.is_file():
            zf.write(path, str(Path(arc_prefix) / path.relative_to(root)))


def _clean_folder_contents(folder: Path):
    _require_dir(folder)
    for child in folder.iterdir():
        try:
            if child.is_dir():
                shutil.rmtree(child, ignore_errors=False)
            else:
                child.unlink()
        except Exception:
            try:
                if child.is_dir():
                    shutil.rmtree(child, ignore_errors=True)
                elif child.exists():
                    child.unlink()
            except Exception:
                pass


def _clear_version_tree_preserve_io(version_root: Path):
    _require_dir(version_root)
    input_dir = version_root / "INPUT"
    output_dir = version_root / "OUTPUT"
    _require_dir(input_dir)
    _require_dir(output_dir)

    for child in list(version_root.iterdir()):
        if child.is_dir() and child.name.upper() in ("INPUT", "OUTPUT"):
            _clean_folder_contents(child)
            continue
        try:
            if child.is_dir():
                shutil.rmtree(child, ignore_errors=False)
            else:
                child.unlink()
        except Exception:
            try:
                if child.is_dir():
                    shutil.rmtree(child, ignore_errors=True)
                elif child.exists():
                    child.unlink()
            except Exception:
                pass


def _emit_residential_marker(data_path: Path):
    print("=== FH_RESIDENTIAL_DATA_PATH ===")
    print("RESIDENTIAL")
    print(str(data_path))
    print("=== END_FH_RESIDENTIAL_DATA_PATH ===")


def _cleanup_created_temps():
    for path in list(temp_files_created):
        _cleanup_temp(Path(path))


def main() -> int:
    residential_marker_path = None
    try:
        if len(sys.argv) != 7:
            print("FAILURE: Wrong number of arguments.")
            print("USAGE: 02 FINAL PROCESS.py job_number drop_number year month count version")
            return 2

        job_number = str(sys.argv[1]).strip()
        drop_number = str(sys.argv[2]).strip()
        year = str(sys.argv[3]).strip()
        count = str(sys.argv[5]).strip()
        version = _normalize_version(sys.argv[6])

        if not version:
            print(f"FAILURE: Invalid version argument: {sys.argv[6]!r}. Expected RESIDENTIAL or HOSPITALITY.")
            return 2
        if not drop_number.isdigit():
            print(f"FAILURE: drop_number must be numeric digits only. Got: {drop_number!r}")
            return 2
        if not job_number.isdigit():
            print(f"FAILURE: job_number must be numeric digits only. Got: {job_number!r}")
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
            source = RES_SOURCE
            output_dir = RES_OUTPUT_DIR
            version_root = RES_DIR
        else:
            source = HOSP_SOURCE
            output_dir = HOSP_OUTPUT_DIR
            version_root = HOSP_DIR

        _require_dir(ARCHIVE_DIR)
        _require_dir(version_root / "INPUT")
        _require_dir(version_root / "OUTPUT")
        _require_dir(BASE_DIR / "ORIGINAL")

        renamed_name = f"{job_number} FOUR HANDS {marker_paren} D{drop_number}_{count}.csv"
        renamed_path = output_dir / renamed_name
        local_csv = _ensure_local_final_csv(source, renamed_path)

        if version == "RESIDENTIAL":
            residential_marker_path = _deliver_residential(local_csv, year, job_number)
        else:
            _deliver_hospitality(local_csv)

        if version not in manifest["versions_complete"]:
            manifest["versions_complete"].append(version)
        manifest["versions_complete"] = [
            complete_version for complete_version in VERSION_ORDER
            if complete_version in manifest["versions_complete"] and complete_version in versions_present
        ]
        manifest["outputs"][version] = str(renamed_path)
        if version == "RESIDENTIAL" and residential_marker_path is not None:
            manifest["outputs"]["RESIDENTIAL_DATA_PATH"] = str(residential_marker_path)
        _save_manifest_atomic(manifest)
        print(f"Marked version complete: {version}")

        incomplete_versions = [
            present_version for present_version in versions_present
            if present_version not in manifest["versions_complete"]
        ]
        if incomplete_versions:
            print("Remaining FOUR HANDS versions are not complete; skipping archive and cleanup.")
            print(f"Remaining versions: {', '.join(incomplete_versions)}")
            if version == "RESIDENTIAL" and residential_marker_path is not None:
                _emit_residential_marker(residential_marker_path)
            print("=== SUCCESS ===")
            return 0

        ts = time.strftime("%Y%m%d-%H%M")
        marker_zip = _archive_marker(versions_present)
        zip_name = f"{job_number} FOUR HANDS {marker_zip}D{drop_number}_{ts}.zip"
        zip_path = ARCHIVE_DIR / zip_name

        if zip_path.exists():
            backup_zip = zip_path.with_suffix(zip_path.suffix + f".bak_{int(time.time())}")
            shutil.move(str(zip_path), str(backup_zip))
            print(f"NOTICE: Existing archive zip was backed up: {backup_zip}")

        with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
            _safe_zip_add(zf, BASE_DIR / "ORIGINAL", "ORIGINAL")
            for present_version in versions_present:
                present_root = _version_root(present_version)
                _safe_zip_add(zf, present_root, present_root.name)
            if MANIFEST_FILE.exists():
                zf.write(MANIFEST_FILE, MANIFEST_FILE.name)

        print(f"Archive created: {zip_path}")

        _clean_folder_contents(BASE_DIR / "ORIGINAL")
        for present_version in versions_present:
            _clear_version_tree_preserve_io(_version_root(present_version))
        if MANIFEST_FILE.exists():
            MANIFEST_FILE.unlink()
            print(f"Manifest removed: {MANIFEST_FILE}")

        if version == "RESIDENTIAL" and residential_marker_path is not None:
            _emit_residential_marker(residential_marker_path)
        print("=== SUCCESS ===")
        return 0

    except Exception as exc:
        print("FAILURE: Exception occurred.")
        print("Reason:", str(exc))
        print(traceback.format_exc())
        _cleanup_created_temps()
        return 1


if __name__ == "__main__":
    sys.exit(main())
