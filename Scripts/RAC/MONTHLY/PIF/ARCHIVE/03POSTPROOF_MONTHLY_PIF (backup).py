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
    user_input = input("Enter Monthly PIF Job Number followed by a space: ")
    
    directories = [
        r"C:\Program Files\Goji\RAC\MONTHLY_PIF\FOLDER\INPUT",
        r"C:\Program Files\Goji\RAC\MONTHLY_PIF\FOLDER\OUTPUT",
        r"C:\Program Files\Goji\RAC\MONTHLY_PIF\FOLDER\PROOF"
    ]
    
    for directory in directories:
        rename_files_in_directory(directory, user_input)
    
    # Zip the files in the PROOF directory
    proof_directory = r"C:\Program Files\Goji\RAC\MONTHLY_PIF\FOLDER\PROOF"
    zip_filename = os.path.join(r"C:\Program Files\Goji\RAC\MONTHLY_PIF\FOLDER", f"{user_input} MONTHLY_PIF PROOFS.zip")
    zip_proof_files(proof_directory, zip_filename)
    
    for directory in directories:
        delete_files_in_directory(directory)

    print("Operation completed successfully.")
