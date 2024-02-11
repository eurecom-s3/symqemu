import pathlib
import subprocess

SYMQEMU_RUN_STDOUT_STDERR = pathlib.Path('/tmp/symqemu_stdout_stderr')
SYMQEMU_EXECUTABLE = pathlib.Path(__file__).parent.parent.parent / "build" / "x86_64-linux-user" / "qemu-x86_64"
BINARIES_DIR = pathlib.Path(__file__).parent / "binaries"

class SymqemuRunFailed(Exception):
    pass

def run_symqemu(
        binary: pathlib.Path,
        binary_arguments: list[str],
        generated_test_cases_output_dir: pathlib.Path,
        symbolized_input_file: pathlib.Path = None,
):
    command = str(SYMQEMU_EXECUTABLE), str(binary), *binary_arguments

    environment_variables = {'SYMCC_OUTPUT_DIR': str(generated_test_cases_output_dir)}
    if symbolized_input_file is not None:
        environment_variables['SYMCC_INPUT_FILE'] = str(symbolized_input_file)

    print(f'about to run command: {" ".join(command)}')
    print(f'with environment variables: {environment_variables}')

    with open(SYMQEMU_RUN_STDOUT_STDERR, 'w') as f:
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
        generated_test_cases_output_dir: pathlib.Path = None
) -> None:
    if generated_test_cases_output_dir is None:
        generated_test_cases_output_dir = pathlib.Path('/tmp/symqemu_output')
        generated_test_cases_output_dir.mkdir(exist_ok=True)

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
