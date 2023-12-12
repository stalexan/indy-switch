#!/bin/bash

function message() {
    printf "$1\n"
}

function errMessage() {
    printf "ERROR: $1\n"
} >&2

function errExit() {
    errMessage "$1"
    exit 1
}

# Exit on error and trace
set -eu

# cd to the directory this script is in
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
cd "${SCRIPT_DIR}"

# Run cpplint on all.
cpplint --quiet --exclude=./components/*/build --recursive ./main ./components/*

echo "Success! All checks passed."
