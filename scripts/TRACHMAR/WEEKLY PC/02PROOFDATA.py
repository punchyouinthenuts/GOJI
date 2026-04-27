import pandas as pd
import random
import os
import sys

def print_status(message):
    """Print status message to stdout and flush"""
    print(message)
    sys.stdout.flush()

def print_error(message):
    """Print error message to stderr and flush"""
    print(f"ERROR: {message}", file=sys.stderr)
    sys.stderr.flush()

def print_warning(message):
    """Print warning message to stderr and flush"""
    print(f"WARNING: {message}", file=sys.stderr)
    sys.stderr.flush()

CANONICAL_TM_WEEKLY_BASE = r"C:\Goji\AUTOMATION\TRACHMAR\WEEKLY PC"

def resolve_tm_weekly_base_path():
    """Resolve WEEKLY PC runtime path using canonical path plus optional non-legacy override."""
    configured_tm_base = os.environ.get("GOJI_TM_BASE_PATH", "").strip()
    if configured_tm_base:
        configured_weekly_path = (
            configured_tm_base
            if configured_tm_base.replace("\\", "/").upper().endswith("/WEEKLY PC")
            else os.path.join(configured_tm_base, "WEEKLY PC")
        )
        normalized_configured = configured_weekly_path.replace("\\", "/").upper()
        if normalized_configured.startswith("C:/GOJI/TRACHMAR"):
            print_warning(
                "Configured GOJI_TM_BASE_PATH resolves to legacy C:\\Goji\\TRACHMAR\\WEEKLY PC and will be ignored. "
                "Use C:\\Goji\\AUTOMATION\\TRACHMAR."
            )
        elif os.path.exists(configured_weekly_path):
            return configured_weekly_path
        else:
            print_warning(f"Configured GOJI_TM_BASE_PATH not found: {configured_weekly_path}")

    if os.path.exists(CANONICAL_TM_WEEKLY_BASE):
        return CANONICAL_TM_WEEKLY_BASE

    os.makedirs(CANONICAL_TM_WEEKLY_BASE, exist_ok=True)
    print_warning(f"Created canonical WEEKLY PC runtime path: {CANONICAL_TM_WEEKLY_BASE}")
    return CANONICAL_TM_WEEKLY_BASE

def validate_dataframe(df, required_columns):
    """Validate that the dataframe has required columns"""
    missing_columns = [col for col in required_columns if col not in df.columns]
    if missing_columns:
        print_error(f"Missing required columns in input file: {', '.join(missing_columns)}")
        return False
    return True

def process_proof_data():
    """Process proof data sampling"""
    
    weekly_base_path = resolve_tm_weekly_base_path()
    input_file = os.path.join(weekly_base_path, "JOB", "OUTPUT", "TM WEEKLYPCEXP.csv")
    output_file = os.path.join(weekly_base_path, "JOB", "PROOF", "TMWEEKLYPROOFDATA.csv")
    
    print_status("=== Processing Proof Data Sample ===")
    print_status(f"Input file: {input_file}")
    print_status(f"Output file: {output_file}")
    
    # Check if input file exists
    if not os.path.exists(input_file):
        print_error(f"Input file not found: {input_file}")
        print_status("Please ensure TM WEEKLYPCEXP.csv exists in the OUTPUT directory")
        return False
    
    # Check if input file is readable
    if not os.access(input_file, os.R_OK):
        print_error(f"Input file not readable (permission denied): {input_file}")
        return False
    
    # Ensure output directory exists
    output_dir = os.path.dirname(output_file)
    try:
        print_status(f"Ensuring output directory exists: {output_dir}")
        os.makedirs(output_dir, exist_ok=True)
    except Exception as e:
        print_error(f"Failed to create output directory: {str(e)}")
        return False
    
    # Check if we can write to output directory
    if not os.access(output_dir, os.W_OK):
        print_error(f"Output directory not writable (permission denied): {output_dir}")
        return False
    
    # Read the CSV into a DataFrame
    try:
        print_status("Reading input CSV file...")
        df = pd.read_csv(input_file)
        print_status(f"Successfully loaded {len(df)} records from input file")
    except pd.errors.EmptyDataError:
        print_error("Input file is empty")
        return False
    except pd.errors.ParserError as e:
        print_error(f"Failed to parse CSV file: {str(e)}")
        return False
    except Exception as e:
        print_error(f"Failed to read input file: {str(e)}")
        return False
    
    # Validate dataframe
    if df.empty:
        print_error("Input file contains no data")
        return False
    
    # Check for required columns
    required_columns = ['language_indicator']
    if not validate_dataframe(df, required_columns):
        return False
    
    # Analyze language distribution
    print_status("Analyzing language distribution...")
    language_counts = df['language_indicator'].value_counts()
    print_status(f"Language distribution:")
    for lang, count in language_counts.items():
        print_status(f"  {lang}: {count} records")
    
    # Find records with Spanish in language_indicator
    try:
        spanish_records = df[df['language_indicator'] == 'Spanish']
        other_records = df[df['language_indicator'] != 'Spanish']
        
        print_status(f"Spanish records found: {len(spanish_records)}")
        print_status(f"Other language records: {len(other_records)}")
        
    except Exception as e:
        print_error(f"Failed to filter records by language: {str(e)}")
        return False
    
    # Calculate how many records we can sample
    total_desired = 15
    spanish_count = min(1, len(spanish_records))
    other_count = min(total_desired - spanish_count, len(other_records))
    
    print_status(f"Sampling strategy:")
    print_status(f"  Target total samples: {total_desired}")
    print_status(f"  Spanish samples to take: {spanish_count}")
    print_status(f"  Other language samples to take: {other_count}")
    
    # Validate sampling is possible
    if spanish_count + other_count == 0:
        print_error("No records available for sampling")
        return False
    
    if spanish_count + other_count < total_desired:
        print_warning(f"Only {spanish_count + other_count} records available, less than desired {total_desired}")
    
    # Get the samples
    try:
        if spanish_count > 0 and len(spanish_records) > 0:
            print_status("Sampling Spanish records...")
            spanish_sample = spanish_records.sample(n=spanish_count, random_state=42)
            print_status(f"Selected {len(spanish_sample)} Spanish record(s)")
            
            if other_count > 0 and len(other_records) > 0:
                print_status("Sampling other language records...")
                other_sample = other_records.sample(n=other_count, random_state=42)
                print_status(f"Selected {len(other_sample)} other language record(s)")
                final_df = pd.concat([spanish_sample, other_sample])
            else:
                final_df = spanish_sample
                print_warning("No other language records to sample")
                
        else:
            # If no Spanish records exist, take what we can from other records
            sample_size = min(total_desired, len(df))
            print_warning(f"No Spanish records available, sampling {sample_size} records from all available")
            final_df = df.sample(n=sample_size, random_state=42)
            
    except ValueError as e:
        print_error(f"Sampling failed: {str(e)}")
        return False
    except Exception as e:
        print_error(f"Unexpected error during sampling: {str(e)}")
        return False
    
    # Shuffle the records
    try:
        print_status("Shuffling sampled records...")
        final_df = final_df.sample(frac=1, random_state=42).reset_index(drop=True)
        print_status(f"Final sample contains {len(final_df)} records")
    except Exception as e:
        print_error(f"Failed to shuffle records: {str(e)}")
        return False
    
    # Convert all numeric columns to Int64 to prevent floats
    try:
        print_status("Converting numeric columns to proper format...")
        numeric_cols = final_df.select_dtypes(include=['int64', 'float64']).columns
        if len(numeric_cols) > 0:
            for col in numeric_cols:
                final_df[col] = final_df[col].astype('Int64')
            print_status(f"Converted {len(numeric_cols)} numeric columns")
        else:
            print_status("No numeric columns found to convert")
    except Exception as e:
        print_warning(f"Failed to convert numeric columns (data may still be valid): {str(e)}")
    
    # Check if output file already exists
    if os.path.exists(output_file):
        print_warning(f"Output file already exists and will be overwritten: {os.path.basename(output_file)}")
    
    # Save to the new location
    try:
        print_status(f"Saving proof data to: {output_file}")
        final_df.to_csv(output_file, index=False)
        
        # Verify the file was created and has content
        if not os.path.exists(output_file):
            print_error("Output file was not created successfully")
            return False
            
        # Check file size
        file_size = os.path.getsize(output_file)
        if file_size == 0:
            print_error("Output file is empty after save operation")
            return False
            
        print_status(f"Successfully saved {len(final_df)} records")
        print_status(f"Output file size: {file_size} bytes")
        
        # Final summary
        final_spanish = len(final_df[final_df['language_indicator'] == 'Spanish'])
        final_other = len(final_df[final_df['language_indicator'] != 'Spanish'])
        
        print_status("=== SAMPLING SUMMARY ===")
        print_status(f"Total records sampled: {len(final_df)}")
        print_status(f"Spanish language records: {final_spanish}")
        print_status(f"Other language records: {final_other}")
        print_status("Proof data processing completed successfully!")
        
        return True
        
    except PermissionError:
        print_error(f"Permission denied writing to output file: {output_file}")
        return False
    except Exception as e:
        print_error(f"Failed to save output file: {str(e)}")
        return False

if __name__ == "__main__":
    try:
        success = process_proof_data()
        
        if success:
            print_status("Script completed successfully")
            sys.exit(0)
        else:
            print_error("Script completed with errors")
            sys.exit(1)
            
    except KeyboardInterrupt:
        print_error("Script interrupted by user")
        sys.exit(2)
    except Exception as e:
        print_error(f"Unexpected script error: {str(e)}")
        sys.exit(3)
