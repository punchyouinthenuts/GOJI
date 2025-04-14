import os
from pathlib import Path
import shutil
import zipfile
import re

# Constants
JOB_TYPES = ['NCWO', 'CBC', 'EXC', 'INACTIVE', 'PREPIF']
JOB_NUMBER_LENGTH = 5
USER_HOME = Path.home()
BASE_PATH = USER_HOME / "Desktop" / "AUTOMATION" / "RAC"

def validate_job_number(job_number):
    return len(job_number) == JOB_NUMBER_LENGTH and job_number.isdigit()

def validate_week_number(week_num):
    pattern = r'^\d{1,2}\.\d{1,2}$'
    if not bool(re.match(pattern, week_num)):
        return False
    week, subweek = map(int, week_num.split('.'))
    return 1 <= week <= 52 and 1 <= subweek <= 99

def get_user_inputs():
    job_numbers = {}
    
    for job_type in JOB_TYPES:
        while True:
            job_num = input(f"Enter {job_type} job number (XXXXX): ").strip()
            if validate_job_number(job_num):
                job_numbers[job_type] = job_num
                break
            print("Job number must be 5 digits.")
    
    while True:
        week_num = input("\nEnter week number (XX.XX): ").strip()
        if validate_week_number(week_num):
            break
        print("Week number must be in format XX.XX (valid week 1-52, subweek 1-99)")
    
    return job_numbers, week_num

def rename_files_in_directory(directory, prefix):
    try:
        directory_path = Path(directory)
        if not directory_path.exists() or not any(directory_path.iterdir()):
            print(f"Directory {directory} is empty or doesn't exist")
            return
        
        for file_path in directory_path.iterdir():
            if file_path.is_file():
                new_name = f"{prefix}-{file_path.name}"
                file_path.rename(directory_path / new_name)
    except PermissionError:
        print(f"Permission denied when accessing {directory}")
    except OSError as e:
        print(f"Error renaming files in {directory}: {e}")

def delete_files_in_directory(directory):
    try:
        directory_path = Path(directory)
        if not directory_path.exists():
            return
        
        for file_path in directory_path.iterdir():
            if file_path.is_file():
                file_path.unlink()
    except PermissionError:
        print(f"Permission denied when deleting files in {directory}")
    except OSError as e:
        print(f"Error deleting files in {directory}: {e}")

def zip_proof_files(directory, zip_filename):
    try:
        Path(zip_filename).parent.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(zip_filename, 'w', zipfile.ZIP_DEFLATED) as zipf:
            directory_path = Path(directory)
            for file_path in directory_path.rglob('*'):
                if file_path.is_file():
                    arcname = file_path.relative_to(directory_path)
                    zipf.write(file_path, arcname)
    except Exception as e:
        print(f"Error creating zip file {zip_filename}: {e}")

def process_job_type(job_type, job_number, week_number):
    combined_input = f"{job_number} {week_number}"
    
    paths = {
        'CBC': {
            'base': BASE_PATH / "CBC",
            'source': BASE_PATH / "CBC" / "JOB",
            'suffix': "CBCPROOFS"
        },
        'EXC': {
            'base': BASE_PATH / "EXC",
            'source': BASE_PATH / "EXC" / "JOB",
            'suffix': "EXCPROOFS"
        },
        'INACTIVE': {
            'base': BASE_PATH / "INACTIVE_2310-DM07",
            'source': BASE_PATH / "INACTIVE_2310-DM07" / "FOLDERS",
            'suffix': "INACTIVE PROOFS"
        },
        'NCWO': {
            'base': BASE_PATH / "NCWO_4TH",
            'source': BASE_PATH / "NCWO_4TH" / "DM03",
            'suffix': "NCWO PROOFS"
        },
        'PREPIF': {
            'base': BASE_PATH / "PREPIF",
            'source': BASE_PATH / "PREPIF" / "FOLDERS",
            'suffix': "PREPIF PROOFS"
        }
    }
    
    current_paths = paths[job_type]
    
    directories = [
        current_paths['source'] / 'INPUT',
        current_paths['source'] / 'OUTPUT',
        current_paths['source'] / 'PROOF'
    ]
    
    for directory in directories:
        rename_files_in_directory(directory, combined_input)
    
    dest_folder = current_paths['base'] / week_number
    if dest_folder.exists():
        backup_folder = Path(f"{dest_folder}_backup")
        shutil.move(dest_folder, backup_folder)
    
    shutil.copytree(current_paths['source'], dest_folder)
    
    for directory in directories:
        delete_files_in_directory(directory)
    
    proof_directory = dest_folder / "PROOF"
    zip_folder = BASE_PATH / "WEEKLY" / "WEEKLY_ZIP"
    zip_filename = zip_folder / f"{combined_input} {current_paths['suffix']}.zip"
    zip_proof_files(proof_directory, zip_filename)
    
    print(f"{job_type} processing completed successfully!")

def main():
    try:
        print("Starting Post-Proof Processing...")
        job_numbers, week_number = get_user_inputs()
        
        for job_type in JOB_TYPES:
            print(f"\nProcessing {job_type}...")
            process_job_type(job_type, job_numbers[job_type], week_number)
        
        print("\nAll operations completed successfully!")
        
    except KeyboardInterrupt:
        print("\nOperation cancelled by user")
    except Exception as e:
        print(f"An error occurred: {str(e)}")
    finally:
        print("\nScript execution completed")

if __name__ == "__main__":
    main()
