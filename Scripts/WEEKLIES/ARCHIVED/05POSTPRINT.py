import os
import shutil
import glob
import re
from pathlib import Path

def validate_job_number(job_number):
    return len(job_number) == 5 and job_number.isdigit()

def validate_week_number(week_num):
    pattern = r'^\d{1,2}\.\d{1,2}$'
    if not re.match(pattern, week_num):
        return False
    try:
        month, week = map(int, week_num.split('.'))
        return 1 <= month <= 12 and 1 <= week <= 53
    except ValueError:
        return False

def get_user_inputs():
    job_numbers = {}
    # Collect all job numbers
    for job_type in ['NCWO', 'INACTIVE', 'PREPIF', 'CBC', 'EXC']:
        while True:
            job_num = input(f"ENTER {job_type} JOB NUMBER: ")
            if validate_job_number(job_num):
                job_numbers[job_type] = job_num
                break
            print("Job number must be 5 digits. Try again.")

    # Collect week number
    while True:
        week_num = input("\nENTER WEEK NUMBER (format: xx.xx): ")
        if validate_week_number(week_num):
            break
        print("Week number must be in format xx.xx with valid month/week. Try again.")
    
    return job_numbers, week_num

def move_zip_files(week_num):
    source_dir = r"C:\Users\JCox\Desktop\AUTOMATION\RAC\WEEKLY\WEEKLY_ZIP"
    dest_dir = os.path.join(source_dir, "OLD")
    
    if not os.path.exists(dest_dir):
        os.makedirs(dest_dir)
    
    matching_files = [f for f in os.listdir(source_dir) 
                     if f.endswith('.zip') and week_num in f]
    
    if not matching_files:
        response = input("NO PROOF ZIPS FOR THAT WEEK ARE FOUND, DO YOU WANT TO CONTINUE? Y/N: ").upper()
        if response != 'Y':
            return False
        print("PLEASE REMEMBER TO CHECK FOR PROOF ZIPS")
    else:
        for file in matching_files:
            shutil.move(os.path.join(source_dir, file), os.path.join(dest_dir, file))
    return True

def process_pdf_files(job_number, week_number, job_type):
    # Define paths based on job type
    paths = {
        'CBC': (r"C:\Users\JCox\Desktop\AUTOMATION\RAC\CBC\JOB\PRINT",
                r"C:\Users\JCox\Desktop\AUTOMATION\RAC\CBC"),
        'EXC': (r"C:\Users\JCox\Desktop\AUTOMATION\RAC\EXC\JOB\PRINT",
                r"C:\Users\JCox\Desktop\AUTOMATION\RAC\EXC"),
        'NCWO': (r"C:\Users\JCox\Desktop\AUTOMATION\RAC\NCWO_4TH\DM03\PRINT",
                 r"C:\Users\JCox\Desktop\AUTOMATION\RAC\NCWO_4TH"),
        'PREPIF': (r"C:\Users\JCox\Desktop\AUTOMATION\RAC\PREPIF\FOLDERS\PRINT",
                   r"C:\Users\JCox\Desktop\AUTOMATION\RAC\PREPIF")
    }

    if job_type not in paths:
        return

    pdf_source, base_folder = paths[job_type]
    network_parent_path = f"\\\\NAS1069D9\\AMPrintData\\2025_SrcFiles\\I\\Innerworkings\\{job_number} {job_type}"
    
    # Create network folders
    os.makedirs(network_parent_path, exist_ok=True)
    week_subfolder = os.path.join(network_parent_path, week_number)
    os.makedirs(week_subfolder, exist_ok=True)

    # Process PDF files
    pdf_files = glob.glob(os.path.join(pdf_source, "*.pdf"))
    for pdf_file in pdf_files:
        old_name = os.path.basename(pdf_file)
        new_name = f"{job_number} {week_number} {old_name}"
        new_path = os.path.join(pdf_source, new_name)
        os.rename(pdf_file, new_path)
        
        week_folder = os.path.join(base_folder, week_number)
        print_folder = os.path.join(week_folder, "PRINT")
        
        # Copy and move files
        shutil.copy2(new_path, print_folder)
        shutil.move(new_path, week_subfolder)

def process_inactive_files(week_number):
    base_path = r"C:\Users\JCox\Desktop\AUTOMATION\RAC\INACTIVE_2310-DM07"
    target_path = r"W:\\"
    week_folder = os.path.join(base_path, week_number)
    output_folder = os.path.join(week_folder, "OUTPUT")
    
    if not os.path.exists(week_folder):
        print("INACTIVE folder does not exist!")
        return
    
    if not os.path.exists(output_folder):
        print("OUTPUT folder not found!")
        return
    
    csv_files = [f for f in os.listdir(output_folder) if f.endswith('.csv')]
    for file in csv_files:
        shutil.copy2(os.path.join(output_folder, file), os.path.join(target_path, file))

def main():
    print("Starting Weekly File Processing...")
    
    # Get all inputs at the start
    job_numbers, week_number = get_user_inputs()
    
    # Process ZIP files first
    if not move_zip_files(week_number):
        return
    
    # Process each job type
    for job_type in ['CBC', 'EXC', 'NCWO', 'PREPIF']:
        print(f"\nProcessing {job_type} files...")
        process_pdf_files(job_numbers[job_type], week_number, job_type)
    
    # Process Inactive files
    print("\nProcessing Inactive files...")
    process_inactive_files(week_number)
    
    print("\nAll processing completed successfully!")

if __name__ == "__main__":
    main()
