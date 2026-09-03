from __future__ import annotations

import csv
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parents[1]
PROJECT_ROOT = SCRIPT_DIR.parents[2]
INITIAL = SCRIPT_DIR / "01 INITIAL.py"
FINAL = SCRIPT_DIR / "02 FINAL PROCESS.py"
HEADERS = [
    "Provider First",
    "Provider Last",
    "Location Address 1",
    "Location Address 2",
    "Location City",
    "Location State",
    "Location Zip",
    "Payee Name",
]
INPUT_HEADERS = [
    "MATCHID",
    "Address Line 1",
    "Address Line 2",
    "City",
    "State",
    "ZIP Code",
    "Business",
    "Addressee",
]
MOVE_HEADERS = [
    "MATCHID",
    "Address Line 1",
    "Address Line 2",
    "City",
    "State",
    "ZIP Code",
]
NOT_MAILED_HEADERS = ["MATCHID", "User Text 3"]
OUTPUT_HEADERS = [
    "MATCHID",
    "Addressee",
    "ZIP Code",
    "Pallet Number",
    "Presort Value",
]
CURRENT_HEADERS = [
    "Current Address Line 1",
    "Current Address Line 2",
    "Current City",
    "Current State",
    "Current ZIP Code",
]


class TMMAScriptTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_directory = tempfile.TemporaryDirectory(prefix="goji_tmma_test_")
        self.base = self.create_base(Path(self.temp_directory.name))

    def tearDown(self) -> None:
        self.temp_directory.cleanup()

    @staticmethod
    def create_base(base: Path) -> Path:
        (base / "RAW INPUT").mkdir(parents=True)
        (base / "DATA").mkdir(parents=True)
        return base

    @staticmethod
    def run_initial(
        base: Path,
    ) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [sys.executable, str(INITIAL), "--base-path", str(base)],
            text=True,
            capture_output=True,
            check=False,
        )

    @staticmethod
    def run_final(
        base: Path, job: str, count: str
    ) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [sys.executable, str(FINAL), job, count, "--base-path", str(base)],
            text=True,
            capture_output=True,
            check=False,
        )

    @staticmethod
    def write_csv(path: Path, headers: list[str], rows: list[list[str]]) -> Path:
        with path.open("w", encoding="utf-8-sig", newline="") as stream:
            writer = csv.writer(stream, lineterminator="\n")
            writer.writerow(headers)
            writer.writerows(rows)
        return path

    @staticmethod
    def read_csv(path: Path) -> tuple[list[str], list[list[str]]]:
        with path.open("r", encoding="utf-8-sig", newline="") as stream:
            parsed = list(csv.reader(stream))
        return parsed[0], [row for row in parsed[1:] if row]

    @staticmethod
    def source_rows(count: int, natural_duplicates: bool = False) -> list[list[str]]:
        rows: list[list[str]] = []
        for index in range(count):
            suffix = 0 if natural_duplicates else index
            rows.append(
                [
                    "  Jane " if natural_duplicates else f"First {index}",
                    " Doe  " if natural_duplicates else f"Last {index}",
                    f"{100 + suffix} Main St",
                    "  Suite 2 " if suffix % 2 == 0 else "",
                    "Boston",
                    "MA",
                    "01748" if suffix == 0 else f"0{1800 + suffix}",
                    "  Example Practice  " if natural_duplicates else f"Practice {index}",
                ]
            )
        return rows

    def write_source(
        self,
        rows: list[list[str]] | None = None,
        headers: list[str] | None = None,
        base: Path | None = None,
    ) -> Path:
        selected_base = base or self.base
        selected_headers = headers or HEADERS
        selected_rows = rows if rows is not None else self.source_rows(1)
        return self.write_csv(
            selected_base / "RAW INPUT" / "source.csv",
            selected_headers,
            selected_rows,
        )

    def initialize(
        self,
        count: int,
        *,
        natural_duplicates: bool = False,
        base: Path | None = None,
    ) -> tuple[list[list[str]], list[str]]:
        selected_base = base or self.base
        rows = self.source_rows(count, natural_duplicates)
        self.write_source(rows, base=selected_base)
        result = self.run_initial(selected_base)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        mapping_headers, mapping_rows = self.read_csv(
            selected_base / "DATA" / "TMMA SOURCE.csv"
        )
        self.assertEqual(mapping_headers, ["MATCHID", *HEADERS])
        return rows, [row[0] for row in mapping_rows]

    def write_exports(
        self,
        output_ids: list[str],
        not_mailed_ids: list[str],
        move_ids: list[str],
        *,
        base: Path | None = None,
        not_mailed_status: str = "14",
        pallet_values: dict[str, str] | None = None,
    ) -> None:
        selected_base = base or self.base
        data = selected_base / "DATA"
        selected_pallet_values = pallet_values or {}
        self.write_csv(
            data / "OUTPUT.csv",
            OUTPUT_HEADERS,
            [
                [
                    match_id,
                    f"Recipient {index}",
                    f"0{1700 + index}",
                    selected_pallet_values.get(match_id, str(index + 1)),
                    f"00{index}",
                ]
                for index, match_id in enumerate(output_ids)
            ],
        )
        self.write_csv(
            data / "NOT MAILED.csv",
            NOT_MAILED_HEADERS,
            [[match_id, not_mailed_status] for match_id in not_mailed_ids],
        )
        self.write_csv(
            data / "MOVE UPDATES.csv",
            MOVE_HEADERS,
            [
                [
                    match_id,
                    f"{900 + index} Current Ave",
                    f"Unit {index}",
                    "Cambridge",
                    "MA",
                    f"02{100 + index}",
                ]
                for index, match_id in enumerate(move_ids)
            ],
        )

    def prepare_final(
        self,
        total: int = 3,
        output_count: int = 2,
        move_count: int = 0,
        *,
        natural_duplicates: bool = False,
        base: Path | None = None,
    ) -> tuple[list[list[str]], list[str], list[str], list[str], list[str]]:
        selected_base = base or self.base
        source_rows, ids = self.initialize(
            total, natural_duplicates=natural_duplicates, base=selected_base
        )
        output_ids = ids[:output_count]
        not_mailed_ids = ids[output_count:]
        move_ids = not_mailed_ids[:move_count]
        self.write_exports(
            output_ids, not_mailed_ids, move_ids, base=selected_base
        )
        return source_rows, ids, output_ids, not_mailed_ids, move_ids

    def assert_final_failure(
        self,
        result: subprocess.CompletedProcess[str],
        message: str,
        count: int,
        *,
        base: Path | None = None,
        expect_output: bool = True,
    ) -> None:
        selected_base = base or self.base
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn(message, result.stdout)
        self.assertEqual(
            (selected_base / "DATA" / "OUTPUT.csv").exists(), expect_output
        )
        self.assertFalse((selected_base / "DATA" / "source_MERGED.csv").exists())
        self.assertFalse(
            (selected_base / "DATA" / f"55564 TM MA_{count}.csv").exists()
        )

    def isolated_base(self, temporary: tempfile.TemporaryDirectory[str]) -> Path:
        return self.create_base(Path(temporary.name))

    def test_initial_creates_verified_source_and_input(self) -> None:
        source = self.write_source(self.source_rows(3))
        original_bytes = source.read_bytes()
        result = self.run_initial(self.base)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(source.read_bytes(), original_bytes)

        source_headers, source_rows = self.read_csv(
            self.base / "DATA" / "TMMA SOURCE.csv"
        )
        input_headers, input_rows = self.read_csv(self.base / "DATA" / "INPUT.csv")
        self.assertEqual(source_headers, ["MATCHID", *HEADERS])
        self.assertEqual(input_headers, INPUT_HEADERS)
        self.assertEqual(len(source_rows), 3)
        self.assertEqual(len(input_rows), 3)

        source_ids = [row[0] for row in source_rows]
        self.assertEqual(source_ids, [row[0] for row in input_rows])
        self.assertEqual(len(source_ids), len(set(source_ids)))
        self.assertTrue(all(re.fullmatch(r"[A-Z]{2}[0-9]{4}", value) for value in source_ids))
        self.assertEqual([row[1:] for row in source_rows], self.source_rows(3))
        self.assertEqual(source_rows[0][HEADERS.index("Location Zip") + 1], "01748")
        self.assertEqual(input_rows[0][INPUT_HEADERS.index("ZIP Code")], "01748")
        self.assertEqual(input_rows[0][INPUT_HEADERS.index("Addressee")], "First 0 Last 0")
        self.assertNotIn("nan", [cell for row in source_rows + input_rows for cell in row])

    def test_initial_rejects_generated_header_collisions_case_insensitively(self) -> None:
        conflicts = [
            "MATCHID",
            "Mailed",
            "current address line 1",
            "Current Address Line 2",
            "CURRENT CITY",
            "Current State",
            "Current ZIP Code",
        ]
        for conflict in conflicts:
            with self.subTest(conflict=conflict), tempfile.TemporaryDirectory(
                prefix="goji_tmma_collision_"
            ) as name:
                base = self.create_base(Path(name))
                headers = [*HEADERS, conflict]
                row = [*self.source_rows(1)[0], "customer value"]
                self.write_source([row], headers=headers, base=base)
                result = self.run_initial(base)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("conflicting generated header", result.stdout)
                self.assertIn(conflict, result.stdout)
                self.assertFalse((base / "DATA" / "TMMA SOURCE.csv").exists())
                self.assertFalse((base / "DATA" / "INPUT.csv").exists())

    def test_initial_rerun_reuses_matchids_for_unchanged_source(self) -> None:
        self.initialize(3)
        before_source = (self.base / "DATA" / "TMMA SOURCE.csv").read_bytes()
        _, before_rows = self.read_csv(self.base / "DATA" / "TMMA SOURCE.csv")
        result = self.run_initial(self.base)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        _, after_rows = self.read_csv(self.base / "DATA" / "TMMA SOURCE.csv")
        self.assertEqual([row[0] for row in before_rows], [row[0] for row in after_rows])
        self.assertIn("Reusing existing MATCHIDs", result.stdout)
        self.assertEqual(
            (self.base / "DATA" / "TMMA SOURCE.csv").read_bytes(), before_source
        )

    def test_initial_downstream_export_blocks_stale_mapping_regeneration(self) -> None:
        self.initialize(2)
        mapping = self.base / "DATA" / "TMMA SOURCE.csv"
        input_path = self.base / "DATA" / "INPUT.csv"
        mapping_before = mapping.read_bytes()
        input_before = input_path.read_bytes()
        self.write_csv(
            self.base / "DATA" / "OUTPUT.csv",
            ["MATCHID", "Addressee"],
            [["ZZ9999", "stale"]],
        )
        changed = self.source_rows(2)
        changed[0][HEADERS.index("Payee Name")] = "Changed Practice"
        self.write_source(changed)
        result = self.run_initial(self.base)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Cannot regenerate MATCHIDs", result.stdout)
        self.assertIn("OUTPUT.csv", result.stdout)
        self.assertEqual(mapping.read_bytes(), mapping_before)
        self.assertEqual(input_path.read_bytes(), input_before)

    def test_initial_source_change_without_exports_regenerates_safely(self) -> None:
        self.initialize(2)
        changed = self.source_rows(2)
        changed[1][HEADERS.index("Payee Name")] = "Changed Practice"
        self.write_source(changed)
        result = self.run_initial(self.base)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("do not match the current RAW INPUT", result.stdout)
        _, mapping_rows = self.read_csv(self.base / "DATA" / "TMMA SOURCE.csv")
        self.assertEqual([row[1:] for row in mapping_rows], changed)

    def test_initial_validation_failure_preserves_existing_pair(self) -> None:
        mapping = self.base / "DATA" / "TMMA SOURCE.csv"
        input_path = self.base / "DATA" / "INPUT.csv"
        mapping.write_text("mapping sentinel", encoding="utf-8")
        input_path.write_text("input sentinel", encoding="utf-8")
        self.write_source([self.source_rows(1)[0][:-1]], headers=HEADERS[:-1])
        result = self.run_initial(self.base)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Missing required header(s): Payee Name", result.stdout)
        self.assertEqual(mapping.read_text(encoding="utf-8"), "mapping sentinel")
        self.assertEqual(input_path.read_text(encoding="utf-8"), "input sentinel")

    def test_initial_source_selection_failures(self) -> None:
        result = self.run_initial(self.base)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("No source file exists directly", result.stdout)
        self.write_source()
        (self.base / "RAW INPUT" / "second.csv").write_text("x\n", encoding="utf-8")
        result = self.run_initial(self.base)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Expected exactly one source file", result.stdout)

    def test_final_happy_path_creates_both_verified_customer_files(self) -> None:
        source_rows, ids, output_ids, not_mailed_ids, move_ids = self.prepare_final(
            total=10, output_count=7, move_count=2
        )
        result = self.run_final(self.base, "55564", "7")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

        output_target = self.base / "DATA" / "55564 TM MA_7.csv"
        merged_target = self.base / "DATA" / "source_MERGED.csv"
        self.assertTrue(output_target.exists())
        self.assertTrue(merged_target.exists())
        self.assertFalse((self.base / "DATA" / "OUTPUT.csv").exists())
        self.assertIn(f"TMMA_FINAL_OUTPUT_FILE={output_target.resolve()}", result.stdout)
        self.assertIn(f"TMMA_FINAL_MERGED_FILE={merged_target.resolve()}", result.stdout)

        output_headers, output_rows = self.read_csv(output_target)
        self.assertNotIn("MATCHID", output_headers)
        self.assertEqual(
            output_headers, ["Addressee", "ZIP Code", "Pallet Number", "Presort Value"]
        )
        self.assertEqual(len(output_rows), 7)
        self.assertEqual(output_rows[0][-1], "000")

        merged_headers, merged_rows = self.read_csv(merged_target)
        self.assertEqual(merged_headers, [*HEADERS, "mailed", *CURRENT_HEADERS])
        self.assertNotIn("MATCHID", merged_headers)
        self.assertEqual(len(merged_rows), 10)
        self.assertEqual([row[: len(HEADERS)] for row in merged_rows], source_rows)
        status_index = merged_headers.index("mailed")
        self.assertEqual(
            [row[status_index] for row in merged_rows], ["13"] * 7 + ["14"] * 3
        )
        self.assertEqual(
            sum(bool(row[merged_headers.index("Current Address Line 1")]) for row in merged_rows),
            2,
        )
        self.assertEqual(set(move_ids), set(not_mailed_ids[:2]))
        self.assertEqual(set(output_ids) | set(not_mailed_ids), set(ids))

    def test_final_sanitizes_pallet_minus_one_as_automatic_14(self) -> None:
        source_rows, ids = self.initialize(10)
        raw_output_ids = ids[:8]
        manual_not_mailed_ids = ids[8:]
        pallet_minus_one_id = raw_output_ids[-1]
        self.write_exports(
            raw_output_ids,
            manual_not_mailed_ids,
            [pallet_minus_one_id],
            pallet_values={pallet_minus_one_id: " -1.0 "},
        )
        raw_headers, raw_output_rows = self.read_csv(self.base / "DATA" / "OUTPUT.csv")
        retained_rows = [
            row for row in raw_output_rows if row[raw_headers.index("MATCHID")] != pallet_minus_one_id
        ]

        result = self.run_final(self.base, "55564", "7")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn(
            "Removed 1 OUTPUT record with Pallet Number = -1", result.stdout
        )
        self.assertIn("Sanitized OUTPUT.csv contains 7 mailed records", result.stdout)

        customer_headers, customer_rows = self.read_csv(
            self.base / "DATA" / "55564 TM MA_7.csv"
        )
        matchid_index = raw_headers.index("MATCHID")
        self.assertEqual(
            customer_headers,
            [header for index, header in enumerate(raw_headers) if index != matchid_index],
        )
        self.assertEqual(
            customer_rows,
            [
                [value for index, value in enumerate(row) if index != matchid_index]
                for row in retained_rows
            ],
        )
        self.assertNotIn("MATCHID", customer_headers)
        customer_pallet_index = customer_headers.index("Pallet Number")
        self.assertTrue(
            all(row[customer_pallet_index].strip() not in {"-1", "-1.0"} for row in customer_rows)
        )

        merged_headers, merged_rows = self.read_csv(
            self.base / "DATA" / "source_MERGED.csv"
        )
        self.assertEqual([row[: len(HEADERS)] for row in merged_rows], source_rows)
        status_index = merged_headers.index("mailed")
        self.assertEqual([row[status_index] for row in merged_rows], ["13"] * 7 + ["14"] * 3)
        pallet_source_index = ids.index(pallet_minus_one_id)
        self.assertEqual(merged_rows[pallet_source_index][status_index], "14")
        current_address_index = merged_headers.index("Current Address Line 1")
        self.assertNotEqual(merged_rows[pallet_source_index][current_address_index], "")

    def test_final_handles_multiple_pallet_minus_one_and_manual_overlap(self) -> None:
        _, ids = self.initialize(10)
        automatic_ids = [ids[1], ids[4], ids[8]]
        self.write_exports(
            ids,
            [],
            [],
            pallet_values={
                automatic_ids[0]: "-1",
                automatic_ids[1]: " -1.0 ",
                automatic_ids[2]: "-01.000",
            },
        )
        result = self.run_final(self.base, "55564", "7")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("Removed 3 OUTPUT records", result.stdout)
        customer_headers, customer_rows = self.read_csv(
            self.base / "DATA" / "55564 TM MA_7.csv"
        )
        self.assertEqual(len(customer_rows), 7)
        pallet_index = customer_headers.index("Pallet Number")
        self.assertTrue(all(row[pallet_index] not in {"-1", " -1.0 ", "-01.000"} for row in customer_rows))
        merged_headers, merged_rows = self.read_csv(
            self.base / "DATA" / "source_MERGED.csv"
        )
        status_index = merged_headers.index("mailed")
        self.assertEqual(sum(row[status_index] == "13" for row in merged_rows), 7)
        self.assertEqual(sum(row[status_index] == "14" for row in merged_rows), 3)
        self.assertTrue(all(merged_rows[ids.index(match_id)][status_index] == "14" for match_id in automatic_ids))

        with tempfile.TemporaryDirectory(prefix="goji_tmma_pallet_overlap_") as name:
            base = self.create_base(Path(name))
            _, overlap_ids = self.initialize(4, base=base)
            overlapping_id = overlap_ids[2]
            self.write_exports(
                overlap_ids[:3],
                [overlapping_id, overlap_ids[3]],
                [],
                base=base,
                pallet_values={overlapping_id: "-1"},
            )
            overlap_result = self.run_final(base, "55564", "2")
            self.assertEqual(
                overlap_result.returncode,
                0,
                overlap_result.stdout + overlap_result.stderr,
            )
            self.assertIn("counted once as mailed=14", overlap_result.stdout)
            headers, rows = self.read_csv(base / "DATA" / "source_MERGED.csv")
            status_index = headers.index("mailed")
            self.assertEqual([row[status_index] for row in rows], ["13", "13", "14", "14"])
            self.assertEqual(len(rows), 4)

    def test_final_zero_pallet_minus_one_preserves_retained_output_values(self) -> None:
        _, ids = self.initialize(4)
        self.write_exports(ids[:3], ids[3:], [])
        raw_headers, raw_rows = self.read_csv(self.base / "DATA" / "OUTPUT.csv")
        result = self.run_final(self.base, "55564", "3")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertNotIn("Removed ", result.stdout)
        customer_headers, customer_rows = self.read_csv(
            self.base / "DATA" / "55564 TM MA_3.csv"
        )
        matchid_index = raw_headers.index("MATCHID")
        self.assertEqual(
            customer_headers,
            [header for index, header in enumerate(raw_headers) if index != matchid_index],
        )
        self.assertEqual(
            customer_rows,
            [
                [value for index, value in enumerate(row) if index != matchid_index]
                for row in raw_rows
            ],
        )

    def test_final_requires_pallet_header_before_altering_output(self) -> None:
        self.prepare_final(total=3, output_count=2)
        output_path = self.base / "DATA" / "OUTPUT.csv"
        headers, rows = self.read_csv(output_path)
        pallet_index = headers.index("Pallet Number")
        self.write_csv(
            output_path,
            [header for index, header in enumerate(headers) if index != pallet_index],
            [
                [value for index, value in enumerate(row) if index != pallet_index]
                for row in rows
            ],
        )
        original_bytes = output_path.read_bytes()
        result = self.run_final(self.base, "55564", "2")
        self.assert_final_failure(result, "Pallet Number", 2)
        self.assertEqual(output_path.read_bytes(), original_bytes)

    def test_final_atomic_sanitization_preserves_working_output_contents(self) -> None:
        _, ids = self.initialize(5)
        pallet_minus_one_id = ids[3]
        self.write_exports(
            ids[:4],
            ids[4:],
            [],
            pallet_values={pallet_minus_one_id: "-1.000"},
        )
        output_path = self.base / "DATA" / "OUTPUT.csv"
        headers, rows = self.read_csv(output_path)
        rows[0][headers.index("Addressee")] = "  Keep Exact Spacing  "
        rows[0][headers.index("ZIP Code")] = "00123"
        rows[0][headers.index("Presort Value")] = "0007"
        self.write_csv(output_path, headers, rows)
        expected_rows = [
            row for row in rows if row[headers.index("MATCHID")] != pallet_minus_one_id
        ]

        result = self.run_final(self.base, "55564", "4")
        self.assert_final_failure(result, "Count mismatch", 4)
        sanitized_headers, sanitized_rows = self.read_csv(output_path)
        self.assertEqual(sanitized_headers, headers)
        self.assertEqual(sanitized_rows, expected_rows)
        self.assertIn("MATCHID", sanitized_headers)
        pallet_index = sanitized_headers.index("Pallet Number")
        self.assertTrue(all(row[pallet_index] != "-1.000" for row in sanitized_rows))
        self.assertEqual(sanitized_rows[0][sanitized_headers.index("ZIP Code")], "00123")
        self.assertEqual(sanitized_rows[0][sanitized_headers.index("Presort Value")], "0007")
        self.assertEqual(
            list((self.base / "DATA").glob(".OUTPUT.csv.*.tmp")), []
        )

    def test_final_live_count_regression_validates_after_sanitization(self) -> None:
        _, ids = self.initialize(1127)
        pallet_minus_one_id = ids[-1]
        self.write_exports(
            ids,
            [],
            [],
            pallet_values={pallet_minus_one_id: "-1"},
        )
        raw_headers, raw_rows = self.read_csv(self.base / "DATA" / "OUTPUT.csv")
        self.assertEqual(len(raw_rows), 1127)
        self.assertEqual(raw_headers, OUTPUT_HEADERS)

        result = self.run_final(self.base, "55564", "1,126")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("Removed 1 OUTPUT record", result.stdout)
        self.assertIn("Sanitized OUTPUT.csv contains 1126 mailed records", result.stdout)
        customer_headers, customer_rows = self.read_csv(
            self.base / "DATA" / "55564 TM MA_1126.csv"
        )
        self.assertEqual(len(customer_rows), 1126)
        self.assertNotIn("MATCHID", customer_headers)
        pallet_index = customer_headers.index("Pallet Number")
        self.assertTrue(all(row[pallet_index] != "-1" for row in customer_rows))
        merged_headers, merged_rows = self.read_csv(
            self.base / "DATA" / "source_MERGED.csv"
        )
        status_index = merged_headers.index("mailed")
        self.assertEqual(sum(row[status_index] == "13" for row in merged_rows), 1126)
        self.assertEqual(sum(row[status_index] == "14" for row in merged_rows), 1)
        self.assertEqual(merged_rows[-1][status_index], "14")

    def test_final_natural_duplicates_are_classified_only_by_matchid(self) -> None:
        self.prepare_final(
            total=3, output_count=1, move_count=0, natural_duplicates=True
        )
        result = self.run_final(self.base, "55564", "1")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        headers, rows = self.read_csv(self.base / "DATA" / "source_MERGED.csv")
        self.assertEqual([row[: len(HEADERS)] for row in rows], self.source_rows(3, True))
        status_index = headers.index("mailed")
        self.assertEqual([row[status_index] for row in rows], ["13", "14", "14"])

    def test_final_duplicate_matchids_fail_without_partial_outputs(self) -> None:
        cases = ["TMMA SOURCE.csv", "INPUT.csv", "MOVE UPDATES.csv", "NOT MAILED.csv", "OUTPUT.csv"]
        for label in cases:
            with self.subTest(label=label), tempfile.TemporaryDirectory(
                prefix="goji_tmma_duplicate_"
            ) as name:
                base = self.create_base(Path(name))
                self.prepare_final(total=4, output_count=2, move_count=1, base=base)
                path = base / "DATA" / label
                headers, rows = self.read_csv(path)
                if label == "TMMA SOURCE.csv":
                    rows[1][0] = rows[0][0]
                elif label == "INPUT.csv":
                    rows[1][0] = rows[0][0]
                else:
                    rows.append(rows[0].copy())
                self.write_csv(path, headers, rows)
                result = self.run_final(base, "55564", "2")
                self.assert_final_failure(result, "Duplicate MATCHID", 2, base=base)

    def test_final_rejects_blank_and_malformed_matchids(self) -> None:
        for value, expected in [("", "Blank MATCHID"), ("123", "Malformed MATCHID")]:
            with self.subTest(value=value), tempfile.TemporaryDirectory(
                prefix="goji_tmma_bad_id_"
            ) as name:
                base = self.create_base(Path(name))
                self.prepare_final(total=3, output_count=2, base=base)
                path = base / "DATA" / "OUTPUT.csv"
                headers, rows = self.read_csv(path)
                rows[0][0] = value
                self.write_csv(path, headers, rows)
                result = self.run_final(base, "55564", "2")
                self.assert_final_failure(result, expected, 2, base=base)

    def test_final_rejects_unknown_ids_in_every_bulk_export(self) -> None:
        for label in ["MOVE UPDATES.csv", "NOT MAILED.csv", "OUTPUT.csv"]:
            with self.subTest(label=label), tempfile.TemporaryDirectory(
                prefix="goji_tmma_unknown_"
            ) as name:
                base = self.create_base(Path(name))
                self.prepare_final(total=4, output_count=2, move_count=1, base=base)
                path = base / "DATA" / label
                headers, rows = self.read_csv(path)
                rows[0][headers.index("MATCHID")] = "ZZ9999"
                self.write_csv(path, headers, rows)
                result = self.run_final(base, "55564", "2")
                self.assert_final_failure(result, "Unknown MATCHID", 2, base=base)

    def test_final_rejects_overlap_and_unaccounted_source_id(self) -> None:
        with tempfile.TemporaryDirectory(prefix="goji_tmma_overlap_") as name:
            base = self.create_base(Path(name))
            _, _, output_ids, not_ids, _ = self.prepare_final(
                total=3, output_count=2, base=base
            )
            self.write_exports(output_ids, [*not_ids, output_ids[0]], [], base=base)
            result = self.run_final(base, "55564", "2")
            self.assert_final_failure(
                result, "both OUTPUT.csv and NOT MAILED.csv", 2, base=base
            )

        with tempfile.TemporaryDirectory(prefix="goji_tmma_missing_partition_") as name:
            base = self.create_base(Path(name))
            _, _, output_ids, _, _ = self.prepare_final(
                total=3, output_count=2, base=base
            )
            self.write_exports(output_ids, [], [], base=base)
            result = self.run_final(base, "55564", "2")
            self.assert_final_failure(
                result, "neither OUTPUT.csv nor NOT MAILED.csv", 2, base=base
            )

    def test_final_rejects_count_status_and_move_classification_errors(self) -> None:
        with tempfile.TemporaryDirectory(prefix="goji_tmma_count_") as name:
            base = self.create_base(Path(name))
            self.prepare_final(total=3, output_count=2, base=base)
            result = self.run_final(base, "55564", "3")
            self.assert_final_failure(result, "Count mismatch", 3, base=base)

        with tempfile.TemporaryDirectory(prefix="goji_tmma_status_") as name:
            base = self.create_base(Path(name))
            _, _, output_ids, not_ids, _ = self.prepare_final(
                total=3, output_count=2, base=base
            )
            self.write_exports(
                output_ids, not_ids, [], base=base, not_mailed_status="13"
            )
            result = self.run_final(base, "55564", "2")
            self.assert_final_failure(result, "must be exactly 14", 2, base=base)

        with tempfile.TemporaryDirectory(prefix="goji_tmma_move_13_") as name:
            base = self.create_base(Path(name))
            _, _, output_ids, not_ids, _ = self.prepare_final(
                total=3, output_count=2, base=base
            )
            self.write_exports(output_ids, not_ids, [output_ids[0]], base=base)
            result = self.run_final(base, "55564", "2")
            self.assert_final_failure(
                result, "not present in NOT MAILED.csv", 2, base=base
            )

    def test_final_rejects_missing_header_file_and_zero_byte_file(self) -> None:
        with tempfile.TemporaryDirectory(prefix="goji_tmma_header_") as name:
            base = self.create_base(Path(name))
            self.prepare_final(total=3, output_count=2, base=base)
            path = base / "DATA" / "MOVE UPDATES.csv"
            self.write_csv(path, MOVE_HEADERS[:-1], [])
            result = self.run_final(base, "55564", "2")
            self.assert_final_failure(result, "missing required header", 2, base=base)

        required = [
            "TMMA SOURCE.csv",
            "INPUT.csv",
            "MOVE UPDATES.csv",
            "NOT MAILED.csv",
            "OUTPUT.csv",
        ]
        for mode in ("missing", "zero"):
            for label in required:
                with self.subTest(mode=mode, label=label), tempfile.TemporaryDirectory(
                    prefix="goji_tmma_required_"
                ) as name:
                    base = self.create_base(Path(name))
                    self.prepare_final(total=3, output_count=2, base=base)
                    path = base / "DATA" / label
                    if mode == "missing":
                        path.unlink()
                    else:
                        path.write_bytes(b"")
                    result = self.run_final(base, "55564", "2")
                    self.assert_final_failure(
                        result,
                        "was not found" if mode == "missing" else "zero bytes",
                        2,
                        base=base,
                        expect_output=not (mode == "missing" and label == "OUTPUT.csv"),
                    )

    def test_final_existing_targets_are_never_overwritten(self) -> None:
        targets = ["source_MERGED.csv", "55564 TM MA_2.csv"]
        for target_name in targets:
            with self.subTest(target=target_name), tempfile.TemporaryDirectory(
                prefix="goji_tmma_target_"
            ) as name:
                base = self.create_base(Path(name))
                self.prepare_final(total=3, output_count=2, base=base)
                target = base / "DATA" / target_name
                target.write_text("existing", encoding="utf-8")
                result = self.run_final(base, "55564", "2")
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("will not be overwritten", result.stdout)
                self.assertEqual(target.read_text(encoding="utf-8"), "existing")
                self.assertTrue((base / "DATA" / "OUTPUT.csv").exists())
                other = (
                    base / "DATA" / "55564 TM MA_2.csv"
                    if target_name == "source_MERGED.csv"
                    else base / "DATA" / "source_MERGED.csv"
                )
                self.assertFalse(other.exists())

    def test_final_header_only_move_updates_succeeds(self) -> None:
        self.prepare_final(total=3, output_count=2, move_count=0)
        result = self.run_final(self.base, "55564", "2")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        headers, rows = self.read_csv(self.base / "DATA" / "source_MERGED.csv")
        for current_header in CURRENT_HEADERS:
            index = headers.index(current_header)
            self.assertTrue(all(row[index] == "" for row in rows))

    def test_final_header_only_not_mailed_succeeds_when_all_mailed(self) -> None:
        self.prepare_final(total=3, output_count=3, move_count=0)
        result = self.run_final(self.base, "55564", "3")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        headers, rows = self.read_csv(self.base / "DATA" / "source_MERGED.csv")
        status_index = headers.index("mailed")
        self.assertEqual([row[status_index] for row in rows], ["13", "13", "13"])

    def test_popup_and_controller_source_contract(self) -> None:
        dialog_header = (PROJECT_ROOT / "tmmaemaildialog.h").read_text(encoding="utf-8")
        dialog_source = (PROJECT_ROOT / "tmmaemaildialog.cpp").read_text(encoding="utf-8")
        controller = (PROJECT_ROOT / "tmmacontroller.cpp").read_text(encoding="utf-8")
        drag_source = (PROJECT_ROOT / "tmfleremailfilelistwidget.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("const QStringList& filePaths", dialog_header)
        self.assertIn("for (const QString& filePath : m_filePaths)", dialog_source)
        self.assertIn("DRAG & DROP FILE INTO E-MAIL", dialog_source)
        self.assertIn("TMMA_FINAL_OUTPUT_FILE=", controller)
        self.assertIn("TMMA_FINAL_MERGED_FILE=", controller)
        self.assertIn("{outputFilePath, mergedFilePath}", controller)
        self.assertIn("QUrl::fromLocalFile", drag_source)
        self.assertIn("drag->exec(Qt::CopyAction)", drag_source)

    def test_tmma_tracker_class_mapping_source_contract(self) -> None:
        controller = (PROJECT_ROOT / "tmmacontroller.cpp").read_text(encoding="utf-8")
        mapping = re.search(
            r"QString trackerClassDisplay\(const QString& mailClass\).*?"
            r"mailClass == QStringLiteral\(\"STANDARD\"\).*?"
            r"mailClass == QStringLiteral\(\"FIRST CLASS\"\).*?"
            r"return QStringLiteral\(\"STD\"\);",
            controller,
            re.DOTALL,
        )
        self.assertIsNotNone(mapping)
        self.assertIn(
            "const QString trackerClass = trackerClassDisplay(mailClass);",
            controller,
        )
        self.assertRegex(
            controller,
            re.compile(r"upsertLogEntry\(.*?perPiece,\s*trackerClass,", re.DOTALL),
        )
        self.assertIn(
            "state.mailClass = m_classDropdown ? m_classDropdown->currentText()",
            controller,
        )

    def test_tmma_html_targeted_wording(self) -> None:
        default_html = (PROJECT_ROOT / "resources" / "tmma" / "default.html").read_text(
            encoding="utf-8"
        )
        instructions = (
            PROJECT_ROOT / "resources" / "tmma" / "instructions.html"
        ).read_text(encoding="utf-8")
        self.assertIn(
            "Drop original data files into <strong>DATA FILE DROP BOX</strong>",
            default_html,
        )
        self.assertNotIn("FARMWORKERS", default_html)
        self.assertIn("TM MA", default_html)
        self.assertIn("<em>Year</em> and <em>Month</em>", default_html)
        self.assertNotIn("Quarter", default_html)
        self.assertIn("Update the <em>Job Number</em>.", instructions)
        self.assertNotIn("YYQX", instructions)
        self.assertIn("AMERCIAN PRINTING XXXXX TM MA", instructions)
        self.assertNotIn("TM FWC", instructions)
        self.assertNotIn("FARMWORKERS", instructions)


if __name__ == "__main__":
    unittest.main(verbosity=2)
