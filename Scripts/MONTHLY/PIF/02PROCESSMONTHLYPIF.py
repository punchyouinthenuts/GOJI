import pandas as pd

def format_phone_number(phone):
    """Reformat phone number to the desired format."""
    cleaned_phone = ''.join(filter(str.isdigit, str(phone)))
    return f"({cleaned_phone[:3]}) {cleaned_phone[3:6]}-{cleaned_phone[6:]}"

def main():
    # Prompt the user for the Contract Dates
    contract_dates = input("Please enter the Contract Dates (format: 99/99/9999 Through 99/99/2023): ")
    
    # Remove any parentheses from the prompted data
    contract_dates = contract_dates.replace("(", "").replace(")", "").strip()
    
    # Open the provided CSV file instead of Excel
    file_path = "C:\\Program Files\\Goji\\RAC\\MONTHLY_PIF\\FOLDER\\OUTPUT\\RAC_MONTHLY_PIF.csv"
    
    # Read CSV instead of Excel
    df = pd.read_csv(file_path)
    
    # Rest of the processing remains the same
    df["Store_Phone_Number"] = df["Store_Phone_Number"].astype(str).str.strip()
    df["Store_Phone_Number"] = df["Store_Phone_Number"].apply(format_phone_number)
    df["CONTRACT_Dates"] = contract_dates
    df["LTR_CODE"] = df["Campaign_Name"].str[:3] + df["Creative_Version_Cd"].str[17:]
    
    # Save the processed CSV
    df.to_csv(file_path, index=False)
    
    # Create the proof sample
    unique_ltr_codes = df["LTR_CODE"].unique()
    sample_df = pd.concat([df[df["LTR_CODE"] == code].sample(15, replace=True) for code in unique_ltr_codes]).sort_values(by="LTR_CODE")
    sample_df.to_csv("C:\\Program Files\\Goji\\RAC\\MONTHLY_PIF\\FOLDER\\PROOF\\PROOF_DATA.csv", index=False)

if __name__ == "__main__":
    main()
