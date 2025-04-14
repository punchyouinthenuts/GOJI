import os
import pandas as pd
import glob

# Input and output paths
input_folder = r"C:\Program Files\Goji\RAC\LOYALTY\JOB\OUTPUT"
output_folder = r"C:\Program Files\Goji\RAC\LOYALTY\JOB\PROOF"

# Find CSV file in input folder
csv_files = glob.glob(os.path.join(input_folder, "*.csv"))

if csv_files:
    # Process the first CSV found
    input_file = csv_files[0]
    
    # Read the CSV
    df = pd.read_csv(input_file)
    
    # Sort by Cell_Cd column
    df_sorted = df.sort_values(by='Cell_Cd')
    
    # Create output filename
    input_filename = os.path.basename(input_file)
    output_filename = os.path.splitext(input_filename)[0] + "-PD.csv"
    output_path = os.path.join(output_folder, output_filename)
    
    # Save sorted DataFrame to new CSV
    df_sorted.to_csv(output_path, index=False)
    print(f"Sorted file saved as: {output_path}")
else:
    print("No CSV files found in the input folder")
