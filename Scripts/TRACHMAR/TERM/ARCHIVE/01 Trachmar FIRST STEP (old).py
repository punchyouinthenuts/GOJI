import pandas as pd
import os

# Set file path and specific file name
file_path = r'C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\TERM\DATA'
target_file = 'FHK_TERM.xlsx'
input_file = os.path.join(file_path, target_file)

# Read the specific XLSX file
df = pd.read_excel(input_file)

# Group by columns H and M
guardian_col = df.columns[7]  # Column H
address_col = df.columns[12]  # Column M

# Create a copy of the original dataframe to preserve all data
result = df.copy()

# Group the data
grouped = df.groupby([guardian_col, address_col])

# Process each group
for name, group in grouped:
    # Get the relevant data from columns E, F, G
    member_data = group[df.columns[[4, 5, 6]]].apply(
        lambda x: f"{x[df.columns[6]]} {x[df.columns[5]]}, {int(x[df.columns[4]])}", 
        axis=1
    ).tolist()
    
    # Add columns for each member in the group
    for i, data in enumerate(member_data, 1):
        col_name = f'ID{i}'
        mask = (result[guardian_col] == name[0]) & (result[address_col] == name[1])
        if col_name not in result.columns:
            result[col_name] = ''
        result.loc[mask, col_name] = data

# Save the intermediate result
output_path = os.path.join(file_path, 'processed_data.xlsx')
result.to_excel(output_path, index=False)

# Read the processed file, remove duplicates based on column H (Guardian Name)
processed_df = pd.read_excel(output_path)
deduped_df = processed_df.drop_duplicates(subset=[guardian_col])

# Remove columns with 'Unnamed' in the header
deduped_df = deduped_df.loc[:, ~deduped_df.columns.str.contains('Unnamed', case=False)]

# Save the final CSV
csv_path = os.path.join(file_path, 'FHK_TERM.csv')
deduped_df.to_csv(csv_path, index=False)

# Delete the intermediate processed_data.xlsx file
os.remove(output_path)
