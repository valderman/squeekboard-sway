#!/bin/sh

# Builds the documentation and places in the selected directory,
# or the working directory.

set -e

SCRIPT_PATH="$(realpath "$0")"
DOCS_DIR="$(dirname "$SCRIPT_PATH")"

TARGET_DIR="${1:-./}"

SPHINX=sphinx-build

if [ ! -d $DOCS_DIR/_static ]; then
    mkdir -p $DOCS_DIR/_static
fi

if ! which sphinx-build ; then
    SPHINX=sphinx-build-3
fi
$SPHINX -b html "${DOCS_DIR}" "${TARGET_DIR}"
