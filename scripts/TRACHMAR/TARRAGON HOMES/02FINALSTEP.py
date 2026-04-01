import os
import shutil
import datetime
import zipfile
import sys

def main():
    # Get command line arguments from Goji
    if len(sys.argv) != 5:
        print("Error: Expected 4 arguments (job_number, drop_number, year, month)")
        return 1
    
    job_number = sys.argv[1]
    drop_number = sys.argv[2]
    year = sys.argv[3]
    month = sys.argv[4]
    
    print(f"Starting TM TARRAGON final step processing...")
    print(f"Job: {job_number}, Drop: {drop_number}, Year: {year}, Month: {month}")
    
    # Define Goji paths
    source_csv = r"C:\Goji\TRACHMAR\TARRAGON HOMES\OUTPUT\TARRAGON HOMES DROP.csv"
    output_dir = r"C:\Goji\TRACHMAR\TARRAGON HOMES\OUTPUT"
    input_dir = r"C:\Goji\TRACHMAR\TARRAGON HOMES\INPUT"
    archive_dir = r"C:\Goji\TRACHMAR\TARRAGON HOMES\ARCHIVE"
    local_save_dir = r"C:\Users\JCox\Desktop\MOVE TO NETWORK DRIVE"
    base_network_dir = r"\\NAS1069D9\AMPrintData"

    # Function to validate job number (5 digits) and drop number (single digit)
    def validate_inputs(job_number, drop_number):
        if not (job_number.isdigit() and len(job_number) == 5):
            raise ValueError("Job number must be exactly 5 digits.")
        if not (drop_number.isdigit() and len(drop_number) == 1):
            raise ValueError("Drop number must be a single digit.")
        return job_number, drop_number

    # Function to get timestamp
    def get_timestamp():
        return datetime.datetime.now().strftime("%Y%m%d-%H%M")

    # Function to find or create job folder
    def get_job_folder(base_dir, job_number):
        job_folder = os.path.join(base_dir, f"{job_number}_TARRAGON")
        os.makedirs(job_folder, exist_ok=True)
        return job_folder

    # Function to ensure HP Indigo\DATA subdirectory exists
    def ensure_data_subdir(job_folder):
        data_dir = os.path.join(job_folder, "HP Indigo", "DATA")
        os.makedirs(data_dir, exist_ok=True)
        return data_dir

    # Function to empty a directory without deleting it
    def empty_directory(directory):
        if not os.path.exists(directory):
            return
        for item in os.listdir(directory):
            item_path = os.path.join(directory, item)
            if os.path.isfile(item_path):
                os.remove(item_path)
            elif os.path.isdir(item_path):
                shutil.rmtree(item_path)

    # Function to get unique ZIP filename with versioning
    def get_unique_zip_name(archive_dir, base_zip_name):
        zip_path = os.path.join(archive_dir, base_zip_name)
        if not os.path.exists(zip_path):
            return zip_path
        
        base, ext = os.path.splitext(base_zip_name)
        version = 2
        while True:
            new_name = f"{base} v{version}{ext}"
            new_path = os.path.join(archive_dir, new_name)
            if not os.path.exists(new_path):
                return new_path
            version += 1

    try:
        # Validate inputs
        job_number, drop_number = validate_inputs(job_number, drop_number)
        
        # Check if source CSV exists
        if not os.path.exists(source_csv):
            raise FileNotFoundError(f"Source CSV not found: {source_csv}")
        
        print("Found TARRAGON HOMES DROP.csv file")

        # Rename the CSV
        base_csv_name = f"{job_number} TARRAGON HOMES DROP {drop_number}.csv"
        new_csv_path = os.path.join(output_dir, base_csv_name)
        shutil.copy2(source_csv, new_csv_path)  # Copy to preserve original until later
        print(f"Created renamed copy: {base_csv_name}")

        # Get timestamp for filename
        timestamp = get_timestamp()
        timestamped_csv_name = f"{job_number} TARRAGON HOMES DROP {drop_number}_{timestamp}.csv"

        # Construct base network directory using the current year
        network_dir = os.path.join(base_network_dir, f"{year}_SrcFiles", "T", "Trachmar")
        
        # Flag to track if network save was successful
        network_save_successful = False
        
        # Try to access network directory
        try:
            if not os.path.exists(network_dir):
                raise FileNotFoundError("Network directory inaccessible")
            
            # Find or create job folder
            job_folder = get_job_folder(network_dir, job_number)
            print(f"Created/found job folder: {job_folder}")
            
            # Ensure HP Indigo\DATA exists
            data_dir = ensure_data_subdir(job_folder)
            print(f"Created/verified data directory: {data_dir}")
            
            # Copy renamed CSV with timestamp to HP Indigo\DATA
            destination_csv = os.path.join(data_dir, timestamped_csv_name)
            shutil.copy2(new_csv_path, destination_csv)
            print(f"File copied to network location: {destination_csv}")
            
            # Output NAS path markers for Goji popup dialog
            print("=== NAS_FOLDER_PATH ===")
            print(data_dir)
            print("=== END_NAS_FOLDER_PATH ===")
            
            network_save_successful = True
        
        except (FileNotFoundError, OSError, PermissionError) as e:
            print(f"Network access failed: {str(e)}")
            # Network unavailable, save to local directory
            os.makedirs(local_save_dir, exist_ok=True)
            destination_csv = os.path.join(local_save_dir, timestamped_csv_name)
            shutil.copy2(new_csv_path, destination_csv)
            print(f"Network drive unavailable - file saved locally: {destination_csv}")

        # Create ZIP file with versioning
        base_zip_name = f"{job_number} TARRAGON HOMES DROP {drop_number}.zip"
        zip_path = get_unique_zip_name(archive_dir, base_zip_name)
        os.makedirs(archive_dir, exist_ok=True)

        print("Creating archive...")
        with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED) as zipf:
            # Add contents of INPUT directory
            if os.path.exists(input_dir):
                for root, _, files in os.walk(input_dir):
                    for file in files:
                        file_path = os.path.join(root, file)
                        arcname = os.path.relpath(file_path, os.path.dirname(input_dir))
                        zipf.write(file_path, arcname)

            # Add contents of OUTPUT directory
            if os.path.exists(output_dir):
                for root, _, files in os.walk(output_dir):
                    for file in files:
                        file_path = os.path.join(root, file)
                        arcname = os.path.relpath(file_path, os.path.dirname(output_dir))
                        zipf.write(file_path, arcname)

        print(f"Archive created: {zip_path}")

        # Empty INPUT and OUTPUT directories
        empty_directory(input_dir)
        empty_directory(output_dir)
        print("Cleaned up INPUT and OUTPUT directories")

        # Final status message
        if network_save_successful:
            print("FINAL STEP COMPLETED SUCCESSFULLY - File saved to network location")
        else:
            print("FINAL STEP COMPLETED - File saved to local directory due to network unavailability")
            print(f"MANUAL ACTION REQUIRED: Move file from {local_save_dir} to network when available")

    except Exception as e:
        print(f"Error: {str(e)}")
        return 1
    
    return 0

if __name__ == "__main__":
    exit_code = main()
    sys.exit(exit_code)