import os
import zipfile
import glob
import csv
import shutil
import sys
import time
import traceback
import random
import string

def generate_match_id_prefix():
    """Generate a random 2-letter prefix for MATCH IDs"""
    return ''.join(random.choices(string.ascii_uppercase, k=2))

def add_match_id_column(csv_file_path, match_id_prefix, start_number):
    """Add MATCHID column to a CSV file and return the next available number"""
    temp_file_path = csv_file_path + '.temp'
    
    with open(csv_file_path, 'r', newline='', encoding='utf-8') as f_in:
        csv_reader = csv.reader(f_in)
        
        with open(temp_file_path, 'w', newline='', encoding='utf-8') as f_out:
            csv_writer = csv.writer(f_out)
            
            # Process header row
            headers = next(csv_reader)
            new_headers = ['MATCHID'] + headers
            csv_writer.writerow(new_headers)
            
            # Process data rows
            current_number = start_number
            for row in csv_reader:
                match_id = f"{match_id_prefix}{current_number:05d}"
                new_row = [match_id] + row
                csv_writer.writerow(new_row)
                current_number += 1
    
    # Replace original file with modified file
    os.replace(temp_file_path, csv_file_path)
    
    return current_number

def rollback(zip_path, moved_files, created_files):
    """Restore everything to original state"""
    print("\nROLLING BACK CHANGES...")
    
    # Remove any created files
    for file_path in created_files:
        if os.path.exists(file_path):
            try:
                os.remove(file_path)
                print(f"Removed: {file_path}")
            except Exception as e:
                print(f"Failed to remove {file_path}: {e}")
    
    # Restore any moved files
    for dest_path, orig_path in moved_files:
        if os.path.exists(dest_path):
            try:
                if os.path.exists(orig_path):
                    os.remove(orig_path)  # Remove existing file at original location if it exists
                shutil.move(dest_path, orig_path)
                print(f"Restored: {orig_path}")
            except Exception as e:
                print(f"Failed to restore {orig_path}: {e}")
    
    # Recreate ZIP file if it was deleted and we have the path
    if zip_path and not os.path.exists(zip_path):
        print(f"Unable to restore deleted ZIP file: {zip_path}")
    
    print("ROLLBACK COMPLETE")

def main():
    print("=== SCRIPT INITIALIZATION ===")
    print(f"Python version: {sys.version}")
    print(f"Current working directory: {os.getcwd()}")
    print(f"Script arguments: {sys.argv}")
    
    # Track changes for potential rollback
    zip_path = None
    moved_files = []  # List of (destination_path, original_path) tuples
    created_files = []  # List of created file paths
    
    try:
        print("\n=== GENERATING MATCH ID PREFIX ===")
        # Generate unique MATCH ID prefix for this run
        match_id_prefix = generate_match_id_prefix()
        print(f"Generated MATCH ID prefix: {match_id_prefix}")
        
        print("\n=== DEFINING DIRECTORY PATHS ===")
        # Define paths - GOJI directory structure
        zip_input_dir = r'C:\Goji\TRACHMAR\HEALTHY BEGINNINGS\INPUT ZIP'
        data_input_dir = r'C:\Goji\TRACHMAR\HEALTHY BEGINNINGS\DATA\INPUT'
        data_original_dir = r'C:\Goji\TRACHMAR\HEALTHY BEGINNINGS\DATA\ORIGINAL'
        
        print(f"ZIP Input Directory: {zip_input_dir}")
        print(f"Data Input Directory: {data_input_dir}")
        print(f"Data Original Directory: {data_original_dir}")
        
        print("\n=== CHECKING DIRECTORY EXISTENCE ===")
        print(f"ZIP Input Directory exists: {os.path.exists(zip_input_dir)}")
        print(f"Data Input Directory exists: {os.path.exists(data_input_dir)}")
        print(f"Data Original Directory exists: {os.path.exists(data_original_dir)}")
        
        if not os.path.exists(zip_input_dir):
            print(f"ERROR: ZIP Input Directory does not exist: {zip_input_dir}")
            # Try to list parent directory to see what's available
            parent_dir = os.path.dirname(zip_input_dir)
            print(f"Parent directory: {parent_dir}")
            if os.path.exists(parent_dir):
                print(f"Contents of parent directory:")
                for item in os.listdir(parent_dir):
                    print(f"  - {item}")
            else:
                print(f"Parent directory also does not exist: {parent_dir}")
            raise Exception(f"Required input directory does not exist: {zip_input_dir}")
        
        print("\n=== CREATING OUTPUT DIRECTORIES ===")
        # Ensure output directories exist
        try:
            os.makedirs(data_input_dir, exist_ok=True)
            print(f"Created/verified data input directory: {data_input_dir}")
        except Exception as e:
            print(f"Failed to create data input directory: {e}")
            raise
            
        try:
            os.makedirs(data_original_dir, exist_ok=True)
            print(f"Created/verified data original directory: {data_original_dir}")
        except Exception as e:
            print(f"Failed to create data original directory: {e}")
            raise
        
        print("\n=== SCANNING FOR FILES ===")
        print(f"Scanning for ZIP files in: {zip_input_dir}")
        
        # Check if there's a ZIP file in the input directory
        zip_pattern = os.path.join(zip_input_dir, '*.zip')
        print(f"ZIP search pattern: {zip_pattern}")
        zip_files = glob.glob(zip_pattern)
        print(f"Found {len(zip_files)} ZIP file(s): {zip_files}")
        
        print(f"Scanning for CSV files in: {zip_input_dir}")
        csv_pattern = os.path.join(zip_input_dir, '*.csv')
        print(f"CSV search pattern: {csv_pattern}")
        csv_files = glob.glob(csv_pattern)
        print(f"Found {len(csv_files)} CSV file(s): {csv_files}")
        
        # List all files in the directory for debugging
        print(f"\nAll files in {zip_input_dir}:")
        try:
            all_files = os.listdir(zip_input_dir)
            for file in all_files:
                file_path = os.path.join(zip_input_dir, file)
                print(f"  - {file} (size: {os.path.getsize(file_path)} bytes)")
        except Exception as e:
            print(f"Failed to list directory contents: {e}")
        
        print("\n=== DECISION LOGIC ===")
        print(f"ZIP files count: {len(zip_files)}")
        print(f"CSV files count: {len(csv_files)}")
        
        if len(zip_files) == 0 and len(csv_files) == 1:
            print("DECISION: Processing single CSV file (no ZIP files)")
            # Only one CSV file exists and no ZIP files
            single_csv = csv_files[0]
            
            # Add MATCHID column to the single CSV file
            print(f"Adding MATCHID column to single CSV file: {os.path.basename(single_csv)}")
            next_number = add_match_id_column(single_csv, match_id_prefix, 1)
            total_records = next_number - 1
            print(f"Added MATCHID column with {total_records} records ({match_id_prefix}00001 - {match_id_prefix}{total_records:05d})")
            
            # Copy the modified CSV file to the INPUT location
            output_path = os.path.join(data_input_dir, 'INPUT.csv')
            
            # Handle existing file automatically - backup with timestamp
            if os.path.exists(output_path):
                backup_path = output_path + f".backup_{int(time.time())}"
                shutil.move(output_path, backup_path)
                print(f"Backed up existing file to: {backup_path}")
            
            shutil.copy2(single_csv, output_path)
            created_files.append(output_path)
            
            # Move the modified original file to ORIGINAL folder
            dest_path = os.path.join(data_original_dir, os.path.basename(single_csv))
            
            # Handle existing file in ORIGINAL folder automatically
            if os.path.exists(dest_path):
                backup_path = dest_path + f".backup_{int(time.time())}"
                shutil.move(dest_path, backup_path)
                print(f"Backed up existing original file to: {backup_path}")
            
            shutil.move(single_csv, dest_path)
            moved_files.append((dest_path, single_csv))
            
            print(f"Single CSV file processed. Copied to {output_path} and moved original to ORIGINAL folder.")
            
        elif len(zip_files) == 0 and len(csv_files) > 1:
            print(f"DECISION: Processing multiple CSV files ({len(csv_files)} files)")
            # Multiple CSV files - process and combine them
            print(f"Processing {len(csv_files)} CSV files with MATCHID columns...")
            
            # Sort CSV files to ensure consistent processing order
            csv_files.sort()
            print(f"Processing order: {[os.path.basename(f) for f in csv_files]}")
            
            # Add MATCHID columns to each file
            current_start_number = 1
            for i, csv_file in enumerate(csv_files):
                print(f"\nProcessing file {i+1}/{len(csv_files)}: {os.path.basename(csv_file)}")
                print(f"File size: {os.path.getsize(csv_file)} bytes")
                
                next_number = add_match_id_column(csv_file, match_id_prefix, current_start_number)
                records_in_file = next_number - current_start_number
                end_number = next_number - 1
                print(f"  Added {records_in_file} MATCHIDs ({match_id_prefix}{current_start_number:05d} - {match_id_prefix}{end_number:05d})")
                current_start_number = next_number
            
            total_records = current_start_number - 1
            print(f"\nTotal records across all files: {total_records} ({match_id_prefix}00001 - {match_id_prefix}{total_records:05d})")
            
            # Now combine all modified CSV files
            print(f"Combining {len(csv_files)} CSV files...")
            
            # Create output file path
            output_path = os.path.join(data_input_dir, 'INPUT.csv')
            print(f"Output path: {output_path}")
            
            # Handle existing file automatically - backup with timestamp
            if os.path.exists(output_path):
                backup_path = output_path + f".backup_{int(time.time())}"
                shutil.move(output_path, backup_path)
                print(f"Backed up existing file to: {backup_path}")
            
            # Get headers from the first CSV file (which now includes MATCHID)
            print(f"Reading headers from: {csv_files[0]}")
            with open(csv_files[0], 'r', newline='', encoding='utf-8') as f:
                csv_reader = csv.reader(f)
                headers = next(csv_reader)
            print(f"Headers: {headers[:5]}{'...' if len(headers) > 5 else ''}")
            
            # Write the combined CSV file
            print(f"Writing combined file to: {output_path}")
            with open(output_path, 'w', newline='', encoding='utf-8') as f_out:
                csv_writer = csv.writer(f_out)
                
                # Write the header row
                csv_writer.writerow(headers)
                print("Headers written to output file")
                
                # Process each CSV file
                total_rows_written = 0
                for csv_file in csv_files:
                    print(f"Copying data from: {os.path.basename(csv_file)}")
                    rows_from_file = 0
                    with open(csv_file, 'r', newline='', encoding='utf-8') as f_in:
                        csv_reader = csv.reader(f_in)
                        
                        # Skip the header row
                        next(csv_reader)
                        
                        # Copy all data rows
                        for row in csv_reader:
                            csv_writer.writerow(row)
                            rows_from_file += 1
                            total_rows_written += 1
                    
                    print(f"  Copied {rows_from_file} rows from {os.path.basename(csv_file)}")
                    
                    # Move the modified original file to ORIGINAL folder
                    dest_path = os.path.join(data_original_dir, os.path.basename(csv_file))
                    
                    # Handle existing file in ORIGINAL folder automatically
                    if os.path.exists(dest_path):
                        backup_path = dest_path + f".backup_{int(time.time())}"
                        shutil.move(dest_path, backup_path)
                        print(f"Backed up existing original file to: {backup_path}")
                    
                    shutil.move(csv_file, dest_path)
                    moved_files.append((dest_path, csv_file))
                    print(f"Moved original to: {dest_path}")
            
            created_files.append(output_path)
            print(f"\nCombined {len(csv_files)} CSV files into {output_path}")
            print(f"Total rows written: {total_rows_written}")
            print(f"Original CSV files (with MATCHID columns) moved to {data_original_dir}")
            
        elif len(zip_files) == 1:
            print("DECISION: Processing single ZIP file")
            # One ZIP file exists
            zip_path = zip_files[0]
            print(f"Processing ZIP file: {os.path.basename(zip_path)}")
            print(f"ZIP file size: {os.path.getsize(zip_path)} bytes")
            
            # Extract ZIP file contents to a temporary directory
            temp_extract_dir = os.path.join(os.path.dirname(zip_path), 'temp_extract')
            print(f"Creating temporary extraction directory: {temp_extract_dir}")
            os.makedirs(temp_extract_dir, exist_ok=True)
            created_files.append(temp_extract_dir)
            
            print("Extracting ZIP file...")
            with zipfile.ZipFile(zip_path, 'r') as zip_ref:
                zip_ref.extractall(temp_extract_dir)
            
            print(f"Extracted ZIP contents to temporary directory")
            
            # Find CSV files in the extracted directory
            print("Searching for CSV files in extracted contents...")
            extracted_csv_files = []
            for root, dirs, files in os.walk(temp_extract_dir):
                print(f"Checking directory: {root}")
                for file in files:
                    print(f"  Found file: {file}")
                    if file.lower().endswith('.csv'):
                        file_path = os.path.join(root, file)
                        extracted_csv_files.append(file_path)
                        print(f"    -> CSV file added: {file_path}")
            
            if not extracted_csv_files:
                raise Exception("No CSV files found in the ZIP archive")
            
            print(f"Found {len(extracted_csv_files)} CSV file(s) in ZIP archive: {[os.path.basename(f) for f in extracted_csv_files]}")
            
            if len(extracted_csv_files) == 1:
                print("Processing single CSV from ZIP")
                # Single CSV file from ZIP
                csv_file = extracted_csv_files[0]
                print(f"Processing single CSV from ZIP: {os.path.basename(csv_file)}")
                
                # Add MATCHID column
                next_number = addહ
                total_records = next_number - 1
                print(f"Added MATCHID column with {total_records} records ({match_id_prefix}00001 - {match_id_prefix}{total_records:05d})")
                
                # Copy to INPUT directory
                output_path = os.path.join(data_input_dir, 'INPUT.csv')
                
                # Handle existing file automatically
                if os.path.exists(output_path):
                    backup_path = output_path + f".backup_{int(time.time())}"
                    shutil.move(output_path, backup_path)
                    print(f"Backed up existing file to: {backup_path}")
                
                shutil.copy2(csv_file, output_path)
                created_files.append(output_path)
                
                # Move processed CSV file to ORIGINAL directory
                for extracted_file in extracted_csv_files:
                    destination = os.path.join(data_original_dir, os.path.basename(extracted_file))
                    shutil.move(extracted_file, destination)
                    print(f"Moved processed CSV to ORIGINAL: {destination}")
                
                print(f"Processed {os.path.basename(csv_file)} from ZIP to INPUT directory")
                
            else:
                print(f"Processing multiple CSV files from ZIP ({len(extracted_csv_files)} files)")
                # Multiple CSV files from ZIP - process and combine
                print(f"Processing {len(extracted_csv_files)} CSV files from ZIP...")
                
                # Sort for consistent processing
                extracted_csv_files.sort()
                
                # Add MATCHID columns to each file
                current_start_number = 1
                for i, csv_file in enumerate(extracted_csv_files):
                    print(f"Processing file {i+1}/{len(extracted_csv_files)}: {os.path.basename(csv_file)}")
                    next_number = add_match_id_column(csv_file, match_id_prefix, current_start_number)
                    records_in_file = next_number - current_start_number
                    end_number = next_number - 1
                    print(f"  Added {records_in_file} MATCHIDs ({match_id_prefix}{current_start_number:05d} - {match_id_prefix}{end_number:05d})")
                    current_start_number = next_number
                
                total_records = current_start_number - 1
                print(f"Total records across all files: {total_records} ({match_id_prefix}00001 - {match_id_prefix}{total_records:05d})")
                
                # Combine all CSV files
                output_path = os.path.join(data_input_dir, 'INPUT.csv')
                
                # Handle existing file automatically
                if os.path.exists(output_path):
                    backup_path = output_path + f".backup_{int(time.time())}"
                    shutil.move(output_path, backup_path)
                    print(f"Backed up existing file to: {backup_path}")
                
                # Get headers from first file
                with open(extracted_csv_files[0], 'r', newline='', encoding='utf-8') as f:
                    csv_reader = csv.reader(f)
                    headers = next(csv_reader)
                
                # Write combined file
                print(f"Writing combined file to: {output_path}")
                with open(output_path, 'w', newline='', encoding='utf-8') as f_out:
                    csv_writer = csv.writer(f_out)
                    csv_writer.writerow(headers)
                    
                    for csv_file in extracted_csv_files:
                        with open(csv_file, 'r', newline='', encoding='utf-8') as f_in:
                            csv_reader = csv.reader(f_in)
                            next(csv_reader)  # Skip header
                            for row in csv_reader:
                                csv_writer.writerow(row)
                
                created_files.append(output_path)
                
                # Move processed CSV files to ORIGINAL directory
                for extracted_file in extracted_csv_files:
                    destination = os.path.join(data_original_dir, os.path.basename(extracted_file))
                    shutil.move(extracted_file, destination)
                    print(f"Moved processed CSV to ORIGINAL: {destination}")
                
                print(f"Combined {len(extracted_csv_files)} CSV files into {output_path}")
            
            # Clean up temporary extraction directory
            shutil.rmtree(temp_extract_dir)
            created_files.remove(temp_extract_dir)
            
            # Delete the original ZIP file from INPUT ZIP directory
            os.remove(zip_path)
            print("Cleaned up processed ZIP from INPUT ZIP")
            
        elif len(zip_files) > 1:
            print(f"DECISION: ERROR - Multiple ZIP files found ({len(zip_files)})")
            raise Exception(f"Multiple ZIP files found ({len(zip_files)}). Please ensure only one ZIP file is present.")
            
        else:
            print("DECISION: ERROR - No files found")
            print(f"ZIP files: {len(zip_files)}, CSV files: {len(csv_files)}")
            raise Exception("No ZIP or CSV files found in INPUT ZIP directory")
        
        print("\n" + "="*50)
        print("INITIAL PROCESSING COMPLETED SUCCESSFULLY!")
        print("Files are now ready for processing in the DATA/INPUT directory")
        print("="*50)
        
        # Exit cleanly without user input
        print("\nPROCESS COMPLETE! TERMINATING...")
        time.sleep(2.5)  # Wait for 2.5 seconds before terminating
        print("Script exiting with success code 0")
        sys.exit(0)
        
    except Exception as e:
        print(f"\n=== ERROR OCCURRED ===")
        print(f"ERROR: {str(e)}")
        print("Stack trace:")
        traceback.print_exc()
        
        print("\n=== INITIATING ROLLBACK ===")
        rollback(zip_path, moved_files, created_files)
        
        # Exit with error code instead of waiting for user input
        print("Script exiting with error code 1")
        sys.exit(1)

if __name__ == "__main__":
    main()
