import pandas as pd
import os
import sys
from datetime import datetime
import csv

def print_progress(message, end='\r'):
    print(f"[{datetime.now().strftime('%H:%M:%S')}] {message}", end=end)
    if end == '\n':
        sys.stdout.flush()

def safe_read_csv(file_path):
    try:
        return pd.read_csv(file_path, low_memory=False)
    except Exception as e:
        print_progress(f"Error reading {os.path.basename(file_path)}: {str(e)}", end='\n')
        return None

def create_household_count():
    print_progress("Creating household count report...")
    
    base_path = r"C:\Users\JCox\Desktop\AUTOMATION\SISNEROS\CLEANUP"
    bulk_mailer_dir = os.path.join(base_path, "BULK MAILER OUTPUT")
    counts_dir = os.path.join(base_path, "COUNTS")
    
    # Create counts directory if it doesn't exist
    os.makedirs(counts_dir, exist_ok=True)
    
    # Get a clean list of CSV files and sort them
    csv_files = [f for f in os.listdir(bulk_mailer_dir) if f.endswith('.csv')]
    csv_files.sort()
    
    # Use proper CSV writer to handle quoting and commas
    with open(os.path.join(counts_dir, "FINALHHCOUNT.csv"), 'w', newline='') as csvfile:
        writer = csv.writer(csvfile, quoting=csv.QUOTE_MINIMAL)
        
        for file in csv_files:
            file_path = os.path.join(bulk_mailer_dir, file)
            df = safe_read_csv(file_path)
            if df is None:
                continue
            
            # Include 'Address Line 2' for household uniqueness
            address_cols = ['Address Line 1', 'Address Line 2', 'City', 'State', 'ZIP Code', 'congressional_district']
            address_cols = [col for col in address_cols if col in df.columns]
            
            # Add the filename as a heading (as a single-item row)
            writer.writerow([file])
            
            if 'congressional_district' in df.columns:
                # Sort and process each district
                districts = sorted([d for d in df['congressional_district'].dropna().unique() 
                                   if not pd.isna(d) and str(d).strip() != ''])
                
                for district in districts:
                    district_str = str(district).strip()
                    
                    district_df = df[df['congressional_district'] == district]
                    unique_households = district_df.drop_duplicates(subset=address_cols).shape[0]
                    
                    # Get state safely
                    state = 'Unknown'
                    if 'State' in df.columns and len(district_df) > 0:
                        states = district_df['State'].dropna().unique()
                        if len(states) > 0:
                            state = states[0]
                    
                    # Keep the thousands separator comma but write as single field
                    row_text = f"{unique_households:,} unique households for {state} District {district_str}"
                    writer.writerow([row_text])  # Write as single-item row
            else:
                # Just count total unique households
                unique_households = df.drop_duplicates(subset=address_cols).shape[0]
                row_text = f"{unique_households:,} unique households"
                writer.writerow([row_text])  # Write as single-item row
            
            # Add an empty line after each file's report
            writer.writerow([""])
            
        print_progress(f"Household count report successfully saved to {os.path.join(counts_dir, 'FINALHHCOUNT.csv')}", end='\n')

def main():
    try:
        create_household_count()
        
        print("\nHOUSEHOLD COUNT REPORT SUCCESSFULLY CREATED")
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