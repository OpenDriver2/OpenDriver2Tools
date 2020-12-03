#!/usr/bin/env bash
set -ex

cd "$APPVEYOR_BUILD_FOLDER/src"

./premake5 gmake2

for config in debug release
do
    make config=$config -j$(nproc)
done
