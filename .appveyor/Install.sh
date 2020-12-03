#!/usr/bin/env bash
set -ex

cd "$APPVEYOR_BUILD_FOLDER/src"

# Download premake5
# because it isn't in the repos (yet?)
curl "$linux_premake_url" -Lo premake5.tar.gz
tar xvf premake5.tar.gz

sudo apt-get update -qq -y
sudo apt-get install -qq aptitude -y

# fix Ubuntu's broken mess of packages using aptitude
sudo aptitude install --quiet=2 \
    g++-multilib -y

