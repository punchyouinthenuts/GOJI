import os
import glob
import re
import shutil
import sys
import pandas as pd
from datetime import datetime

def print_status(message):
    """Print status message to stdout and flush"""
    print(message)
    sys.stdout.flush()

def print_error(message):
    """Print error message to stderr and flush"""
    print(f"ERROR: {message}", file=sys.stderr)
    sys.stderr.flush()

def print_warning(message):
    """Print warning message to stderr and flush"""
    print(f"WARNING: {message}", file=sys.stderr)
    sys.stderr.flush()

CANONICAL_TM_WEEKLY_BASE = r"C:\Goji\AUTOMATION\TRACHMAR\WEEKLY PC"
LEGACY_TM_WEEKLY_BASE = r"C:\Goji\TRACHMAR\WEEKLY PC"

def resolve_tm_weekly_base_path():
    """Resolve WEEKLY PC runtime path with canonical-first + legacy fallback behavior."""
    configured_tm_base = os.environ.get("GOJI_TM_BASE_PATH", "").strip()
    if configured_tm_base:
        configured_weekly_path = (
            configured_tm_base
            if configured_tm_base.replace("\\", "/").upper().endswith("/WEEKLY PC")
            else os.path.join(configured_tm_base, "WEEKLY PC")
        )
        if os.path.exists(configured_weekly_path):
            return configured_weekly_path
        print_warning(f"Configured GOJI_TM_BASE_PATH not found: {configured_weekly_path}")

    if os.path.exists(CANONICAL_TM_WEEKLY_BASE):
        return CANONICAL_TM_WEEKLY_BASE

    if os.path.exists(LEGACY_TM_WEEKLY_BASE):
        print_warning(
            "Using legacy WEEKLY PC runtime path C:\\Goji\\TRACHMAR\\WEEKLY PC. "
            "Migrate to C:\\Goji\\AUTOMATION\\TRACHMAR\\WEEKLY PC."
        )
        return LEGACY_TM_WEEKLY_BASE

    os.makedirs(CANONICAL_TM_WEEKLY_BASE, exist_ok=True)
    print_warning(f"Created canonical WEEKLY PC runtime path: {CANONICAL_TM_WEEKLY_BASE}")
    return CANONICAL_TM_WEEKLY_BASE

def add_unique_id_column(file_path):
    """Add UNIQUE ID column as the first column with sequential numbers"""
    try:
        print_status("Adding UNIQUE ID column to the file...")
        
        # Read the CSV file
        df = pd.read_csv(file_path)
        row_count = len(df)
        
        print_status(f"File contains {row_count} data rows (excluding header)")
        
        # Calculate the number of digits needed based on row count
        if row_count == 0:
            print_warning("File has no data rows - UNIQUE ID column will be empty")
            digit_count = 1
        else:
            digit_count = len(str(row_count))
        
        print_status(f"UNIQUE ID will use {digit_count} digits")
        
        # Generate sequential UNIQUE ID values with appropriate padding
        unique_ids = []
        for i in range(1, row_count + 1):
            unique_id = str(i).zfill(digit_count)
            unique_ids.append(unique_id)
        
        # Insert UNIQUE ID as the first column
        df.insert(0, 'UNIQUE ID', unique_ids)
        
        print_status(f"Generated UNIQUE ID values from {unique_ids[0] if unique_ids else 'N/A'} to {unique_ids[-1] if unique_ids else 'N/A'}")
        
        # Save the modified file back
        df.to_csv(file_path, index=False)
        
        print_status("UNIQUE ID column added successfully")
        return df
        
    except pd.errors.EmptyDataError:
        print_error("CSV file is empty")
        return None
    except pd.errors.ParserError as e:
        print_error(f"Failed to parse CSV file: {str(e)}")
        return None
    except Exception as e:
        print_error(f"Failed to add UNIQUE ID column: {str(e)}")
        return None

def create_input_csv(df, job_input_path):
    """Create INPUT.csv with specific column structure"""
    try:
        print_status("Creating INPUT.csv file...")
        
        # Define required columns and their mappings
        required_columns = {
            'language_indicator': 'language_indicator',
            'hoh_guardian_name': 'Full Name',
            'member_address1': 'Address Line 1',
            'member_address2': 'Address Line 2',
            'member_city': 'City',
            'member_state': 'State',
            'member_zip': 'ZIP Code'
        }
        
        # Check if all required columns exist in the dataframe
        missing_columns = []
        for original_col in required_columns.keys():
            if original_col not in df.columns:
                missing_columns.append(original_col)
        
        if missing_columns:
            print_error(f"Missing required columns in source data: {missing_columns}")
            return False
        
        # Create new dataframe with only the required columns in the correct order
        input_df = pd.DataFrame()
        
        # Add UNIQUE ID as first column
        input_df['UNIQUE ID'] = df['UNIQUE ID']
        
        # Add other columns in the specified order with their new names
        column_order = [
            ('language_indicator', 'language_indicator'),
            ('hoh_guardian_name', 'Full Name'),
            ('member_address1', 'Address Line 1'),
            ('member_address2', 'Address Line 2'),
            ('member_city', 'City'),
            ('member_state', 'State'),
            ('member_zip', 'ZIP Code')
        ]
        
        for original_col, new_col in column_order:
            input_df[new_col] = df[original_col]
        
        # Define destination file path
        input_destination = os.path.join(job_input_path, "INPUT.csv")
        
        # Check if destination file already exists
        if os.path.exists(input_destination):
            print_warning(f"INPUT.csv already exists and will be overwritten")
        
        # Save the INPUT.csv file
        print_status(f"Saving INPUT.csv to: {input_destination}")
        input_df.to_csv(input_destination, index=False)
        
        # Verify the file was created
        if not os.path.exists(input_destination):
            print_error("INPUT.csv creation failed - file does not exist after save operation")
            return False
        
        print_status("INPUT.csv created successfully!")
        print_status(f"INPUT.csv contains {len(input_df)} rows with columns: {list(input_df.columns)}")
        
        return True
        
    except Exception as e:
        print_error(f"Failed to create INPUT.csv: {str(e)}")
        return False

def process_fhk_file():
    # Updated paths
    downloads_path = os.path.expanduser(r"C:\Users\JCox\Downloads")
    weekly_base_path = resolve_tm_weekly_base_path()
    job_input_path = os.path.join(weekly_base_path, "JOB", "INPUT")
    
    print_status("Starting FHK file processing...")
    print_status(f"Searching for FHK files in: {downloads_path}")
    
    # Check if Downloads folder exists and is accessible
    if not os.path.exists(downloads_path):
        print_error(f"Downloads folder not found: {downloads_path}")
        return False
    
    if not os.access(downloads_path, os.R_OK):
        print_error(f"Downloads folder not accessible (permission denied): {downloads_path}")
        return False
    
    # Find FHK files in downloads
    try:
        fhk_files = glob.glob(os.path.join(downloads_path, "*FHK*"))
    except Exception as e:
        print_error(f"Failed to search for FHK files: {str(e)}")
        return False
    
    if not fhk_files:
        print_error("No FHK files found in Downloads folder")
        print_status("Please ensure an FHK file is downloaded before running this script")
        return False
    
    print_status(f"Found {len(fhk_files)} FHK file(s)")
    
    # Get the most recent FHK file
    try:
        fhk_file = max(fhk_files, key=os.path.getctime)
        filename = os.path.basename(fhk_file)
        print_status(f"Selected most recent FHK file: {filename}")
        
        if len(fhk_files) > 1:
            print_warning(f"Multiple FHK files found ({len(fhk_files)}), using most recent: {filename}")
    
    except Exception as e:
        print_error(f"Failed to select FHK file: {str(e)}")
        return False
    
    # Extract date pattern
    digits = re.findall(r'\d{4}', filename)
    
    if not digits:
        print_warning("No 4-digit date sequence found in filename - this may be unexpected")
    else:
        print_status(f"Found date pattern in filename: {digits[0]}")
    
    # Check source file accessibility
    if not os.access(fhk_file, os.R_OK):
        print_error(f"Source FHK file not readable (permission denied): {fhk_file}")
        return False
    
    try:
        # Ensure input path exists
        print_status(f"Ensuring destination directory exists: {job_input_path}")
        os.makedirs(job_input_path, exist_ok=True)
        
        # Check if we can write to the destination directory
        if not os.access(job_input_path, os.W_OK):
            print_error(f"Destination directory not writable (permission denied): {job_input_path}")
            return False
        
        # Define destination files
        fhk_weekly_destination = os.path.join(job_input_path, "FHK_WEEKLY.csv")
        
        # Check if destination file already exists
        if os.path.exists(fhk_weekly_destination):
            print_warning(f"Destination file already exists and will be overwritten: FHK_WEEKLY.csv")
        
        # Copy the file to input destination with the new name
        print_status(f"Copying file to: {fhk_weekly_destination}")
        shutil.copy2(fhk_file, fhk_weekly_destination)
        
        # Verify the copy was successful
        if not os.path.exists(fhk_weekly_destination):
            print_error("File copy failed - destination file does not exist after copy operation")
            return False
        
        # Check file sizes match
        source_size = os.path.getsize(fhk_file)
        dest_size = os.path.getsize(fhk_weekly_destination)
        
        if source_size != dest_size:
            print_error(f"File copy verification failed - size mismatch (source: {source_size}, dest: {dest_size})")
            return False
        
        print_status("File copy completed successfully!")
        
        # Add UNIQUE ID column to the copied file and get the modified dataframe
        df_with_unique_id = add_unique_id_column(fhk_weekly_destination)
        if df_with_unique_id is None:
            return False
        
        # Create INPUT.csv with the specific column structure
        if not create_input_csv(df_with_unique_id, job_input_path):
            return False
        
        # Delete the original file from Downloads after successful processing
        try:
            print_status(f"Deleting original file from Downloads: {filename}")
            os.remove(fhk_file)
            
            # Verify the file was deleted
            if os.path.exists(fhk_file):
                print_warning("Original file still exists after deletion attempt")
            else:
                print_status("Original file successfully deleted from Downloads")
                
        except PermissionError:
            print_warning(f"Permission denied - could not delete original file: {filename}")
            print_status("File processing completed successfully, but original file remains in Downloads")
        except Exception as e:
            print_warning(f"Could not delete original file: {str(e)}")
            print_status("File processing completed successfully, but original file remains in Downloads")
        
        print_status("File processing completed successfully!")
        print_status(f"Source file: {fhk_file} (deleted)")
        print_status(f"FHK_WEEKLY.csv: {fhk_weekly_destination}")
        print_status(f"INPUT.csv: {os.path.join(job_input_path, 'INPUT.csv')}")
        print_status(f"File size: {os.path.getsize(fhk_weekly_destination)} bytes")
        
        return True
        
    except PermissionError as e:
        print_error(f"Permission denied during file operation: {str(e)}")
        return False
    except shutil.SameFileError:
        print_error("Source and destination are the same file")
        return False
    except shutil.DiskUsageError:
        print_error("Insufficient disk space for file copy")
        return False
    except Exception as e:
        print_error(f"Unexpected error during file processing: {str(e)}")
        return False

if __name__ == "__main__":
    print_status("=== FHK File Processing Script ===")
    
    try:
        success = process_fhk_file()
        
        if success:
            print_status("Script completed successfully")
            sys.exit(0)
        else:
            print_error("Script completed with errors")
            sys.exit(1)
            
    except KeyboardInterrupt:
        print_error("Script interrupted by user")
        sys.exit(2)
    except Exception as e:
        print_error(f"Unexpected script error: {str(e)}")
        sys.exit(3)
