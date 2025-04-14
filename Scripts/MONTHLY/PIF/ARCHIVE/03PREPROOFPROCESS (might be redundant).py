import pandas as pd
import numpy as np

# Define the specific variables we want to keep
variables = [
    'GIN-OFF',
    'GIN-LAST',
    'GIN-SAVE',
    'HOM-OFF',
    'HOM-LAST',
    'HOM-SAVE'
]

# Read the CSV file
input_file = r"C:\Program Files\Goji\RAC\MONTHLY_PIF\FOLDER\OUTPUT\RAC_MONTHLY_PIF.csv"
df = pd.read_csv(input_file)

# Create empty DataFrame to store results
result_df = pd.DataFrame()

# For each variable, get 15 random rows
for var in variables:
    # Filter rows for current variable
    var_df = df[df.iloc[:, 27] == var]
    
    # If there are rows for this variable, take 15 random ones
    if not var_df.empty:
        random_rows = var_df.sample(n=min(15, len(var_df)))
        result_df = pd.concat([result_df, random_rows])

# Sort the final DataFrame by column AB
result_df = result_df.sort_values(by=df.columns[27])

# Define output path
output_file = r"C:\Program Files\Goji\RAC\MONTHLY_PIF\FOLDER\PROOF\RAC_MONTHLY_PIF_VARIABLE_PROOF.csv"

# Save the processed data to new CSV
result_df.to_csv(output_file, index=False)
