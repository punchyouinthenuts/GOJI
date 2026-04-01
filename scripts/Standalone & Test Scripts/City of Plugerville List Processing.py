import sys
import re
from pathlib import Path
from typing import Optional, List, Tuple, Dict

import pandas as pd


# =========================
# Heuristics / dictionaries
# =========================

# Legal entity markers (hard veto anywhere in the string)
ENTITY_TYPES = {
    "LLC","LLC.","INC","INC.","LTD","LTD.","LP","LP.","LLP","LLP.","PLLC","PLC","PLC.","PC","PC.",
    "CO","CO.","CORP","CORP.","INCORPORATED","COMPANY","CORPORATION","LIMITED","PARTNERSHIP"
}

# Organization / venue / commercial terms
BUSINESS_TERMS = {
    # civic/edu/gov
    "UNIVERSITY","COLLEGE","SCHOOL","DISTRICT","ISD","CISD","PISD",
    "CITY","COUNTY","STATE","GOVERNMENT","AUTHORITY","BOARD","COUNCIL",
    "DEPT","DEPARTMENT","MINISTRY","MINISTRIES","LIBRARY","COURT","JUSTICE",
    # faith/community/charity
    "CHURCH","BAPTIST","CATHOLIC","METHODIST","LUTHERAN","CHAPEL","PARISH","SALVATION","ARMY",
    # medical/health
    "HOSPITAL","HOSP","CLINIC","MEDICAL","NURSING","REHAB","HOSPICE","DENTAL","DENTISTRY",
    "ASSISTED","LIVING","HEALTH","HEALTHCARE","CARE","MED","CTR","CENTER",
    # finance/banking/insurance/credit unions
    "BANK","TRUST","MORTGAGE","FINANCIAL","INVESTMENT","INVESTMENTS",
    "CREDIT","UNION","CU","FED","FEDERAL","CHASE","JP","MORGAN",
    # commerce/services/real-estate/venues
    "SERVICE","SERVICES","SALON","BARBER","SPA","MASSAGE","ASSOCIATE","ASSOCIATES",
    "TECH","TECHNOLOGY","COMMUNICATIONS","REALTY","PROPERTIES","PROPERTY","DEVELOPMENT",
    "EXCAVATING","EXCAVATION","CONSTRUCTION","CONSTRUCTORS","CONTRACTOR","CONTRACTORS",
    "HARDSCAPES","BODYWORKS","AUTOMOTIVE","WAREHOUSE","STORAGE","MINI",
    "OFFICE","OFFICES","SPACE","CENTER","CENTRE","MALL","PARK","PARKWAY","PLANT","HALL",
    "ESTATES","HOMES","COMMUNITIES","RESIDENTIAL","COMMUNITY","HOMEOWNERS","ASSOC","ASSOCIATION",
    "APARTMENT","APARTMENTS","APT","APTS","PLAZA","SHOPPES","SHOPPING","ENTERTAINMENT","WATERPARK",
    "GROCERY","MARKET","MART","XPRESS","LUBE","LAUNDROMAT","CAR","WASH","DEVELOPMENT","FACTORY",
    "METAL","PROP","MNGMT","MGMT","MGM","CONDO","CONDOS","CONDOMINIUM","IRRIGATION","LANDSCAPE","LANDSCAPING",
    "KIDS","ROOM","RESTAURANTS","HOA","MASTER","COMM","PRC","LOGISTICS","REAL","ESTATE","WHOLESALE",
    "INDUSTRY","INDUSTRIES","GYMNASTICS","MONTESSORI","DAYCARE","KENNEL","CONCRETE","ICEHOUSE","GRILL",
    "HOLDINGS","HOLDING","ADVOCAC"
}

# Core place/city/state tokens
PLACE_TOKENS = {"PFLUGERVILLE","AUSTIN","TEXAS","TX","PFLUGER","PFLUGERS"}

# Cemetery spellings
CEMETERY_WORDS = {"CEMETERY","CEMETARY"}

# Brands (token set) — regex also below
BRAND_TOKENS = {
    "COSTCO","FEDEX","MARRIOTT","GOODWILL","ROSS","COURTYARD","BAYLOR","SCOTT","WHITE",
    "AUTOZONE","AUT0ZONE","INNOUT","IN-N-OUT","WALMART","WAL-MART","IN","OUT","NOLOGY","THINNOLOGY"
}

# N-gram org patterns
BUSINESS_NGRAMS = {
    ("CAR","WASH"),
    ("MINI","STORAGE"),
    ("POST","OFFICE"),
    ("CHAMBER","OF","COMMERCE"),
    ("HOMEOWNERS","ASSOC"),
    ("ASSISTED","LIVING"),
    ("VALVOLINE","INSTANT"),
    ("INSTANT","OIL"),
    ("OIL","CHANGE"),
    ("BODYWORKS","OF"),
    ("NURSING","AND","REHAB"),
    ("BILLIARD","FACTORY"),
    ("CREDIT","UNION"),
    ("STORE","#"),
    ("WATERPARK","PFLUGERVILLE"),
    ("REALTY","ACC"),
    ("EASTGROUP","PROPERTIES"),
    ("MASTER","COMM"),
    ("CARDINAL","CROSSING"),
    ("GREEN","N","GROWING"),
    ("GATHER","N","GIVE"),
    ("HOME","DEPOT"),
    ("HOBBY","LOBBY"),
    ("RAISING","CANE'S"),
    ("OF","PFLUGERVILLE"),
    ("HEALTH","CARE"),
    ("LIONS","CLUB"),
    ("TREE","HOUSE"),
    ("TREE","HOUSE","KIDS"),
    ("DRESS","FOR"),
}

# Address-like tokens / symbols
ADDRESSY_TOKENS = {"PO","P.O.","BOX","#","UNIT","SUITE","STE","BLDG","RM","APT","APTS","@"}

# "Saint" prefixes (but allow comma-people)
SAINT_PREFIXES = {"ST","ST.","SAINT"}

# Titles and suffixes allowed in person names
TITLES = {"MR","MR.","MRS","MRS.","MS","MS.","MISS","DR","DR.","PROF","REV","SGT","CAPT","LT","COL","FR","FR."}
SUFFIXES = {"JR","JR.","SR","SR.","II","III","IV","V"}

NAME_ALLOWED = r"[A-Za-zÀ-ÖØ-öø-ÿ'’\-\.\s]"

# Flexible brand regexes (handle spaces/hyphens/variants)
BRAND_REGEXES = [
    re.compile(r"\bIN\s*-\s*N\s*-\s*OUT\b", re.I),
    re.compile(r"\bINN?\s*O?U?T\b", re.I),
    re.compile(r"\bAUTO\s*Z?ONE\b", re.I),
    re.compile(r"\bCOSTCO\b", re.I),
    re.compile(r"\bFEDEX\b", re.I),
    re.compile(r"\bMARRIOTT\b", re.I),
    re.compile(r"\bGOODWILL\b", re.I),
    re.compile(r"\bCOURTYARD\b", re.I),
    re.compile(r"\bBAYLOR\s+SCOTT\s*&\s*WHITE\b", re.I),
    re.compile(r"\bROSS\b.*\bDRESS\b.*\bLESS\b", re.I),
    re.compile(r"\bTHIN\s*-\s*NOLOGY\b", re.I),
]


# =========================
# Text helpers / normalize
# =========================

def collapse_ws(s: str) -> str:
    return re.sub(r"\s+", " ", str(s)).strip()

def pre_normalize_glued_entities(s: str) -> str:
    return re.sub(r"(?i)(llc|inc|lp|llp|pllc|plc|pc)(\b)", r" \1 ", s)

def normalize_for_tokenization(s: str) -> str:
    s = pre_normalize_glued_entities(s)
    s = s.replace("’", "'")
    s = re.sub(r"[-,_\\()\[\]{};:]", " ", s)
    s = s.replace("&", " & ")
    s = s.replace("/", " / ")
    s = re.sub(r"\s+", " ", s)
    s = re.sub(r"-\s*$", "", s)
    return collapse_ws(s)

def tokens_upper(s: str) -> List[str]:
    toks = normalize_for_tokenization(s).upper().split(" ")
    norm = []
    for t in toks:
        if t.endswith("'S") or t.endswith("’S"):
            t = t[:-2] + "S"
        norm.append(t)
    return [t for t in norm if t]

def has_digits(s: str) -> bool:
    return any(ch.isdigit() for ch in s)

def ngrams(seq: List[str], n: int) -> List[Tuple[str, ...]]:
    return [tuple(seq[i:i+n]) for i in range(len(seq)-n+1)]

def ocr_fix_token(tok: str) -> str:
    if re.search(r"[0O]", tok, re.I):
        letters = sum(c.isalpha() for c in tok)
        digits = sum(c.isdigit() for c in tok)
        if digits > 0 and letters >= digits:
            tok = re.sub(r"0", "O", tok)
    return tok


# ==================================
# Person-shape / parsing
# ==================================

def is_initial_token(up: str) -> bool:
    return bool(re.fullmatch(r"[A-Z](?:\.[A-Z])+\.?", up)) or bool(re.fullmatch(r"[A-Z]\.?", up))

def normalize_initials(up: str) -> str:
    u = up.replace(".", "")
    if len(u) == 1:
        return u + "."
    return ".".join(list(u)) + "."

def split_suffix(tokens: List[str]) -> Tuple[List[str], Optional[str]]:
    if tokens:
        last = tokens[-1].rstrip(".").upper()
        if last in {s.rstrip(".") for s in SUFFIXES}:
            return tokens[:-1], last
    return tokens, None

def join_name(parts: List[str]) -> str:
    return " ".join(p for p in parts if p).strip()

def looks_like_human_token(tok: str) -> bool:
    tok = ocr_fix_token(tok)
    return bool(re.fullmatch(r"[A-Za-zÀ-ÖØ-öø-ÿ'’\-\.]+", tok))

def parse_person_fragment(fragment: str) -> Optional[str]:
    toks = [ocr_fix_token(t) for t in fragment.split()]
    out: List[str] = []
    for t in toks:
        up = t.upper().rstrip(".")
        if up in TITLES:
            out.append(t if t.endswith(".") else t + ".")
        elif is_initial_token(up):
            out.append(normalize_initials(up))
        else:
            out.append(t)
    return join_name(out) if out else None

def parse_rest_tokens(rest: str) -> Tuple[List[str], Optional[str]]:
    parts = collapse_ws(rest).split(" ")
    parts = [ocr_fix_token(p) for p in parts]
    out = []
    for p in parts:
        up = p.upper()
        if up in TITLES:
            out.append(p if p.endswith(".") else p + ".")
        elif is_initial_token(up):
            out.append(normalize_initials(up))
        else:
            out.append(p)
    out, suffix = split_suffix(out)
    return out, suffix

def looks_like_person_with_comma(raw: str) -> bool:
    s = collapse_ws(raw)
    if "," not in s:
        return False
    left, right = s.split(",", 1)
    left = collapse_ws(left); right = collapse_ws(right)
    if not left or not right:
        return False
    left_tokens = [ocr_fix_token(t) for t in left.split()]
    if len(left_tokens) > 4:
        return False
    if not all(looks_like_human_token(t) for t in left_tokens):
        return False
    right_clean = right.replace("&", " & ").replace("OR", " OR ").replace("/", " / ")
    rtoks = [t for t in right_clean.split() if t]
    if not rtoks:
        return False
    humanish = sum(looks_like_human_token(t) or t.upper() in {"&","OR","/"} for t in rtoks)
    return humanish >= 1

def looks_like_no_comma_person(raw: str) -> bool:
    s = collapse_ws(raw)
    if not s or "," in s:
        return False
    if any(d in s for d in ["&","/"]) or re.search(r"\bOR\b", s, flags=re.IGNORECASE):
        return False
    toks = tokens_upper(s)
    if len(toks) < 2 or len(toks) > 6:
        return False
    if toks[0] in SAINT_PREFIXES:
        return False
    tset = set(toks)
    if tset & (ENTITY_TYPES | BUSINESS_TERMS | ADDRESSY_TOKENS | PLACE_TOKENS | CEMETERY_WORDS):
        return False
    return all(looks_like_human_token(t) for t in s.split())


# =======================
# Business detection
# =======================

def is_letter_amp_company(s: str) -> bool:
    toks = tokens_upper(s)
    if "&" not in toks:
        return False
    return all(t == "&" or re.fullmatch(r"[A-Z]{1,3}\.?", t) for t in toks)

def brand_regex_hit(s: str) -> bool:
    return any(rx.search(s) for rx in BRAND_REGEXES)

def looks_like_business(raw: str, debug: Optional[Dict[str,int]] = None) -> bool:
    s = collapse_ws(raw)
    if not s:
        return False

    toks = tokens_upper(s)
    tset = set(toks)

    if tset & ENTITY_TYPES:
        if debug is not None: debug["entity_marker"] += 1
        return True

    if brand_regex_hit(s):
        if debug is not None: debug["brand_regex"] += 1
        return True

    if re.search(r"\bSTORE\b\s*#\s*\d+", s, flags=re.IGNORECASE):
        if debug is not None: debug["store_number"] += 1
        return True

    if tset & ADDRESSY_TOKENS:
        if debug is not None: debug["address_token"] += 1
        return True
    if has_digits(s):
        if debug is not None: debug["has_digits"] += 1
        return True

    if toks and toks[0] in SAINT_PREFIXES and "," not in s:
        if debug is not None: debug["saint_prefix"] += 1
        return True

    if is_letter_amp_company(s):
        if debug is not None: debug["letter_amp_company"] += 1
        return True

    if tset & CEMETERY_WORDS:
        if debug is not None: debug["cemetery"] += 1
        return True

    if (tset & BUSINESS_TERMS) or (tset & PLACE_TOKENS):
        if debug is not None: debug["biz_or_place_token"] += 1
        return True

    bi = set(ngrams(toks, 2)); tri = set(ngrams(toks, 3))
    if any(b in BUSINESS_NGRAMS for b in bi) or any(t in BUSINESS_NGRAMS for t in tri):
        if debug is not None: debug["biz_ngram"] += 1
        return True

    return False


# ==========================
# Couples / combinators
# ==========================

def split_delimited(s: str) -> Tuple[List[str], Optional[str]]:
    if "&" in s:
        parts = [collapse_ws(p) for p in re.split(r"\s*&\s*", s)]
        return [p for p in parts if p], "&"
    if re.search(r"\bOR\b", s, flags=re.IGNORECASE):
        parts = [collapse_ws(p) for p in re.split(r"\s*\bOR\b\s*", s, flags=re.IGNORECASE)]
        return [p for p in parts if p], "OR"
    if "/" in s:
        parts = [collapse_ws(p) for p in s.split("/") if p.strip()]
        return [p for p in parts if p], "/"
    return [s], None

def is_single_surname(s: str) -> bool:
    s = collapse_ws(s)
    toks = s.split()
    return len(toks) == 1 and bool(re.fullmatch(r"[A-Za-zÀ-ÖØ-öø-ÿ'’\-]+", toks[0]))

def build_pairwise(firsts: List[str], lasts: List[str]) -> Optional[str]:
    if not firsts or not lasts:
        return None
    if len(firsts) == len(lasts):
        return " & ".join(f"{f} {l}" for f, l in zip(firsts, lasts))
    if len(firsts) == 1:
        f = firsts[0]
        return " & ".join(f"{f} {l}" for l in lasts)
    if len(lasts) == 1:
        l = lasts[0]
        return " & ".join(f"{f} {l}" for f in firsts)
    m = min(len(firsts), len(lasts))
    paired = [f"{firsts[i]} {lasts[i]}" for i in range(m)]
    if len(firsts) > m:
        paired += [f"{f} {lasts[-1]}" for f in firsts[m:]]
    else:
        paired += [f"{firsts[-1]} {l}" for l in lasts[m:]]
    return " & ".join(paired)

def reformat_couple_with_comma(value: str) -> Optional[str]:
    s = collapse_ws(value)
    if "," not in s:
        return None

    left, right = s.split(",", 1)
    left = collapse_ws(left); right = collapse_ws(right)
    left = left.rstrip("&/").strip()
    right = right.rstrip("&/").strip()
    if not left or not right:
        return None

    left_parts, _ = split_delimited(left)
    right_parts, _ = split_delimited(right)

    left_norm = [parse_person_fragment(p) for p in left_parts]
    right_norm = [parse_person_fragment(p) for p in right_parts]

    if all(is_single_surname(p) for p in left_parts):
        lasts = [p for p in left_parts]
        firsts = [parse_person_fragment(p) for p in right_parts]
        if all(firsts):
            return build_pairwise(firsts, lasts)

    if is_single_surname(right) and len(right_parts) == 1:
        last = right_parts[0]
        firsts = [parse_person_fragment(p) for p in left_parts]
        if all(firsts):
            return build_pairwise(firsts, [last])

    if all(is_single_surname(p) for p in left_parts) and len(right_parts) >= 1:
        firsts = [parse_person_fragment(p) for p in right_parts]
        lasts = [p for p in left_parts]
        if all(firsts):
            return build_pairwise(firsts, lasts)

    if all(is_single_surname(p) for p in right_parts) and len(left_parts) >= 1:
        firsts = [parse_person_fragment(p) for p in left_parts]
        lasts = [p for p in right_parts]
        if all(firsts):
            return build_pairwise(firsts, lasts)

    return None

def looks_like_ampersand_people_no_comma(raw: str) -> bool:
    s = collapse_ws(raw)
    if "," in s:
        return False
    if not any(d in s for d in ["&","/"]) and re.search(r"\bOR\b", s, flags=re.IGNORECASE) is None:
        return False
    parts, _ = split_delimited(s)
    if len(parts) < 2:
        return False
    for side in parts:
        toks = tokens_upper(side)
        if not (1 <= len(toks) <= 4):
            return False
        if set(toks) & (ENTITY_TYPES | BUSINESS_TERMS | ADDRESSY_TOKENS | PLACE_TOKENS | CEMETERY_WORDS):
            return False
        if not all(looks_like_human_token(t) for t in side.split()):
            return False
    return True

def reformat_couple_no_comma_multi(value: str) -> Optional[str]:
    if not looks_like_ampersand_people_no_comma(value):
        return None
    parts, _ = split_delimited(value)
    norm = [parse_person_fragment(p) for p in parts]
    if all(norm):
        return " & ".join(norm)
    return None


# ==========================
# Reformat single person
# ==========================

def reformat_single_last_comma_first(value: str) -> Optional[str]:
    s = collapse_ws(value)
    if "," not in s:
        return None

    m = re.fullmatch(rf"\s*({NAME_ALLOWED}+?)\s*,\s*([A-Za-z\.]+)\s*,\s*({NAME_ALLOWED}+)\s*", s)
    if m:
        last = collapse_ws(m.group(1))
        mid = collapse_ws(m.group(2)).rstrip(".").upper()
        rest = collapse_ws(m.group(3))
        first_tokens, _ = parse_rest_tokens(rest)
        if first_tokens and mid in {sfx.rstrip(".") for sfx in SUFFIXES}:
            full = join_name(first_tokens + [last])
            return f"{full}, {mid}"

    last_part, rest = s.split(",", 1)
    last = collapse_ws(last_part)
    rest = collapse_ws(rest)
    if not last or not rest:
        return None

    first_tokens, suffix = parse_rest_tokens(rest)
    if not first_tokens:
        return None

    full = join_name(first_tokens + [last])
    if suffix:
        full = f"{full}, {suffix}"
    return full if full else None


# ==========================
# Orchestrator
# ==========================

def reformat_person_name(value: str, dbg: Optional[Dict[str,int]] = None) -> Optional[str]:
    if not isinstance(value, str):
        return None
    s = collapse_ws(value)
    if not s:
        return None

    toks = tokens_upper(s)

    # 1) Try parsing comma-people first (unless legal entity markers).
    if looks_like_person_with_comma(s) and not (set(toks) & ENTITY_TYPES):
        couple_general = reformat_couple_with_comma(s)
        if couple_general:
            if dbg is not None: dbg["parsed_couple_comma"] += 1
            return couple_general
        std = reformat_single_last_comma_first(s)
        if std:
            if dbg is not None: dbg["parsed_single_comma"] += 1
            return std

    # 2) Hard business veto
    if looks_like_business(s, dbg):
        return None

    # 3) No-comma couples (&/OR//)
    couple_multi = reformat_couple_no_comma_multi(s)
    if couple_multi:
        if dbg is not None: dbg["parsed_couple_nocomma"] += 1
        return couple_multi

    # 4) No-comma single person
    if looks_like_no_comma_person(s):
        if dbg is not None: dbg["parsed_nocomma_person"] += 1
        return s

    if dbg is not None: dbg["unhandled"] += 1
    return None


# =================
# I/O + main logic
# =================

def unique_column_name(df: pd.DataFrame, base: str = "Full Name") -> str:
    name = base
    i = 2
    while name in df.columns:
        name = f"{base} ({i})"
        i += 1
    return name

def load_table(path: Path) -> pd.DataFrame:
    ext = path.suffix.lower()
    if ext == ".csv":
        try:
            return pd.read_csv(path)
        except UnicodeDecodeError:
            return pd.read_csv(path, encoding="utf-8-sig")
    elif ext in (".xlsx", ".xls"):
        return pd.read_excel(path)
    else:
        raise ValueError("Unsupported file type. Use CSV, XLS, or XLSX.")

def save_table(df: pd.DataFrame, original_path: Path) -> Path:
    out_path = original_path.with_name(f"{original_path.stem}_processed{original_path.suffix}")
    ext = original_path.suffix.lower()
    if ext == ".csv":
        df.to_csv(out_path, index=False)
    elif ext in (".xlsx", ".xls"):
        df.to_excel(out_path, index=False)
    else:
        raise ValueError("Unsupported file type for saving.")
    return out_path

def main():
    # 1) Ask for file input
    file_input = input("INPUT FILE: ").strip()
    if (file_input.startswith('"') and file_input.endswith('"')) or (
        file_input.startswith("'") and file_input.endswith("'")
    ):
        file_input = file_input[1:-1]

    in_path = Path(file_input).expanduser()
    if not in_path.exists() or not in_path.is_file():
        print("ERROR: File not found.")
        sys.exit(1)

    try:
        df = load_table(in_path)
    except Exception as e:
        print(f"ERROR: Failed to read file: {e}")
        sys.exit(1)

    # 2) Show numbered list of headers and ask which column
    print("\nCOLUMNS:")
    for i, col in enumerate(df.columns, start=1):
        print(f"{i}. {col}")

    try:
        selection = int(input("\nWHICH COLUMN SHOULD I SCAN AND PROCESS? ").strip())
        if not (1 <= selection <= len(df.columns)):
            raise ValueError
    except ValueError:
        print("ERROR: Please enter a valid column number.")
        sys.exit(1)

    col_idx = selection - 1

    # >>> NEW: Create Title column BEFORE processing and move any 'VACANT' entries <<<
    title_col = unique_column_name(df, "Title")
    df[title_col] = pd.NA  # append at end (after current columns)

    # Move rows containing 'VACANT' to Title, clear original
    for i in range(len(df)):
        val = df.iat[i, col_idx]
        if pd.isna(val):
            continue
        s = str(val)
        if "VACANT" in s.upper():
            df.iat[i, df.columns.get_loc(title_col)] = s
            df.iat[i, col_idx] = pd.NA  # clear original

    # 3) Append 'Full Name' column and process remaining rows
    full_col = unique_column_name(df, "Full Name")
    df[full_col] = pd.NA  # append at end

    processed = 0
    dbg = {
        "parsed_couple_comma": 0,
        "parsed_single_comma": 0,
        "parsed_couple_nocomma": 0,
        "parsed_nocomma_person": 0,
        "entity_marker": 0,
        "store_number": 0,
        "address_token": 0,
        "has_digits": 0,
        "saint_prefix": 0,
        "letter_amp_company": 0,
        "cemetery": 0,
        "biz_or_place_token": 0,
        "biz_ngram": 0,
        "brand_regex": 0,
        "unhandled": 0,
    }

    for i in range(len(df)):
        val = df.iat[i, col_idx]
        if pd.isna(val):
            continue
        reformatted = reformat_person_name(str(val), dbg)
        if reformatted:
            df.iat[i, df.columns.get_loc(full_col)] = reformatted
            df.iat[i, col_idx] = pd.NA
            processed += 1

    # Save with _processed appended
    try:
        out_path = save_table(df, in_path)
    except Exception as e:
        print(f"ERROR: Failed to save the processed file: {e}")
        sys.exit(1)

    # 4) Completion message + diagnostics + exit prompt
    print(f"\n{processed} records were processed.")
    print(f"Saved to: {out_path}")

    print("\nDiagnostics (skip reasons / parse paths):")
    for k in [
        "parsed_single_comma","parsed_couple_comma","parsed_couple_nocomma","parsed_nocomma_person",
        "entity_marker","brand_regex","biz_ngram","biz_or_place_token",
        "address_token","has_digits","store_number","saint_prefix","letter_amp_company","cemetery","unhandled"
    ]:
        print(f"  {k}: {dbg[k]}")

    while True:
        key = input("PRESS X TO TERMINATE... ").strip().lower()
        if key == "x":
            break

if __name__ == "__main__":
    main()
