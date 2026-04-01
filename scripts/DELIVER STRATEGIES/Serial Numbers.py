import csv

def extract_serial(barcode: str) -> str:
    # Skip the first 14 digits, then take the next 6
    return barcode[14:20] if len(barcode) > 20 else ''

def main():
    file_path = r"C:\Users\JCox\Downloads\BARCODES.csv"

    first_serial = None
    last_serial = None

    try:
        with open(file_path, mode='r', newline='', encoding='utf-8') as csvfile:
            reader = csv.DictReader(csvfile)
            rows = list(reader)

            if not rows:
                print("CSV file is empty.")
                return

            # Extract from the first row
            first_barcode = rows[0]["Numeric IM Barcode"]
            first_serial = extract_serial(first_barcode)

            # Extract from the last row
            last_barcode = rows[-1]["Numeric IM Barcode"]
            last_serial = extract_serial(last_barcode)

            print(f"FIRST SERIAL NUMBER: {first_serial}")
            print(f"LAST SERIAL NUMBER: {last_serial}")
            print("\nPRESS Q TO TERMINATE...")

            while True:
                user_input = input().strip()
                if user_input.lower() == 'q':
                    break

    except FileNotFoundError:
        print(f"File not found: {file_path}")
    except KeyError as e:
        print(f"Missing expected column in CSV: {e}")
    except Exception as e:
        print(f"An error occurred: {e}")

if __name__ == "__main__":
    main()
