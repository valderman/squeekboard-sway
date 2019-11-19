#!/bin/sh

# This script manages Cargo builds
# while keeping the artifact directory within the build tree
# instead of the source tree

set -e

SCRIPT_PATH="$(realpath "$0")"
SOURCE_DIR="$(dirname "$SCRIPT_PATH")"

RELEASE=""
BINARY_DIR="debug"
if [ "${1}" = "--release" ]; then
    shift
    BINARY_DIR="release"
    RELEASE="--release"
fi

if [ "${1}" = "--rename" ]; then
    shift
    FILENAME="${1}"
    shift
fi
OUT_PATH="$(realpath "${1}")"
shift
OUT_BASENAME="$(basename "${OUT_PATH}")"
FILENAME="${FILENAME:-"${OUT_BASENAME}"}"

sh "$SOURCE_DIR"/cargo.sh build $RELEASE "$@"

if [ -n "${OUT_PATH}" ]; then
    cp -a ./"${BINARY_DIR}"/"${FILENAME}" "${OUT_PATH}"
fi
