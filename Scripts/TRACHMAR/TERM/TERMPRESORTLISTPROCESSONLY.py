import pandas as pd
import os

# Set the working directory
work_dir = r'C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\TERM\DATA'

# Read PRESORTLIST.csv
presort_file = os.path.join(work_dir, 'PRESORTLIST.csv')
df_presort = pd.read_csv(presort_file)

# Process PRESORTLIST.csv
# Remove empty columns after User Text
user_text_columns = [col for col in df_presort.columns if 'User Text' in str(col)]
if user_text_columns:
    user_text_idx = df_presort.columns.get_loc(user_text_columns[0])
    columns_to_check = df_presort.columns[user_text_idx+1:]
    empty_cols = [col for col in columns_to_check if df_presort[col].isna().all()]
    df_presort = df_presort.drop(columns=empty_cols)

# Create PRESORTLIST_PRINT.csv with records where Pallet Number != -1
df_presort_print = df_presort[df_presort['Pallet Number'] != -1]
presort_print_file = os.path.join(work_dir, 'PRESORTLIST_PRINT.csv')
df_presort_print.to_csv(presort_print_file, index=False)

# Print summary
print(f"Total records processed: {len(df_presort)}")
print(f"Records in PRESORTLIST_PRINT.csv: {len(df_presort_print)}")
