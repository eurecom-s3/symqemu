import filecmp
import pathlib
import shutil
import unittest
import tempfile
import subprocess
import os

import util


class SymQemuTests(unittest.TestCase):
    def run_symqemu_and_assert_correct_result(self, binary_name):
        symqemu_ref_output_dir = util.BINARIES_DIR / binary_name / 'expected_outputs'
        symqemu_gen_output_dir = util.BINARIES_DIR / binary_name / 'generated_outputs'

        if not symqemu_gen_output_dir.exists():
            symqemu_gen_output_dir.mkdir()

        util.run_symqemu_on_test_binary(binary_name=binary_name, generated_test_cases_output_dir=symqemu_gen_output_dir)

        # `filecmp.dircmp` does a "shallow" comparison, but this is not a problem here because
        # the timestamps should always be different, so the actual content of the files will be compared.
        # See https://docs.python.org/3/library/filecmp.html#filecmp.dircmp
        expected_vs_actual_output_comparison = filecmp.dircmp(symqemu_gen_output_dir, symqemu_ref_output_dir)

        for diff_file in expected_vs_actual_output_comparison.diff_files:
            ref_file = symqemu_ref_output_dir / diff_file
            gen_file = symqemu_gen_output_dir / diff_file

            tmp_ref = tempfile.NamedTemporaryFile("w+")
            subprocess.run(["xxd", f"{ref_file}"], stdout=tmp_ref, check=True)

            tmp_gen = tempfile.NamedTemporaryFile("w+")
            subprocess.run(["xxd", f"{gen_file}"], stdout=tmp_gen, check=True)

            wdiff = subprocess.run(["wdiff", f"{tmp_ref.name}", f"{tmp_gen.name}"], capture_output=True)
            colordiff = subprocess.run(["colordiff"], input=wdiff.stdout, capture_output=True)

            print(f"===== Diff found in {diff_file} ======")
            print(f"{colordiff.stdout.decode('utf-8').strip()}")
            print(f"=================================")
            print()

        self.assertEqual(expected_vs_actual_output_comparison.diff_files, [])
        self.assertEqual(expected_vs_actual_output_comparison.left_only, [])
        self.assertEqual(expected_vs_actual_output_comparison.right_only, [])
        self.assertEqual(expected_vs_actual_output_comparison.funny_files, [])

    def test_simple(self):
        self.run_symqemu_and_assert_correct_result('simple')

    def test_printf(self):
        self.run_symqemu_and_assert_correct_result('printf')

    def test_simple_i128(self):
        self.run_symqemu_and_assert_correct_result('simple_i128')
