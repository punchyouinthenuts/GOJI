import os
import zipfile
import shutil
from pathlib import Path

# Define paths
BASE_PATH = Path(r'C:\Program Files\Goji\RAC\SWEEPS')
INPUT_ZIP = BASE_PATH / 'INPUTZIP'
FILEBOX = BASE_PATH / 'FILEBOX'
JOB_INPUT = BASE_PATH / 'JOB' / 'INPUT'
MACOSX = FILEBOX / '__MACOSX'

def process_files():
    """Main processing function"""
    try:
        # Ensure directories exist
        FILEBOX.mkdir(parents=True, exist_ok=True)
        JOB_INPUT.mkdir(parents=True, exist_ok=True)

        # Find and extract ZIP file
        zip_files = list(INPUT_ZIP.glob('*.zip'))
        if zip_files:
            with zipfile.ZipFile(zip_files[0], 'r') as zip_ref:
                zip_ref.extractall(FILEBOX)
            print(f"Extracted zip file to {FILEBOX}")

        # Remove __MACOSX folder if it exists
        if MACOSX.exists():
            shutil.rmtree(MACOSX)
            print("Removed __MACOSX folder")

        # Find and move TXT files - excluding __MACOSX
        txt_files = []
        for txt_file in FILEBOX.rglob('*.txt'):
            if '__MACOSX' not in str(txt_file):
                txt_files.append(txt_file)

        # Move and rename files
        for index, txt_file in enumerate(sorted(txt_files), 1):
            new_name = f"SWEEPS {index:02d}.txt"
            shutil.move(str(txt_file), str(JOB_INPUT / new_name))
            print(f"Moved and renamed {txt_file.name} to {new_name}")

        if not txt_files:
            print("No TXT files found to move")

        # Clean up directories
        if FILEBOX.exists():
            shutil.rmtree(FILEBOX)
            FILEBOX.mkdir()
        
        if INPUT_ZIP.exists():
            for file in INPUT_ZIP.iterdir():
                file.unlink()

        print("File processing completed successfully!")

    except Exception as e:
        print(f"Error during processing: {e}")
        raise

if __name__ == "__main__":
    process_files()
