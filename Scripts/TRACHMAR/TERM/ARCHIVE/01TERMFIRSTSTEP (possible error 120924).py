import pandas as pd
import os

def process_term_file():
    # Set file path and specific file name
    file_path = r'C:\Users\JCox\Desktop\AUTOMATION\TRACHMAR\TERM\DATA'
    target_file = 'FHK_TERM.xlsx'
    input_file = os.path.join(file_path, target_file)

    while True:
        # Check if file exists
        if not os.path.exists(input_file):
            response = input("FHK_TERM.xlsx NOT FOUND. ENSURE THE FILE IS LOCATED IN AUTOMATION\\TRACHMAR\\TERM\\DATA. ENTER 1 TO CONTINUE OR 0 TO TERMINATE: ")
            
            if response == '0':
                print("Script terminated.")
                return
            elif response == '1':
                continue
            else:
                print("Invalid input. Please enter 0 or 1.")
                continue
        
        try:
            # Read the specific XLSX file
            df = pd.read_excel(input_file)

            # Group by columns H and M
            guardian_col = df.columns[7]  # Column H
            address_col = df.columns[12]  # Column M

            # Create a copy of the original dataframe
            result = df.copy()

            # Group the data
            grouped = df.groupby([guardian_col, address_col])

            # Process each group
            for name, group in grouped:
                member_data = group[df.columns[[4, 5, 6]]].apply(
                    lambda x: f"{x[df.columns[6]]} {x[df.columns[5]]}, {int(x[df.columns[4]])}", 
                    axis=1
                ).tolist()
                
                for i, data in enumerate(member_data, 1):
                    col_name = f'ID{i}'
                    mask = (result[guardian_col] == name[0]) & (result[address_col] == name[1])
                    if col_name not in result.columns:
                        result[col_name] = ''
                    result.loc[mask, col_name] = data

            # Save intermediate result
            output_path = os.path.join(file_path, 'processed_data.xlsx')
            result.to_excel(output_path, index=False)

            # Process final output
            processed_df = pd.read_excel(output_path)
            deduped_df = processed_df.drop_duplicates(subset=[guardian_col])
            deduped_df = deduped_df.loc[:, ~deduped_df.columns.str.contains('Unnamed', case=False)]

            # Save final CSV
            csv_path = os.path.join(file_path, 'FHK_TERM.csv')
            deduped_df.to_csv(csv_path, index=False)

            # Delete intermediate file
            os.remove(output_path)
            
            print("Processing completed successfully.")
            break

        except Exception as e:
            print(f"Error processing file: {str(e)}")
            break

if __name__ == "__main__":
    process_term_file()
