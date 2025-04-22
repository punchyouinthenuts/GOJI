import sys
import os
import pandas as pd
import chardet

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
        print(f"The file {input_file_path} is empty.", file=sys.stderr)
        return False
    except Exception as e:
        print(f"An error occurred while processing the file: {str(e)}", file=sys.stderr)
        return False

    return True

def main():
    if len(sys.argv) != 5:
        print("Usage: 02EXCa.py <job_num> <week> <base_path> <exc_postage>", file=sys.stderr)
        sys.exit(1)

    job_num = sys.argv[1]
    week = sys.argv[2]
    base_path = sys.argv[3]
    exc_postage = sys.argv[4]

    print(f"Starting processing for EXC job {job_num}, week {week}")

    input_file_path = os.path.join(base_path, "JOB", "OUTPUT", "EXC_OUTPUT.csv")
    output_file_path = os.path.join(base_path, "JOB", "PROOF", "EXC_PROOF_DATA.csv")

    # Check if input file exists
    if not os.path.exists(input_file_path):
        print(f"Error: Input file {input_file_path} not found", file=sys.stderr)
        sys.exit(1)

    # Ensure output directory exists
    output_dir = os.path.dirname(output_file_path)
    os.makedirs(output_dir, exist_ok=True)

    success = process_exc_file(input_file_path, output_file_path)

    if success:
        print(f"Processed EXC job {job_num} for week {week}. Output written to {output_file_path}")
    else:
        print("Failed to process the EXC file.", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()