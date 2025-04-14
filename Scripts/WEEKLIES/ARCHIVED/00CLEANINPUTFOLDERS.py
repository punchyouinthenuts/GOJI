import os
import shutil

# List of folders to clean
folders_to_clean = [
    r"C:\Users\JCox\Desktop\AUTOMATION\RAC\CBC\JOB\INPUT",
    r"C:\Users\JCox\Desktop\AUTOMATION\RAC\EXC\JOB\INPUT",
    r"C:\Users\JCox\Desktop\AUTOMATION\RAC\INACTIVE_2310-DM07\FOLDERS\INPUT",
    r"C:\Users\JCox\Desktop\AUTOMATION\RAC\NCWO_4TH\DM03\INPUT",
    r"C:\Users\JCox\Desktop\AUTOMATION\RAC\PREPIF\FOLDERS\INPUT"
]

def clean_folder(folder_path):
    try:
        # Check if folder exists
        if os.path.exists(folder_path):
            # Get list of all files in the folder
            files = os.listdir(folder_path)
            
            # Delete each file
            for file in files:
                file_path = os.path.join(folder_path, file)
                if os.path.isfile(file_path):
                    os.remove(file_path)
                    print(f"Deleted: {file_path}")
            
            print(f"Cleaned folder: {folder_path}")
        else:
            print(f"Folder not found: {folder_path}")
            
    except Exception as e:
        print(f"Error cleaning {folder_path}: {str(e)}")

def main():
    print("Starting cleanup process...")
    
    for folder in folders_to_clean:
        clean_folder(folder)
    
    print("Cleanup process completed!")

if __name__ == "__main__":
    main()
