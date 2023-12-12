#!/bin/bash

# Exit on error
set -eu

# cd to the directory this script is in
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
cd "${SCRIPT_DIR}"

# Build
idf.py build

# Extract the version number
CONFIG_H="components/indy_common/indy_config.h"
VERSION_MAJOR=$(grep "#define VERSION_MAJOR" $CONFIG_H | awk '{print $3}')
VERSION_MINOR=$(grep "#define VERSION_MINOR" $CONFIG_H | awk '{print $3}')
VERSION_PATCH=$(grep "#define VERSION_PATCH" $CONFIG_H | awk '{print $3}')
VERSION="${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}"

# Create bin file name that has version number
PROJNAME="indy_switch"
TARGET="esp32"
VER_NAME="build/${PROJNAME}_${VERSION}_${TARGET}.bin"

# Create bin copy with version number in file name
ORIG_NAME="build/${PROJNAME}.bin"
cp "$ORIG_NAME" "$VER_NAME"

