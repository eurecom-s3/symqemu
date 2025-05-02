import pathlib
import shutil
import unittest
import tempfile
import subprocess
import os
import hashlib

import util


class SymQemuTests(unittest.TestCase):
    def run_symqemu_and_assert_correct_result(self, binary_name):
        symqemu_ref_output_dir = util.BINARIES_DIR / binary_name / 'expected_outputs'
        symqemu_gen_output_dir = util.BINARIES_DIR / binary_name / 'generated_outputs'

        if not symqemu_gen_output_dir.exists():
            symqemu_gen_output_dir.mkdir()

        util.run_symqemu_on_test_binary(binary_name=binary_name, generated_test_cases_output_dir=symqemu_gen_output_dir)

        expected_hashes = {}
        for ref_file in symqemu_ref_output_dir.iterdir():
            with open(ref_file, 'rb', buffering=0) as f:
                f_hash = hashlib.file_digest(f, "sha256").hexdigest()
                expected_hashes[f_hash] = [False, ref_file]

        testcase_not_found = False
        for gen_file in symqemu_gen_output_dir.iterdir():
            with open(gen_file, 'rb', buffering=0) as f:
                f_hash = hashlib.file_digest(f, "sha256").hexdigest()
                ret = expected_hashes.get(f_hash)
                if ret is not None:
                    ret[0] = True
                    expected_hashes[f_hash] = ret
                else:
                    print(f"Error: content of file {gen_file} not found in expected testcases.");
                    testcase_not_found = True

        for (is_found, fname) in expected_hashes.values():
            # (is_found, fname) = tuple(ret)
            if not is_found:
                print(f"Warning: expected testcase {fname} has not been generated.")

        self.assertFalse(testcase_not_found)

    def test_simple(self):
        self.run_symqemu_and_assert_correct_result('simple')

    def test_printf(self):
        self.run_symqemu_and_assert_correct_result('printf')

    def test_simple_i128(self):
        self.run_symqemu_and_assert_correct_result('simple_i128')
