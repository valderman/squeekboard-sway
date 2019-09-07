#!/bin/sh
set -e

export CARGO_TARGET_DIR=`pwd`
if [ ! -z ${2} ]; then
    OUT_PATH=`realpath "${2}"`
fi

cd $1
cargo $3 -p rs

if [ ! -z ${OUT_PATH} ]; then
    cp "${CARGO_TARGET_DIR}"/debug/librs.a "${OUT_PATH}"
fi
