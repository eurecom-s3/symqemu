import filecmp
import pathlib
import shutil
import unittest

import util


class SymQemuTests(unittest.TestCase):
    SYMQEMU_OUTPUT_DIR = pathlib.Path(__file__).parent / "symqemu_output"

    def setUp(self):
        self.SYMQEMU_OUTPUT_DIR.mkdir()

    def tearDown(self):
        shutil.rmtree(self.SYMQEMU_OUTPUT_DIR)

    def run_symqemu_and_assert_correct_result(self, binary_name):

        util.run_symqemu_on_test_binary(binary_name=binary_name, generated_test_cases_output_dir=self.SYMQEMU_OUTPUT_DIR)

        # `filecmp.dircmp` does a "shallow" comparison, but this is not a problem here because
        # the timestamps should always be different, so the actual content of the files will be compared.
        # See https://docs.python.org/3/library/filecmp.html#filecmp.dircmp
        expected_vs_actual_output_comparison = filecmp.dircmp(self.SYMQEMU_OUTPUT_DIR, util.BINARIES_DIR / binary_name / 'expected_outputs')
        self.assertEqual(expected_vs_actual_output_comparison.diff_files, [])
        self.assertEqual(expected_vs_actual_output_comparison.left_only, [])
        self.assertEqual(expected_vs_actual_output_comparison.right_only, [])
        self.assertEqual(expected_vs_actual_output_comparison.funny_files, [])

    def test_simple(self):
        self.run_symqemu_and_assert_correct_result('simple')

    def test_printf(self):
        self.run_symqemu_and_assert_correct_result('printf')
