import contextlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

from openpyxl import Workbook

SCRIPTS = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("flahca_pipeline", SCRIPTS / "flahca_pipeline.py")
pipeline = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pipeline)


class PipelineTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name) / "FL AHCA"
        (self.base / "RAW INPUT").mkdir(parents=True)
        self.data = self.base / "DATA"
        self.identity = ["12345", "2026", "10"]
        self.source = self.base / "RAW INPUT" / "Pregnancy 10.2026.xlsx"
        self.headers = ["Mem_Subscriber_ID", *pipeline.FIELD_MAP, "Extra original field"]
        self.originals = [
            ["000001", "Zoë", "One", "1 Main St", "", "Orlando", "FL", "01234", "NA"],
            ["000001", "Zoë", "One", "1 Main St", "", "Orlando", "FL", "01234", ""],
            ["000003", "Three", "Last", "3 Main St", "Unit 1", "Miami", "FL", 33101, "Keep me"],
        ]
        self.write_source()

    def write_source(self, headers=None, rows=None):
        workbook = Workbook()
        sheet = workbook.active
        sheet.title = "Sheet2"
        sheet.append(headers or self.headers)
        for row in self.originals if rows is None else rows:
            sheet.append(row)
        workbook.save(self.source)
        workbook.close()

    def prepare(self):
        with contextlib.redirect_stdout(io.StringIO()):
            pipeline.prepare(self.base, self.identity)
        return pipeline.read_csv(self.data / "INPUT.csv")[1]

    def exports(self, nonmailed=(1,), code_header="User text 3"):
        rows = pipeline.read_csv(self.data / "INPUT.csv")[1]
        self.ids = [r[0] for r in rows]
        final = [[uid, str(n + 1), "Corrected address"] for n, uid in enumerate(self.ids) if n not in nonmailed]
        codes = [[self.ids[n], "14"] for n in nonmailed]
        (self.data / pipeline.EXPORT).write_bytes(pipeline.csv_bytes([pipeline.ID, "Sort Position", "Address Line 1"], list(reversed(final))))
        (self.data / pipeline.CODE_LIST).write_bytes(pipeline.csv_bytes([pipeline.ID, code_header], codes))

    def prearchive(self):
        with contextlib.redirect_stdout(io.StringIO()) as output:
            pipeline.prearchive(self.base, self.identity, Path(self.temp.name) / "print")
        return output.getvalue()

    def archive(self):
        with contextlib.redirect_stdout(io.StringIO()):
            pipeline.archive(self.base, self.identity)

    def test_initial_schema_strings_original_retention_and_duplicate_source_rows(self):
        original = self.source.read_bytes()
        rows = self.prepare()
        headers, _ = pipeline.read_csv(self.data / "INPUT.csv")
        self.assertEqual(headers, pipeline.INPUT_HEADERS)
        self.assertEqual(len(rows), 3)
        self.assertEqual(len({r[0] for r in rows}), 3)
        self.assertEqual(rows[0][-1], "01234")
        self.assertEqual(rows[0][1], "Zoë")
        self.assertEqual(self.source.read_bytes(), original)
        headers, mapping = pipeline.read_csv(self.data / pipeline.MAPPING)
        self.assertEqual(headers, [pipeline.ID, *self.headers])
        self.assertEqual(mapping[0][1], "000001")
        self.assertEqual(mapping[0][-1], "NA")
        self.assertEqual(mapping[1][-1], "")

    def test_retry_retains_identical_ids_and_input(self):
        self.prepare()
        before = {p.name: p.read_bytes() for p in self.data.iterdir()}
        self.prepare()
        self.assertEqual(before, {p.name: p.read_bytes() for p in self.data.iterdir()})

    def test_changed_source_is_rejected_without_replacing_input(self):
        self.prepare()
        before = (self.data / "INPUT.csv").read_bytes()
        self.originals[0][1] = "Changed"
        self.write_source()
        with self.assertRaisesRegex(pipeline.JobError, "Source workbook changed"):
            self.prepare()
        self.assertEqual(before, (self.data / "INPUT.csv").read_bytes())

    def test_existing_unowned_input_is_never_overwritten(self):
        self.data.mkdir()
        existing = self.data / "INPUT.csv"
        existing.write_text("operator test data", encoding="utf-8")
        with self.assertRaises(pipeline.JobError):
            self.prepare()
        self.assertEqual(existing.read_text(), "operator test data")

    def test_missing_required_column_is_rejected(self):
        self.write_source(headers=self.headers[:-2] + ["Wrong ZIP", "Extra original field"])
        with self.assertRaisesRegex(pipeline.JobError, "seven MEM_"):
            self.prepare()
        self.assertFalse((self.data / "INPUT.csv").exists())

    def test_multiple_workbooks_are_rejected(self):
        self.source.with_name("second.xlsx").write_bytes(self.source.read_bytes())
        with self.assertRaisesRegex(pipeline.JobError, "exactly one"):
            self.prepare()

    def test_initial_publish_failure_rolls_back_and_retry_succeeds(self):
        original_replace = pipeline.os.replace
        count = 0

        def fail_second(source, dest):
            nonlocal count
            count += 1
            if count == 2:
                raise OSError("Injected publication failure")
            return original_replace(source, dest)

        with patch.object(pipeline.os, "replace", side_effect=fail_second):
            with self.assertRaises(OSError):
                self.prepare()
        self.assertEqual(list(self.data.iterdir()), [])
        self.assertTrue(self.source.exists())
        self.assertEqual(len(self.prepare()), 3)

    def test_merge_preserves_all_columns_order_values_and_classification(self):
        self.prepare()
        self.exports()
        output = self.prearchive()
        self.assertIn("=== PAUSE_FOR_EMAIL ===", output)
        headers, merged = pipeline.read_csv(self.data / (self.source.stem + "_MERGED.csv"))
        _, source_rows = pipeline.read_csv(self.data / pipeline.MAPPING)
        self.assertEqual(headers, [*self.headers, "MAILED"])
        self.assertEqual([r[:-1] for r in merged], [r[1:] for r in source_rows])
        self.assertEqual([r[-1] for r in merged], ["13", "14", "13"])
        self.assertNotIn(pipeline.ID, headers)

    def test_empty_14_list_all_mailed(self):
        self.prepare()
        self.exports(nonmailed=())
        self.prearchive()
        self.archive()
        self.assertEqual(len(list((Path(self.temp.name) / "print").glob("*.csv"))), 1)

    def test_two_column_14_list_accepts_export_header_capitalization(self):
        self.prepare()
        for header in ["User text 3", "User Text 3", " USER TEXT 3 "]:
            with self.subTest(header=header):
                self.exports(code_header=header)
                self.assertEqual(pipeline.read_csv(self.data / pipeline.CODE_LIST)[0],
                                 [pipeline.ID, header])
                self.prearchive()
                _, rows = pipeline.read_csv(self.data / (self.source.stem + "_MERGED.csv"))
                self.assertEqual([r[-1] for r in rows], ["13", "14", "13"])

    def test_14_status_validation_applies_to_each_header_capitalization(self):
        self.prepare()
        for header in ["User text 3", "User Text 3", " USER TEXT 3 "]:
            for value in ["13", "", "not a code"]:
                with self.subTest(header=header, value=value):
                    self.exports(code_header=header)
                    (self.data / pipeline.CODE_LIST).write_bytes(
                        pipeline.csv_bytes([pipeline.ID, header], [[self.ids[1], value]]))
                    with self.assertRaisesRegex(pipeline.JobError, "value other than 14"):
                        self.prearchive()
                    self.assertFalse((self.data / (self.source.stem + "_MERGED.csv")).exists())

    def test_14_list_rejects_ambiguous_status_headers(self):
        self.prepare()
        self.exports()
        (self.data / pipeline.CODE_LIST).write_bytes(pipeline.csv_bytes(
            [pipeline.ID, "User text 3", "User Text 3"], [[self.ids[1], "14", "13"]]))
        with self.assertRaisesRegex(pipeline.JobError, "duplicate User text 3"):
            self.prearchive()

    def test_all_14_creates_merge_and_archive_without_print_delivery(self):
        self.prepare()
        self.exports(nonmailed=(0, 1, 2))
        self.prearchive()
        self.archive()
        self.assertFalse((Path(self.temp.name) / "print").exists())

    def test_missing_export_id_is_rejected(self):
        self.prepare()
        self.exports()
        path = self.data / pipeline.EXPORT
        path.write_bytes(pipeline.csv_bytes(["Sort Position"], [[1], [2]]))
        with self.assertRaisesRegex(pipeline.JobError, "UNIQUE ID"):
            self.prearchive()

    def test_duplicate_export_id_is_rejected(self):
        self.prepare()
        self.exports()
        path = self.data / pipeline.EXPORT
        h, r = pipeline.read_csv(path)
        path.write_bytes(pipeline.csv_bytes(h, [*r, r[0]]))
        with self.assertRaisesRegex(pipeline.JobError, "Duplicate UNIQUE ID"):
            self.prearchive()

    def test_unknown_export_id_is_rejected(self):
        self.prepare()
        self.exports()
        path = self.data / pipeline.CODE_LIST
        path.write_bytes(pipeline.csv_bytes([pipeline.ID], [["ZZZZZZZZZZ"]]))
        with self.assertRaisesRegex(pipeline.JobError, "Unknown UNIQUE ID"):
            self.prearchive()

    def test_missing_record_and_overlapping_lists_are_rejected(self):
        self.prepare()
        self.exports()
        path = self.data / pipeline.CODE_LIST
        path.write_bytes(pipeline.csv_bytes([pipeline.ID], []))
        with self.assertRaisesRegex(pipeline.JobError, "every source"):
            self.prearchive()
        path.write_bytes(pipeline.csv_bytes([pipeline.ID], [[uid] for uid in self.ids]))
        with self.assertRaisesRegex(pipeline.JobError, "both"):
            self.prearchive()

    def test_modified_mapping_and_input_are_rejected(self):
        self.prepare()
        self.exports()
        path = self.data / "INPUT.csv"
        before = path.read_bytes()
        path.write_bytes(before + b"\n")
        with self.assertRaisesRegex(pipeline.JobError, "INPUT.csv changed"):
            self.prearchive()
        path.write_bytes(before)
        path = self.data / pipeline.MAPPING
        path.write_bytes(path.read_bytes() + b"\n")
        with self.assertRaisesRegex(pipeline.JobError, "SOURCE.csv changed"):
            self.prearchive()

    def test_wrong_job_identity_is_rejected(self):
        self.prepare()
        self.exports()
        with self.assertRaisesRegex(pipeline.JobError, "different job"):
            pipeline.prearchive(self.base, ["54321", "2026", "10"])

    def test_archive_requires_handoff_and_unchanged_exports(self):
        self.prepare()
        self.exports()
        with self.assertRaisesRegex(pipeline.JobError, "prearchive"):
            self.archive()
        self.prearchive()
        path = self.data / pipeline.EXPORT
        path.write_bytes(path.read_bytes() + b"\r\n")
        with self.assertRaisesRegex(pipeline.JobError, "changed after"):
            self.archive()
        self.assertTrue(self.source.exists())

    def test_archive_is_verified_before_cleanup_and_contains_original(self):
        self.prepare()
        self.exports()
        self.prearchive()
        source = self.source.read_bytes()
        expected_data = {p.name: p.read_bytes() for p in self.data.iterdir()}
        self.archive()
        self.assertFalse(self.source.exists())
        self.assertEqual(list(self.data.iterdir()), [])
        archives = list((self.base / "ARCHIVE").glob("*.zip"))
        self.assertEqual(len(archives), 1)
        with zipfile.ZipFile(archives[0]) as z:
            self.assertIsNone(z.testzip())
            self.assertEqual(z.read("RAW INPUT/" + self.source.name), source)
            for name, content in expected_data.items():
                self.assertEqual(z.read("DATA/" + name), content)

    def test_partial_cleanup_restores_inputs_and_can_retry(self):
        self.prepare()
        self.exports()
        self.prearchive()
        before = {p: p.read_bytes() for p in self.data.iterdir()}
        before[self.source] = self.source.read_bytes()
        original_unlink = Path.unlink
        failed = False

        def fail_input(path, *args, **kwargs):
            nonlocal failed
            if path == self.data / "INPUT.csv" and not failed:
                failed = True
                raise OSError("Injected cleanup failure")
            return original_unlink(path, *args, **kwargs)

        with patch.object(Path, "unlink", fail_input):
            with self.assertRaises(OSError):
                self.archive()
        self.assertEqual(before, {p: p.read_bytes() for p in before})
        self.archive()
        self.assertEqual(len(list((self.base / "ARCHIVE").glob("*.zip"))), 1)

    def test_delivery_collision_retains_all_job_files(self):
        self.prepare()
        self.exports()
        target = Path(self.temp.name) / "print" / "12345 TRACHMAR FL AHCA_3.csv"
        target.parent.mkdir()
        target.write_text("existing print file", encoding="utf-8")
        with self.assertRaisesRegex(pipeline.JobError, "Refusing to replace"):
            self.prearchive()
        self.assertEqual(target.read_text(), "existing print file")
        self.assertTrue(self.source.exists())
        self.assertTrue((self.data / pipeline.MANIFEST).exists())

    def test_job_lock_rejects_concurrent_processing(self):
        with pipeline.job_lock(self.base):
            with self.assertRaisesRegex(pipeline.JobError, "already processing"):
                with pipeline.job_lock(self.base):
                    self.fail("Second lock was acquired")
        self.assertFalse((self.base / ".flahca-processing.lock").exists())

    def test_network_copy_reuses_existing_job_folder_and_handoff_has_actual_path(self):
        self.prepare()
        self.exports()
        server = Path(self.temp.name) / "server"
        existing = server / "12345_Trachmar_Pregnancy"
        existing.mkdir(parents=True)
        (server / "9123450_unrelated").mkdir()
        (server / "Previous_12345").mkdir()
        with contextlib.redirect_stdout(io.StringIO()) as output:
            pipeline.prearchive(self.base, self.identity, network_root=server)
        target = existing / "HP Indigo" / "DATA" / "12345 TRACHMAR FL AHCA_3.csv"
        self.assertEqual(target.read_bytes(), (self.data / pipeline.EXPORT).read_bytes())
        self.assertIn(str(target.parent.resolve()), output.getvalue())
        self.assertFalse((server / "12345_FLAHCA").exists())
        self.archive()

    def test_network_copy_creates_missing_job_folder_and_retry_reuses_identical_file(self):
        self.prepare()
        self.exports()
        server = Path(self.temp.name) / "server"
        server.mkdir()
        with contextlib.redirect_stdout(io.StringIO()):
            for _ in range(2):
                pipeline.prearchive(self.base, self.identity, network_root=server)
        target = server / "12345_FLAHCA" / "HP Indigo" / "DATA" / "12345 TRACHMAR FL AHCA_3.csv"
        self.assertEqual(target.read_bytes(), (self.data / pipeline.EXPORT).read_bytes())
        self.assertEqual(len(list(target.parent.iterdir())), 1)
        for relative in ("PDF for Client", "Files for Ricoh", "Original Files",
                         "HP Indigo/PRINT", "HP Indigo/PROOF"):
            folder = server / "12345_FLAHCA" / relative
            self.assertTrue(folder.is_dir())
            self.assertEqual(list(folder.iterdir()), [])

    def test_job_60244_ignores_nonprefix_and_longer_job_numbers(self):
        server = Path(self.temp.name) / "server"
        for name in ("Previous_60244", "602440_OtherJob"):
            (server / name).mkdir(parents=True)
        folder = pipeline.find_print_folder(server, "60244")
        self.assertEqual(folder, server / "60244_FLAHCA" / "HP Indigo" / "DATA")
        self.assertTrue(folder.is_dir())
        self.assertEqual(list((server / "Previous_60244").iterdir()), [])
        self.assertEqual(list((server / "602440_OtherJob").iterdir()), [])

    def test_incomplete_job_folder_is_completed_without_changing_existing_files(self):
        server = Path(self.temp.name) / "server"
        job_folder = server / "60244_FLAHCA"
        originals = job_folder / "Original Files"
        originals.mkdir(parents=True)
        existing = originals / "keep.txt"
        existing.write_bytes(b"Existing job file")
        folder = pipeline.find_print_folder(server, "60244")
        self.assertEqual(folder, job_folder / "HP Indigo" / "DATA")
        self.assertTrue((job_folder / "HP Indigo" / "PROOF").is_dir())
        self.assertTrue((job_folder / "Files for Ricoh").is_dir())
        self.assertEqual(existing.read_bytes(), b"Existing job file")

    def test_unavailable_network_uses_verified_fallback_and_reports_its_real_path(self):
        self.prepare()
        self.exports()
        server = Path(self.temp.name) / "unavailable"
        fallback = Path(self.temp.name) / "fallback"
        with patch.object(pipeline.time, "sleep"), contextlib.redirect_stdout(io.StringIO()) as output:
            pipeline.prearchive(self.base, self.identity, network_root=server, fallback_dir=fallback)
        target = fallback / "12345 TRACHMAR FL AHCA_3.csv"
        self.assertEqual(target.read_bytes(), (self.data / pipeline.EXPORT).read_bytes())
        self.assertIn(str(server / "12345_FLAHCA" / "HP Indigo" / "DATA"), target.with_suffix(".txt").read_text())
        marker = output.getvalue().split("=== PRINT_FOLDER_PATH ===\n", 1)[1].splitlines()[0]
        self.assertEqual(marker, str(fallback.resolve()))
        legacy_marker = output.getvalue().split("=== NAS_FOLDER_PATH ===\n", 1)[1].splitlines()[0]
        self.assertEqual(legacy_marker, str(self.data.resolve()))
        self.archive()

    def test_ambiguous_network_folders_stop_without_delivery_or_cleanup(self):
        self.prepare()
        self.exports()
        server = Path(self.temp.name) / "server"
        (server / "12345_A").mkdir(parents=True)
        (server / "12345_B").mkdir()
        with self.assertRaisesRegex(pipeline.JobError, "More than one network folder"):
            pipeline.prearchive(self.base, self.identity, network_root=server)
        self.assertFalse(list(server.rglob("*.csv")))
        self.assertTrue(self.source.exists())
        self.assertNotIn("prepared", json.loads((self.data / pipeline.MANIFEST).read_text()))

    def test_delivered_file_must_stay_unchanged_before_archive_cleanup(self):
        self.prepare()
        self.exports()
        self.prearchive()
        target = Path(self.temp.name) / "print" / "12345 TRACHMAR FL AHCA_3.csv"
        target.write_bytes(b"another job's file")
        with self.assertRaisesRegex(pipeline.JobError, "Delivered print file changed"):
            self.archive()
        self.assertTrue(self.source.exists())
        self.assertTrue((self.data / pipeline.EXPORT).exists())

    def test_all_14_does_not_contact_network_or_report_a_false_print_location(self):
        self.prepare()
        self.exports(nonmailed=(0, 1, 2))
        with patch.object(pipeline, "deliver_print") as delivery, contextlib.redirect_stdout(io.StringIO()) as output:
            pipeline.prearchive(self.base, self.identity)
        delivery.assert_not_called()
        self.assertIn("=== PRINT_FOLDER_PATH ===\n\n=== END_PRINT_FOLDER_PATH ===", output.getvalue())
        self.archive()

    def test_exports_changing_during_delivery_prevent_handoff(self):
        self.prepare()
        self.exports()
        original = pipeline.deliver_print

        def changed(content, *args):
            destination = original(content, *args)
            path = self.data / pipeline.EXPORT
            path.write_bytes(path.read_bytes() + b"\r\n")
            return destination

        with patch.object(pipeline, "deliver_print", side_effect=changed):
            with self.assertRaisesRegex(pipeline.JobError, "changed during print delivery"):
                self.prearchive()
        self.assertNotIn("prepared", json.loads((self.data / pipeline.MANIFEST).read_text()))
        self.assertTrue(self.source.exists())

    def test_command_line_wrappers_use_isolated_paths_and_exit_codes(self):
        initial = subprocess.run([sys.executable, "-B", str(SCRIPTS / "01 INITIAL.py"),
                                  *self.identity, "--base-dir", str(self.base)],
                                 capture_output=True, text=True)
        self.assertEqual(initial.returncode, 0, initial.stdout + initial.stderr)
        self.exports()
        final = subprocess.run([sys.executable, "-B", str(SCRIPTS / "02 FINAL PROCESS.py"),
                                *self.identity, "--base-dir", str(self.base),
                                "--delivery-dir", str(Path(self.temp.name) / "print")],
                               capture_output=True, text=True)
        self.assertEqual(final.returncode, 0, final.stdout + final.stderr)
        self.assertIn("=== PAUSE_FOR_EMAIL ===", final.stdout)


if __name__ == "__main__":
    unittest.main()
