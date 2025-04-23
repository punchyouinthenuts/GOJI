import pandas as pd
import os
import shutil
import re
import sys
import time
import zipfile
from datetime import datetime

def print_progress(message, end='\r'):
    print(f"[{datetime.now().strftime('%H:%M:%S')}] {message}", end=end)
    if end == '\n':
        sys.stdout.flush()

def create_directories():
    base_path = r"C:\Users\JCox\Desktop\AUTOMATION\SISNEROS\CLEANUP"
    dirs = {
        'bulk_mailer': os.path.join(base_path, "BULK MAILER OUTPUT"),
        'broken_down': os.path.join(base_path, "BROKEN DOWN CSV FILES"),
        'duplicates': os.path.join(base_path, "DUPLICATE FILE OUTPUT"),
        'base': base_path
    }
    
    for dir_path in dirs.values():
        os.makedirs(dir_path, exist_ok=True)
    
    return dirs

def safe_read_csv(file_path):
    try:
        return pd.read_csv(file_path, low_memory=False)
    except Exception as e:
        print_progress(f"Error reading {os.path.basename(file_path)}: {str(e)}", end='\n')
        return None

def safe_write_csv(df, file_path):
    try:
        df.to_csv(file_path, index=False)
        time.sleep(0.5)  # Small delay after writing
        return True
    except Exception as e:
        print_progress(f"Error writing {os.path.basename(file_path)}: {str(e)}", end='\n')
        return False

def fix_header(file_path):
    print_progress(f"Fixing header in {os.path.basename(file_path)}...")
    df = safe_read_csv(file_path)
    if df is not None and 'congressional_distric' in df.columns:
        df = df.rename(columns={'congressional_distric': 'congressional_district'})
        safe_write_csv(df, file_path)

def combine_numbered_files(bulk_mailer_dir, broken_down_dir):
    print_progress("Combining numbered files...")
    file_groups = {}
    consolidated_files = []  # New list to track consolidated files
    
    for file in os.listdir(bulk_mailer_dir):
        if file.endswith('.csv'):
            match = re.match(r'(.+)_(\d{2})\.csv$', file)
            if match:
                base_name = match.group(1)
                if base_name not in file_groups:
                    file_groups[base_name] = []
                file_groups[base_name].append(file)
    
    for base_name, files in file_groups.items():
        files.sort()
        combined_df = None
        processed_files = []
        
        for i, file in enumerate(files):
            file_path = os.path.join(bulk_mailer_dir, file)
            df = safe_read_csv(file_path)
            
            if df is not None:
                if i == 0:
                    combined_df = df
                else:
                    combined_df = pd.concat([combined_df, df.iloc[1:]], ignore_index=True)
                processed_files.append(file_path)
        
        if combined_df is not None:
            output_path = os.path.join(bulk_mailer_dir, f"{base_name}.csv")
            if safe_write_csv(combined_df, output_path):
                consolidated_files.append(f"{base_name}.csv")  # Add to consolidated files list
                for file_path in processed_files:
                    try:
                        time.sleep(0.5)  # Delay before moving
                        shutil.move(file_path, os.path.join(broken_down_dir, os.path.basename(file_path)))
                    except Exception as e:
                        print_progress(f"Error moving {os.path.basename(file_path)}: {str(e)}", end='\n')
    
    return consolidated_files  # Return the list of consolidated files

def process_duplicates(file_path, duplicates_dir):
    print_progress(f"Processing duplicates in {os.path.basename(file_path)}...")
    df = safe_read_csv(file_path)
    if df is None:
        return
    
    duplicate_check_columns = [
        'First Name',
        'Last Name',
        'Address Line 1',
        'Address Line 2',
        'City',
        'State',
        'ZIP Code'
    ]
    
    # Preprocess columns to ignore case and whitespace
    for col in duplicate_check_columns:
        if col in df.columns:
            # Convert to string, lowercase, and strip whitespace
            df[col] = df[col].astype(str).str.lower().str.strip()
    
    missing_columns = [col for col in duplicate_check_columns if col not in df.columns]
    if missing_columns:
        print_progress(f"Warning: Missing columns in {os.path.basename(file_path)}: {', '.join(missing_columns)}", end='\n')
        return
    
    duplicates = df[df.duplicated(subset=duplicate_check_columns, keep='first')]
    
    if not duplicates.empty:
        base_name = os.path.splitext(os.path.basename(file_path))[0]
        dup_path = os.path.join(duplicates_dir, f"{base_name}_duplicates.csv")
        if safe_write_csv(duplicates, dup_path):
            df = df.drop_duplicates(subset=duplicate_check_columns, keep='first')
            if safe_write_csv(df, file_path):
                print_progress(f"Found and removed {len(duplicates)} duplicate entries", end='\n')
    else:
        print_progress(f"No duplicates found in {os.path.basename(file_path)}", end='\n')

def create_zip_archive(bulk_mailer_dir, base_dir):
    print_progress("Creating ZIP archive of processed files...")
    
    # Get all CSV files in the bulk mailer directory
    csv_files = [f for f in os.listdir(bulk_mailer_dir) if f.endswith('.csv')]
    
    if not csv_files:
        print_progress("No CSV files found to archive.", end='\n')
        return False
    
    # Determine common prefix for the ZIP file name
    # Extract prefix before "_excel_" from the first file
    zip_name = None
    for file in csv_files:
        parts = file.split('_excel_')
        if len(parts) > 1:
            zip_name = parts[0]
            break
    
    # If no common prefix found, use a default name with date
    if not zip_name:
        zip_name = f"BulkMailer_{datetime.now().strftime('%Y%m%d')}"
    
    zip_path = os.path.join(base_dir, f"{zip_name}.zip")
    
    try:
        # Create the ZIP file
        with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED) as zipf:
            for file in csv_files:
                file_path = os.path.join(bulk_mailer_dir, file)
                # Add file to the ZIP archive
                zipf.write(file_path, arcname=file)
                
                # Delete the original file after adding to ZIP
                time.sleep(0.5)  # Small delay
                os.remove(file_path)
                print_progress(f"Added and deleted: {file}", end='\n')
        
        print_progress(f"Successfully created ZIP archive: {zip_path}", end='\n')
        return True
    except Exception as e:
        print_progress(f"Error creating ZIP archive: {str(e)}", end='\n')
        return False

def main():
    try:
        dirs = create_directories()
        
        for file in os.listdir(dirs['bulk_mailer']):
            if file.endswith('.csv'):
                fix_header(os.path.join(dirs['bulk_mailer'], file))
        
        # Now get the list of consolidated files
        consolidated_files = combine_numbered_files(dirs['bulk_mailer'], dirs['broken_down'])
        
        # Process duplicates only for consolidated files
        for file in consolidated_files:
            process_duplicates(os.path.join(dirs['bulk_mailer'], file), dirs['duplicates'])
        
        # Create ZIP archive of all processed files
        create_zip_archive(dirs['bulk_mailer'], dirs['base'])
        
        print("\nALL FILES HAVE BEEN SUCCESSFULLY PROCESSED")
        while True:
            if input("Press X to terminate...").lower() == 'x':
                break
                
    except Exception as e:
        print(f"\nError occurred: {str(e)}")
        while True:
            if input("Press X to terminate...").lower() == 'x':
                break

if __name__ == "__main__":
    main()