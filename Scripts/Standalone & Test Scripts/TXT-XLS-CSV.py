import os
import pandas as pd
import csv

def convert_txt_to_csv():
    print("\nEnter the path to your TXT file:")
    txt_path = input().strip('"')
    
    if not os.path.exists(txt_path):
        print(f"File not found: {txt_path}")
        return
    
    base_path = os.path.splitext(txt_path)[0]
    csv_path = f"{base_path}.csv"
    
    try:
        # Read TXT content with proper delimiter and quote handling
        df = pd.read_csv(txt_path, 
                        encoding='utf-8',
                        quoting=csv.QUOTE_MINIMAL,
                        quotechar='"',
                        delimiter=',')
        
        # Write to CSV maintaining original structure
        df.to_csv(csv_path, 
                 encoding='utf-8',
                 index=False,
                 quoting=csv.QUOTE_MINIMAL,
                 quotechar='"',
                 lineterminator='\r\n')  # Corrected parameter name
        
        print("\nConversion completed successfully!")
        
        while True:
            print("\nPress X to exit...")
            if input().lower() == 'x':
                break
                
    except Exception as e:
        print(f"\nError during conversion: {str(e)}")

if __name__ == "__main__":
    convert_txt_to_csv()
