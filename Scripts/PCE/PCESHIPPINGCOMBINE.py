import pandas as pd
import os

# Set the working directory
input_path = r'C:\Program Files\Goji\RAC\PCE\DATA'

# Define files and their multiplication factors
file_multipliers = {
    'BI_75.csv': 75,
    'BI_100.csv': 100,
    'BI_150.csv': 150,
    'BI_300.csv': 300,
    'BI_400.csv': 400,
    'English_75.csv': 75,
    'English_100.csv': 100,
    'English_300.csv': 300
}

# Process each file
for filename, multiplier in file_multipliers.items():
    file_path = os.path.join(input_path, filename)
    
    if os.path.exists(file_path):
        # Read the CSV file with no header
        df = pd.read_csv(file_path, header=None, skiprows=1)
        
        # Remove columns A, B, G, H, I, J (0, 1, 6, 7, 8, 9 in zero-based indexing)
        columns_to_drop = [0, 1, 6, 7, 8, 9]
        df = df.drop(df.columns[columns_to_drop], axis=1)
        
        # Multiply rows
        df_multiplied = pd.DataFrame(df.values.repeat(multiplier, axis=0), columns=df.columns)
        
        # Save and overwrite the original file without header
        df_multiplied.to_csv(file_path, index=False, header=False)
        
        print(f"Successfully processed {filename} - removed header, columns, and multiplied rows {multiplier} times!")
    else:
        print(f"File not found: {filename}")

print("\nAll files have been processed successfully!")

# Define the order of files for combination
file_order = [
    'BI_75.csv',
    'BI_100.csv',
    'BI_150.csv',
    'BI_300.csv',
    'BI_400.csv',
    'English_75.csv',
    'English_100.csv',
    'English_300.csv'
]

# Combine files in specified order
combined_df = pd.DataFrame()
for filename in file_order:
    file_path = os.path.join(input_path, filename)
    if os.path.exists(file_path):
        temp_df = pd.read_csv(file_path, header=None)
        combined_df = pd.concat([combined_df, temp_df], ignore_index=True)
        print(f"Added {filename} to combined file")

# Remove empty rows
combined_df = combined_df.dropna(how='all')
print("Successfully removed empty rows from combined data")

# Add "BM" to column E (index 4) for the last row of each unique group
combined_df['group'] = combined_df.groupby([2, 3]).cumcount()
last_rows = combined_df.groupby([2, 3])['group'].transform('max') == combined_df['group']
combined_df.loc[last_rows, 4] = 'BM'
combined_df = combined_df.drop('group', axis=1)

# Add header row
header = ['Address Line 1', 'City', 'State', 'ZIP Code', 'Break Mark']
combined_df.columns = header

# Save combined file with header
combined_file_path = os.path.join(input_path, 'PCE_COMBINE.csv')
combined_df.to_csv(combined_file_path, index=False)

# Delete all input CSV files
for filename in file_order:
    file_path = os.path.join(input_path, filename)
    if os.path.exists(file_path):
        os.remove(file_path)
        print(f"Deleted input file: {filename}")

print(f"\nSuccessfully created combined file: PCE_COMBINE.csv")
print("All input CSV files have been deleted")
