import pandas as pd

def process_data(file_path, output_path):
    try:
        # Read the data
        data = pd.read_csv(file_path)
        
        # Verify column exists
        if 'CUSTOM_03' not in data.columns:
            raise KeyError("CUSTOM_03 column not found in the CSV file")
            
        # Format 'CUSTOM_03' as US currency without the dollar sign
        data['CUSTOM_03'] = data['CUSTOM_03'].apply(
            lambda x: f"{float(x):,.2f}" if pd.notnull(x) else x
        )
        
        # Save the modified data to the output file
        data.to_csv(output_path, index=False)
        return True
        
    except FileNotFoundError:
        print(f"File not found: {file_path}")
        return False
    except ValueError as e:
        print(f"Error processing values: {e}")
        return False
    except Exception as e:
        print(f"An unexpected error occurred: {e}")
        return False

if __name__ == "__main__":
    # Input and output file paths
    file_path = r'C:\Program Files\Goji\RAC\SWEEPS\JOB\OUTPUT\SWEEPS.csv'
    output_path = r'C:\Program Files\Goji\RAC\SWEEPS\JOB\OUTPUT\SWEEPSREFORMAT.csv'
    
    # Call the process_data function
    success = process_data(file_path, output_path)
