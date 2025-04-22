import pandas as pd

# Define file paths
input_file = r"C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\WEEKLY\JOB\INPUT\FHK_WEEKLY.csv"
pcexp_file = r"C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\WEEKLY\JOB\OUTPUT\TM WEEKLYPCEXP.csv"
move_updates_file = r"C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\WEEKLY\JOB\OUTPUT\MOVE UPDATES.csv"

# Read the CSV files
print("Reading CSV files...")
df_main = pd.read_csv(input_file)
df_pcexp = pd.read_csv(pcexp_file)
df_move = pd.read_csv(move_updates_file)

# Check data types and sample values in each DataFrame
print("\n=== MAIN DataFrame ===")
print(f"Shape: {df_main.shape}")
print("Data types:")
print(df_main.dtypes)
print("Sample data:")
print(df_main.head(2))

print("\n=== PCEXP DataFrame ===")
print(f"Shape: {df_pcexp.shape}")
print("Data types:")
print(df_pcexp.dtypes)
print("Sample data:")
print(df_pcexp.head(2))

print("\n=== MOVE UPDATES DataFrame ===")
print(f"Shape: {df_move.shape}")
print("Data types:")
print(df_move.dtypes)
print("Sample data:")
print(df_move.head(2))

# Check for missing or non-string values in key columns
print("\n=== Checking for issues in 'Original Address Line 1' ===")
if 'Original Address Line 1' in df_move.columns:
    print(f"Contains NaN values: {df_move['Original Address Line 1'].isna().any()}")
    print(f"Count of NaN values: {df_move['Original Address Line 1'].isna().sum()}")
    print("Value types in the column:")
    print(df_move['Original Address Line 1'].apply(type).value_counts())
    print("Sample values:")
    print(df_move['Original Address Line 1'].head())
else:
    print("Column 'Original Address Line 1' not found in move updates file")
    print("Available columns:")
    print(df_move.columns.tolist())