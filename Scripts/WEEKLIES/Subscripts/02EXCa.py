import os
import pandas as pd
import chardet
import shutil
import tempfile

def process_exc_file(input_file_path, output_file_path):
    try:
        # Detect the file encoding
        with open(input_file_path, 'rb') as file:
            result = chardet.detect(file.read())
            encoding = result['encoding']

        # Try reading the CSV file with the detected encoding
        data = pd.read_csv(input_file_path, encoding=encoding)

        # Filter for Creative_Version_Cd = 'RAC2406-DM03-RACXW-A'
        racxw_a_df = data[data['Creative_Version_Cd'] == 'RAC2406-DM03-RACXW-A']

        # Ensure at least one record has a value in Store_License
        racxw_a_with_license = racxw_a_df[racxw_a_df['Store_License'].notna()]
        if len(racxw_a_with_license) > 0:
            selected_racxw_a_df = pd.concat([racxw_a_with_license.head(1), racxw_a_df.head(14)])
        else:
            selected_racxw_a_df = racxw_a_df.head(15)

        # Filter for Creative_Version_Cd = 'RAC2406-DM03-RACXW-PR'
        racxw_pr_df = data[data['Creative_Version_Cd'] == 'RAC2406-DM03-RACXW-PR']

        # Get up to 15 records
        selected_racxw_pr_df = racxw_pr_df.head(15)

        # Concatenate the selected records
        final_df = pd.concat([selected_racxw_a_df, selected_racxw_pr_df])

        # Save the final DataFrame to the output CSV
        final_df.to_csv(output_file_path, index=False)

    except pd.errors.EmptyDataError:
        print(f"The file {input_file_path} is empty.")
        return False
    except Exception as e:
        print(f"An error occurred while processing the file: {str(e)}")
        return False

    return True

def main():
    # Load the CSV file with encoding handling
    input_file_path = r'C:\Users\JCox\Desktop\AUTOMATION\RAC\EXC\JOB\OUTPUT\EXC_OUTPUT.csv'
    output_file_path = r'C:\Users\JCox\Desktop\AUTOMATION\RAC\EXC\JOB\PROOF\EXC_PROOF_DATA.csv'

    # Create a temporary directory for rollback
    with tempfile.TemporaryDirectory() as temp_dir:
        try:
            # Copy the input file to the temporary directory
            temp_input_file = os.path.join(temp_dir, os.path.basename(input_file_path))
            shutil.copy2(input_file_path, temp_input_file)

            # Process the EXC file
            success = process_exc_file(temp_input_file, output_file_path)

            if success:
                print("EXC file processed successfully.")
            else:
                raise Exception("Failed to process the EXC file.")

        except Exception as e:
            print(f"An error occurred: {str(e)}")
            print("Rolling back changes...")

            # Delete the output file if it exists
            if os.path.exists(output_file_path):
                os.remove(output_file_path)

            # Restore the input file from the temporary directory
            shutil.copy2(temp_input_file, input_file_path)

            print("Rollback completed. The original file has been restored.")

if __name__ == "__main__":
    main()
