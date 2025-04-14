import os
import shutil
import zipfile

def rename_files_in_directory(directory, prefix):
    for filename in os.listdir(directory):
        new_filename = f"{prefix}-{filename}"
        os.rename(os.path.join(directory, filename), os.path.join(directory, new_filename))

def delete_files_in_directory(directory):
    for filename in os.listdir(directory):
        os.remove(os.path.join(directory, filename))

def zip_proof_files(directory, zip_filename):
    with zipfile.ZipFile(zip_filename, 'w', zipfile.ZIP_DEFLATED) as zipf:
        for root, _, files in os.walk(directory):
            for file in files:
                zipf.write(os.path.join(root, file), os.path.relpath(os.path.join(root, file), directory))

if __name__ == "__main__":
    user_input = input("ENTER PREPIF JOB NUMBER FOLLOWED BY WEEK NUMBER XXXXX XX.XX: ")
    
    directories = [
        r"C:\Users\JCox\Desktop\AUTOMATION\RAC\PREPIF\FOLDERS\INPUT",
        r"C:\Users\JCox\Desktop\AUTOMATION\RAC\PREPIF\FOLDERS\OUTPUT",
        r"C:\Users\JCox\Desktop\AUTOMATION\RAC\PREPIF\FOLDERS\PROOF"
    ]
    
    for directory in directories:
        rename_files_in_directory(directory, user_input)
        
    source_folder = r"C:\Users\JCox\Desktop\AUTOMATION\RAC\PREPIF\FOLDERS"
    dest_folder = os.path.join(r"C:\Users\JCox\Desktop\AUTOMATION\RAC\PREPIF", user_input.split(" ")[1])
    shutil.copytree(source_folder, dest_folder)
    
    for directory in directories:
        delete_files_in_directory(directory)
        
    # Zip the files in the new \PROOF sub-directory
    proof_directory = os.path.join(dest_folder, "PROOF")
    zip_folder = r"C:\Users\JCox\Desktop\AUTOMATION\RAC\WEEKLY\WEEKLY_ZIP"
    zip_filename = os.path.join(zip_folder, f"{user_input} PREPIF PROOFS.zip")
    zip_proof_files(proof_directory, zip_filename)
    
    print("Operation completed successfully.")
