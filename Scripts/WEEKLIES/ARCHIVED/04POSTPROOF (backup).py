import os
import shutil
import zipfile
import re

def validate_job_number(job_number):
    return len(job_number) == 5 and job_number.isdigit()

def validate_week_number(week_num):
    pattern = r'^\d{1,2}\.\d{1,2}$'
    return bool(re.match(pattern, week_num))

def get_user_inputs():
    job_numbers = {}
    
    # Collect all job numbers
    for job_type in ['NCWO', 'CBC', 'EXC', 'INACTIVE', 'PREPIF']:
        while True:
            job_num = input(f"Enter {job_type} job number (XXXXX): ").strip()
            if validate_job_number(job_num):
                job_numbers[job_type] = job_num
                break
            print("Job number must be 5 digits.")
    
    # Collect week number once
    while True:
        week_num = input("\nEnter week number (XX.XX): ").strip()
        if validate_week_number(week_num):
            break
        print("Week number must be in format XX.XX")
    
    return job_numbers, week_num

def rename_files_in_directory(directory, prefix):
    if not os.path.exists(directory) or not os.listdir(directory):
        print(f"Directory {directory} is empty or doesn't exist")
        return
    
    for filename in os.listdir(directory):
        new_filename = f"{prefix}-{filename}"
        os.rename(os.path.join(directory, filename), os.path.join(directory, new_filename))

def delete_files_in_directory(directory):
    if not os.path.exists(directory):
        return
    
    for filename in os.listdir(directory):
        file_path = os.path.join(directory, filename)
        if os.path.isfile(file_path):
            os.remove(file_path)

def zip_proof_files(directory, zip_filename):
    os.makedirs(os.path.dirname(zip_filename), exist_ok=True)
    with zipfile.ZipFile(zip_filename, 'w', zipfile.ZIP_DEFLATED) as zipf:
        for root, _, files in os.walk(directory):
            for file in files:
                file_path = os.path.join(root, file)
                arcname = os.path.relpath(file_path, directory)
                zipf.write(file_path, arcname)

def process_job_type(job_type, job_number, week_number):
    # Combine job number and week number in required format
    combined_input = f"{job_number} {week_number}"
    
    # Define paths based on job type
    paths = {
        'CBC': {
            'base': r"C:\Users\JCox\Desktop\AUTOMATION\RAC\CBC",
            'source': r"C:\Users\JCox\Desktop\AUTOMATION\RAC\CBC\JOB",
            'suffix': "CBCPROOFS"
        },
        'EXC': {
            'base': r"C:\Users\JCox\Desktop\AUTOMATION\RAC\EXC",
            'source': r"C:\Users\JCox\Desktop\AUTOMATION\RAC\EXC\JOB",
            'suffix': "EXCPROOFS"
        },
        'INACTIVE': {
            'base': r"C:\Users\JCox\Desktop\AUTOMATION\RAC\INACTIVE_2310-DM07",
            'source': r"C:\Users\JCox\Desktop\AUTOMATION\RAC\INACTIVE_2310-DM07\FOLDERS",
            'suffix': "INACTIVE PROOFS"
        },
        'NCWO': {
            'base': r"C:\Users\JCox\Desktop\AUTOMATION\RAC\NCWO_4TH",
            'source': r"C:\Users\JCox\Desktop\AUTOMATION\RAC\NCWO_4TH\DM03",
            'suffix': "NCWO PROOFS"
        },
        'PREPIF': {
            'base': r"C:\Users\JCox\Desktop\AUTOMATION\RAC\PREPIF",
            'source': r"C:\Users\JCox\Desktop\AUTOMATION\RAC\PREPIF\FOLDERS",
            'suffix': "PREPIF PROOFS"
        }
    }
    
    current_paths = paths[job_type]
    
    # Define directories to process
    directories = [
        os.path.join(current_paths['source'], 'INPUT'),
        os.path.join(current_paths['source'], 'OUTPUT'),
        os.path.join(current_paths['source'], 'PROOF')
    ]
    
    # Process each directory
    for directory in directories:
        rename_files_in_directory(directory, combined_input)
    
    # Create destination folder and copy files
    dest_folder = os.path.join(current_paths['base'], week_number)
    if os.path.exists(dest_folder):
        backup_folder = f"{dest_folder}_backup"
        shutil.move(dest_folder, backup_folder)
    
    shutil.copytree(current_paths['source'], dest_folder)
    
    # Clean up original directories
    for directory in directories:
        delete_files_in_directory(directory)
    
    # Create zip file
    proof_directory = os.path.join(dest_folder, "PROOF")
    zip_folder = r"C:\Users\JCox\Desktop\AUTOMATION\RAC\WEEKLY\WEEKLY_ZIP"
    zip_filename = os.path.join(zip_folder, f"{combined_input} {current_paths['suffix']}.zip")
    zip_proof_files(proof_directory, zip_filename)
    
    print(f"{job_type} processing completed successfully!")

def main():
    try:
        print("Starting Post-Proof Processing...")
        job_numbers, week_number = get_user_inputs()
        
        # Process each job type
        for job_type in ['CBC', 'EXC', 'INACTIVE', 'NCWO', 'PREPIF']:
            print(f"\nProcessing {job_type}...")
            process_job_type(job_type, job_numbers[job_type], week_number)
        
        print("\nAll operations completed successfully!")
        
    except Exception as e:
        print(f"An error occurred: {str(e)}")

if __name__ == "__main__":
    main()
