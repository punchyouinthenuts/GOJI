from pathlib import Path
import shutil
import zipfile
import logging
from typing import List, Optional

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)

class SweepsProcessor:
    def __init__(self, user_input: str):
        self.prefix = user_input
        self.sweeps_number = user_input.split(" ")[0]
        self.quarter = user_input.split(" ")[1]
        
        # Define paths
        self.directories = [
            Path(r"C:\Program Files\Goji\RAC\SWEEPS\JOB\INPUT"),
            Path(r"C:\Program Files\Goji\RAC\SWEEPS\JOB\OUTPUT"),
            Path(r"C:\Program Files\Goji\RAC\SWEEPS\JOB\PROOF")
        ]
        self.source_folder = Path(r"C:\Program Files\Goji\RAC\SWEEPS\JOB")
        self.dest_folder = Path(r"C:\Program Files\Goji\RAC\SWEEPS") / f"{self.sweeps_number} {self.quarter}"
        self.zip_folder = Path(r"C:\Program Files\Goji\RAC\SWEEPS\SWEEPS_ZIP")

    def rename_files(self, directory: Path) -> None:
        """Rename files with error handling"""
        for file_path in directory.iterdir():
            if file_path.is_file():
                try:
                    new_name = f"{self.prefix}-{file_path.name}"
                    file_path.rename(directory / new_name)
                    logging.info(f"Renamed: {file_path.name} -> {new_name}")
                except Exception as e:
                    logging.error(f"Error renaming {file_path.name}: {str(e)}")

    def delete_files(self, directory: Path) -> None:
        """Delete files with error handling"""
        for file_path in directory.iterdir():
            if file_path.is_file():
                try:
                    file_path.unlink()
                    logging.info(f"Deleted: {file_path.name}")
                except Exception as e:
                    logging.error(f"Error deleting {file_path.name}: {str(e)}")

    def create_zip(self, source_dir: Path, zip_path: Path) -> None:
        """Create zip file with error handling"""
        try:
            with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED) as zipf:
                for file_path in source_dir.rglob('*'):
                    if file_path.is_file():
                        arcname = file_path.relative_to(source_dir)
                        zipf.write(file_path, arcname)
            logging.info(f"Created zip file: {zip_path}")
        except Exception as e:
            logging.error(f"Error creating zip file: {str(e)}")

    def process(self) -> bool:
        try:
            # Validate directories
            for directory in self.directories:
                if not directory.exists():
                    raise FileNotFoundError(f"Directory not found: {directory}")

            # Process files
            for directory in self.directories:
                self.rename_files(directory)

            # Copy directory tree
            shutil.copytree(self.source_folder, self.dest_folder)
            logging.info(f"Copied directory tree to: {self.dest_folder}")

            # Clean up original directories
            for directory in self.directories:
                self.delete_files(directory)

            # Create zip file
            proof_directory = self.dest_folder / "PROOF"
            zip_path = self.zip_folder / f"{self.prefix} SWEEPSPROOFS.zip"
            self.create_zip(proof_directory, zip_path)

            logging.info("All operations completed successfully")
            return True

        except Exception as e:
            logging.error(f"Process failed: {str(e)}")
            return False

def main():
    # Get user input with validation
    while True:
        user_input = input("ENTER SWEEPS NUMBER FOLLOWED BY QUARTER NUMBER XXXXX YX: ").strip()
        if len(user_input.split()) == 2:
            break
        logging.warning("Please enter both sweeps number and quarter number")

    # Process files
    processor = SweepsProcessor(user_input)
    processor.process()

if __name__ == "__main__":
    main()
