import os
import shutil
import zipfile

# Global tracking for rollback
changes_made = []

def log_change(operation, original_path, new_path):
    changes_made.append({
        'operation': operation,
        'original': original_path,
        'new': new_path
    })

def rollback_changes():
    print("\nExecuting rollback of changes...")
    for change in reversed(changes_made):
        try:
            if change['operation'] == 'rename':
                if os.path.exists(change['new']):
                    os.rename(change['new'], change['original'])
        except Exception as e:
            print(f"Error during rollback: {e}")
    print("Rollback completed")

def rename_proof_files(directory, job_number, month):
    for filename in os.listdir(directory):
        if filename.lower().endswith(('.pdf', '.csv')):
            original_path = os.path.join(directory, filename)
            new_filename = f"{job_number} {month} {filename}"
            new_path = os.path.join(directory, new_filename)
            os.rename(original_path, new_path)
            log_change('rename', original_path, new_path)

def zip_proof_files(directory, zip_filename):
    with zipfile.ZipFile(zip_filename, 'w', zipfile.ZIP_DEFLATED) as zipf:
        for root, _, files in os.walk(directory):
            for file in files:
                file_path = os.path.join(root, file)
                zipf.write(file_path, os.path.relpath(file_path, directory))

if __name__ == "__main__":
    try:
        # Get user input with validation
        while True:
            user_input = input("ENTER LOYALTY JOB NUMBER FOLLOWED BY MONTH: ")
            if len(user_input) >= 9 and ' ' in user_input:
                job_number, month = user_input.split(' ', 1)
                if len(job_number) == 5 and len(month) == 3:
                    month = month.upper()
                    break
            print("Please enter 5-digit job number, space, and 3-letter month")

        # Define directories
        proof_directory = r"C:\Program Files\Goji\RAC\LOYALTY\JOB\PROOF"
        zip_path = r"C:\Program Files\Goji\RAC\LOYALTY\JOB"
        
        # Rename files in PROOF directory
        rename_proof_files(proof_directory, job_number, month)
        
        # Create ZIP of PROOF files
        zip_filename = os.path.join(zip_path, f"{job_number} {month} LOYALTY PROOFS.zip")
        zip_proof_files(proof_directory, zip_filename)
        
        print("Operation completed successfully.")
        
    except Exception as e:
        print(f"\nAn error occurred: {e}")
        if changes_made:
            rollback_changes()
        input("Press Enter to exit...")
    else:
        input("\nPress Enter to exit...")
