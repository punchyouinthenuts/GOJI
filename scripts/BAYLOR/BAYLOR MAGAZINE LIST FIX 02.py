import pandas as pd
import re

# ---------------------------
# Helper functions
# ---------------------------

def display_numbered_columns(df):
    print("\nCURRENT COLUMNS:")
    for idx, col in enumerate(df.columns, 1):
        print(f"{idx}. {col}")

def get_address_columns(df):
    display_numbered_columns(df)
    print(
        "\nWHICH COLUMNS CONTAIN ADDRESS LINES?\n"
        "(SELECT ALL ADDRESS LINE COLUMNS, SUCH AS 'ADDRESS LINE 1', 'ADDRESS LINE 2', 'ADDR3', ETC.)\n"
        "LIST THE COLUMN NUMBERS IN THE ORDER THEY SHOULD APPEAR ON MAILING LABELS, SEPARATED BY COMMAS."
    )
    cols = input("\nCOLUMNS: ").replace(" ", "").split(",")
    return [int(x)-1 for x in cols]

def get_extra_columns(df, address_cols):
    print(
        "\nWHICH OF THESE ADDRESS LINE COLUMNS SHOULD BE EMPTY AND DELETED AFTER COLLAPSING?\n"
        "(SELECT ANY EXTRA ADDRESS LINES THAT SHOULD BE REMOVED, FOR EXAMPLE 'ADDR3')."
    )
    for idx, col_idx in enumerate(address_cols, 1):
        print(f"{idx}. {df.columns[col_idx]}")
    extra = input("\nCOLUMNS: ").replace(" ", "").split(",")
    return [address_cols[int(x)-1] for x in extra]

# ---------------------------
# Pattern detection helpers
# ---------------------------

def is_unit_only(text):
    """True if the entire line is just a unit/apartment reference."""
    if not text or pd.isna(text):
        return False
    text = text.strip().lower()
    return bool(re.match(r"^(#?\s*\d+|((apt|apartment|suite|ste|unit|bldg|building|floor|room)\b.*))$", text))

def is_street_plus_unit(text):
    """True if the line is a street address followed by a unit/apartment reference."""
    if not text or pd.isna(text):
        return False
    text = text.strip().lower()
    return bool(re.search(r".+\b(apt|apartment|suite|ste|unit|bldg|building|floor|room)\b.*$", text))

def is_secondary_info(text):
    """Building/floor/department type info that can be appended to Address 2."""
    if not text or pd.isna(text):
        return False
    text = text.strip().lower()
    return bool(re.search(r"(building|dept|department|floor)", text))

# ---------------------------
# Core address collapsing logic
# ---------------------------

def collapse_address_row(row, address_cols, df):
    # Extract values for each selected address column, defaulting to empty strings
    line_values = [str(row[df.columns[idx]]) if pd.notna(row[df.columns[idx]]) else '' for idx in address_cols]
    while len(line_values) < 3:
        line_values.append('')

    line1, line2, line3 = line_values[0], line_values[1], line_values[2]

    # STEP 1: PRE-COLLAPSE SHIFTING
    if not line1.strip() and line2.strip():
        line1, line2 = line2, ''
    if not line2.strip() and line3.strip():
        line2, line3 = line3, ''
    if not line1.strip() and not line2.strip() and line3.strip():
        line1, line3 = line3, ''

    # STEP 2: UNIT/STREET SWAPPING
    if is_unit_only(line1) and not is_unit_only(line2) and not is_street_plus_unit(line1):
        if line2.strip():
            line1, line2 = line2, line1

    # STEP 3: LINE 3 HANDLING
    if line3.strip():
        if is_unit_only(line3):
            if is_unit_only(line2) or is_street_plus_unit(line2):
                line3 = ''
            else:
                line2 = line3
                line3 = ''
        elif is_secondary_info(line3):
            if line2.strip():
                line2 = f"{line2}, {line3}"
            else:
                line2 = line3
            line3 = ''
        else:
            line3 = f"FLAGGED: {line3}"

    return pd.Series([line1, line2, line3])

# ---------------------------
# Verify & handle extra columns
# ---------------------------

def verify_and_handle_extra_columns(df, extra_cols, address2_name):
    not_empty = {}
    for col in extra_cols:
        nonempty_rows = df[df[col].notna() & (df[col].astype(str).str.strip() != '')]
        if not nonempty_rows.empty:
            not_empty[col] = nonempty_rows

    if not not_empty:
        print("\n✅ All selected extra columns are empty. Deleting them...")
        df = df.drop(columns=extra_cols)
        return df

    print("\n⚠️ The following extra columns still contain data:")
    for col, rows in not_empty.items():
        print(f"\nColumn: {col}")
        for idx, val in rows[col].items():
            print(f"Row {idx + 2}: {val}")  # +2 if header is row 1

    for col in not_empty.keys():
        action = input(f"\nWhat do you want to do with column '{col}'?\n"
                       f"1 = Merge into '{address2_name}' with comma\n"
                       f"2 = Clear contents\n"
                       f"3 = Skip\n> ")

        if action == '1':
            df[address2_name] = df[address2_name].fillna('') + \
                df[col].apply(lambda x: f", {x}" if pd.notna(x) and str(x).strip() else '')
            df[col] = ''
        elif action == '2':
            df[col] = ''
        else:
            print(f"Skipping column '{col}'.")

    # Final re-check
    still_nonempty = [col for col in extra_cols if df[col].notna().any() and (df[col].astype(str).str.strip() != '').any()]
    if still_nonempty:
        print("\n⚠️ The following columns still contain data and were not deleted:")
        print(still_nonempty)
    else:
        df = df.drop(columns=extra_cols)
        print("\n✅ Extra columns have been cleared and deleted.")

    return df

# ---------------------------
# Main
# ---------------------------

if __name__ == "__main__":
    file_path = input("WHICH FILE NEEDS PROCESSING? ").strip('"')
    df = pd.read_csv(file_path) if file_path.lower().endswith('.csv') else pd.read_excel(file_path)

    # 1. Select address columns
    address_cols = get_address_columns(df)
    address_col_names = [df.columns[idx] for idx in address_cols]

    # 2. Select extra columns (subset of address_cols)
    extra_cols = get_extra_columns(df, address_cols)
    extra_col_names = [df.columns[idx] for idx in extra_cols]

    # 3. Collapse addresses row by row
    print("\nCollapsing address lines...")
    df[address_col_names[:3]] = df.apply(lambda row: collapse_address_row(row, address_cols, df), axis=1)

    # 4. Verify & handle extra columns
    if len(address_col_names) > 1 and extra_col_names:
        df = verify_and_handle_extra_columns(df, extra_col_names, address_col_names[1])

    # 5. Save output
    output_path = file_path.replace('.csv', '_collapsed.csv').replace('.xlsx', '_collapsed.csv')
    df.to_csv(output_path, index=False)
    print(f"\n✅ Address collapsing complete. File saved as:\n{output_path}")
