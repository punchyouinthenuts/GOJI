import os
import shutil
import pandas as pd
import time

# Define directories
input_dir = r"C:\Users\JCox\Desktop\AUTOMATION\BAYLOR\ACCEPTANCE\INPUT"
archive_dir = r"C:\Users\JCox\Desktop\AUTOMATION\BAYLOR\ACCEPTANCE\XLSX ARCHIVE"

# Function to delete temporary files and folders during rollback
def rollback(tmp_files, tmp_dirs):
    for file in tmp_files:
        if os.path.exists(file):
            try:
                os.remove(file)
                print(f"Deleted temporary file: {file}")
            except Exception as e:
                print(f"Failed to delete {file}: {e}")
    for dir in tmp_dirs:
        if os.path.exists(dir):
            try:
                shutil.rmtree(dir)
                print(f"Deleted temporary directory: {dir}")
            except Exception as e:
                print(f"Failed to delete {dir}: {e}")

# Function to attempt file deletion with retries
def remove_file_with_retry(file_path, retries=5, delay=1):
    for attempt in range(retries):
        try:
            os.remove(file_path)
            print(f"Deleted original file: {file_path}")
            return True
        except PermissionError as e:
            if attempt < retries - 1:
                print(f"Attempt {attempt + 1} failed: {e}. Retrying in {delay} second(s)...")
                time.sleep(delay)
            else:
                raise e
    return False

# Function to format Merit Award as whole dollar amounts without decimals
def format_merit_award(value):
    if pd.isna(value) or str(value).strip() == "":
        return ""
    try:
        # Remove non-numeric characters except for the decimal point
        cleaned = str(value).replace('$', '').replace(',', '')
        # Convert to float first, then to int to truncate decimals
        num = int(float(cleaned))
        # Format with dollar sign and commas
        return f"${num:,}"
    except (ValueError, TypeError):
        # If conversion fails, return the original value
        return str(value)

# Initialize lists to track temporary files and folders
tmp_files = []
tmp_dirs = []

try:
    # Step 1: Find all XLS or XLSX files in the input directory
    files = [f for f in os.listdir(input_dir) if f.lower().endswith(('.xls', '.xlsx'))]

    if len(files) == 0:
        raise Exception("No XLS or XLSX files found in the input directory.")
    elif len(files) == 1:
        selected_file = files[0]
        print(f"Processing file: {selected_file}")
    else:
        print("Multiple files found. Please choose one:")
        for i, f in enumerate(files, 1):
            print(f"{i}. {f}")
        while True:
            try:
                choice = int(input("Enter the number of the file to process: "))
                if 1 <= choice <= len(files):
                    selected_file = files[choice - 1]
                    print(f"Processing file: {selected_file}")
                    break
                else:
                    print("Invalid number. Please try again.")
            except ValueError:
                print("Please enter a valid number.")

    # Construct full path of the selected file
    file_path = os.path.join(input_dir, selected_file)

    # Step 2: Check the number of sheets and read the Excel file
    excel_file = pd.ExcelFile(file_path)
    if len(excel_file.sheet_names) > 1:
        excel_file.close()
        print("MULTIPLE SHEETS PRESENT! CHECK FILE BEFORE PROCESSING.")
        while True:
            user_input = input("Press X to terminate...").strip().upper()
            if user_input == 'X':
                raise Exception("User terminated script due to multiple sheets.")
            else:
                print("Invalid input. Please press X to terminate.")
    
    # Read the first (and only) sheet, then close the file
    df = pd.read_excel(excel_file, sheet_name=0)
    excel_file.close()
    print(f"Read data from {selected_file}")

    # Step 3: Define the desired columns
    desired_columns = [
        "Ref",
        "First",
        "Middle",
        "Last",
        "Merit Award",
        "Active Street 1",
        "Active Street 2",
        "Active City",
        "Active Region",
        "Active Postal"
    ]

    # Check for missing columns
    missing_columns = [col for col in desired_columns if col not in df.columns]
    if missing_columns:
        print("The following columns are missing:")
        for col in missing_columns:
            print(f"- {col}")
        while True:
            proceed = input("DO YOU WANT TO PROCEED WITH PROCESSING? Y/N: ").strip().upper()
            if proceed == 'Y':
                break
            elif proceed == 'N':
                raise Exception("User aborted processing due to missing columns.")
            else:
                print("Please enter Y or N.")

    # Filter to keep only the desired columns that exist
    existing_columns = [col for col in desired_columns if col in df.columns]
    df = df[existing_columns]

    # Step 4: Rename specified headers (excluding 'Ref' and 'Merit Award')
    rename_dict = {
        "First": "First Name",
        "Middle": "Middle Name",
        "Last": "Last Name",
        "Active Street 1": "Address Line 1",
        "Active Street 2": "Address Line 2",
        "Active City": "City",
        "Active Region": "State",
        "Active Postal": "ZIP Code"
    }
    df = df.rename(columns=rename_dict)

    # Step 5: Format 'Merit Award' column as whole dollar amounts
    if "Merit Award" in df.columns:
        df["Merit Award"] = df["Merit Award"].apply(format_merit_award)
        print("Formatted 'Merit Award' values as whole dollar amounts.")

    # Step 6: Handle archiving
    archive_path = os.path.join(archive_dir, selected_file)
    if os.path.exists(archive_path):
        while True:
            proceed = input(f"{selected_file} ALREADY EXISTS IN ARCHIVE. DO YOU WANT TO PROCEED? Y/N: ").strip().upper()
            if proceed == 'Y':
                while True:
                    overwrite = input("DO YOU WANT TO OVERWRITE ARCHIVED FILE? Y/N: ").strip().upper()
                    if overwrite == 'Y':
                        shutil.copy2(file_path, archive_path)
                        print(f"Overwrote archived file: {archive_path}")
                        break
                    elif overwrite == 'N':
                        base, ext = os.path.splitext(selected_file)
                        i = 1
                        while True:
                            new_name = f"{base}_{i:02d}{ext}"
                            new_path = os.path.join(archive_dir, new_name)
                            if not os.path.exists(new_path):
                                shutil.copy2(file_path, new_path)
                                print(f"Archived as: {new_path}")
                                break
                            i += 1
                        break
                    else:
                        print("Please enter Y or N.")
                break
            elif proceed == 'N':
                raise Exception("User aborted processing due to archive conflict.")
            else:
                print("Please enter Y or N.")
    else:
        shutil.copy2(file_path, archive_path)
        print(f"Archived file to: {archive_path}")

    # Step 7: Save the processed data as CSV with default quoting to keep double quotes
    csv_path = os.path.join(input_dir, "INPUT.csv")
    df.to_csv(csv_path, index=False)  # Default quoting adds quotes around values with commas
    tmp_files.append(csv_path)
    print(f"Saved processed data to: {csv_path}")

    # Step 8: Delete the original Excel file with retry logic
    remove_file_with_retry(file_path)

    # Step 9: Final success message
    print("\n=== Processing Summary ===")
    print(f"File processed: {selected_file}")
    print(f"Output saved: {csv_path}")
    print(f"Original file deleted: {file_path}")
    print("Processing completed successfully.")
    print("Press any key to exit...")
    input()

except Exception as e:
    # Step 10: Error handling with rollback
    print("\n=== Processing Summary ===")
    print(f"An error occurred: {e}")
    print("Rolling back changes...")
    rollback(tmp_files, tmp_dirs)
    print("Processing failed. Check the error above.")
    print("Press any key to exit...")
    input()