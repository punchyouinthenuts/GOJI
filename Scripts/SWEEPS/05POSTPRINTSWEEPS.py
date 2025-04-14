import os
import shutil
import glob
from pathlib import Path

def validate_job_number(job_number: str) -> bool:
    return len(job_number) == 5 and job_number.isdigit()

def validate_quarter_number(quarter: str) -> bool:
    return quarter in ['1', '2', '3', '4']

def main():
    print("Starting SWEEPS File Handler...")

    # Get SWEEPS Job Number
    while True:
        sweeps_job_number = input("ENTER SWEEPS JOB NUMBER (5 digits): ")
        if validate_job_number(sweeps_job_number):
            break
        print("Job number must be 5 digits. Try again.")

    # Get Quarter Number
    while True:
        sweeps_quarter_number = input("ENTER QUARTER NUMBER (1-4): ")
        if validate_quarter_number(sweeps_quarter_number):
            break
        print("Quarter number must be 1, 2, 3, or 4. Try again.")

    # Format quarter number with Q prefix
    quarter_formatted = f"Q{sweeps_quarter_number}"

    # Create parent folder name
    parent_folder_name = f"{sweeps_job_number} {quarter_formatted} SWEEPS"

    # Network parent folder path and creation
    print(f"\nChecking for network parent folder...")
    network_parent_path = Path(f"\\\\NAS1069D9\\AMPrintData\\2025_SrcFiles\\I\\Innerworkings\\{parent_folder_name}")
    
    network_parent_path.mkdir(parents=True, exist_ok=True)
    print(f"Network parent folder ready: {network_parent_path}")

    # Source PDF files location
    print("\nSearching for PDF files...")
    pdf_source = Path(r"C:\Program Files\Goji\RAC\SWEEPS\JOB\PRINT")
    pdf_files = list(pdf_source.glob("*.pdf"))
    print(f"Found {len(pdf_files)} PDF files")

    # Rename PDF files with the correct format
    print("\nRenaming PDF files...")
    renamed_files = []
    for pdf_file in pdf_files:
        old_name = pdf_file.name
        new_name = f"{sweeps_job_number} {quarter_formatted} {old_name}"
        new_path = pdf_file.parent / new_name
        pdf_file.rename(new_path)
        renamed_files.append(new_path)
        print(f"Renamed: {old_name} -> {new_name}")

    # Copy and move files
    print("\nProcessing files...")
    for pdf_file in renamed_files:
        file_name = pdf_file.name
        
        # Move to network folder
        print(f"Moving {file_name} to network folder...")
        shutil.move(str(pdf_file), str(network_parent_path / file_name))

    print("\nFile processing complete!")

if __name__ == "__main__":
    main()
