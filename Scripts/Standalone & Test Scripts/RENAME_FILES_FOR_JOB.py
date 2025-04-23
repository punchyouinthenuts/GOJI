
import os

def rename_files(directory, prefix):
    # List all files in the given directory
    files = os.listdir(directory)
    
    # Rename each file by adding the prefix
    for file in files:
        new_name = f"{prefix} {file}"
        old_path = os.path.join(directory, file)
        new_path = os.path.join(directory, new_name)
        os.rename(old_path, new_path)
        print(f"Renamed {file} to {new_name}")

def main():
    # Prompt user for the directory name
    directory_name = input("Please enter the directory name: ")
    # Prompt user for the job number and version
    job_number = input("Please enter the job number: ")
    version = input("Please enter the version: ")
    
    # Construct the prefix using job number and version
    prefix = f"{job_number} {version}"
    
    # Construct the full directory paths for OUTPUT and PROOF folders
    output_directory = f"C:\\Users\\JCox\\Desktop\\AUTOMATION\\RAC\\NCWO\\{directory_name}\\OUTPUT"
    proof_directory = f"C:\\Users\\JCox\\Desktop\\AUTOMATION\\RAC\\NCWO\\{directory_name}\\PROOF"
    
    # Rename files in OUTPUT directory
    rename_files(output_directory, prefix)
    # Rename files in PROOF directory
    rename_files(proof_directory, prefix)

if __name__ == "__main__":
    main()
