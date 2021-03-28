#!/usr/bin/env bash
set -ex

cd "$APPVEYOR_BUILD_FOLDER"

./premake5 gmake2 verbose

for config in debug release
do
    make config=$config -j$(nproc)
done
