#!/usr/bin/env bash
set -ex

cd "$APPVEYOR_BUILD_FOLDER"
git submodule update --init --recursive

# Download premake5
# because it isn't in the repos (yet?)
curl "$linux_premake_url" -Lo premake5.tar.gz
tar xvf premake5.tar.gz

sudo apt-get update

sudo apt-get install -qq \
    libsdl2-dev \
    g++-multilib -y \

