import os
import re
import sys
import time
import pandas as pd
from pathlib import Path

def get_job_info():
    attempts = 0
    while attempts < 5:
        user_input = input("INPUT CBC JOB NUMBER AND WEEK: ")
        try:
            job_num, week = user_input.split()
            if len(job_num) != 5 or not job_num.isdigit():
                raise ValueError("JOB NUMBER REQUIRES FIVE DIGITS")
            if not re.match(r'\d{2}\.\d{2}', week):
                raise ValueError("WEEK NUMBER NEEDS TO BE FORMATTED LIKE 00.00")
            return job_num, week
        except ValueError as e:
            print(str(e))
            attempts += 1
    print("MAX ATTEMPTS MADE, SCRIPT TERMINATING")
    for i in range(5, 0, -1):
        print(i)
        time.sleep(1)
    sys.exit(1)

def process_cbc_file(file_path, output_path):
    try:
        data = pd.read_csv(file_path, encoding='ISO-8859-1')
        if 'CUSTOM_03' not in data.columns:
            raise KeyError("CUSTOM_03 column not found in the CSV file")
        data['CUSTOM_03'] = data['CUSTOM_03'].apply(lambda x: f"{float(x):,.2f}" if pd.notnull(x) else x)
        data.to_csv(output_path, index=False)
        return True
    except (UnicodeDecodeError, ValueError, KeyError) as e:
        print(f"Error processing {file_path}: {str(e)}")
        return False

def generate_proof_data(input_path, output_path):
    try:
        df = pd.read_csv(input_path)
        result = []
        for version, group in df.groupby('Creative_Version_Cd'):
            licensed = group[group['Store_License'].notna()].head(1)
            unlicensed = group[group['Store_License'].isna()]
            version_records = pd.concat([licensed, unlicensed]).head(15)
            result.append(version_records)
        final_df = pd.concat(result)
        final_df.to_csv(output_path, index=False)
        return True
    except Exception as e:
        print(f"Error generating proof data for {input_path}: {str(e)}")
        return False

def rename_ppwk_files(folder_path, prefix):
    target_files = [
        "CBC2_PresortReport.PDF",
        "CBC2_PT.PDF",
        "CBC2_TT.PDF",
        "CBC3_PresortReport.PDF",
        "CBC3_PT.PDF",
        "CBC3_TT.PDF"
    ]
    for filename in os.listdir(folder_path):
        if filename in target_files:
            old_path = os.path.join(folder_path, filename)
            new_filename = f"{prefix}_{filename}"
            new_path = os.path.join(folder_path, new_filename)
            os.rename(old_path, new_path)

def main():
    job_num, week = get_job_info()
    prefix = f"{job_num}_{week}"

    cbc2_input = r'C:\Users\JCox\Desktop\AUTOMATION\RAC\CBC\JOB\OUTPUT\CBC2_WEEKLY.csv'
    cbc2_output = r'C:\Users\JCox\Desktop\AUTOMATION\RAC\CBC\JOB\OUTPUT\CBC2WEEKLYREFORMAT.csv'
    cbc3_input = r'C:\Users\JCox\Desktop\AUTOMATION\RAC\CBC\JOB\OUTPUT\CBC3_WEEKLY.csv'
    cbc3_output = r'C:\Users\JCox\Desktop\AUTOMATION\RAC\CBC\JOB\OUTPUT\CBC3WEEKLYREFORMAT.csv'

    cbc2_proof_output = r'C:\Users\JCox\Desktop\AUTOMATION\RAC\CBC\JOB\PROOF\CBC2WEEKLYREFORMAT-PD.csv'
    cbc3_proof_output = r'C:\Users\JCox\Desktop\AUTOMATION\RAC\CBC\JOB\PROOF\CBC3WEEKLYREFORMAT-PD.csv'

    ppwk_folder = r"C:\Users\JCox\Desktop\PPWK Temp"

    try:
        if not process_cbc_file(cbc2_input, cbc2_output):
            raise Exception("Error processing CBC2_WEEKLY.csv")
        if not process_cbc_file(cbc3_input, cbc3_output):
            raise Exception("Error processing CBC3_WEEKLY.csv")

        if not generate_proof_data(cbc2_output, cbc2_proof_output):
            raise Exception("Error generating proof data for CBC2")
        if not generate_proof_data(cbc3_output, cbc3_proof_output):
            raise Exception("Error generating proof data for CBC3")

        rename_ppwk_files(ppwk_folder, prefix)

        print("ALL CBC FILES SUCCESSFULLY PROCESSED!")

    except Exception as e:
        print(f"Error: {str(e)}")
        input("Press any key to terminate the script...")
        # Rollback changes (not implemented in this example)

if __name__ == "__main__":
    main()
