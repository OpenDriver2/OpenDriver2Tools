#!/usr/bin/env bash
set -ex

for config in debug release release_dev
do
    cd "${APPVEYOR_BUILD_FOLDER}/src/bin/${config^}"
    tar -czf "OpenDriver2Tools_${config^}.tar.gz" *
done
