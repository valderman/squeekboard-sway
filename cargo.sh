#!/bin/sh

# This script manages Cargo operations
# while keeping the artifact directory within the build tree
# instead of the source tree

set -e

SCRIPT_PATH="$(realpath "$0")"
SOURCE_DIR="$(dirname "$SCRIPT_PATH")"

CARGO_TARGET_DIR="$(pwd)"
export CARGO_TARGET_DIR
if [ "${1}" = "--rename" ]; then
    shift
    FILENAME="${1}"
    shift
    OUT_PATH="$(realpath "${1}")"
elif [ "${1}" = "--output" ]; then
    shift
    OUT_PATH="$(realpath "${1}")"
    FILENAME="$(basename "${OUT_PATH}")"
fi
shift

cd "$SOURCE_DIR"
cargo "$@"

if [ -n "${OUT_PATH}" ]; then
    cp -a "${CARGO_TARGET_DIR}"/debug/"${FILENAME}" "${OUT_PATH}"
fi
