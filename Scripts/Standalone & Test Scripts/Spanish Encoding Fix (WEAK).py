import os
import shutil
import sys
import csv

def fix_special_characters(text):
    # Direct mapping for common Spanish characters
    replacements = {
        'Ã"': 'Ó',
        'Ã"n': 'ón',
        'ÃƒÂ"': 'Ñ',
        'Ã"n': 'ón',
        'Ã±': 'ñ',
        'Ã³': 'ó'
    }
    
    # First apply direct replacements
    for old, new in replacements.items():
        text = text.replace(old, new)
    
    # Then handle any remaining encoding issues
    text = text.encode('cp1252', errors='ignore').decode('utf-8', errors='ignore')
    
    return text

def fix_encoding(file_path):
    txt_path = file_path.replace('.csv', '_temp.txt')
    backup_path = file_path.replace('.csv', '_(backup).csv')
    shutil.copy2(file_path, backup_path)
    
    try:
        # Read original file
        with open(file_path, 'r', encoding='latin1') as csv_file:
            content = csv_file.read()
            reader = csv.reader(content.splitlines())
            tab_content = '\n'.join('\t'.join(row) for row in reader)
        
        # Fix encoding
        fixed_content = fix_special_characters(tab_content)
        
        # Write to temporary TXT
        with open(txt_path, 'w', encoding='utf-8') as txt_file:
            txt_file.write(fixed_content)
        
        # Convert back to CSV
        with open(txt_path, 'r', encoding='utf-8') as txt_file:
            reader = csv.reader(txt_file, delimiter='\t')
            with open(file_path, 'w', encoding='utf-8', newline='') as csv_file:
                writer = csv.writer(csv_file)
                writer.writerows(reader)
                
    finally:
        if os.path.exists(txt_path):
            os.remove(txt_path)

def main():
    while True:
        print("\nWHAT IS THE FILE YOU NEED TO PROCESS FOR PROPER SPANISH ENCODING?:")
        file_path = input().strip('"')
        
        if os.path.exists(file_path):
            fix_encoding(file_path)
            print("\nENCODING FIX COMPLETE! DO YOU WANT TO PROCESS ANOTHER FILE? Y/N")
            
            if input().upper() != 'Y':
                print("\nALL ENCODING FIXES COMPLETE! Press any key to exit...")
                input()
                sys.exit()
        else:
            print(f"\nFile not found: {file_path}")

if __name__ == "__main__":
    main()
