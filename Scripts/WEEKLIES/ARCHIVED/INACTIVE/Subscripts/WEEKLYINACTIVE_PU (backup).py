import os
import pandas as pd
import random

# Define your directories
INPUT_DIR = "C:\\Users\\JCox\\Desktop\\AUTOMATION\\RAC\\INACTIVE_2310-DM07\\FOLDERS\\OUTPUT"
PROOF_DIR = "C:\\Users\\JCox\\Desktop\\AUTOMATION\\RAC\\INACTIVE_2310-DM07\\FOLDERS\\PROOF"

# Function to format the SZIP field
def format_szip(szip):
    if len(szip) < 5:
        return szip.zfill(5)
    return szip

# Function to save VERSION rows to FZAPU.csv and sample 15 rows
def save_version_rows(df, input_dir, proof_dir, file_name):
    df_version = df[df['VERSION'].notna()].copy()
    output_file = os.path.join(input_dir, "FZAPU.csv")
    df_version.to_csv(output_file, index=False, encoding='latin1')
    
    if len(df_version) > 15:
        sample_df = df_version.sample(n=15)
    else:
        sample_df = df_version
    
    # Ensure at least one record in the sample has a non-empty Store_License
    if sample_df['Store_License'].notna().sum() == 0 and df_version['Store_License'].notna().sum() > 0:
        additional_sample = df_version[df_version['Store_License'].notna()].sample(n=1)
        sample_df = sample_df.drop(sample_df.index[-1])
        sample_df = pd.concat([sample_df, additional_sample])

    sample_output_file = os.path.join(proof_dir, "FZAPU_PD.csv")
    sample_df.to_csv(sample_output_file, index=False, encoding='latin1')

    # Remove VERSION rows from the original DataFrame
    df = df[~df.index.isin(df_version.index)]
    
    return df

# Loop through all files in the input directory
file_name = "A-PU.csv"
if file_name.endswith(".csv"):
    # Load the file into a pandas DataFrame with 'latin1' encoding
    df = pd.read_csv(os.path.join(INPUT_DIR, file_name), encoding='latin1')

    # Capitalize the 'First Name' field
    df['First Name'] = df['First Name'].str.upper()
    # Format the 'CREDIT' field as dollar value
    df['CREDIT'] = df['CREDIT'].apply(lambda x: f'{float(str(x).replace(",", "")):,.2f}' if pd.notna(x) else x)
    # Extract rows where 'Creative_Version_Cd' contains 'PR'
    df_pr = df[df['Creative_Version_Cd'].str.contains('PR', na=False)].copy()

    # Split 'EXPIRES' field into 'MESSAGE' and 'EXPDATE' for PR records
    df_pr['MESSAGE'] = "La oferta es válida hasta el "
    df_pr['EXPDATE'] = df_pr['EXPIRES'].str[29:]
    df_pr['EXPIRES'] = df_pr['MESSAGE'] + df_pr['EXPDATE']

    # Apply the format_szip function to the 'SZIP' field
    df_pr['SZIP'] = df_pr['SZIP'].apply(format_szip)

    # Save these rows to a new file with 'PR' replacing 'A' in the file name
    df_pr.to_csv(os.path.join(INPUT_DIR, file_name.replace('A', 'PR')), index=False, encoding='latin1')
    df_pr.to_csv(os.path.join(PROOF_DIR, file_name.replace('A', 'PD-PR')), index=False, encoding='latin1')

    # If there are more than 15 rows, keep only the first 15 and overwrite the file in the proof directory
    if len(df_pr) > 15:
        df_pr[:15].to_csv(os.path.join(PROOF_DIR, file_name.replace('A', 'PD-PR')), index=False, encoding='latin1')

    # Remove these rows from the original DataFrame
    df = df[~df.index.isin(df_pr.index)]

    # Call the function to save VERSION rows and update df
    df = save_version_rows(df, INPUT_DIR, PROOF_DIR, file_name)

    # Select 15 random rows, ensuring at least one has a non-empty 'Store_License' field
    if df['Store_License'].count() > 0:
        df_with_license = df[df['Store_License'].notna()]
        df_without_license = df[df['Store_License'].isna()]
        
        random_sample = []
        random_sample.append(df_with_license.sample(n=1))
        remaining_samples = 14 if len(random_sample) > 0 else 15
        if remaining_samples > 0:
            random_sample.append(df_without_license.sample(n=remaining_samples))
        
        random_df = pd.concat(random_sample)
        
        # Save these rows to the proof directory with '-PD' added to the file name
        random_df.to_csv(os.path.join(PROOF_DIR, file_name.replace('.csv', '-PD.csv')), index=False, encoding='latin1')

    # Save the updated original DataFrame
    df.sort_values('Sort Position', ascending=True).to_csv(os.path.join(INPUT_DIR, file_name), index=False, encoding='latin1')
