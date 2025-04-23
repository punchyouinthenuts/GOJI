import pandas as pd
import os

def get_job_type():
    attempts = 0
    while attempts < 10:
        job_type = input("WHICH JOB ARE YOU PROCESSING? LOC, PIF, or LOYALTY? ").upper()
        if job_type in ['LOC', 'PIF', 'LOYALTY']:
            return job_type
        attempts += 1
        print("Invalid input. Please enter LOC, PIF, or LOYALTY")
    print("Too many incorrect attempts. Program terminating.")
    exit()

def read_input_files(job_type):
    if job_type == 'LOC':
        input_path = r'C:\Program Files\Goji\RAC\LOC\JOB\INPUT'
        output_file = r'C:\Program Files\Goji\RAC\LOC\JOB\OUTPUT\LOC_MONTHLY_OUTPUT.csv'
    elif job_type == 'PIF':
        input_path = r'C:\Program Files\Goji\RAC\MONTHLY_PIF\FOLDER\INPUT'
        output_file = r'C:\Program Files\Goji\RAC\MONTHLY_PIF\FOLDER\OUTPUT\RAC_MONTHLY_PIF.csv'
    else:  # LOYALTY
        input_path = r'C:\Program Files\Goji\RAC\LOYALTY\JOB\INPUT'
        output_file = r'C:\Program Files\Goji\RAC\LOYALTY\JOB\OUTPUT\LOYALTY.csv'

    # Read main output file
    main_df = pd.read_csv(output_file)
    
    # Check for input files
    gin_exists = os.path.exists(os.path.join(input_path, 'GIN.txt'))
    hom_exists = os.path.exists(os.path.join(input_path, 'HOM.txt'))
    
    if job_type == 'LOYALTY' and (not gin_exists or not hom_exists):
        file_type = 'GIN' if gin_exists else 'HOM'
        confirm = input(f"Only {file_type} file found. Is this correct for this month? (Y/N): ").upper()
        if confirm != 'Y':
            print("Please confirm that job has required input TXT files")
            input("Press any key to exit...")
            exit()
    
    # Read available files
    gin_df = pd.read_csv(os.path.join(input_path, 'GIN.txt'), delimiter='\t') if gin_exists else pd.DataFrame({'individual_id': []})
    hom_df = pd.read_csv(os.path.join(input_path, 'HOM.txt'), delimiter='\t') if hom_exists else pd.DataFrame({'individual_id': []})
    
    # Convert all column names to lowercase for case-insensitive matching
    main_df.columns = main_df.columns.str.lower()
    gin_df.columns = gin_df.columns.str.lower()
    hom_df.columns = hom_df.columns.str.lower()
    
    return main_df, gin_df, hom_df

if __name__ == "__main__":
    # Get job type from user
    job_type = get_job_type()
    
    # Read files
    main_df, gin_df, hom_df = read_input_files(job_type)

    # Convert individual_id to string type for consistent comparison
    main_df['individual_id'] = main_df['individual_id'].astype(str)
    gin_df['individual_id'] = gin_df['individual_id'].astype(str) if not gin_df.empty else gin_df['individual_id']
    hom_df['individual_id'] = hom_df['individual_id'].astype(str) if not hom_df.empty else hom_df['individual_id']

    # Find matches
    gin_matches = main_df['individual_id'].isin(gin_df['individual_id']).sum() if not gin_df.empty else 0
    hom_matches = main_df['individual_id'].isin(hom_df['individual_id']).sum() if not hom_df.empty else 0

    # Calculate totals
    total_matches = gin_matches + hom_matches
    total_main_records = len(main_df)
    difference = total_main_records - total_matches

    # Print report
    print("\nMatch Analysis Report")
    print("-" * 50)
    print(f"GIN Matches Found: {gin_matches:,}")
    print(f"HOM Matches Found: {hom_matches:,}")
    print(f"Total Matches Found: {total_matches:,}")
    print(f"Difference from MAIN total ({total_main_records:,}): {difference:,}")

    if difference == 0:
        while True:
            try:
                job_postage = float(input("\nENTER JOB POSTAGE TOTAL: "))
                if job_postage > 0:
                    break
                print("Please enter a valid postage total")
            except ValueError:
                print("Please enter a valid postage total")

        while True:
            try:
                piece_count = int(input("ENTER JOB PIECE COUNT: "))
                if piece_count > 0:
                    break
                print("Please enter a valid piece count")
            except ValueError:
                print("Please enter a valid piece count")

        postage_rate = job_postage / piece_count
        gin_postage = gin_matches * postage_rate
        hom_postage = hom_matches * postage_rate
        total_postage = gin_postage + hom_postage
        
        print(f"\nCALCULATED AVERAGE POSTAGE RATE: ${postage_rate:.3f}")
        print(f"GIN POSTAGE: ${gin_postage:,.2f}")
        print(f"HOM POSTAGE: ${hom_postage:,.2f}")
        print(f"TOTAL POSTAGE: ${total_postage:,.2f}")
        
        while True:
            user_input = input("\nPROCESSING COMPLETE, PRESS X KEY TO CLOSE: ").lower()
            if user_input == 'x':
                break
    else:
        print("\nRECORDS DO NOT MATCH FINAL COUNT")
        print("CHECK INPUT RECORDS AND TRY AGAIN")
        input("Press any key to close...")
