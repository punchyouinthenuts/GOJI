import os
import glob
import shutil
import pandas as pd
from datetime import datetime

def process_file():
    raw_path = r"C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\DATA PROCESSING\RAW FILES"
    input_path = r"C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\DATA PROCESSING\BM INPUT"
    old_path = os.path.join(input_path, "old")
    input_csv = os.path.join(input_path, "INPUT.csv")
    
    # Handle existing INPUT.csv
    if os.path.exists(input_csv):
        os.makedirs(old_path, exist_ok=True)
        timestamp = datetime.now().strftime("%Y%m%d-%H%M")
        old_filename = f"INPUT_{timestamp}.csv"
        old_file_path = os.path.join(old_path, old_filename)
        shutil.move(input_csv, old_file_path)
        print(f"Moved existing INPUT.csv to: {old_file_path}")
    
    # Get list of numbered files (both XLSX and CSV)
    raw_files = [f for f in os.listdir(raw_path) if (f.endswith('.xlsx') or f.endswith('.csv')) and f[:2].isdigit()]
    
    if not raw_files:
        print("No numbered XLSX or CSV files found in the specified folder")
        return
    
    # Prompt user for file selection
    print("WHICH DATA FILE DO YOU WANT TO PROCESS? ENTER NUMBER:")
    file_number = input().zfill(2)  # Pad single digit with leading zero
    
    # Find matching file
    target_file = None
    for file in raw_files:
        if file.startswith(file_number):
            target_file = file
            break
    
    if not target_file:
        print(f"No file found starting with {file_number}")
        return
    
    try:
        os.makedirs(input_path, exist_ok=True)
        source_file_path = os.path.join(raw_path, target_file)
        destination_path = os.path.join(input_path, "INPUT.csv")
        
        if target_file.endswith('.xlsx'):
            # Convert XLSX to CSV
            df = pd.read_excel(source_file_path)
            df.to_csv(destination_path, index=False)
            print("Converted XLSX to CSV")
        else:
            # Direct copy for CSV files
            shutil.copy2(source_file_path, destination_path)
            print("Copied CSV file directly")
        
        print(f"Successfully processed file:")
        print(f"- Source file: {source_file_path}")
        print(f"- Destination: {destination_path}")
        
    except Exception as e:
        print(f"Error processing file: {str(e)}")

if __name__ == "__main__":
    process_file()
