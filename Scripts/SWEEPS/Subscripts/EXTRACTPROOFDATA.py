import pandas as pd
import os
import logging
from typing import List
from pathlib import Path

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

def setup_file_paths(input_dir: str, output_dir: str, file_names: List[str]) -> List[tuple]:
    """Set up input and output file paths"""
    return [(
        Path(input_dir) / name,
        Path(output_dir) / name.replace('.csv', '-PD.csv')
    ) for name in file_names]

def process_and_save(file_path: Path, output_path: Path) -> bool:
    try:
        # Read all columns from CSV
        data = pd.read_csv(file_path)
        
        # Process each version
        result = []
        for version, group in data.groupby('Creative_Version_Cd'):
            # Get records with and without licenses
            licensed = group[group['Store_License'].notna()].head(1)
            unlicensed = group[group['Store_License'].isna()]
            
            # Combine and limit to 15 records
            version_records = pd.concat([licensed, unlicensed]).head(15)
            result.append(version_records)
        
        # Combine all results and save
        output_path.parent.mkdir(parents=True, exist_ok=True)
        final_data = pd.concat(result)
        final_data.to_csv(output_path, index=False)
        
        logging.info(f"Successfully processed data and saved to {output_path}")
        return True

    except Exception as e:
        logging.error(f"An error occurred: {str(e)}")
        return False

if __name__ == "__main__":
    # Configuration
    INPUT_DIR = Path(r'C:\Program Files\Goji\RAC\SWEEPS\JOB\OUTPUT')
    OUTPUT_DIR = Path(r'C:\Program Files\Goji\RAC\SWEEPS\JOB\PROOF')
    FILE_NAMES = ['SWEEPSREFORMAT.csv']

    # Process files
    for input_path, output_path in setup_file_paths(INPUT_DIR, OUTPUT_DIR, FILE_NAMES):
        process_and_save(input_path, output_path)
