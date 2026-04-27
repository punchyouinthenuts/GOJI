import os
import pandas as pd
import sys

CANONICAL_TM_ROOT = r"C:\Goji\AUTOMATION\TRACHMAR"

def resolve_tm_root():
    if os.path.isdir(CANONICAL_TM_ROOT):
        return CANONICAL_TM_ROOT
    os.makedirs(CANONICAL_TM_ROOT, exist_ok=True)
    print("INFO: created canonical TRACHMAR root C:\\Goji\\AUTOMATION\\TRACHMAR.")
    return CANONICAL_TM_ROOT

def main():
    # Get command line arguments from Goji
    if len(sys.argv) != 5:
        print("Error: Expected 4 arguments (job_number, drop_number, year, month)")
        return 1
    
    job_number = sys.argv[1]
    drop_number = sys.argv[2]
    year = sys.argv[3]
    month = sys.argv[4]
    
    print(f"Starting TM TARRAGON initial processing...")
    print(f"Job: {job_number}, Drop: {drop_number}, Year: {year}, Month: {month}")
    
    # Define Goji directory paths
    downloads_dir = r"C:\Users\JCox\Downloads"
    input_dir = os.path.join(resolve_tm_root(), "TARRAGON HOMES", "INPUT")
    output_filename = "TARRAGON INPUT.csv"
    
    # Define the columns to keep and their new names
    columns_to_keep = {
        'name': 'Full Name',
        'add1': 'Address Line 1',
        'city': 'City',
        'st': 'State',
        'zip': None,  # Will be used for ZIP Code
        'zip4': None  # Will be used for ZIP Code
    }

    # Function to create ZIP Code based on zip and zip4
    def create_zip_code(row):
        # Convert zip and zip4 to strings, remove any decimals
        zip_val = str(row['zip']).strip().split('.')[0]
        zip4_val = str(row['zip4']).strip().split('.')[0]
        
        # If zip is empty, NaN, or 'nan', return blank
        if pd.isna(row['zip']) or zip_val == '' or zip_val.lower() == 'nan':
            return ''
        
        # If zip4 is empty, NaN, or 'nan', return just zip
        if pd.isna(row['zip4']) or zip4_val == '' or zip4_val.lower() == 'nan':
            return zip_val
        
        # If both zip and zip4 have data, combine with hyphen
        return f"{zip_val}-{zip4_val}"

    # Function to process the CSV file
    def process_csv_file(file_path):
        print(f"Processing file: {os.path.basename(file_path)}")
        
        # Read the CSV file
        df = pd.read_csv(file_path)
        print(f"Loaded {len(df)} records")
        
        # Keep only the specified columns
        df = df[list(columns_to_keep.keys())]
        
        # Rename columns
        df.rename(columns={k: v for k, v in columns_to_keep.items() if v}, inplace=True)
        
        # Create ZIP Code column using the custom function
        df['ZIP Code'] = df.apply(create_zip_code, axis=1)
        
        # Drop zip and zip4 columns
        df.drop(['zip', 'zip4'], axis=1, inplace=True)
        
        # Ensure input directory exists
        os.makedirs(input_dir, exist_ok=True)
        print(f"Created/verified input directory: {input_dir}")
        
        # Create full output path
        output_path = os.path.join(input_dir, output_filename)
        
        # Save the processed DataFrame to a new CSV
        df.to_csv(output_path, index=False)
        print(f"Saved processed file: {output_path}")
        
        # Delete the original CSV file
        os.remove(file_path)
        print(f"Deleted original file: {file_path}")
        
        return True

    try:
        # Search for CSV and XLSX files with "Tarragon" in the name
        found_file = False
        for filename in os.listdir(downloads_dir):
            if 'Tarragon' in filename and (filename.endswith('.csv') or filename.endswith('.xlsx')):
                file_path = os.path.join(downloads_dir, filename)
                process_file = file_path
                is_xlsx = filename.endswith('.xlsx')
                
                if is_xlsx:
                    # Convert XLSX to temporary CSV
                    temp_csv_path = os.path.splitext(file_path)[0] + '.csv'
                    df = pd.read_excel(file_path)
                    df.to_csv(temp_csv_path, index=False)
                    process_file = temp_csv_path
                
                try:
                    process_csv_file(process_file)
                    if is_xlsx:
                        os.remove(file_path)  # Delete the original XLSX file
                    print("FILE SUCCESSFULLY PROCESSED!")
                    print("Data is ready for Bulk Mailer processing.")
                    found_file = True
                    break
                except Exception as e:
                    print(f"Error processing {filename}: {str(e)}")
                    if is_xlsx and os.path.exists(temp_csv_path):
                        os.remove(temp_csv_path)  # Clean up temp CSV on error
                    return 1
        
        if not found_file:
            print("Error: No CSV or XLSX file with 'Tarragon' in the name found in Downloads folder")
            return 1
            
    except Exception as e:
        print(f"Error: {str(e)}")
        return 1
    
    return 0

if __name__ == "__main__":
    exit_code = main()
    sys.exit(exit_code)

