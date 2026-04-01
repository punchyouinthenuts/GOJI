import pandas as pd
import re
import os

# -------------------------
# CONFIGURATION: PATTERNS
# -------------------------

BUSINESS_KEYWORDS = [
    r"LLC", r"INC\.?", r"CORP\.?", r"COMPANY", r"CORPORATION", r"FOUNDATION",
    r"UNIVERSITY", r"COLLEGE", r"MINISTRIES", r"CHURCH", r"SCHOOL", r"HOSPITAL",
    r"BANK", r"CLUB", r"ASSOCIATION", r"GROUP", r"CO-OP", r"COOP",
    r"FUND", r"GIVING FUND", r"ENERGY", r"PRESS", r"LIBRARY", r"TRUST",
    r"BOARD", r"COUNCIL", r"SOCIETY", r"COMMITTEE", r"AGENCY", r"CENTER", r"MINISTRY",
    r"DEPARTMENT", r"FELLOWSHIP", r"INSTITUTE", r"INDUSTRIES", r"AUTHORITY", r"DISTRICT", r"OFFICE"
]
BUSINESS_PATTERN = re.compile(r"\b(" + "|".join(BUSINESS_KEYWORDS) + r")\b", re.IGNORECASE)

# Expanded pattern for titles, honorifics, military ranks, & personal structures
PERSON_NAME_PATTERN = re.compile(
    r"\b("
    r"Mr\.|Mrs\.|Ms\.|Miss|Dr\.|Rev\.|Prof\.|"
    r"1st\s+Lt\.|2nd\s+Lt\.|Lt\.|Capt\.|Maj\.|Lt\. Col\.|Col\.|"
    r"Brig\. Gen\.|Maj\. Gen\.|Lt\. Gen\.|Gen\.|"
    r"Sgt\.|SFC|SSG|MSgt\.|CMSgt\.|Cpl\.|Pvt\.|Spc\.|"
    r"WO1|CW2|CW3|CW4|CW5|"
    r"and|&|\(Ret\.\)"
    r")\b",
    re.IGNORECASE
)

ATTN_PATTERN = re.compile(r"(?i)(ATTN:|ATTN|C/O:|C/O)\s*(.*)")

# -------------------------
# CLASSIFICATION FUNCTIONS
# -------------------------

def classify_entry_refined(text):
    if not text or not text.strip():
        return "person", None

    # 1. Person structure (titles, ranks, etc.)
    if PERSON_NAME_PATTERN.search(text):
        return "person", "person-structure"

    # 2. Keyword match
    if BUSINESS_PATTERN.search(text):
        return "business", "keyword"

    # 3. All caps heuristic
    if text.isupper() and len(text.split()) >= 2:
        return "business", "all-caps"

    # 4. Long string / punctuation
    if len(text.split()) > 6 or re.search(r"[,@/]", text):
        return "business", "structure"

    # 5. Token count fallback
    tokens = re.findall(r"[A-Za-z']+", text)
    if 1 <= len(tokens) <= 5:
        return "person", "token-count"

    # Default
    return "business", "fallback"

def split_business_and_address(text):
    text = text.strip()
    if not text:
        return "", ""

    # Business + street address
    match = re.match(r"^(.+?)(\d{1,5}\s+.+)$", text)
    if match:
        return match.group(1).strip(), match.group(2).strip()

    # Business + PO Box
    match = re.match(r"^(.+?)(P\.?\s*O\.?\s*Box.+)$", text, re.IGNORECASE)
    if match:
        return match.group(1).strip(), match.group(2).strip()

    # Entire string is business (keywords, no digits)
    if BUSINESS_PATTERN.search(text) and not re.search(r"\d", text):
        return text, ""

    return "", text

# -------------------------
# HELPER FUNCTIONS
# -------------------------

def get_file_path():
    return input("WHICH FILE NEEDS PROCESSING? ").strip('"')

def display_numbered_columns(df):
    print("\nCURRENT COLUMNS:")
    for idx, col in enumerate(df.columns, 1):
        print(f"{idx}. {col}")

def rename_columns(df):
    response = input("\nDO ANY HEADERS NEED TO BE RENAMED? Y/N ").upper()
    if response != 'Y':
        return df

    while True:
        display_numbered_columns(df)
        cols = input("\nINPUT COLUMN NUMBERS TO RENAME, SEPARATED BY COMMAS: ").replace(" ", "").split(",")
        cols = [int(x) for x in cols]

        new_names = {}
        for col_num in cols:
            old_name = df.columns[col_num - 1]
            new_name = input(f"NEW NAME FOR COLUMN {col_num} ({old_name}): ")
            new_names[old_name] = new_name

        print("\nPLEASE CONFIRM THE FOLLOWING RENAMES:")
        for old, new in new_names.items():
            print(f"{old} -> {new}")

        if input("\nIS THIS CORRECT? Y/N ").upper() == 'Y':
            return df.rename(columns=new_names)

def get_multiple_columns_with_business_prompt(df):
    display_numbered_columns(df)
    print(
        "\nWHICH COLUMNS MIGHT CONTAIN BUSINESS OR ORGANIZATION NAMES?\n"
        "(THIS MAY INCLUDE THE NAME COLUMN AND ONE OR MORE ADDRESS COLUMNS)\n"
        "INPUT THE COLUMN NUMBERS SEPARATED BY COMMAS.\n"
        "I WILL SCAN ALL SELECTED COLUMNS FOR BUSINESS NAMES, MOVE THEM INTO THE NEW BUSINESS COLUMN,\n"
        "AND SPLIT OUT ANY BUSINESS NAMES THAT APPEAR TOGETHER WITH ADDRESSES."
    )
    cols = input("\nCOLUMNS: ").replace(" ", "").split(",")
    return [int(c) - 1 for c in cols]

def get_multiple_columns_for_attn(df):
    display_numbered_columns(df)
    print(
        "\nWHICH COLUMNS MIGHT CONTAIN ATTN OR C/O INFORMATION?\n"
        "(THIS IS USUALLY FOUND IN ADDRESS OR NAME COLUMNS AND MAY LOOK LIKE 'ATTN: JOHN DOE' OR 'C/O MARY SMITH')\n"
        "INPUT THE COLUMN NUMBERS SEPARATED BY COMMAS.\n"
        "I WILL SCAN THESE COLUMNS FOR ATTN OR C/O PATTERNS AND MOVE ANY MATCHES INTO THE TITLE COLUMN."
    )
    cols = input("\nCOLUMNS: ").replace(" ", "").split(",")
    return [int(c) - 1 for c in cols]

# -------------------------
# PROCESSING FUNCTIONS
# -------------------------

def process_business_columns(df, col_indices):
    for col_idx in col_indices:
        col_name = df.columns[col_idx]
        for row_idx, val in df[col_name].fillna("").items():
            val = val.strip()
            if not val:
                continue

            classification, _ = classify_entry_refined(val)
            if classification == "business":
                business_part, address_part = split_business_and_address(val)
                if business_part:
                    if not df.at[row_idx, "Business"]:
                        df.at[row_idx, "Business"] = business_part
                    df.at[row_idx, col_name] = address_part
    return df

def process_attn_data(df, attn_cols):
    for col in attn_cols:
        series = df.iloc[:, col].astype(str)
        mask = series.str.contains(ATTN_PATTERN, na=False, regex=True)
        matches = series[mask].str.extract(ATTN_PATTERN)

        df.loc[mask, 'Title'] = matches[0] + ' ' + matches[1]
        df.loc[mask, df.columns[col]] = ''
    return df

# -------------------------
# MAIN EXECUTION
# -------------------------

if __name__ == "__main__":
    file_path = get_file_path()

    # Read XLSX or XLSB
    if file_path.lower().endswith('.xlsb'):
        df = pd.read_excel(file_path, engine='pyxlsb')
    else:
        df = pd.read_excel(file_path)

    # Optional header renaming
    df = rename_columns(df)

    # Insert Title and Business columns at user-chosen positions
    display_numbered_columns(df)
    title_pos = int(input("\nWHAT COLUMN SHOULD TITLE PRECEDE? ")) - 1
    df.insert(title_pos, 'Title', '')

    display_numbered_columns(df)
    business_pos = int(input("\nWHAT COLUMN SHOULD BUSINESS PRECEDE? ")) - 1
    df.insert(business_pos, 'Business', '')

    # Business detection step
    business_cols = get_multiple_columns_with_business_prompt(df)
    df = process_business_columns(df, business_cols)

    # ATTN/C/O extraction step
    attn_cols = get_multiple_columns_for_attn(df)
    df = process_attn_data(df, attn_cols)

    # Confirm processing
    if input("\nPRESS Y TO START PROCESSING THE FILE WITH YOUR SELECTED COLUMNS. ").upper() == 'Y':
        output_path = os.path.splitext(file_path)[0] + "_processed.csv"
        df.to_csv(output_path, index=False)
        print(f"\nPROCESSING COMPLETE. FILE SAVED AS: {output_path}")
