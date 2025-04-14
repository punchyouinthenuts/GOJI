import os
import pandas as pd
import random
import time
import logging
from datetime import timedelta
from pathlib import Path
from typing import List, Dict, Optional, Callable
import re

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('processing.log'),
        logging.StreamHandler()
    ]
)

class DataProcessor:
    def __init__(self):
        self.base_path = Path(r"C:\Users\JCox\Desktop\AUTOMATION\RAC")
        self.paths = {
            'CBC': self.base_path / "CBC" / "JOB",
            'EXC': self.base_path / "EXC" / "JOB",
            'INACTIVE': self.base_path / "INACTIVE_2310-DM07" / "FOLDERS",
            'NCWO': self.base_path / "NCWO_4TH" / "DM03",
            'PREPIF': self.base_path / "PREPIF" / "FOLDERS",
            'PPWK': Path(r"C:\Users\JCox\Desktop\PPWK Temp")
        }
        self.encodings = ['ISO-8859-1', 'latin1', 'utf-8']
        self.job_info = self.get_job_info()

    def get_job_info(self) -> tuple:
        while True:
            try:
                user_input = input("ENTER CBC JOB AND WEEK NUMBER (e.g., 12345 01.15): ")
                pattern = r'^\d{5}\s+\d{2}\.\d{2}$'
                
                if not re.match(pattern, user_input):
                    raise ValueError("Invalid format. Use: 5 digits, space, 2 digits, period, 2 digits")
                
                job_num, week = user_input.split()
                month, day = week.split('.')
                
                if not (1 <= int(month) <= 12 and 1 <= int(day) <= 31):
                    raise ValueError("Invalid month or day")
                
                logging.info(f"Job info set: {job_num} {week}")
                return job_num, week
            except ValueError as e:
                logging.error(f"Input error: {e}")
                print(f"Error: {e}. Please try again.")

    def safe_read_csv(self, file_path: Path) -> pd.DataFrame:
        for encoding in self.encodings:
            try:
                return pd.read_csv(file_path, encoding=encoding)
            except UnicodeDecodeError:
                continue
        raise ValueError(f"Unable to read {file_path} with any supported encoding")

    def format_currency(self, value):
        """Central currency formatting method for all sections"""
        return f"{float(value):,.2f}" if pd.notnull(value) else value
        
    def format_currency(self, value):
        """Enhanced currency formatting method"""
        try:
            return '{:.2f}'.format(float(value)) if pd.notnull(value) else value
        except (ValueError, TypeError):
            return value

    def validate_input_files(self):
        required_files = {
            'CBC': ['CBC2_WEEKLY.csv', 'CBC3_WEEKLY.csv'],
            'EXC': ['EXC_OUTPUT.csv'],
            'INACTIVE': ['A-PO.csv', 'A-PU.csv'],
            'NCWO': ['1-A_OUTPUT.csv', '2-A_OUTPUT.csv', '1-AP_OUTPUT.csv', '2-AP_OUTPUT.csv'],
            'PREPIF': ['PRE_PIF.csv']
        }

    def validate_creative_version(self, df: pd.DataFrame, expected_versions: List[str]) -> bool:
        actual_versions = df['Creative_Version_Cd'].unique()
        missing_versions = [v for v in expected_versions if v not in actual_versions]
        if missing_versions:
            logging.warning(f"Missing versions: {missing_versions}")
        return all(version in actual_versions for version in expected_versions)

    def process_with_progress(self, name: str, func: Callable):
        logging.info(f"Starting {name}...")
        print(f"\n{'='*20}")
        print(f"Starting {name}...")
        start_time = time.time()
        func()
        duration = time.time() - start_time
        print(f"Completed {name} in {duration:.2f} seconds")
        print(f"{'='*20}\n")
        logging.info(f"Completed {name} in {duration:.2f} seconds")
        
    def process_cbc(self):
        cbc_path = self.paths['CBC']
        logging.info("Starting CBC processing")
        
        # Process CBC2
        cbc2_input = cbc_path / "OUTPUT" / "CBC2_WEEKLY.csv"
        cbc2_output = cbc_path / "OUTPUT" / "CBC2WEEKLYREFORMAT.csv"
        cbc2_df = self.safe_read_csv(cbc2_input)
        cbc2_df['CUSTOM_03'] = pd.to_numeric(cbc2_df['CUSTOM_03'], errors='coerce')
        cbc2_df['CUSTOM_03'] = cbc2_df['CUSTOM_03'].apply(self.format_currency)
        cbc2_df.to_csv(cbc2_output, index=False, float_format='%.2f')
        logging.info("CBC2 processing completed")
        
        # Process CBC3
        cbc3_input = cbc_path / "OUTPUT" / "CBC3_WEEKLY.csv"
        cbc3_output = cbc_path / "OUTPUT" / "CBC3WEEKLYREFORMAT.csv"
        cbc3_df = self.safe_read_csv(cbc3_input)
        cbc3_df['CUSTOM_03'] = pd.to_numeric(cbc3_df['CUSTOM_03'], errors='coerce')
        cbc3_df['CUSTOM_03'] = cbc3_df['CUSTOM_03'].apply(self.format_currency)
        cbc3_df.to_csv(cbc3_output, index=False, float_format='%.2f')
        logging.info("CBC3 processing completed")

        # Generate proof data
        proof_path = cbc_path / "PROOF"
        proof_path.mkdir(exist_ok=True)
        
        for version in ['CBC2', 'CBC3']:
            df = self.safe_read_csv(cbc_path / "OUTPUT" / f"{version}WEEKLYREFORMAT.csv")
            
            # Process in chunks using groupby
            result = []
            for creative_version, group in df.groupby('Creative_Version_Cd'):
                licensed = group[group['Store_License'].notna()].head(1)
                unlicensed = group[group['Store_License'].isna()]
                version_records = pd.concat([licensed, unlicensed]).head(15)
                result.append(version_records)
            
            final_df = pd.concat(result)
            final_df.to_csv(proof_path / f"{version}WEEKLYREFORMAT-PD.csv", index=False, float_format='%.2f')
            logging.info(f"Generated proof data for {version}")

    def process_exc(self):
        exc_path = self.paths['EXC']
        input_file = exc_path / "OUTPUT" / "EXC_OUTPUT.csv"
        proof_path = exc_path / "PROOF"
        proof_path.mkdir(exist_ok=True)
        
        logging.info("Starting Exchange file processing")
        df = self.safe_read_csv(input_file)
        
        # Process RACXW-A version
        racxw_a_df = df[df['Creative_Version_Cd'] == 'RAC2406-DM03-RACXW-A']
        if not racxw_a_df.empty:
            racxw_a_with_license = racxw_a_df[racxw_a_df['Store_License'].notna()]
            if len(racxw_a_with_license) > 0:
                selected_racxw_a_df = pd.concat([racxw_a_with_license.head(1), racxw_a_df.head(14)])
            else:
                selected_racxw_a_df = racxw_a_df.head(15)
        
        # Process RACXW-PR version
        racxw_pr_df = df[df['Creative_Version_Cd'] == 'RAC2406-DM03-RACXW-PR']
        selected_racxw_pr_df = racxw_pr_df.head(15)
        
        # Combine and save proof data
        final_proof = pd.concat([selected_racxw_a_df, selected_racxw_pr_df])
        final_proof.to_csv(proof_path / "EXC_PROOF_DATA.csv", index=False)
        logging.info("Exchange processing completed")

    def process_inactive(self):
        inactive_path = self.paths['INACTIVE']
        logging.info("Starting Inactive processing")
        self._process_inactive_po(inactive_path)
        self._process_inactive_pu(inactive_path)
        logging.info("Inactive processing completed")

    def _process_inactive_po(self, base_path: Path):
        input_file = base_path / "OUTPUT" / "A-PO.csv"
        df = self.safe_read_csv(input_file)
        
        # Process PR records
        df_pr = df[df['Creative_Version_Cd'].str.contains('PR', na=False)].copy()
        if not df_pr.empty:
            df_pr['MESSAGE'] = "La oferta es válida hasta el "
            df_pr['EXPDATE'] = df_pr['EXPIRES'].str[29:]
            df_pr['EXPIRES'] = df_pr['MESSAGE'] + df_pr['EXPDATE']
            df_pr['SZIP'] = df_pr['SZIP'].apply(lambda x: str(x).zfill(5) if pd.notna(x) else '00000')
            
            df_pr.to_csv(base_path / "OUTPUT" / "PR-PO.csv", index=False, encoding='latin1')
            self._generate_proof_data_inactive(df_pr, base_path / "PROOF" / "PD-PR-PO.csv")
            logging.info("Processed PR-PO records")
        
        # Process VERSION records
        df = df[~df.index.isin(df_pr.index)]
        df_version = df[df['VERSION'].notna()].copy()
        if not df_version.empty:
            df_version.to_csv(base_path / "OUTPUT" / "FZAPO.csv", index=False, encoding='latin1', float_format='%.2f')
            self._generate_proof_data_inactive(df_version, base_path / "PROOF" / "FZAPO_PD.csv")
            logging.info("Processed FZAPO records")
        
        # Process remaining records
        df_remaining = df[~df.index.isin(df_version.index)]
        if not df_remaining.empty:
            df_remaining.sort_values('Sort Position', ascending=True).to_csv(
                base_path / "OUTPUT" / "A-PO.csv", index=False, encoding='latin1', float_format='%.2f')
            self._generate_proof_data_inactive(
                df_remaining, 
                base_path / "PROOF" / "A-PO-PD.csv", 
                store_license_required=True
            )
            logging.info("Processed remaining PO records")

    def _process_inactive_pu(self, base_path: Path):
        input_file = base_path / "OUTPUT" / "A-PU.csv"
        df = self.safe_read_csv(input_file)
        
        # Format fields
        df['Full Name'] = df['Full Name'].str.upper()
        
        # Process PR records
        df_pr = df[df['Creative_Version_Cd'].str.contains('PR', na=False)].copy()
        if not df_pr.empty:
            df_pr['MESSAGE'] = "La oferta es válida hasta el "
            df_pr['EXPDATE'] = df_pr['EXPIRES'].str[29:]
            df_pr['EXPIRES'] = df_pr['MESSAGE'] + df_pr['EXPDATE']
            df_pr['SZIP'] = df_pr['SZIP'].apply(lambda x: str(x).zfill(5) if pd.notna(x) else '00000')
            
            df_pr.to_csv(base_path / "OUTPUT" / "PR-PU.csv", index=False, encoding='latin1')
            self._generate_proof_data_inactive(df_pr, base_path / "PROOF" / "PD-PR-PU.csv")
            logging.info("Processed PR-PU records")
        
        # Process VERSION records
        df = df[~df.index.isin(df_pr.index)]
        df_version = df[df['VERSION'].notna()].copy()
        if not df_version.empty:
            df_version.to_csv(base_path / "OUTPUT" / "FZAPU.csv", index=False, encoding='latin1', float_format='%.2f')
            self._generate_proof_data_inactive(df_version, base_path / "PROOF" / "FZAPU_PD.csv")
            logging.info("Processed FZAPU records")
        
        # Process remaining records
        df_remaining = df[~df.index.isin(df_version.index)]
        if not df_remaining.empty:
            df_remaining.sort_values('Sort Position', ascending=True).to_csv(
                base_path / "OUTPUT" / "A-PU.csv", index=False, encoding='latin1', float_format='%.2f')
            self._generate_proof_data_inactive(
                df_remaining, 
                base_path / "PROOF" / "A-PU-PD.csv", 
                store_license_required=True
            )
            logging.info("Processed remaining PU records")

    def _generate_proof_data_inactive(self, df: pd.DataFrame, output_path: Path, store_license_required: bool = False):
        if len(df) <= 15:
            proof_df = df
        else:
            if store_license_required and df['Store_License'].notna().any():
                licensed = df[df['Store_License'].notna()].sample(1)
                remaining = df[~df.index.isin(licensed.index)].sample(14)
                proof_df = pd.concat([licensed, remaining])
            else:
                proof_df = df.sample(15)
        
        proof_df.to_csv(output_path, index=False, encoding='latin1', float_format='%.2f')
        logging.info(f"Generated proof data: {output_path}")

    def process_ncwo(self):
        ncwo_path = self.paths['NCWO']
        logging.info("Starting NCWO processing")
        self._process_ncwo_1a(ncwo_path)
        self._process_ncwo_2a(ncwo_path)
        self._process_ncwo_1ap(ncwo_path)
        self._process_ncwo_2ap(ncwo_path)
        logging.info("NCWO processing completed")

    def _process_ncwo_1a(self, base_path: Path):
        input_file = base_path / "OUTPUT" / "1-A_OUTPUT.csv"
        df = self.safe_read_csv(input_file)
        
        df['FIRST_NAME'] = df['First Name'].str.upper()
        
        for version in ['1-A', '1-PR']:
            version_df = df[df['VERSION'] == version].sort_values('Sort Position')
            
            if version == '1-PR':
                version_df['START_DATE'] = version_df['START_DATE'].str.replace('/', ' de ')
                version_df['END_DATE'] = version_df['END_DATE'].str.replace('/', ' de ')
            
            version_df.to_csv(base_path / "OUTPUT" / f"{version}.csv", index=False)
            
            if version == '1-A':
                self._generate_proof_data_ncwo(version_df, base_path / "PROOF" / f"{version}-PD.csv", True)
            else:
                self._generate_proof_data_ncwo(version_df, base_path / "PROOF" / f"{version}-PD.csv")
            logging.info(f"Processed NCWO {version}")

    def _process_ncwo_2a(self, base_path: Path):
        input_file = base_path / "OUTPUT" / "2-A_OUTPUT.csv"
        df = self.safe_read_csv(input_file)
        
        df['FIRST_NAME'] = df['First Name'].str.upper()
        
        for version in ['2-A', '2-PR']:
            version_df = df[df['VERSION'] == version].sort_values('Sort Position')
            
            if version == '2-PR':
                version_df['START_DATE'] = version_df['START_DATE'].str.replace('/', ' de ')
                version_df['END_DATE'] = version_df['END_DATE'].str.replace('/', ' de ')
            
            version_df.to_csv(base_path / "OUTPUT" / f"{version}.csv", index=False)
            
            if version == '2-A':
                self._generate_proof_data_ncwo(version_df, base_path / "PROOF" / f"{version}-PD.csv", True)
            else:
                self._generate_proof_data_ncwo(version_df, base_path / "PROOF" / f"{version}-PD.csv")
            logging.info(f"Processed NCWO {version}")

    def _process_ncwo_1ap(self, base_path: Path):
        input_file = base_path / "OUTPUT" / "1-AP_OUTPUT.csv"
        df = self.safe_read_csv(input_file)
        
        df['FIRST_NAME'] = df['First Name'].str.upper()
        
        for version in ['1-AP', '1-APPR']:
            version_df = df[df['VERSION'] == version].sort_values('Sort Position')
            
            if version == '1-APPR':
                version_df['START_DATE'] = version_df['START_DATE'].str.replace('/', ' de ')
                version_df['END_DATE'] = version_df['END_DATE'].str.replace('/', ' de ')
            
            version_df.to_csv(base_path / "OUTPUT" / f"{version}.csv", index=False, encoding='latin1')
            
            if version == '1-AP':
                self._generate_proof_data_ncwo(version_df, base_path / "PROOF" / f"{version}-PD.csv", True)
            else:
                self._generate_proof_data_ncwo(version_df, base_path / "PROOF" / f"{version}-PD.csv")
            logging.info(f"Processed NCWO {version}")

    def _process_ncwo_2ap(self, base_path: Path):
        input_file = base_path / "OUTPUT" / "2-AP_OUTPUT.csv"
        df = self.safe_read_csv(input_file)
        
        df['FIRST_NAME'] = df['First Name'].str.upper()
        
        for version in ['2-AP', '2-APPR']:
            version_df = df[df['VERSION'] == version].sort_values('Sort Position')
            
            if version == '2-APPR':
                version_df['START_DATE'] = version_df['START_DATE'].str.replace('/', ' de ')
                version_df['END_DATE'] = version_df['END_DATE'].str.replace('/', ' de ')
            
            version_df.to_csv(base_path / "OUTPUT" / f"{version}.csv", index=False, encoding='latin1')
            
            if version == '2-AP':
                self._generate_proof_data_ncwo(version_df, base_path / "PROOF" / f"{version}-PD.csv", True)
            else:
                self._generate_proof_data_ncwo(version_df, base_path / "PROOF" / f"{version}-PD.csv")
            logging.info(f"Processed NCWO {version}")

    def _generate_proof_data_ncwo(self, df: pd.DataFrame, output_path: Path, store_license_required: bool = False):
        if len(df) <= 15:
            proof_df = df
        else:
            if store_license_required and df['Store_License'].notna().any():
                licensed = df[df['Store_License'].notna()].sample(1)
                remaining = df[~df.index.isin(licensed.index)].sample(14)
                proof_df = pd.concat([licensed, remaining])
            else:
                proof_df = df.sample(15)
        
        proof_df.to_csv(output_path, index=False, encoding='latin1', float_format='%.2f')
        logging.info(f"Generated NCWO proof data: {output_path}")

    def process_prepif(self):
        prepif_path = self.paths['PREPIF']
        input_file = prepif_path / "OUTPUT" / "PRE_PIF.csv"
        logging.info("Starting PREPIF processing")
        
        df = self.safe_read_csv(input_file)
        
        # Calculate END DATE
        df['END DATE'] = pd.to_datetime(df['BEGIN DATE'], format='%m/%d/%Y') + timedelta(days=54)
        df['END DATE'] = df['END DATE'].dt.strftime('%m/%d/%Y')
        
        # Process PR records
        df_pr = df[df['Creative_Version_Cd'].str.contains('PR', na=False)].copy()
        if not df_pr.empty:
            df_pr['END DATE'] = pd.to_datetime(df_pr['END DATE']).dt.strftime('%d/%m/%Y')
            df_pr.to_csv(prepif_path / "OUTPUT" / "PRE_PIF-PR.csv", index=False, encoding='latin1')
            self._generate_proof_data_prepif(df_pr, prepif_path / "PROOF" / "PRE_PIF-PR-PD.csv")
            logging.info("Processed PREPIF PR records")
        
        # Process US records
        df_us = df[~df.index.isin(df_pr.index)]
        if not df_us.empty:
            df_us.to_csv(prepif_path / "OUTPUT" / "PRE_PIF-US.csv", index=False, encoding='latin1')
            self._generate_proof_data_prepif(df_us, prepif_path / "PROOF" / "PRE_PIF-US-PD.csv", True)
            logging.info("Processed PREPIF US records")

    def _generate_proof_data_prepif(self, df: pd.DataFrame, output_path: Path, store_license_required: bool = False):
        if len(df) <= 15:
            proof_df = df
        else:
            if store_license_required and df['Store_License'].notna().any():
                licensed = df[df['Store_License'].notna()].sample(1)
                remaining = df[~df.index.isin(licensed.index)].sample(14)
                proof_df = pd.concat([licensed, remaining])
            else:
                proof_df = df.sample(15)
        
        proof_df.to_csv(output_path, index=False, encoding='latin1', float_format='%.2f')
        logging.info(f"Generated PREPIF proof data: {output_path}")

    def process_ppwk_rename(self):
        ppwk_path = self.paths['PPWK']
        job_num, week = self.job_info
        logging.info("Starting PPWK renaming")
        
        # Define exact patterns to match (case-insensitive)
        patterns = [p.lower() for p in [
            'CBC2_PT.PDF', 'CBC2_TT.PDF', 'CBC2_PresortReport.PDF',
            'CBC3_PT.PDF', 'CBC3_TT.PDF', 'CBC3_PresortReport.PDF'
        ]]
        
        for file in ppwk_path.glob('*.[Pp][Dd][Ff]'):
            if file.name.lower() in patterns:
                new_name = f"{job_num} {week} {file.name}"
                new_path = file.parent / new_name
                file.rename(new_path)
                logging.info(f"Renamed {file.name} to {new_name}")

    def cleanup(self):
        """Clean up temporary files and perform final checks"""
        logging.info("Starting cleanup process...")
        
        # Verify output files exist
        for path_type in self.paths.values():
            output_dir = path_type / "OUTPUT"
            proof_dir = path_type / "PROOF"
            
            if output_dir.exists():
                for file in output_dir.glob("*.csv"):
                    if file.stat().st_size == 0:
                        logging.warning(f"Empty output file found: {file}")
            
            if proof_dir.exists():
                for file in proof_dir.glob("*.csv"):
                    if file.stat().st_size == 0:
                        logging.warning(f"Empty proof file found: {file}")
        
        logging.info("Cleanup completed")

def main():
    processor = DataProcessor()
    
    try:
        logging.info("Starting processing job")
        processor.validate_input_files()
        
        processes = [
            ("CBC Files", processor.process_cbc),
            ("Exchange Files", processor.process_exc),
            ("Inactive Files", processor.process_inactive),
            ("NCWO Files", processor.process_ncwo),
            ("PREPIF Files", processor.process_prepif),
            ("PPWK Renaming", processor.process_ppwk_rename)
        ]
        
        for name, func in processes:
            processor.process_with_progress(name, func)
            
        processor.cleanup()
        logging.info("All processing completed successfully")
        print("\nAll processing completed successfully.")
        
    except Exception as e:
        logging.error(f"Error during processing: {str(e)}")
        print(f"\nError during processing: {str(e)}")
        raise
    finally:
        logging.info("Processing job ended")

if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print(f"Fatal error: {str(e)}")
        logging.critical(f"Fatal error: {str(e)}")
        exit(1)
