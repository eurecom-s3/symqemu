#!/bin/bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

for bin in binaries/*; do
    rm -rf "$SCRIPT_DIR/$bin/expected_outputs"
    mv "$SCRIPT_DIR/$bin/generated_outputs/" "$SCRIPT_DIR/$bin/expected_outputs/"
done
