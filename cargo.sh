#!/bin/bash

# This script manages Cargo operations
# while keeping the artifact directory within the build tree
# instead of the source tree

set -e

SOURCE_DIR="$1"

export CARGO_TARGET_DIR=`pwd`
if [ ! -z ${2} ]; then
    OUT_PATH=`realpath "${2}"`
fi

cd $SOURCE_DIR
shift
shift
cargo $BUILD_ARG $@

if [ ! -z ${OUT_PATH} ]; then
    cp "${CARGO_TARGET_DIR}"/debug/librs.a "${OUT_PATH}"
fi
