#!/bin/sh

# This script manages Cargo operations
# while keeping the artifact directory within the build tree
# instead of the source tree

set -e

SCRIPT_PATH="$(realpath "$0")"
SOURCE_DIR="$(dirname "$SCRIPT_PATH")"

CARGO_TARGET_DIR="$(pwd)"
export CARGO_TARGET_DIR

cd "$SOURCE_DIR"
cargo "$@"

