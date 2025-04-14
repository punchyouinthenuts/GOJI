
import os
import pandas as pd

def process_files(input_dir, versions):
    # Retrieve all file names in the input directory
    file_names = [file for file in os.listdir(input_dir) if file.endswith('.txt')]

    for file_name in file_names:
        file_path = os.path.join(input_dir, file_name)
        try:
            # Read the file into a dataframe using tab as the delimiter
            df = pd.read_csv(file_path, sep='\t')

            # Iterate over each version
            for version in versions:
                # Filter records
                filtered_df = df[df.iloc[:, 6] == version]  # Assuming the 7th column (index 6) is the target

                # Save to a CSV file
                output_file = os.path.join(input_dir, f"{version}.csv")
                
                # Check if the file exists to determine if headers should be written
                file_exists = os.path.isfile(output_file)
                
                # Append data to CSV file, without headers if the file already exists
                filtered_df.to_csv(output_file, mode='a', index=False, header=not file_exists)
                
                print(f"Appended records for version {version} to {output_file}")
        except Exception as e:
            print(f"An error occurred while processing {file_path}: {e}")

if __name__ == "__main__":
    input_directory = r'C:\Program Files\Goji\RAC\SWEEPS\JOB\INPUT'
    versions = ['RAC2404-DM07-CBC2-A', 'RAC2404-DM07-CBC2-PR', 'RAC2404-DM07-CBC2-CANC']


    process_files(input_directory, versions)
