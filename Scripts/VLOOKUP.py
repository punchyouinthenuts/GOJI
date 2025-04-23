import os
import csv
import glob

def find_final_file():
    final_file = None
    file_list = glob.glob("C:\\Users\\JCox\\Desktop\\VLOOKUP\\*Sorted*.csv") + glob.glob("C:\\Users\\JCox\\Desktop\\VLOOKUP\\*Processed*.csv")
    if len(file_list) == 1:
        final_file = file_list[0]
    elif len(file_list) > 1:
        print("MULTIPLE SORTED/PROCESSED LISTS PRESENT! REMOVE UNWANTED FILES AND TRY AGAIN...")
        attempts = 0
        while attempts < 5:
            choice = input("PRESS R WHEN READY: ")
            if choice.upper() == "R":
                return find_final_file()
            else:
                attempts += 1
        print("TOO MANY FAILED ATTEMPTS, TERMINATING SCRIPT, PRESS ANY KEY TO EXIT")
        input()
        exit()
    return final_file

def get_user_file_input():
    attempts = 0
    while attempts < 5:
        file_name = input("INPUT FULL FILE NAME HERE (PRESS ENTER WHEN COMPLETE): ")
        if os.path.isfile(file_name):
            return file_name
        else:
            print("FILE NOT FOUND!")
            attempts += 1
    print("TOO MANY FAILED ATTEMPTS, TERMINATING SCRIPT, PRESS ANY KEY TO EXIT")
    input()
    exit()

def get_original_file():
    original_files = [file for file in os.listdir("C:\\Users\\JCox\\Desktop\\VLOOKUP") if file.endswith(".csv")]
    original_files.remove(os.path.basename(final_file))
    if len(original_files) == 1:
        return os.path.join("C:\\Users\\JCox\\Desktop\\VLOOKUP", original_files[0])
    else:
        print("MULTIPLE ORIGINAL FILES FOUND, PLEASE SPECIFY THE CORRECT FILE")
        return get_user_file_input()

def check_id_column(file_path):
    with open(file_path, "r") as file:
        reader = csv.reader(file)
        headers = next(reader)
        first_column = headers[0].strip().replace('ï»¿', '')
        return first_column.upper() == "ID"
def display_missing_id_message(final_file, original_file):
    missing_files = []
    if not check_id_column(final_file):
        missing_files.append(os.path.basename(final_file))
    if not check_id_column(original_file):
        missing_files.append(os.path.basename(original_file))

    if len(missing_files) == 1:
        print(f"{missing_files[0]} IS MISSING ID FIELD IN FIRST COLUMN")
    elif len(missing_files) == 2:
        print(f"{missing_files[0]} AND {missing_files[1]} ARE MISSING ID FIELDS IN FIRST COLUMN")

    input("Press any key to exit...")
    exit()
    
def get_field_mapping(original_fields):
    print("WHICH SETS OF DATA DO YOU NEED TO MERGE?")
    print("INPUT NUMBERS SEPARATED BY A COMMA")
    for i, field in enumerate(original_fields, start=1):
        print(f"{i}. {field}")
    while True:
        user_input = input("HIT ENTER WHEN COMPLETE: ")
        if user_input == "":
            break
        try:
            selected_fields = [int(x.strip()) for x in user_input.split(",")]
            if all(1 <= field <= len(original_fields) for field in selected_fields):
                return [original_fields[i - 1] for i in selected_fields]
            else:
                print("INVALID INPUT, PLEASE TRY AGAIN")
        except ValueError:
            print("INVALID INPUT, PLEASE TRY AGAIN")

def confirm_selection(selected_fields):
    print(f"CONFIRM SELECTION: {', '.join(selected_fields)}")
    while True:
        choice = input("PROCEED? Y/N: ")
        if choice.upper() == "Y":
            return True
        elif choice.upper() == "N":
            return False
        else:
            print("INVALID INPUT, PLEASE TRY AGAIN")

def preprocess_files():
    backup_dir = r"C:\Users\JCox\Desktop\VLOOKUP\BACKUP"
    if not os.path.exists(backup_dir):
        os.makedirs(backup_dir)
    
    vlookup_dir = r"C:\Users\JCox\Desktop\VLOOKUP"
    for filename in os.listdir(vlookup_dir):
        if filename.endswith('.csv'):
            original_path = os.path.join(vlookup_dir, filename)
            backup_path = os.path.join(backup_dir, filename)
            
            with open(original_path, 'r', encoding='utf-8') as original:
                with open(backup_path, 'w', encoding='utf-8') as backup:
                    backup.write(original.read())
            
            with open(original_path, 'r', encoding='utf-8') as file:
                content = file.read()
            
            with open(original_path, 'w', newline='', encoding='utf-8') as file:
                file.write(content)
    
    print("Files preprocessed successfully!")
    
def merge_data(final_file, original_file, selected_fields):
    merged_data = []
    with open(final_file, "r", encoding='utf-8') as final_csv, open(original_file, "r", encoding='utf-8') as original_csv:
        final_reader = csv.DictReader(final_csv)
        original_reader = csv.DictReader(original_csv)
        
        # Clean column names of BOM characters
        final_first_col = final_reader.fieldnames[0].replace('\ufeff', '').strip()
        original_first_col = original_reader.fieldnames[0].replace('\ufeff', '').strip()
        
        # Create mapping dictionary using first column only
        original_data = {}
        for row in original_reader:
            id_value = row[original_reader.fieldnames[0]].strip()
            if id_value:
                original_data[id_value] = row
        
        # Set up headers
        fieldnames = final_reader.fieldnames + selected_fields
        merged_data.append(fieldnames)
        
        # Merge data
        matches = 0
        total = 0
        
        for final_row in final_reader:
            total += 1
            id_value = str(final_row[final_reader.fieldnames[0]]).strip()
            merged_row = [final_row[field] for field in final_reader.fieldnames]
            
            if id_value in original_data:
                matches += 1
                for field in selected_fields:
                    value = original_data[id_value].get(field, '')
                    merged_row.append(value)
            else:
                merged_row.extend(["" for _ in selected_fields])
            
            merged_data.append(merged_row)
        
        print(f"\nMatching Summary:")
        print(f"Total records processed: {total}")
        print(f"Successful matches: {matches}")
    
    return merged_data

def write_merged_file(final_file, merged_data):
    merged_file = f"{os.path.splitext(final_file)[0]}_MERGED.csv"
    with open(merged_file, "w", newline="") as file:
        writer = csv.writer(file)
        writer.writerows(merged_data)
    print(f"MERGE COMPLETE! MERGED FILE: {merged_file}")
    input("Press any key to exit...")

def main():
    preprocess_files()
    global final_file
    final_file = find_final_file()
    if not final_file:
        print("NO [SORTED] or [PROCESSED] FILE FOUND! DO YOU WANT TO:")
        print("1: SPECIFY THE FILE NAME")
        print("2. EXIT")
        choice = input("I WANT TO: ")
        if choice == "1":
            final_file = get_user_file_input()
        elif choice == "2":
            exit()
        else:
            print("INVALID CHOICE, TERMINATING SCRIPT")
            exit()

    original_file = get_original_file()

    if not check_id_column(final_file) or not check_id_column(original_file):
        display_missing_id_message(final_file, original_file)

    with open(original_file, "r") as file:
        reader = csv.reader(file)
        original_fields = next(reader)[1:]  # Skip ID field

    selected_fields = get_field_mapping(original_fields)
    if not confirm_selection(selected_fields):
        main()

    merged_data = merge_data(final_file, original_file, selected_fields)
    write_merged_file(final_file, merged_data)

if __name__ == "__main__":
    main()
