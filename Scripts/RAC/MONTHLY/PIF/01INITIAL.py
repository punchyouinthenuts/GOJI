import os
import zipfile
import shutil
import glob

# Define paths
zip_folder = r'C:\Program Files\Goji\RAC\MONTHLY_PIF\INPUTZIP'
extract_folder = r'C:\Program Files\Goji\RAC\MONTHLY_PIF\FOLDER\INPUT'

# Find and extract ZIP file
zip_files = glob.glob(os.path.join(zip_folder, '*.zip'))
if zip_files:
    with zipfile.ZipFile(zip_files[0], 'r') as zip_ref:
        zip_ref.extractall(extract_folder)

# Find "File Box" folder
file_box_folders = []
for root, dirs, files in os.walk(extract_folder):
    for dir in dirs:
        if "File Box" in dir:
            file_box_folders.append(os.path.join(root, dir))

# Move TXT files from File Box folder
if file_box_folders:
    file_box_path = file_box_folders[0]
    for file in os.listdir(file_box_path):
        if file.lower().endswith('.txt'):
            source = os.path.join(file_box_path, file)
            destination = os.path.join(extract_folder, file)
            shutil.move(source, destination)

# Delete non-TXT files
for root, dirs, files in os.walk(extract_folder, topdown=False):
    # Remove directories first
    for dir in dirs:
        shutil.rmtree(os.path.join(root, dir))
    # Remove non-TXT files
    for file in files:
        if not file.lower().endswith('.txt'):
            os.remove(os.path.join(root, file))

# Rename files
for file in os.listdir(extract_folder):
    if file.lower().endswith('.txt'):
        file_path = os.path.join(extract_folder, file)
        if 'GIN' in file:
            new_path = os.path.join(extract_folder, 'GIN.txt')
            os.rename(file_path, new_path)
        elif 'HOM' in file:
            new_path = os.path.join(extract_folder, 'HOM.txt')
            os.rename(file_path, new_path)

# Delete the ZIP file
if zip_files:
    os.remove(zip_files[0])
