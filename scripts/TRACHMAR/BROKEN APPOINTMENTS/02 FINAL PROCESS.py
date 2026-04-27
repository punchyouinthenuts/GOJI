# BROKEN APPOINTMENTS — 02 FINAL PROCESS (GOJI-compatible, TERM-style two-phase model)
# Save to: C:\Goji\scripts\TRACHMAR\BROKEN APPOINTMENTS\02 FINAL PROCESS.py
# Usage: python "02 FINAL PROCESS.py" <job_number> <year> --mode {prearchive|archive}
#
# TWO-PHASE MODEL:
# - prearchive: Enrichment, MERGED creation, ZIP, print NAS markers, exit(0)
# - archive: NAS copy, archive ZIP, cleanup
#
# Features preserved:
# - Robust logging to LOGS folder
# - Defensive header validation with debug peeks and delimiter fallback
# - Enrichment (MATCHID) + TITLE + ENGLISH/SPANISH DATE + Provider fields
# - Output filename suffix uses Sort Position (digits only)
# - MERGED creation for each ORIGINAL CSV (supports MATCHID or Call ID) with Mailed=14/13
# - MERGED ZIP only if >1 file; if only 1, keep unzipped
# - Network copy with folder creation, multi-attempt retry, SHA-256 verify, fallback + note
# - ARCHIVE ZIP of entire DATA tree, then DATA cleanup
# - Rollback scaffolding for best-effort restoration on fatal errors

import sys, os, csv, datetime, traceback, re, shutil, zipfile, hashlib, time, logging, argparse
from pathlib import Path

# =========================
# Paths & Logging
# =========================
def get_paths(year: str):
    here = Path(__file__).resolve()
    goji_root = here.parents[3]  # C:\Goji
    base = goji_root / "AUTOMATION" / "TRACHMAR" / "BROKEN APPOINTMENTS"
    return {
        "base_dir": base,
        "input_csv": base / "DATA" / "INPUT" / "INPUT.csv",
        "output_dir": base / "DATA" / "OUTPUT",
        "original_dir": base / "DATA" / "ORIGINAL",
        "merged_dir": base / "DATA" / "MERGED",
        "archive_dir": base / "ARCHIVE",
        "logs_dir": base / "LOGS",
        "fallback_dir": base / "FALLBACK_TO_NETWORK",
        "tmba_code_list": base / "DATA" / "OUTPUT" / "TMBA14 CODE LIST.csv",
        "network_base": Path(rf"\\NAS1069D9\AMPrintData\{year}_SrcFiles\T\Trachmar"),
    }

def setup_logging(logs_dir: Path):
    logs_dir.mkdir(parents=True, exist_ok=True)
    log_file = logs_dir / "trachmar_final_process.log"
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s [%(levelname)s] %(message)s",
        handlers=[
            logging.FileHandler(log_file, encoding="utf-8"),
            logging.StreamHandler(sys.stdout)
        ],
    )
    logging.info("Logging initialized")
    return log_file

# =========================
# Rollback scaffolding
# =========================
changes = {
    "created_files": [],
    "created_dirs": [],
    "moved_files": [],
    "copied_files": [],
    "modified_files": [],
    "deleted_files": [],
    "original_data": {},
}

def backup_file(fp: Path):
    try:
        if fp.exists() and fp.is_file():
            content = fp.read_text(encoding="utf-8", errors="ignore")
            changes["original_data"][str(fp)] = content
            if str(fp) not in changes["modified_files"]:
                changes["modified_files"].append(str(fp))
            logging.info(f"Backed up file content: {fp}")
    except Exception as e:
        logging.warning(f"Could not backup {fp}: {e}")

def rollback():
    logging.info("Starting rollback...")
    for s, content in changes["original_data"].items():
        try:
            p = Path(s)
            if p.exists():
                p.write_text(content, encoding="utf-8")
                logging.info(f"Restored content: {p}")
        except Exception as e:
            logging.error(f"Failed to restore {s}: {e}")
    for s in changes["created_files"]:
        try:
            p = Path(s)
            if p.exists():
                p.unlink()
                logging.info(f"Removed created file: {p}")
        except Exception as e:
            logging.error(f"Failed to remove created file {s}: {e}")
    for s in reversed(changes["created_dirs"]):
        try:
            d = Path(s)
            if d.exists() and d.is_dir():
                if not any(d.iterdir()):
                    d.rmdir()
                    logging.info(f"Removed created dir: {d}")
        except Exception as e:
            logging.error(f"Failed to remove dir {s}: {e}")
    logging.info("Rollback complete.")

# =========================
# Utilities
# =========================
def parse_args():
    parser = argparse.ArgumentParser(description="TM Broken Appointments Final Process")
    parser.add_argument("job_number", help="Job number (5 digits)")
    parser.add_argument("year", help="Year (4 digits)")
    parser.add_argument("--mode", choices=["prearchive", "archive"], required=True,
                        help="Execution mode: prearchive or archive")
    args = parser.parse_args()
    
    job = args.job_number.strip()
    year = args.year.strip()
    
    if not re.fullmatch(r"\d{5}", job):
        print(f"ERROR invalid job number: {job} (expected exactly 5 digits)")
        sys.exit(1)
    if not re.fullmatch(r"\d{4}", year):
        print(f"ERROR invalid year: {year} (expected exactly 4 digits)")
        sys.exit(1)
    
    return job, year, args.mode

def debug_peek_file(csv_path: Path, lines=2):
    try:
        with open(csv_path, "r", encoding="utf-8-sig") as f:
            peek = []
            for _ in range(lines):
                try:
                    peek.append(next(f).rstrip("\n"))
                except StopIteration:
                    break
        if peek:
            print(f"DEBUG first lines of {csv_path}: {peek}")
            logging.info(f"Peek {csv_path}: {peek}")
    except Exception as e:
        logging.warning(f"Peek failed for {csv_path}: {e}")

def validate_headers(csv_path: Path, required: list):
    debug_peek_file(csv_path)
    headers = None
    try:
        with open(csv_path, "r", encoding="utf-8-sig", newline="") as f:
            r = csv.reader(f, delimiter=",")
            headers = [h.strip() for h in next(r)]
    except Exception:
        headers = None
    if not headers:
        with open(csv_path, "r", encoding="utf-8-sig", newline="") as f:
            r = csv.reader(f, delimiter="\t")
            headers = [h.strip() for h in next(r)]
    if required:
        missing = [h for h in required if h not in headers]
        if missing:
            raise ValueError(f"Missing required columns {missing} in {csv_path}. Found: {headers}")
    return headers

def pick_id_column(headers):
    if "MATCHID" in headers: return "MATCHID"
    if "Call ID" in headers: return "Call ID"
    raise ValueError(f"No MATCHID or Call ID column present in headers: {headers}")

def to_title_case(s):
    if not isinstance(s, str): return s
    return " ".join(w.capitalize() for w in s.lower().split()) if s.isupper() else s

def format_date(yyyymmdd: str):
    if not isinstance(yyyymmdd, str) or not re.fullmatch(r"\d{8}", yyyymmdd):
        return "", ""
    try:
        dt = datetime.datetime.strptime(yyyymmdd, "%Y%m%d")
        return dt.strftime("%m/%d/%Y"), dt.strftime("%d/%m/%Y")
    except ValueError:
        return "", ""

def sha256(path: Path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            h.update(chunk)
    return h.hexdigest()

def verify_copy(src: Path, dst: Path):
    if sha256(src) != sha256(dst):
        raise ValueError("Copy verification failed (hash mismatch)")

# =========================
# Enrichment (MATCHID) + suffix from Sort Position (digits only)
# =========================
def enrich_job_file(paths, job: str) -> Path:
    out_dir = paths["output_dir"]; out_dir.mkdir(parents=True, exist_ok=True)
    src = out_dir / "TRACHMAR BROKEN APPOINTMENTS.csv"
    dst = out_dir / f"{job} TRACHMAR BROKEN APPOINTMENTS.csv"
    if not src.exists(): raise FileNotFoundError(f"Expected source file not found: {src}")
    if dst.exists(): raise FileExistsError(f"File already exists: {dst}")
    shutil.copy2(src, dst); verify_copy(src, dst)
    changes["created_files"].append(str(dst))

    validate_headers(dst, ["Endorsement Line","Broken Appointment Da","MATCHID","Sort Position"])
    validate_headers(paths["input_csv"], ["MATCHID","Provider Name"])

    provider = {}
    with open(paths["input_csv"], "r", encoding="utf-8-sig") as f:
        for row in csv.DictReader(f):
            mid = (row.get("MATCHID","") or "").strip()
            if mid:
                provider[mid] = {
                    "Provider Name": to_title_case(row.get("Provider Name","")),
                    "Provider Location Name": to_title_case(row.get("Provider Location Name",""))
                }

    tmp = dst.with_suffix(".tmp")
    record_count = "0"
    with open(dst, "r", encoding="utf-8-sig") as fin, open(tmp, "w", encoding="utf-8", newline="") as fout:
        rdr, wtr = csv.reader(fin), csv.writer(fout)
        headers = next(rdr)
        ei, bi, ci, spi = headers.index("Endorsement Line"), headers.index("Broken Appointment Da"), headers.index("MATCHID"), headers.index("Sort Position")
        newh = headers[:]; newh.insert(ei+1, "TITLE")
        newh += ["ENGLISH DATE", "SPANISH DATE", "Provider Location Name", "Provider Name"]
        wtr.writerow(newh)
        for row in rdr:
            row.insert(ei+1, "HEAD OF HOUSEHOLD")
            eng, spa = format_date(row[bi+1])
            info = provider.get(row[ci+1], {"Provider Name":"","Provider Location Name":""})
            sp_val = row[spi] if spi < len(row) else ""
            digits = "".join(ch for ch in sp_val if ch.isdigit())
            if digits: record_count = digits
            row += [eng, spa, info["Provider Location Name"], info["Provider Name"]]
            wtr.writerow(row)
    dst.unlink(); tmp.rename(dst)

    suffixed = dst.with_name(dst.stem + f"_{record_count or '0'}" + dst.suffix)
    if suffixed.exists():
        ts = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
        suffixed = dst.with_name(dst.stem + f"_{record_count}_{ts}" + dst.suffix)
    dst.rename(suffixed)
    logging.info(f"Enriched file created: {suffixed}")
    return suffixed

# =========================
# MERGED creation (MATCHID or Call ID) + 14/13 marking
# =========================
def process_merged_files(paths, job: str):
    tmba_path = paths["tmba_code_list"]
    original_dir, merged_dir = paths["original_dir"], paths["merged_dir"]
    merged_dir.mkdir(parents=True, exist_ok=True)

    tmba_headers = validate_headers(tmba_path, [])
    tmba_id_col = pick_id_column(tmba_headers)
    tmba_ids = set()
    with open(tmba_path, "r", encoding="utf-8-sig") as f:
        for row in csv.DictReader(f):
            val = (row.get(tmba_id_col, "") or "").strip()
            if val: tmba_ids.add(val)
    if not tmba_ids: raise ValueError(f"No IDs found in {tmba_path} using '{tmba_id_col}'")

    originals = sorted(original_dir.glob("*.csv"))
    if not originals:
        print("No CSV files in ORIGINAL; skipping MERGED step.")
        return []

    merged_files = []
    marked_14_count = 0
    for orig in originals:
        with open(orig, "r", encoding="utf-8-sig", newline="") as f:
            rdr = csv.reader(f)
            try:
                headers = [h.strip() for h in next(rdr)]
            except StopIteration:
                print(f"WARNING: {orig.name} is empty; skipping.")
                continue
        id_col = pick_id_column(headers)
        merged_path = merged_dir / (orig.stem + "_MERGED.csv")
        if merged_path.exists():
            ts = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
            merged_path = merged_dir / (orig.stem + f"_MERGED_{ts}.csv")
        with open(orig, "r", encoding="utf-8-sig", newline="") as fin, open(merged_path, "w", encoding="utf-8", newline="") as fout:
            rdr, wtr = csv.reader(fin), csv.writer(fout)
            headers = next(rdr); idx = headers.index(id_col)
            wtr.writerow(headers + ["Mailed"])
            for row in rdr:
                id_val = row[idx] if idx < len(row) else ""
                mailed = "14" if id_val in tmba_ids else "13"
                if mailed == "14": marked_14_count += 1
                wtr.writerow(row + [mailed])
        merged_files.append(str(merged_path))
        changes["created_files"].append(str(merged_path))
        logging.info(f"Created merged file: {merged_path} (ID={id_col})")

    if marked_14_count != len(tmba_ids):
        warn = f"WARNING: Marked '14' count ({marked_14_count}) != TMBA14 size ({len(tmba_ids)})"
        print(warn); logging.warning(warn)

    return merged_files

# =========================
# Zip MERGED (only if >1 file, delete originals after ZIP)
# =========================
def zip_merged_files(paths, job: str, merged_files: list):
    if len(merged_files) <= 1:
        logging.info("MERGED ZIP skipped (<=1 file).")
        return None
    merged_dir = paths["merged_dir"]
    zip_path = merged_dir / f"{job} TM BROKEN APPTS (MERGED).zip"
    if zip_path.exists():
        ts = datetime.datetime.now().strftime("%Y%m%d-%H%M")
        zip_path = merged_dir / f"{job} TM BROKEN APPTS (MERGED)_{ts}.zip"
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
        for f in merged_files:
            p = Path(f); zf.write(p, p.name)
    with zipfile.ZipFile(zip_path, "r") as zf:
        if zf.testzip() is not None:
            raise ValueError("MERGED ZIP is corrupted")
    changes["created_files"].append(str(zip_path))
    logging.info(f"Created MERGED ZIP: {zip_path}")
    
    # Delete original _MERGED files after successful ZIP
    for f in merged_files:
        try:
            p = Path(f)
            if p.exists():
                p.unlink()
                logging.info(f"Deleted original MERGED file: {p}")
        except Exception as e:
            logging.warning(f"Failed to delete {p}: {e}")
    
    return zip_path

# =========================
# Network copy with retries + fallback
# =========================
def sha_match(a: Path, b: Path):
    return sha256(a) == sha256(b)

def copy_to_network_folder(job: str, paths, file_to_copy: Path, max_attempts=3, wait_seconds=2):
    base = paths["network_base"]
    job_folder = base / f"{job}_BrokenAppt"
    data_folder = job_folder / "HP Indigo" / "DATA"

    for attempt in range(1, max_attempts + 1):
        try:
            job_folder.mkdir(parents=True, exist_ok=True)

            # Define all subfolders to create
            subfolders = [
                job_folder / "PDF for Client",
                job_folder / "Files for Ricoh",
                job_folder / "Original Files",
                job_folder / "HP Indigo" / "DATA",
                job_folder / "HP Indigo" / "PRINT",
                job_folder / "HP Indigo" / "PROOF",
            ]

            for folder in subfolders:
                if not folder.exists():
                    folder.mkdir(parents=True)
                    logging.info(f"Created folder: {folder}")

            data_folder = job_folder / "HP Indigo" / "DATA"

            dest = data_folder / file_to_copy.name
            shutil.copy2(file_to_copy, dest)
            if not sha_match(file_to_copy, dest):
                raise ValueError("Hash mismatch after network copy")
            changes["copied_files"].append((str(file_to_copy), str(dest)))
            print(f"COPIED_TO_NAS={dest}")
            logging.info(f"Copied to NAS: {dest}")
            return dest
        except Exception as e:
            logging.error(f"Network copy attempt {attempt}/{max_attempts} failed: {e}")
            if attempt < max_attempts:
                time.sleep(wait_seconds)

    # Fallback
    fb = paths["fallback_dir"]; fb.mkdir(parents=True, exist_ok=True)
    dest = fb / file_to_copy.name
    shutil.copy2(file_to_copy, dest)
    if not sha_match(file_to_copy, dest):
        raise ValueError("Fallback copy hash mismatch")
    changes["copied_files"].append((str(file_to_copy), str(dest)))
    note = dest.with_suffix(".txt")
    note.write_text(f"Intended network location: {data_folder}", encoding="utf-8")
    changes["created_files"].append(str(note))
    print(f"FALLBACK_COPY={dest}")
    logging.info(f"Copied to fallback: {dest}; note: {note}")
    return dest


# =========================
# Ensure NAS folder structure exists (no file copy)
# =========================
def ensure_network_folders(job: str, paths):
    """Create the standard NAS folder structure for the job (best-effort)."""
    try:
        base = paths["network_base"]
        job_folder = base / f"{job}_BrokenAppt"
        subfolders = [
            job_folder / "PDF for Client",
            job_folder / "Files for Ricoh",
            job_folder / "Original Files",
            job_folder / "HP Indigo" / "DATA",
            job_folder / "HP Indigo" / "PRINT",
            job_folder / "HP Indigo" / "PROOF",
        ]
        for folder in subfolders:
            folder.mkdir(parents=True, exist_ok=True)
        logging.info(f"Ensured NAS folder structure exists under: {job_folder}")
        return job_folder
    except Exception as e:
        logging.error(f"Could not ensure NAS folder structure (will retry later during copy): {e}")
        return None

# =========================
# Archive DATA and cleanup
# =========================
def zip_data_folders(paths, job: str):
    data_dir = paths["base_dir"] / "DATA"
    archive_dir = paths["archive_dir"]; archive_dir.mkdir(parents=True, exist_ok=True)
    zip_path = archive_dir / f"{job} TM BROKEN APPOINTMENTS.zip"
    if zip_path.exists():
        ts = datetime.datetime.now().strftime("%Y%m%d-%H%M")
        zip_path = archive_dir / f"{job} TM BROKEN APPOINTMENTS_{ts}.zip"
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zipf:
        for root, _, files in os.walk(data_dir):
            for file in files:
                fp = Path(root) / file
                rel = fp.relative_to(paths["base_dir"].parent)
                zipf.write(fp, rel)
    with zipfile.ZipFile(zip_path, "r") as z:
        if z.testzip() is not None:
            raise ValueError("ARCHIVE ZIP is corrupted")
    changes["created_files"].append(str(zip_path))
    logging.info(f"Created ARCHIVE ZIP: {zip_path}")

    # Clean DATA
    for root, _, files in os.walk(data_dir):
        for file in files:
            try:
                p = Path(root) / file
                backup_file(p)
                p.unlink()
                changes["deleted_files"].append(str(p))
            except Exception as e:
                logging.warning(f"Could not remove {p}: {e}")
    print("Cleared contents of DATA folders")
    logging.info("Cleared contents of DATA folders")
    return zip_path

# =========================
# Main
# =========================
def main():
    job, year, mode = parse_args()
    paths = get_paths(year)
    log_file = setup_logging(paths["logs_dir"])

    try:
        if mode == "prearchive":
            print("=== START PREARCHIVE ===")
            print(f"JOB={job} YEAR={year}")
            print(f"OUTPUT_DIR={paths['output_dir']}")
            print(f"INPUT_CSV={paths['input_csv']}")
            print(f"ORIGINAL_DIR={paths['original_dir']}")
            print(f"MERGED_DIR={paths['merged_dir']}")
            logging.info(f"Begin prearchive job {job}/{year}")

            enriched = enrich_job_file(paths, job)
            print(f"ENRICHED_FILE={enriched}")

            merged = process_merged_files(paths, job)
            print(f"MERGED_FILES_COUNT={len(merged)}")

            merged_zip = zip_merged_files(paths, job, merged) if merged else None
            if merged_zip:
                print(f"MERGED_ZIP={merged_zip}")

            # Ensure NAS folder structure exists BEFORE any UI pause/pop-up
            ensure_network_folders(job, paths)

            # Print NAS target
            print("=== NAS_FOLDER_PATH ===")
            print(str(paths['network_base'] / f'{job}_BrokenAppt' / 'HP Indigo' / 'DATA'))
            print("=== END_NAS_FOLDER_PATH ===")

            print("=== DONE PREARCHIVE ===")
            logging.info(f"Prearchive job {job}/{year} completed successfully")
            sys.exit(0)

        elif mode == "archive":
            print("=== START ARCHIVE ===")
            print(f"JOB={job} YEAR={year}")
            logging.info(f"Begin archive job {job}/{year}")

            # Find enriched file
            out_dir = paths["output_dir"]
            enriched_files = list(out_dir.glob(f"{job} TRACHMAR BROKEN APPOINTMENTS_*.csv"))
            if not enriched_files:
                raise FileNotFoundError(f"No enriched file found for job {job}")
            enriched = enriched_files[0]

            copied_to = copy_to_network_folder(job, paths, enriched)
            print(f"NET_COPY_RESULT={copied_to}")

            archive_zip = zip_data_folders(paths, job)
            print(f"ARCHIVE_ZIP={archive_zip}")

            print("=== DONE ARCHIVE ===")
            logging.info(f"Archive job {job}/{year} completed successfully")

    except Exception as e:
        print("ERROR", e)
        logging.exception(f"Fatal error: {e}")
        rollback()
        sys.exit(1)

if __name__ == "__main__":
    main()
