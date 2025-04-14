import os
import pandas as pd
import random
from datetime import timedelta
from pathlib import Path
from typing import List, Dict, Optional

class DataProcessor:
    def __init__(self):
        self.base_path = Path(r"C:\Users\JCox\Desktop\AUTOMATION\RAC")
        self.paths = {
            'CBC': self.base_path / "CBC" / "JOB",
            'EXC': self.base_path / "EXC" / "JOB",  # Updated path
            'INACTIVE': self.base_path / "INACTIVE_2310-DM07" / "FOLDERS",
            'NCWO': self.base_path / "NCWO_4TH" / "DM03",
            'PREPIF': self.base_path / "PREPIF" / "FOLDERS"
        }
        self.encodings = ['ISO-8859-1', 'latin1', 'utf-8']
    def safe_read_csv(self, file_path: Path) -> pd.DataFrame:
        """Smart CSV reader that handles different encodings"""
        for encoding in self.encodings:
            try:
                return pd.read_csv(file_path, encoding=encoding)
            except UnicodeDecodeError:
                continue
        raise ValueError(f"Unable to read {file_path} with any supported encoding")

    def format_currency(self, value: str) -> str:
        """Standardized currency formatting"""
        try:
            cleaned = str(value).replace('$', '').replace(',', '')
            return f"{float(cleaned):,.2f}"
        except (ValueError, AttributeError):
            return value

    def format_date(self, date_str: str, spanish_format: bool = False) -> str:
        """Unified date formatting with Spanish option"""
        try:
            date = pd.to_datetime(date_str)
            if spanish_format:
                return date.strftime('%d/%m/%Y')
            return date.strftime('%m/%d/%Y')
        except:
            return date_str

    def generate_proof_data(self, df: pd.DataFrame, output_path: Path, store_license_required: bool = False, sample_size: int = 15) -> None:
        """Generate proof data with optional store license requirement"""
        if len(df) <= sample_size:
            proof_df = df
        else:
            if store_license_required:
                licensed = df[df['Store_License'].notna()].sample(min(1, len(df[df['Store_License'].notna()])))
                remaining_size = sample_size - len(licensed)
                remaining = df[~df.index.isin(licensed.index)].sample(remaining_size)
                proof_df = pd.concat([licensed, remaining])
            else:
                proof_df = df.sample(sample_size)
        
        proof_df.to_csv(output_path, index=False, encoding='latin1')

    def process_cbc(self):
        """Process CBC2 and CBC3 files with proof data generation"""
        cbc_path = self.paths['CBC']
        
        # Process CBC2
        cbc2_input = cbc_path / "OUTPUT" / "CBC2_WEEKLY.csv"
        cbc2_output = cbc_path / "OUTPUT" / "CBC2WEEKLYREFORMAT.csv"
        cbc2_df = self.safe_read_csv(cbc2_input)
        cbc2_df['CUSTOM_03'] = cbc2_df['CUSTOM_03'].apply(self.format_currency)
        cbc2_df.to_csv(cbc2_output, index=False)
        
        # Process CBC3
        cbc3_input = cbc_path / "OUTPUT" / "CBC3_WEEKLY.csv"
        cbc3_output = cbc_path / "OUTPUT" / "CBC3WEEKLYREFORMAT.csv"
        cbc3_df = self.safe_read_csv(cbc3_input)
        cbc3_df['CUSTOM_03'] = cbc3_df['CUSTOM_03'].apply(self.format_currency)
        cbc3_df.to_csv(cbc3_output, index=False)
        
        # Generate proof data
        proof_path = cbc_path / "PROOF"
        proof_path.mkdir(exist_ok=True)
        
        for prefix in ['CBC2', 'CBC3']:
            df = self.safe_read_csv(cbc_path / "OUTPUT" / f"{prefix}WEEKLYREFORMAT.csv")
            self.generate_proof_data(
                df, 
                proof_path / f"{prefix}WEEKLYREFORMAT-PD.csv",
                store_license_required=True
            )

    def process_exc(self):
        """Process Exchange files with specific version handling"""
        exc_path = self.paths['EXC']
        input_file = exc_path / "OUTPUT" / "EXC_OUTPUT.csv"
        proof_path = exc_path / "PROOF"
        proof_path.mkdir(exist_ok=True)
        
        # Read and process data
        df = self.safe_read_csv(input_file)
        
        # Process RACXW-A version
        racxw_a_df = df[df['Creative_Version_Cd'] == 'RAC2406-DM03-RACXW-A']
        if not racxw_a_df.empty:
            licensed = racxw_a_df[racxw_a_df['Store_License'].notna()].head(1)
            unlicensed = racxw_a_df[racxw_a_df['Store_License'].isna()]
            racxw_a_proof = pd.concat([licensed, unlicensed]).head(15)
        
        # Process RACXW-PR version
        racxw_pr_df = df[df['Creative_Version_Cd'] == 'RAC2406-DM03-RACXW-PR']
        racxw_pr_proof = racxw_pr_df.head(15)
        
        # Combine and save proof data
        final_proof = pd.concat([racxw_a_proof, racxw_pr_proof])
        final_proof.to_csv(proof_path / "EXC_PROOF_DATA.csv", index=False)
    def process_inactive(self):
        """Process INACTIVE PO and PU files"""
        inactive_path = self.paths['INACTIVE']
        
        # Process PO files
        self._process_inactive_po(inactive_path)
        # Process PU files
        self._process_inactive_pu(inactive_path)

    def _process_inactive_po(self, base_path: Path):
        """Handle INACTIVE PO processing"""
        input_file = base_path / "OUTPUT" / "A-PO.csv"
        df = self.safe_read_csv(input_file)
        
        # Process PR records first
        df_pr = df[df['Creative_Version_Cd'].str.contains('PR', na=False)].copy()
        if not df_pr.empty:
            df_pr['MESSAGE'] = "La oferta es válida hasta el "
            df_pr['EXPDATE'] = df_pr['EXPIRES'].str[29:]
            df_pr['EXPIRES'] = df_pr['MESSAGE'] + df_pr['EXPDATE']
            df_pr['SZIP'] = df_pr['SZIP'].apply(lambda x: str(x).zfill(5) if pd.notna(x) else '00000')
            
            # Save PR files
            df_pr.to_csv(base_path / "OUTPUT" / "PR-PO.csv", index=False, encoding='latin1')
            self.generate_proof_data(df_pr, base_path / "PROOF" / "PD-PR-PO.csv")
        
        # Remove PR records from main dataframe
        df = df[~df.index.isin(df_pr.index)]
        
        # Process VERSION records (FZAPO) from remaining non-PR records
        df_version = df[df['VERSION'].notna()].copy()
        if not df_version.empty:
            df_version.to_csv(base_path / "OUTPUT" / "FZAPO.csv", index=False, encoding='latin1')
            self.generate_proof_data(df_version, base_path / "PROOF" / "FZAPO_PD.csv")
        
        # Process remaining records
        df_remaining = df[~df.index.isin(df_version.index)]
        if not df_remaining.empty:
            df_remaining.sort_values('Sort Position', ascending=True).to_csv(
                base_path / "OUTPUT" / "A-PO.csv", index=False, encoding='latin1')
            self.generate_proof_data(df_remaining, base_path / "PROOF" / "A-PO-PD.csv", store_license_required=True)

    def _process_inactive_pu(self, base_path: Path):
        """Handle INACTIVE PU processing"""
        input_file = base_path / "OUTPUT" / "A-PU.csv"
        df = self.safe_read_csv(input_file)
        
        # Format common fields
        df['First Name'] = df['First Name'].str.upper()
        df['CREDIT'] = df['CREDIT'].apply(self.format_currency)
        
        # Process PR records first
        df_pr = df[df['Creative_Version_Cd'].str.contains('PR', na=False)].copy()
        if not df_pr.empty:
            df_pr['MESSAGE'] = "La oferta es válida hasta el "
            df_pr['EXPDATE'] = df_pr['EXPIRES'].str[29:]
            df_pr['EXPIRES'] = df_pr['MESSAGE'] + df_pr['EXPDATE']
            df_pr['SZIP'] = df_pr['SZIP'].apply(lambda x: str(x).zfill(5) if pd.notna(x) else '00000')
            
            # Save PR files
            df_pr.to_csv(base_path / "OUTPUT" / "PR-PU.csv", index=False, encoding='latin1')
            self.generate_proof_data(df_pr, base_path / "PROOF" / "PD-PR-PU.csv")
        
        # Remove PR records from main dataframe
        df = df[~df.index.isin(df_pr.index)]
        
        # Process VERSION records (FZAPU) from remaining non-PR records
        df_version = df[df['VERSION'].notna()].copy()
        if not df_version.empty:
            df_version.to_csv(base_path / "OUTPUT" / "FZAPU.csv", index=False, encoding='latin1')
            self.generate_proof_data(df_version, base_path / "PROOF" / "FZAPU_PD.csv")
        
        # Process remaining records
        df_remaining = df[~df.index.isin(df_version.index)]
        if not df_remaining.empty:
            df_remaining.sort_values('Sort Position', ascending=True).to_csv(
                base_path / "OUTPUT" / "A-PU.csv", index=False, encoding='latin1')
            self.generate_proof_data(df_remaining, base_path / "PROOF" / "A-PU-PD.csv", store_license_required=True)

    def process_ncwo(self):
        """Process all NCWO files"""
        ncwo_path = self.paths['NCWO']
        
        # Process each NCWO file type
        self._process_ncwo_1ap(ncwo_path)
        self._process_ncwo_1a(ncwo_path)
        self._process_ncwo_2a(ncwo_path)
        self._process_ncwo_2ap(ncwo_path)

    def _process_ncwo_1ap(self, base_path: Path):
        """Process 1-AP files"""
        input_file = base_path / "OUTPUT" / "1-AP_OUTPUT.csv"
        df = self.safe_read_csv(input_file)
        
        # Format fields
        df['FIRST_NAME'] = df['First Name'].str.upper()
        df['TOTAL'] = df['TOTAL'].apply(self.format_currency)
        df['WEEKLY'] = df['WEEKLY'].apply(self.format_currency)
        
        # Process by version
        for version in ['1-AP', '1-APPR']:
            version_df = df[df['VERSION'] == version].sort_values('Sort Position')
            
            if version == '1-APPR':
                version_df = self._format_spanish_dates(version_df)
                
            # Save version files
            version_df.to_csv(base_path / "OUTPUT" / f"{version}.csv", index=False, encoding='latin1')
            self.generate_proof_data(
                version_df,
                base_path / "PROOF" / f"{version}-PD.csv",
                store_license_required=(version == '1-AP')
            )

    def _format_spanish_dates(self, df: pd.DataFrame) -> pd.DataFrame:
        """Format dates for Spanish versions"""
        if 'START_DATE' in df.columns and 'END_DATE' in df.columns:
            df['START_DATE'] = df['START_DATE'].str.replace('/', ' de ')
            df['END_DATE'] = df['END_DATE'].str.replace('/', ' de ')
        return df

    def _process_ncwo_1a(self, base_path: Path):
        """Process 1-A files"""
        input_file = base_path / "OUTPUT" / "1-A_OUTPUT.csv"
        df = self.safe_read_csv(input_file)
        
        df['FIRST_NAME'] = df['First Name'].str.upper()
        
        # Process by version
        for version in ['1-A', '1-PR']:
            version_df = df[df['VERSION'] == version].sort_values('Sort Position')
            
            if version == '1-PR':
                version_df = self._format_spanish_dates(version_df)
                
            version_df.to_csv(base_path / "OUTPUT" / f"{version}.csv", index=False)
            self.generate_proof_data(
                version_df,
                base_path / "PROOF" / f"{version}-PD.csv",
                store_license_required=(version == '1-A')
            )

    def _process_ncwo_2a(self, base_path: Path):
        """Process 2-A files"""
        input_file = base_path / "OUTPUT" / "2-A_OUTPUT.csv"
        df = self.safe_read_csv(input_file)
        
        df['FIRST_NAME'] = df['First Name'].str.upper()
        
        # Process by version
        for version in ['2-A', '2-PR']:
            version_df = df[df['VERSION'] == version].sort_values('Sort Position')
            
            if version == '2-PR':
                version_df = self._format_spanish_dates(version_df)
                
            version_df.to_csv(base_path / "OUTPUT" / f"{version}.csv", index=False)
            self.generate_proof_data(
                version_df,
                base_path / "PROOF" / f"{version}-PD.csv",
                store_license_required=(version == '2-A')
            )

    def _process_ncwo_2ap(self, base_path: Path):
        """Process 2-AP files"""
        input_file = base_path / "OUTPUT" / "2-AP_OUTPUT.csv"
        df = self.safe_read_csv(input_file)
        
        # Format fields
        df['FIRST_NAME'] = df['First Name'].str.upper()
        df['TOTAL'] = df['TOTAL'].apply(self.format_currency)
        df['WEEKLY'] = df['WEEKLY'].apply(self.format_currency)
        
        # Process by version
        for version in ['2-AP', '2-APPR']:
            version_df = df[df['VERSION'] == version].sort_values('Sort Position')
            
            if version == '2-APPR':
                version_df = self._format_spanish_dates(version_df)
                
            version_df.to_csv(base_path / "OUTPUT" / f"{version}.csv", index=False, encoding='latin1')
            self.generate_proof_data(
                version_df,
                base_path / "PROOF" / f"{version}-PD.csv",
                store_license_required=(version == '2-AP')
            )

    def process_prepif(self):
        """Process PREPIF files with date calculations"""
        prepif_path = self.paths['PREPIF']
        input_file = prepif_path / "OUTPUT" / "PRE_PIF.csv"
        
        df = self.safe_read_csv(input_file)
        
        # Calculate END DATE as BEGIN DATE + 54 days
        df['END DATE'] = pd.to_datetime(df['BEGIN DATE'], format='%m/%d/%Y') + timedelta(days=54)
        df['END DATE'] = df['END DATE'].dt.strftime('%m/%d/%Y')
        
        # Process PR records
        df_pr = df[df['Creative_Version_Cd'].str.contains('PR', na=False)].copy()
        if not df_pr.empty:
            # Convert dates to Spanish format for PR
            df_pr['END DATE'] = pd.to_datetime(df_pr['END DATE']).dt.strftime('%d/%m/%Y')
            
            # Save PR files
            df_pr.to_csv(prepif_path / "OUTPUT" / "PRE_PIF-PR.csv", index=False, encoding='latin1')
            self.generate_proof_data(df_pr, prepif_path / "PROOF" / "PRE_PIF-PR-PD.csv")
        
        # Process US records
        df_us = df[~df.index.isin(df_pr.index)]
        if not df_us.empty:
            df_us.to_csv(prepif_path / "OUTPUT" / "PRE_PIF-US.csv", index=False, encoding='latin1')
            self.generate_proof_data(df_us, prepif_path / "PROOF" / "PRE_PIF-US-PD.csv", store_license_required=True)

def main():
    processor = DataProcessor()
    
    # Process all file types
    processor.process_cbc()
    processor.process_exc()
    processor.process_inactive()
    processor.process_ncwo()
    processor.process_prepif()
    
    print("All processing completed successfully.")

if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print(f"Error during processing: {str(e)}")
