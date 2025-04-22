import os
import shutil
import chardet  # For detecting file encoding

def detect_encoding(file_path):
    with open(file_path, 'rb') as file:
        raw_data = file.read()
        result = chardet.detect(raw_data)
        return result['encoding']

def convert_file(file_path):
    # Get file name parts
    directory = os.path.dirname(file_path)
    filename = os.path.basename(file_path)
    name, ext = os.path.splitext(filename)
    
    # Create backup filename
    backup_path = os.path.join(directory, f"{name}_original{ext}")
    
    # Create backup
    shutil.copy2(file_path, backup_path)
    
    # Detect the file's encoding
    detected_encoding = detect_encoding(backup_path)
    print(f"Detected encoding: {detected_encoding}")
    
    # Read and convert
    try:
        with open(backup_path, 'r', encoding=detected_encoding) as input_file:
            content = input_file.read()
            
        with open(file_path, 'w', encoding='cp1252', errors='replace') as output_file:
            output_file.write(content)
            
        print("Conversion to Windows-1252 (ANSI) successful!")
        print(f"Original file backed up as: {os.path.basename(backup_path)}")
        return True
    except Exception as e:
        print(f"Error during conversion: {str(e)}")
        return False

def main():
    print("CSV Encoding Converter - Windows-1252 (ANSI)")
    file_path = input("Please enter the full path to your CSV file: ").strip('"')
    
    if not os.path.exists(file_path):
        print("File not found. Please check the path and try again.")
        return
        
    convert_file(file_path)

if __name__ == "__main__":
    main()