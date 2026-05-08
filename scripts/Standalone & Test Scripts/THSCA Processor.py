#!/usr/bin/env python3
"""
THSCA XLSX Processor (v2 - fixes 'nan' issues)
----------------------------------------------
Key fixes:
- Normalize missing values BEFORE casting to string to avoid literal "nan" text.
- Use safe joining that treats None/NaN/"nan"/"None" as empty strings.
- Trim whitespace consistently after joins.
"""

import re
import sys
from pathlib import Path
from typing import Optional

try:
    import pandas as pd
    import numpy as np
except ImportError:
    print("This script requires the 'pandas' and 'openpyxl' packages. Install with: pip install pandas openpyxl")
    sys.exit(1)

try:
    from PyQt5.QtWidgets import (
        QApplication,
        QFileDialog,
        QLabel,
        QMessageBox,
        QPushButton,
        QVBoxLayout,
        QWidget,
    )
except ImportError:
    print("This script requires PyQt5. Install with: pip install pyqt5")
    sys.exit(1)


def find_col(df: pd.DataFrame, target: str) -> str:
    """
    Finds a column name in df that matches target case-insensitively
    and ignoring leading/trailing spaces.
    Raises ValueError if not found.
    """
    tnorm = target.strip().lower()
    for col in df.columns:
        if str(col).strip().lower() == tnorm:
            return col
    raise ValueError(f"Required column '{target}' not found in the sheet.")


def sanitize_cell(x) -> str:
    """Convert cell to a clean string; treat NaN/None/'nan'/'none' as empty."""
    if x is None or (isinstance(x, float) and np.isnan(x)):
        return ""
    s = str(x).strip()
    if s.lower() in {"nan", "none", "nat"}:
        return ""
    return s


def trim_join(*parts) -> str:
    """Join string parts with a single space, skipping empties, and trim the result."""
    cleaned = [sanitize_cell(p) for p in parts if sanitize_cell(p)]
    return " ".join(cleaned).strip()


def process_thsca_xlsx(xlsx_path: Path, job_number: str = "00000") -> Path:
    if not xlsx_path.exists() or xlsx_path.is_dir() or xlsx_path.suffix.lower() != ".xlsx":
        raise ValueError("Selected file must be an existing .xlsx file.")

    if not re.fullmatch(r"\d{5}", job_number):
        raise ValueError("Job number must be exactly five digits.")

    try:
        # Read with object dtype, keep NaN as NaN, then sanitize
        df = pd.read_excel(xlsx_path, dtype=object)
    except Exception as e:
        raise RuntimeError(f"Failed to read Excel file: {e}") from e

    # Sanitize every cell to avoid literal 'nan' text
    df = df.applymap(sanitize_cell)

    # --- Step 2: Build FULL NAME for blanks from FIRST, MIDDLE, LAST, SUFFIX ---
    try:
        col_fullname = find_col(df, "FULL NAME")
        col_first = find_col(df, "FIRST")
        col_middle = find_col(df, "MIDDLE")
        col_last = find_col(df, "LAST")
        col_suffix = find_col(df, "SUFFIX")
    except ValueError as e:
        raise RuntimeError(str(e)) from e

    # Only fill where FULL NAME is blank
    mask_blank_full = df[col_fullname].str.strip().eq("")
    if mask_blank_full.any():
        df.loc[mask_blank_full, col_fullname] = [
            trim_join(f, m, l, s)
            for f, m, l, s in zip(
                df.loc[mask_blank_full, col_first],
                df.loc[mask_blank_full, col_middle],
                df.loc[mask_blank_full, col_last],
                df.loc[mask_blank_full, col_suffix],
            )
        ]

    # --- Step 3: Insert combined address column after ADDRESS LINE 2 and before CITY ---
    try:
        col_addr1 = find_col(df, "ADDRESS LINE 1")
        col_addr2 = find_col(df, "ADDRESS LINE 2")
        _ = find_col(df, "CITY")  # just to ensure it exists per spec
    except ValueError as e:
        raise RuntimeError(str(e)) from e

    # Combine safely (no 'nan')
    combined_series = [
        trim_join(a1, a2) for a1, a2 in zip(df[col_addr1], df[col_addr2])
    ]

    # Insert new blank-named column after ADDRESS LINE 2
    insert_idx = df.columns.get_loc(col_addr2) + 1
    df.insert(insert_idx, "", combined_series)

    # --- Step 4: Delete specified columns ---
    cols_to_drop = [col_first, col_middle, col_last, col_suffix, col_addr1, col_addr2]
    existing = [c for c in cols_to_drop if c in df.columns]
    if existing:
        df.drop(columns=existing, inplace=True)

    # --- Step 5: Rename columns ---
    # Find our blank-named column and rename to 'ADDRESS LINE 1'
    blank_cols = [c for c in df.columns if str(c).strip() == ""]
    if not blank_cols:
        raise RuntimeError("Could not locate the combined address column to rename.")
    combined_col_actual = blank_cols[0]

    rename_map = {combined_col_actual: "ADDRESS LINE 1"}

    # Optional renames if present
    try:
        rename_map[find_col(df, "Individual__PrimaryOrganization__Name")] = "BUSINESS"
    except ValueError:
        pass

    try:
        rename_map[find_col(df, "ZIP")] = "ZIP CODE"
    except ValueError:
        pass

    df.rename(columns=rename_map, inplace=True)

    # --- Step 6: Save CSV ---
    out_name = f"{job_number} THSCA.csv"
    out_path = xlsx_path.parent / out_name

    try:
        df.to_csv(out_path, index=False, encoding="utf-8-sig")
    except Exception as e:
        raise RuntimeError(f"Failed to save CSV: {e}") from e

    return out_path


class THSCAProcessorWindow(QWidget):
    def __init__(self) -> None:
        super().__init__()
        self.selected_file: Optional[Path] = None
        self._setup_ui()

    def _setup_ui(self) -> None:
        self.setWindowTitle("THSCA Processor")
        self.setFixedSize(480, 170)

        layout = QVBoxLayout()
        layout.setSpacing(10)
        layout.setContentsMargins(18, 18, 18, 18)

        self.file_label = QLabel("No .xlsx file selected.")
        self.file_label.setWordWrap(True)
        layout.addWidget(self.file_label)

        self.choose_button = QPushButton("CHOOSE FILE")
        self.choose_button.clicked.connect(self._choose_file)
        layout.addWidget(self.choose_button)

        self.process_button = QPushButton("PROCESS")
        self.process_button.setEnabled(False)
        self.process_button.clicked.connect(self._process_file)
        layout.addWidget(self.process_button)

        self.setLayout(layout)

    def _choose_file(self) -> None:
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Select THSCA Excel File",
            "",
            "Excel Files (*.xlsx)",
        )
        if not file_path:
            return

        selected = Path(file_path).resolve()
        self.selected_file = selected
        self.file_label.setText(str(selected))
        self.process_button.setEnabled(True)
        print(f"Selected file: {selected}")

    def _process_file(self) -> None:
        if not self.selected_file:
            QMessageBox.warning(self, "THSCA Processor", "Please choose an .xlsx file first.")
            return

        try:
            out_path = process_thsca_xlsx(self.selected_file, job_number="00000")
        except Exception as exc:
            QMessageBox.critical(self, "Processing Error", str(exc))
            print(f"ERROR: {exc}")
            return

        QMessageBox.information(
            self,
            "THSCA Processor",
            f"PROCESS COMPLETE!\n\nOutput file:\n{out_path}",
        )
        print(f"PROCESS COMPLETE! Output: {out_path}")


def main() -> None:
    app = QApplication(sys.argv)
    window = THSCAProcessorWindow()
    window.show()
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
