#!/bin/sh

# This script manages Cargo operations
# while keeping the artifact directory within the build tree
# instead of the source tree

set -e

SCRIPT_PATH="$(realpath "$0")"
SOURCE_DIR="$(dirname "$SCRIPT_PATH")"

CARGO_TARGET_DIR="$(pwd)"
export CARGO_TARGET_DIR
if [ -n "${1}" ]; then
    OUT_PATH="$(realpath "$1")"
fi

cd "$SOURCE_DIR"
shift
cargo "$@"

if [ -n "${OUT_PATH}" ]; then
    FILENAME="$(basename "${OUT_PATH}")"
    cp -a "${CARGO_TARGET_DIR}"/debug/"${FILENAME}" "${OUT_PATH}"
fi
