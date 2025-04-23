from pathlib import Path
import logging
from typing import List, Set

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)

def setup_target_files() -> Set[str]:
    """Create a set of unique target files"""
    return {
        "SWEEPS_PresortReport.PDF",
        "SWEEPS_PT.PDF",
        "SWEEPS_TT.PDF"
    }

def rename_files(folder_path: Path, target_files: Set[str], prefix: str) -> None:
    """Rename files with error handling"""
    for file_path in folder_path.iterdir():
        if file_path.name in target_files:
            try:
                new_name = f"{prefix}_{file_path.name}"
                new_path = file_path.with_name(new_name)
                file_path.rename(new_path)
                logging.info(f"Renamed: {file_path.name} -> {new_name}")
            except Exception as e:
                logging.error(f"Error renaming {file_path.name}: {str(e)}")

def main():
    # Setup paths and configurations
    folder_path = Path(r"C:\Users\JCox\Desktop\PPWK Temp")
    target_files = setup_target_files()
    
    # Validate folder path
    if not folder_path.exists():
        logging.error(f"Folder not found: {folder_path}")
        return

    # Get user input with validation
    while True:
        prefix = input("ENTER SWEEPS JOB AND QUARTER NUMBER XXXXX YX: ").strip()
        if prefix:
            logging.info(f"Using prefix: {prefix}")
            break
        logging.warning("Please enter a valid prefix")

    # Process files
    rename_files(folder_path, target_files, prefix)
    logging.info("File renaming completed")

if __name__ == "__main__":
    main()
