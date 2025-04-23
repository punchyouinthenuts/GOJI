import os
import shutil
from datetime import datetime

def clear_directory_contents(directory):
    for item in os.listdir(directory):
        item_path = os.path.join(directory, item)
        if os.path.isfile(item_path):
            os.remove(item_path)
        elif os.path.isdir(item_path):
            shutil.rmtree(item_path)
            os.makedirs(item_path)

def main():
    # Get user inputs
    job_number = input("ENTER TRACHMAR JOB NUMBER: ")
    while len(job_number) != 5 or not job_number.isdigit():
        job_number = input("Please enter a valid 5-digit job number: ")

    week_number = input("ENTER WEEK NUMBER: ")
    while not week_number.count('.') == 1 or not all(part.isdigit() for part in week_number.split('.')):
        week_number = input("Please enter a valid week number (format: xx.xx): ")

    # Get current year
    current_year = str(datetime.now().year)
    
    # Define paths
    nas_base_path = f"\\\\NAS1069D9\\AMPrintData\\{current_year}_SrcFiles\\T\\Trachmar"
    job_folder_path = os.path.join(nas_base_path, job_number)
    week_folder_path = os.path.join(job_folder_path, week_number)
    
    # Create directories if they don't exist
    os.makedirs(week_folder_path, exist_ok=True)
    
    # Source paths
    source_print_path = r"C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\WEEKLY\JOB\PRINT"
    source_job_path = r"C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\WEEKLY\JOB"
    
    # Copy PDF file
    for file in os.listdir(source_print_path):
        if file.endswith('.pdf'):
            shutil.copy2(
                os.path.join(source_print_path, file),
                week_folder_path
            )
    
    # Weekly folder path with hyphen
    weekly_folder = week_number.replace('.', '-')
    weekly_destination = os.path.join(r"C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\WEEKLY", weekly_folder)
    
    # Copy job contents to weekly folder
    if os.path.exists(source_job_path):
        for item in os.listdir(source_job_path):
            source_item = os.path.join(source_job_path, item)
            dest_item = os.path.join(weekly_destination, item)
            
            if os.path.isfile(source_item):
                shutil.copy2(source_item, dest_item)
            elif os.path.isdir(source_item):
                shutil.copytree(source_item, dest_item, dirs_exist_ok=True)
    
    # Clear contents of job folders while preserving the folders
    job_path = r"C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\WEEKLY\JOB"
    for subfolder in os.listdir(job_path):
        subfolder_path = os.path.join(job_path, subfolder)
        if os.path.isdir(subfolder_path):
            clear_directory_contents(subfolder_path)
    
    # Final message and wait for input
    input("POST PRINT PROCESS COMPLETE! Press any key to terminate...")

if __name__ == "__main__":
    main()
