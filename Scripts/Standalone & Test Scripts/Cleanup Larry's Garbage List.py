import pandas as pd
import os
import sys
import shutil
import time
import threading
from datetime import datetime

class ProcessingAnimation:
    def __init__(self):
        self.running = True
        
    def animate(self):
        chars = "⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏"
        i = 0
        while self.running:
            sys.stdout.write('\r' + 'Processing ' + chars[i % len(chars)])
            sys.stdout.flush()
            time.sleep(0.1)
            i += 1
            
    def stop(self):
        self.running = False

def create_backup(file_path):
    backup_path = file_path.rsplit('.', 1)[0] + '_backup.' + file_path.rsplit('.', 1)[1]
    shutil.copy2(file_path, backup_path)
    return backup_path

def create_log_file(input_file_path):
    file_dir = os.path.dirname(os.path.abspath(input_file_path))
    timestamp = datetime.now().strftime("%Y%m%d-%I%M%p")
    log_path = os.path.join(file_dir, f"ListFixLog_{timestamp}.txt")
    
    with open(log_path, 'w') as f:
        f.write(f"=== PROCESS INITIALIZATION ===\n")
        f.write(f"Start Time: {datetime.now().strftime('%I:%M:%S %p')}\n")
        f.write(f"Input File: {input_file_path}\n")
        f.write("-" * 50 + "\n")
    
    return log_path

def write_processing_summary(log_file, pattern1_count, pattern2_count, pattern3_count):
    with open(log_file, 'a') as f:
        f.write("\n=== DATA MOVEMENT SUMMARY ===\n")
        f.write("\nPattern Analysis:\n")
        f.write("-----------------\n")
        f.write(f"1. Records with data only in MailingAddr3\n")
        f.write(f"   Moved to: MailingAddr2\n")
        f.write(f"   Count: {pattern1_count}\n\n")
        
        f.write(f"2. Records with data in MailingAddr2 + MailingAddr3\n")
        f.write(f"   Action: Cascaded left\n")
        f.write(f"   Count: {pattern2_count}\n\n")
        
        f.write(f"3. Records with data in all three address columns\n")
        f.write(f"   Action: Complete shift to Title\n")
        f.write(f"   Count: {pattern3_count}\n\n")
        
        f.write("=== TOTALS ===\n")
        f.write(f"Total records processed: {pattern1_count + pattern2_count + pattern3_count}\n")
        f.write("-" * 50 + "\n")

def log_error(log_file, message):
    with open(log_file, 'a') as f:
        f.write(f"{datetime.now().strftime('%I:%M:%S %p')}: {message}\n")

def has_data(value):
    value_str = str(value).strip()
    return bool(value_str and value_str != 'nan')

def clean_csv():
    try:
        print("Please enter the input file path:")
        input_file = input().strip('"')
        
        if not os.path.exists(input_file):
            raise FileNotFoundError("The specified file does not exist.")
            
        print(f"Process file: {input_file}? (Y/N)")
        if input().lower() != 'y':
            print("Operation cancelled. Press X to exit...")
            wait_for_x()
            return
            
        backup_path = create_backup(input_file)
        log_file = create_log_file(input_file)
        
        animation = ProcessingAnimation()
        animation_thread = threading.Thread(target=animation.animate)
        animation_thread.start()
        
        df = pd.read_csv(input_file, encoding='cp1252', low_memory=False)
        
        # Initialize pattern counters
        pattern1_count = 0  # MailingAddr3 only
        pattern2_count = 0  # MailingAddr2 + MailingAddr3
        pattern3_count = 0  # All three columns
        
        # Clear single dashes
        df = df.replace('-', '')
        
        # Insert Title column
        df.insert(df.columns.get_loc('Mailing Addr1'), 'Title', '')
        
        # Process address movements
        for idx in range(len(df)):
            addr1 = str(df.at[idx, 'Mailing Addr1']).strip()
            addr2 = str(df.at[idx, 'Mailing Addr2']).strip()
            addr3 = str(df.at[idx, 'Mailing Addr3']).strip()
            
            # Move data if MailingAddr3 has content
            if has_data(addr3):
                if has_data(addr1) and has_data(addr2):
                    pattern3_count += 1
                    df.at[idx, 'Title'] = addr1
                    df.at[idx, 'Mailing Addr1'] = addr2
                    df.at[idx, 'Mailing Addr2'] = addr3
                elif has_data(addr2):
                    pattern2_count += 1
                    df.at[idx, 'Mailing Addr1'] = addr2
                    df.at[idx, 'Mailing Addr2'] = addr3
                else:
                    pattern1_count += 1
                    df.at[idx, 'Mailing Addr2'] = addr3
                    
                df.at[idx, 'Mailing Addr3'] = ''
        
        # Write processing summary to log
        write_processing_summary(log_file, pattern1_count, pattern2_count, pattern3_count)
        
        # Verify MailingAddr3 is empty
        non_empty_addr3 = df[df['Mailing Addr3'].str.strip() != '']
        if not non_empty_addr3.empty:
            for idx in non_empty_addr3.index:
                log_error(log_file, f"Row {idx + 1}: MailingAddr3 still contains data: {df.at[idx, 'Mailing Addr3']}")
            raise ValueError("MailingAddr3 column contains data after processing")
        
        # Rename columns and remove MailingAddr3
        column_mapping = {
            'Name': 'Full Name',
            'Mailing Addr1': 'Address Line 1',
            'Mailing Addr2': 'Address Line 2',
            'Zip': 'ZIP Code'
        }
        df = df.rename(columns=column_mapping)
        df = df.drop('Mailing Addr3', axis=1)
        
        animation.stop()
        animation_thread.join()
        
        df.to_csv(input_file, index=False, encoding='cp1252')
        
        print("\nFile successfully processed! Press X to exit...")
        wait_for_x()
        
    except Exception as e:
        animation.stop() if 'animation' in locals() else None
        animation_thread.join() if 'animation_thread' in locals() else None
        
        print(f"\nError: {str(e)}")
        print("Press X to terminate...")
        wait_for_x()

def wait_for_x():
    while True:
        key = input().lower()
        if key == 'x':
            break

if __name__ == "__main__":
    clean_csv()
