import sys

import util

if __name__ == '__main__':

    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <binary name>")
        sys.exit(1)

    binary_name = sys.argv[1]
    binary_dir = util.BINARIES_DIR / binary_name

    if not binary_dir.exists():
        print(f"Error: {binary_dir} does not exist")
        sys.exit(1)

    output_dir = binary_dir / 'expected_outputs'

    if output_dir.exists():
        print(f"Error: {output_dir} already exists")
        sys.exit(1)

    output_dir.mkdir()

    util.run_symqemu_on_test_binary(binary_name=binary_name, generated_test_cases_output_dir=output_dir)

    print(f"Expected outputs for {binary_name} generated in {output_dir}")
