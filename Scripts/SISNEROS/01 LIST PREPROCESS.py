import pandas as pd
import os
import shutil
import sys
import time
from datetime import datetime

class FileTracker:
    def __init__(self):
        self.new_files = []
        self.moved_files = {}  # Original location -> New location

    def add_new_file(self, file_path):
        self.new_files.append(file_path)

    def add_moved_file(self, original_path, new_path):
        self.moved_files[new_path] = original_path

def print_progress(message, end='\r'):
    print(f"[{datetime.now().strftime('%H:%M:%S')}] {message}", end=end)
    if end == '\n':
        sys.stdout.flush()

def rollback_changes(tracker):
    print("\nERROR OCCURRED - Initiating rollback...")
    
    # Delete new files
    for file_path in tracker.new_files:
        if os.path.exists(file_path):
            try:
                os.remove(file_path)
            except Exception:
                pass

    # Restore moved files
    for new_path, original_path in tracker.moved_files.items():
        if os.path.exists(new_path):
            try:
                shutil.move(new_path, original_path)
            except Exception:
                pass

    print("\nROLLBACK COMPLETE - All changes have been reversed")
    print("An error occurred during processing. Check input files and try again.")
    while True:
        if input("Press X to terminate...").lower() == 'x':
            sys.exit(1)

def create_directories():
    base_path = r"C:\Users\JCox\Desktop\AUTOMATION\SISNEROS\CLEANUP"
    excel_dir = os.path.join(base_path, "ORIGINAL EXCEL FILES")
    csv_dir = os.path.join(base_path, "ORIGINAL LARGE CSV FILES")
    
    os.makedirs(excel_dir, exist_ok=True)
    os.makedirs(csv_dir, exist_ok=True)
    
    return base_path, excel_dir, csv_dir

def process_excel_file(file_path, base_name, excel_dir, tracker):
    try:
        print_progress(f"Processing Excel file: {os.path.basename(file_path)}...")
        
        excel = pd.ExcelFile(file_path)
        sheet_names = excel.sheet_names
        
        if len(sheet_names) > 1:
            for sheet in sheet_names:
                df = pd.read_excel(file_path, sheet_name=sheet)
                df = df.replace(r'^\s*$', pd.NA, regex=True)
                df = df.dropna(how='all')
                output_name = f"{base_name}_{sheet}.csv"
                output_path = os.path.join(os.path.dirname(file_path), output_name)
                df.to_csv(output_path, index=False)
                tracker.add_new_file(output_path)
                del df
        else:
            df = pd.read_excel(file_path)
            df = df.replace(r'^\s*$', pd.NA, regex=True)
            df = df.dropna(how='all')
            output_name = f"{base_name}.csv"
            output_path = os.path.join(os.path.dirname(file_path), output_name)
            df.to_csv(output_path, index=False)
            tracker.add_new_file(output_path)
            del df
        
        excel.close()
        
        new_path = os.path.join(excel_dir, os.path.basename(file_path))
        shutil.move(file_path, new_path)
        tracker.add_moved_file(file_path, new_path)
        
        print_progress(f"Completed: {os.path.basename(file_path)}", end='\n')
        
    except Exception as e:
        print(f"\nError processing {file_path}: {str(e)}")
        raise

def process_csv_file(file_path, csv_dir, tracker, is_new=False):
    try:
        print_progress(f"Processing CSV file: {os.path.basename(file_path)}...")
        
        df = pd.read_csv(file_path)
        df = df.replace(r'^\s*$', pd.NA, regex=True)
        df = df.dropna(how='all')
        
        data_rows = len(df)
        
        if data_rows >= 100000:
            chunk_size = 100000
            base_name = os.path.splitext(os.path.basename(file_path))[0]
            
            # Calculate how many full chunks we can have
            full_chunks = data_rows // chunk_size
            remainder = data_rows % chunk_size
            
            # Check if the remainder will be less than 200
            if 0 < remainder < 200:
                # Adjust the last full chunk to ensure the final chunk has exactly 200 entries
                adjusted_last_chunk_size = chunk_size - (200 - remainder)
                
                # Process all chunks except the last two
                for i in range(full_chunks - 1):
                    chunk_df = df.iloc[i*chunk_size:min((i+1)*chunk_size, data_rows)]
                    output_name = f"{base_name}_{str(i+1).zfill(2)}.csv"
                    output_path = os.path.join(os.path.dirname(file_path), output_name)
                    chunk_df.to_csv(output_path, index=False)
                    tracker.add_new_file(output_path)
                    del chunk_df
                
                # Process the adjusted second-to-last chunk
                i = full_chunks - 1
                chunk_df = df.iloc[i*chunk_size:(i*chunk_size + adjusted_last_chunk_size)]
                output_name = f"{base_name}_{str(i+1).zfill(2)}.csv"
                output_path = os.path.join(os.path.dirname(file_path), output_name)
                chunk_df.to_csv(output_path, index=False)
                tracker.add_new_file(output_path)
                del chunk_df
                
                # Process the final chunk with exactly 200 entries
                final_chunk_df = df.iloc[(i*chunk_size + adjusted_last_chunk_size):]
                output_name = f"{base_name}_{str(i+2).zfill(2)}.csv"
                output_path = os.path.join(os.path.dirname(file_path), output_name)
                final_chunk_df.to_csv(output_path, index=False)
                tracker.add_new_file(output_path)
                del final_chunk_df
            else:
                # Normal processing - no need to adjust chunk sizes
                num_chunks = (data_rows + chunk_size - 1) // chunk_size
                
                for i in range(num_chunks):
                    chunk_df = df.iloc[i*chunk_size:min((i+1)*chunk_size, data_rows)]
                    output_name = f"{base_name}_{str(i+1).zfill(2)}.csv"
                    output_path = os.path.join(os.path.dirname(file_path), output_name)
                    chunk_df.to_csv(output_path, index=False)
                    tracker.add_new_file(output_path)
                    del chunk_df
            
            new_path = os.path.join(csv_dir, os.path.basename(file_path))
            shutil.move(file_path, new_path)
            tracker.add_moved_file(file_path, new_path)
        else:
            if not is_new:
                df.to_csv(file_path, index=False)
        
        del df
        print_progress(f"Completed: {os.path.basename(file_path)}", end='\n')
        
    except Exception as e:
        print(f"\nError processing {file_path}: {str(e)}")
        raise

def main():
    tracker = FileTracker()
    try:
        base_path, excel_dir, csv_dir = create_directories()
        
        # Step 1 & 2: Process Excel files
        excel_files = [f for f in os.listdir(base_path) if f.lower().endswith(('.xlsx', '.xls'))]
        for file in excel_files:
            file_path = os.path.join(base_path, file)
            base_name = os.path.splitext(file)[0]
            process_excel_file(file_path, base_name, excel_dir, tracker)
        
        # Step 4 & 5: Process all CSV files (including newly created ones)
        csv_files = [f for f in os.listdir(base_path) if f.lower().endswith('.csv')]
        for file in csv_files:
            file_path = os.path.join(base_path, file)
            is_new = file_path in tracker.new_files
            process_csv_file(file_path, csv_dir, tracker, is_new)
        
        print("\nALL DATA FILES HAVE BEEN SUCCESSFULLY PROCESSED")
        while True:
            if input("Press X to terminate...").lower() == 'x':
                sys.exit(0)
                
    except Exception as e:
        print(f"\nCritical Error: {str(e)}")
        rollback_changes(tracker)

if __name__ == "__main__":
    main()
