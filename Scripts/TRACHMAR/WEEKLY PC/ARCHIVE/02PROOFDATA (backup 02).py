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

# Calculate how many records we can sample
total_desired = 15
spanish_count = min(1, len(spanish_records))
other_count = min(total_desired - spanish_count, len(other_records))

# Get the samples
if spanish_count > 0:
    spanish_sample = spanish_records.sample(n=spanish_count)
    other_sample = other_records.sample(n=other_count)
    final_df = pd.concat([spanish_sample, other_sample])
else:
    # If no Spanish records exist, take what we can from other records
    sample_size = min(total_desired, len(df))
    final_df = df.sample(n=sample_size)

# Shuffle the records
final_df = final_df.sample(frac=1).reset_index(drop=True)

# Save to the new location
final_df.to_csv(output_file, index=False)
