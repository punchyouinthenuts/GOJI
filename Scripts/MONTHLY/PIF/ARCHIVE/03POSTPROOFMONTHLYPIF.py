import os
import shutil
import zipfile

def move_and_rename_pdfs(source_dir, target_dir, job_number, month):
    for filename in os.listdir(source_dir):
        if filename.lower().endswith('.pdf'):
            new_filename = f"{job_number} {month} PIF {filename}"
            source_path = os.path.join(source_dir, filename)
            target_path = os.path.join(target_dir, new_filename)
            shutil.move(source_path, target_path)

def rename_proof_files(directory, job_number, month):
    for filename in os.listdir(directory):
        new_filename = f"{job_number} {month} PIF {filename}"
        os.rename(os.path.join(directory, filename), os.path.join(directory, new_filename))

def zip_proof_files(directory, zip_filename):
    with zipfile.ZipFile(zip_filename, 'w', zipfile.ZIP_DEFLATED) as zipf:
        for root, _, files in os.walk(directory):
            for file in files:
                zipf.write(os.path.join(root, file), os.path.relpath(os.path.join(root, file), directory))

if __name__ == "__main__":
    user_input = input("ENTER PIF JOB NUMBER FOLLOWED BY MONTH: ")
    job_number = user_input[:5]  # Gets first 5 digits
    month = user_input[-3:]      # Gets last 3 characters
    
    # Handle PDFs from OUTPUT
    output_dir = r"C:\Program Files\Goji\RAC\MONTHLY_PIF\FOLDER\OUTPUT"
    ppwk_temp_dir = r"C:\Users\JCox\Desktop\PPWK Temp"
    move_and_rename_pdfs(output_dir, ppwk_temp_dir, job_number, month)
    
    # Handle PROOF folder files
    proof_directory = r"C:\Program Files\Goji\RAC\MONTHLY_PIF\FOLDER\PROOF"
    rename_proof_files(proof_directory, job_number, month)
    
    # Create ZIP of PROOF files
    zip_filename = os.path.join(r"C:\Program Files\Goji\RAC\MONTHLY_PIF\FOLDER", f"{job_number} {month} PIF MONTHLY_PIF PROOFS.zip")
    zip_proof_files(proof_directory, zip_filename)

    print("Operation completed successfully.")
