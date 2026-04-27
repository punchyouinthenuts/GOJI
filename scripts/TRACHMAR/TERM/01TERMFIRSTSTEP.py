import glob
import pandas as pd
import os
import time
import sys
import threading
import shutil
from tqdm import tqdm
import tkinter as tk
from tkinter import messagebox

CANONICAL_TM_ROOT = r"C:\Goji\AUTOMATION\TRACHMAR"

def resolve_tm_root():
    if os.path.isdir(CANONICAL_TM_ROOT):
        return CANONICAL_TM_ROOT
    os.makedirs(CANONICAL_TM_ROOT, exist_ok=True)
    print("INFO: created canonical TRACHMAR root C:\\Goji\\AUTOMATION\\TRACHMAR.")
    return CANONICAL_TM_ROOT

def loading_animation(description="Processing"):
    """GOJI-compatible loading animation that outputs to terminal"""
    with tqdm(total=100, desc=description, bar_format='{l_bar}{bar}| {n_fmt}/{total_fmt}') as pbar:
        while not loading_complete:
            pbar.update(1)
            time.sleep(0.1)
            if pbar.n >= 99:
                pbar.n = 0

def find_header_row(df):
    """Find the row containing Member ID and Guardian Name headers"""
    for idx, row in df.iterrows():
        if "Member ID" in row.values and "Guardian Name" in row.values:
            return idx
    return None

def clean_data_folder(output_dir):
    """Clean out the DATA folder before processing"""
    print("Cleaning DATA folder...")
    
    # List of files to keep (don't delete these)
    files_to_keep = []
    
    # Files to definitely delete if they exist
    files_to_delete = ['FHK_TERM_UPDATED.xlsx', 'PRESORTLIST.csv', 'MOVE UPDATES.csv']
    
    for file_name in files_to_delete:
        file_path = os.path.join(output_dir, file_name)
        if os.path.exists(file_path):
            try:
                os.remove(file_path)
                print(f"Deleted: {file_name}")
            except Exception as e:
                print(f"Could not delete {file_name}: {str(e)}")
                
    # Delete any other files in the folder except those in files_to_keep
    for file in os.listdir(output_dir):
        file_path = os.path.join(output_dir, file)
        if os.path.isfile(file_path) and file not in files_to_keep:
            try:
                os.remove(file_path)
                print(f"Deleted: {file}")
            except Exception as e:
                print(f"Could not delete {file}: {str(e)}")
    
    print("DATA folder cleanup complete")

def show_no_files_popup():
    """Show popup dialog when no TERM files are found"""
    # Create root window and hide it
    root = tk.Tk()
    root.withdraw()  # Hide the main window
    
    # Show message box
    messagebox.showinfo("No Files Found", "No FHK_Term XLSX files found in Downloads folder")
    
    # Destroy the root window
    root.destroy()

def clean_text_for_csv(text):
    """Clean text to prevent CSV issues but keep the original ID format"""
    if pd.isna(text) or text is None:
        return ""
    
    text = str(text).strip()
    
    # Remove quotes and problematic characters
    text = text.replace('"', '')
    text = text.replace('\n', ' ')
    text = text.replace('\r', ' ')
    
    # Fix .0 suffix for numbers
    if text.endswith('.0') and text.replace('.', '').replace('-', '').isdigit():
        text = text[:-2]
    
    return text

def clean_for_no_quotes_csv(text):
    """Clean text specifically for quote-free CSV export"""
    if pd.isna(text) or text is None:
        return ""
    
    text = str(text).strip()
    
    # Remove ALL problematic characters for quote-free CSV
    text = text.replace('"', '')
    text = text.replace(',', ' ')  # Replace commas with spaces
    text = text.replace('\n', ' ')
    text = text.replace('\r', ' ')
    
    # Fix .0 suffix for numbers
    if text.endswith('.0') and text.replace('.', '').replace('-', '').isdigit():
        text = text[:-2]
    
    # Clean up multiple spaces
    while '  ' in text:
        text = text.replace('  ', ' ')
    
    return text.strip()

def process_term_file():
    """Main processing function - RESTORED to original logic with clean output"""
    global loading_complete
    loading_complete = False
    
    file_path = r'C:\Users\JCox\Downloads'
    output_dir = os.path.join(resolve_tm_root(), "TERM", "DATA")
    
    # Create directory if it doesn't exist
    os.makedirs(output_dir, exist_ok=True)
    
    # Clean the data folder first
    clean_data_folder(output_dir)
    
    # Search for TERM files
    input_files = glob.glob(os.path.join(file_path, 'FHK_Term*.xlsx'))
    
    if not input_files:
        # Show popup and log to terminal
        print("ERROR: No FHK_Term*.xlsx files found in Downloads folder")
        show_no_files_popup()
        print("Script terminated - no input files found")
        sys.exit(1)

    # Get the newest file
    input_file = max(input_files, key=os.path.getctime)
    print(f"Processing file: {os.path.basename(input_file)}")
    
    try:
        # Copy and rename the file to the destination folder
        destination_file = os.path.join(output_dir, 'FHK_TERM.xlsx')
        shutil.copy2(input_file, destination_file)
        print(f"Copied to: {destination_file}")

        # Start loading animation in separate thread
        animation_thread = threading.Thread(target=loading_animation, args=("Processing TERM data",))
        animation_thread.start()
        
        # Read the Excel file without headers
        print("Reading Excel file...")
        df = pd.read_excel(destination_file, header=None)
        
        # Find the header row
        print("Locating header row...")
        header_row_idx = find_header_row(df)
        
        if header_row_idx is None:
            raise ValueError("Could not find header row with 'Member ID' and 'Guardian Name'")
        
        print(f"Header row found at index: {header_row_idx}")
        
        # Set the header row as column names and remove previous rows
        df.columns = df.iloc[header_row_idx]
        df = df.iloc[header_row_idx + 1:].reset_index(drop=True)
        
        # Save the cleaned file
        df.to_excel(destination_file, index=False)
        print("Excel file cleaned and saved")

        # Process the data - group by Guardian Name and Address (ORIGINAL LOGIC)
        print("Processing member data...")
        guardian_col = df.columns[7]  # Column H - Guardian Name
        address_col = df.columns[12]  # Column M - Address
        result = df.copy()
        grouped = df.groupby([guardian_col, address_col])

        print(f"Found {len(grouped)} unique household groups")

        # Create ID columns for each household - ORIGINAL FORMAT but clean
        for name, group in grouped:
            member_data = group[df.columns[[4, 5, 6]]].apply(
                lambda x: f"{clean_text_for_csv(x[df.columns[6]])} {clean_text_for_csv(x[df.columns[5]])}, {clean_text_for_csv(x[df.columns[4]])}", 
                axis=1
            ).tolist()
            
            for i, data in enumerate(member_data, 1):
                col_name = f'ID{i}'
                mask = (result[guardian_col] == name[0]) & (result[address_col] == name[1])
                if col_name not in result.columns:
                    result[col_name] = ''
                # Store clean data
                result.loc[mask, col_name] = data

        # Save intermediate processed data
        print("Saving processed data...")
        output_path = os.path.join(output_dir, 'processed_data.xlsx')
        result.to_excel(output_path, index=False)

        # Create final deduplicated CSV
        print("Creating final CSV output...")
        processed_df = pd.read_excel(output_path)
        deduped_df = processed_df.drop_duplicates(subset=[guardian_col])
        deduped_df = deduped_df.loc[:, ~deduped_df.columns.str.contains('Unnamed', case=False)]
        
        # Add Full Name column using the Guardian Name
        deduped_df['Full Name'] = deduped_df[guardian_col].apply(clean_for_no_quotes_csv)
        
        # Clean all text columns for quote-free export
        for col in deduped_df.columns:
            if deduped_df[col].dtype == 'object':
                deduped_df[col] = deduped_df[col].apply(clean_for_no_quotes_csv)
        
        # Save CSV with NO QUOTES and safe escaping
        csv_path = os.path.join(output_dir, 'FHK_TERM.csv')
        deduped_df.to_csv(csv_path, index=False, quoting=3, escapechar='\\')
        
        # Clean up intermediate file only if CSV creation succeeded
        try:
            os.remove(output_path)
            print("Cleaned up intermediate processed_data.xlsx file")
        except Exception as e:
            print(f"Note: Could not remove intermediate file: {str(e)}")
        
        # Delete the original file from Downloads folder
        os.remove(input_file)
        print(f"Deleted original file from Downloads: {os.path.basename(input_file)}")
        
        # Stop loading animation
        loading_complete = True
        animation_thread.join()
        
        # Final status
        record_count = len(deduped_df)
        print(f"\nPROCESSING COMPLETE!")
        print(f"Final record count: {record_count} households")
        print(f"Output files created:")
        print(f"  - {destination_file}")
        print(f"  - {csv_path}")
        print(f"CSV created with original ID1, ID2, ID3 format but NO quotes")
        
        # Give GOJI time to capture all output
        time.sleep(2)

    except Exception as e:
        # Stop loading animation on error
        loading_complete = True
        if 'animation_thread' in locals():
            animation_thread.join()
        
        error_msg = f"Error processing file: {str(e)}"
        print(f"\n{error_msg}")
        
        # Log the error details for debugging
        print("Error details:")
        import traceback
        traceback.print_exc()
        
        # Exit with error code for GOJI to detect
        sys.exit(1)

def main():
    """Main entry point for GOJI integration"""
    print("Starting TERM First Step Processing...")
    print("Script: 01TERMFIRSTSTEP.py")
    print("Target: GOJI integration")
    print("-" * 50)
    
    try:
        process_term_file()
        print("-" * 50)
        print("TERM First Step completed successfully")
        
    except KeyboardInterrupt:
        print("\nScript interrupted by user")
        sys.exit(1)
    except Exception as e:
        print(f"\nUnexpected error: {str(e)}")
        sys.exit(1)

if __name__ == "__main__":
    # Global variable for loading animation control
    loading_complete = False
    main()

