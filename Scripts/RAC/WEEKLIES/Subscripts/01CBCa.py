import os
import sys
import pandas as pd
import argparse
import re

def process_cbc_file(file_path, output_path):
    """
    Process a CBC CSV file by formatting the CUSTOM_03 column.
    
    Args:
        file_path (str): Path to the input CSV file
        output_path (str): Path to save the reformatted CSV
    
    Returns:
        bool: True if successful, False if an error occurs
    """
    try:
        data = pd.read_csv(file_path, encoding='ISO-8859-1')
        if 'CUSTOM_03' not in data.columns:
            raise KeyError("CUSTOM_03 column not found in the CSV file")
        data['CUSTOM_03'] = data['CUSTOM_03'].apply(lambda x: f"{float(x):,.2f}" if pd.notnull(x) else x)
        data.to_csv(output_path, index=False)
        return True
    except (UnicodeDecodeError, ValueError, KeyError) as e:
        print(f"Error processing {file_path}: {str(e)}", file=sys.stderr)
        return False

def generate_proof_data(input_path, output_path):
    """
    Generate proof data by grouping and filtering CSV data.
    
    Args:
        input_path (str): Path to the reformatted CSV file
        output_path (str): Path to save the proof data CSV
    
    Returns:
        bool: True if successful, False if an error occurs
    """
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
        print(f"Error generating proof data for {input_path}: {str(e)}", file=sys.stderr)
        return False

def rename_ppwk_files(folder_path, prefix):
    """
    Rename specific PDF files in the PPWK folder with a prefix, ensuring only one underscore after CBC2 or CBC3.
    
    Args:
        folder_path (str): Path to the PPWK Temp folder
        prefix (str): Prefix to add to filenames (e.g., '12345 04.22')
    
    Returns:
        bool: True if all renames succeed, False if an error occurs
    """
    target_files = [
        "CBC2_PresortReport.PDF",
        "CBC2_PT.PDF",
        "CBC2_TT.PDF",
        "CBC3_PresortReport.PDF",
        "CBC3_PT.PDF",
        "CBC3_TT.PDF"
    ]
    success = True
    for filename in os.listdir(folder_path):
        if filename in target_files:
            old_path = os.path.join(folder_path, filename)
            # Extract CBC2 or CBC3 and the rest of the filename
            cbc_part = filename[:4]  # e.g., "CBC2" or "CBC3"
            rest = filename[4:]      # e.g., "_PresortReport.PDF"
            new_filename = f"{prefix}_{cbc_part}{rest}"  # e.g., "12345 04.22_CBC2_PresortReport.PDF"
            new_path = os.path.join(folder_path, new_filename)
            try:
                os.rename(old_path, new_path)
                print(f"Renamed {filename} to {new_filename}")
            except Exception as e:
                print(f"Error renaming {filename}: {
