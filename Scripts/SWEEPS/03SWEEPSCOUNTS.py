import pandas as pd

# Process SWEEPS file
sweeps_path = r"C:\Program Files\Goji\RAC\SWEEPS\JOB\OUTPUT\SWEEPSREFORMAT.csv"
df = pd.read_csv(sweeps_path, low_memory=False, encoding='latin1')
counts = df.iloc[:, 14].value_counts()  # Column O
counts_df = counts.reset_index()
counts_df.columns = ['Value', 'Count']

# Display results
print('=== SWEEPSREFORMAT.csv Counts ===')
print(counts_df.to_string(index=False))

while True:
    user_input = input('\nQUIT? Y/N: ').upper()
    if user_input == 'Y':
        break
    elif user_input == 'N':
        print("why'd you even press anything, then, nerd?")
