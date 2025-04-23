import os
import glob
import pandas as pd
import sys

def find_excel_file():
    print("Looking for Excel files in C:\\Users\\JCox\\Desktop\\AUTOMATION\\AIL\\INPUT...")
    excel_files = glob.glob("C:\\Users\\JCox\\Desktop\\AUTOMATION\\AIL\\INPUT\\*.xls*")
    if not excel_files:
        print("No Excel files found!")
        return None
    print(f"Found file: {excel_files[0]}")
    return excel_files[0]

def process_domestic_sheets(excel_file):
    print("Processing US and NEW YORK sheets...")
    
    # Define the header mappings
    header_mappings = {
        "First": "First Name",
        "Middle": "Middle Name",
        "Last": "Last Name",
        "Suffix": "Name Suffix",
        "SGA Agency Office": "Business",
        "Address": "Address Line 1",
        "City": "City",
        "State": "State",
        "Zip": "ZIP Code"
    }
    
    # Read the US sheet
    us_df = pd.read_excel(excel_file, sheet_name="US", header=None)
    # Find the header row (the one with "First")
    for i, row in us_df.iterrows():
        if row.iloc[0] == "First":
            header_row = i
            break
    # Set the header row and remove it from data
    us_df.columns = us_df.iloc[header_row]
    us_df = us_df.iloc[header_row+1:].reset_index(drop=True)
    
    # Read the NEW YORK sheet
    ny_df = pd.read_excel(excel_file, sheet_name="NEW YORK", header=None)
    # Find the header row
    for i, row in ny_df.iterrows():
        if row.iloc[0] == "First":
            header_row = i
            break
    # Set the header row and remove it from data
    ny_df.columns = ny_df.iloc[header_row]
    ny_df = ny_df.iloc[header_row+1:].reset_index(drop=True)
    
    # Rename columns in both dataframes
    us_df = us_df.rename(columns=header_mappings)
    ny_df = ny_df.rename(columns=header_mappings)
    
    # Combine the dataframes
    combined_df = pd.concat([us_df, ny_df], ignore_index=True)
    
    # Reorder columns to match the desired output
    column_order = [
        "First Name", "Middle Name", "Last Name", "Name Suffix", 
        "Business", "Address Line 1", "City", "State", "ZIP Code"
    ]
    combined_df = combined_df[column_order]
    
    # Save to CSV
    output_path = os.path.join("C:\\Users\\JCox\\Desktop\\AUTOMATION\\AIL\\INPUT", "AIL DOMESTIC.csv")
    combined_df.to_csv(output_path, index=False)
    print(f"Domestic data saved to {output_path}")
    
    return output_path

def process_international_sheets(excel_file):
    print("Processing CANADA and NEW ZEALAND sheets...")
    
    # Define the header mappings for CANADA
    canada_mappings = {
        "First": "First Name",
        "Middle": "Middle Name",
        "Last": "Last Name",
        "Suffix": "Name Suffix",
        "SGA Agency Office": "Business",
        "Address": "Address Line 1",
        "City": "City",
        "Prov": "State",
        "Country": "Country",
        "Postal Code": "ZIP Code"
    }
    
    # Define the header mappings for NEW ZEALAND
    nz_mappings = {
        "First": "First Name",
        "Middle": "Middle Name",
        "Last": "Last Name",
        "Suffix": "Name Suffix",
        "SGA Agency Office": "Business",
        "Address": "Address Line 1",
        "Address 2": "Address Line 2",
        "Suburb": "City",
        "City": "State",
        "Postal Code": "ZIP Code",
        "Country": "Country"
    }
    
    # Read the CANADA sheet
    canada_df = pd.read_excel(excel_file, sheet_name="CANADA", header=None)
    # Find the header row
    for i, row in canada_df.iterrows():
        if row.iloc[0] == "First":
            header_row = i
            break
    # Set the header row and remove it from data
    canada_df.columns = canada_df.iloc[header_row]
    canada_df = canada_df.iloc[header_row+1:].reset_index(drop=True)
    
    # Read the NEW ZEALAND sheet
    nz_df = pd.read_excel(excel_file, sheet_name="NEW ZEALAND", header=None)
    # Find the header row
    for i, row in nz_df.iterrows():
        if row.iloc[0] == "First":
            header_row = i
            break
    # Set the header row and remove it from data
    nz_df.columns = nz_df.iloc[header_row]
    nz_df = nz_df.iloc[header_row+1:].reset_index(drop=True)
    
    # Rename columns
    canada_df = canada_df.rename(columns=canada_mappings)
    nz_df = nz_df.rename(columns=nz_mappings)
    
    # Define the final column order
    column_order = [
        "First Name", "Middle Name", "Last Name", "Name Suffix", 
        "Business", "Address Line 1", "Address Line 2", 
        "City", "State", "ZIP Code", "Country"
    ]
    
    # Add missing columns with empty values
    for col in column_order:
        if col not in canada_df.columns:
            canada_df[col] = ""
    
    # Reorder columns
    canada_df = canada_df[column_order]
    nz_df = nz_df[column_order]
    
    # Combine the dataframes
    combined_df = pd.concat([canada_df, nz_df], ignore_index=True)
    
    # Save to CSV
    output_path = os.path.join("C:\\Users\\JCox\\Desktop\\AUTOMATION\\AIL\\INPUT", "AIL INTERNATIONAL.csv")
    combined_df.to_csv(output_path, index=False)
    print(f"International data saved to {output_path}")
    
    return output_path

def main():
    print("Starting AIL Excel Processing Script")
    
    # Find the Excel file
    excel_file = find_excel_file()
    if not excel_file:
        print("No Excel file found. Exiting.")
        input("Press Enter to exit...")
        return
    
    # Process the domestic sheets
    domestic_csv = process_domestic_sheets(excel_file)
    
    # Process the international sheets
    international_csv = process_international_sheets(excel_file)
    
    print("\nCOMBINED CSV FILES COMPLETE! Press X to terminate...")
    
    # Wait for user to press X
    while True:
        user_input = input().strip().lower()
        if user_input == 'x':
            break
    
    print("Exiting script.")

if __name__ == "__main__":
    main()
