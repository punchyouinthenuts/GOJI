import pandas as pd
import random

# Read the original CSV file
input_file = r"C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\WEEKLY\JOB\OUTPUT\TM WEEKLYPCEXP.csv"
output_file = r"C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\WEEKLY\JOB\PROOF\TMWEEKLYPROOFDATA.csv"

# Read the CSV into a DataFrame
df = pd.read_csv(input_file)

# Find records with Spanish in language_indicator
spanish_records = df[df['language_indicator'] == 'Spanish']
other_records = df[df['language_indicator'] != 'Spanish']

# Ensure we have at least one Spanish record
if len(spanish_records) > 0:
    spanish_sample = spanish_records.sample(n=1)
    other_sample = other_records.sample(n=14)
    # Concatenate the samples
    final_df = pd.concat([spanish_sample, other_sample])
else:
    # If no Spanish records exist, just take 15 random records
    final_df = df.sample(n=15)

# Shuffle the records
final_df = final_df.sample(frac=1).reset_index(drop=True)

# Save to the new location
final_df.to_csv(output_file, index=False)
