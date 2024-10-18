import pathlib
import subprocess

SYMQEMU_EXECUTABLE = pathlib.Path(__file__).parent.parent.parent / "build" / "qemu-x86_64"
BINARIES_DIR = pathlib.Path(__file__).parent / "binaries"


class SymqemuRunFailed(Exception):
    pass


def run_symqemu(
        binary: pathlib.Path,
        binary_arguments: list[str],
        generated_test_cases_output_dir: pathlib.Path,
        symbolized_input_file: pathlib.Path,
) -> None:
    command = str(SYMQEMU_EXECUTABLE), str(binary), *binary_arguments

    environment_variables = {
        'SYMCC_OUTPUT_DIR': str(generated_test_cases_output_dir),
        'SYMCC_INPUT_FILE': str(symbolized_input_file)
    }

    print(f'about to run command: {" ".join(command)}')
    print(f'with environment variables: {environment_variables}')

    try:
        subprocess.run(
            command,
            env=environment_variables,
            capture_output=True,
            check=True,
        )
    except subprocess.CalledProcessError as e:
        raise SymqemuRunFailed(
            f'command {e.cmd} failed with exit code {e.returncode} and output: {e.stderr.decode()}'
        )


def run_symqemu_on_test_binary(
        binary_name: str,
        generated_test_cases_output_dir: pathlib.Path
) -> None:
    binary_dir = BINARIES_DIR / binary_name

    with open(binary_dir / 'args', 'r') as f:
        binary_args = f.read().strip().split(' ')

    def replace_placeholder_with_input(arg: str) -> str:
        return str(binary_dir / 'input') if arg == '@@' else arg

    binary_args = list(map(replace_placeholder_with_input, binary_args))

    run_symqemu(
        binary=binary_dir / 'binary',
        binary_arguments=binary_args,
        generated_test_cases_output_dir=generated_test_cases_output_dir,
        symbolized_input_file=binary_dir / 'input',
    )
