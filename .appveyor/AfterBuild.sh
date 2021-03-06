#!/usr/bin/env bash
set -ex

for config in debug release
do
    cd "${APPVEYOR_BUILD_FOLDER}/DriverLevelTool/bin/${config^}"
    tar -czf "DriverLevelTool_${config^}.tar.gz" *
	
	cd "${APPVEYOR_BUILD_FOLDER}/DriverSoundTool/bin/${config^}"
    tar -czf "DriverSoundTool_${config^}.tar.gz" *
	
	cd "${APPVEYOR_BUILD_FOLDER}/DriverImageTool/bin/${config^}"
    tar -czf "DriverImageTool_${config^}.tar.gz" *
	
	cd "${APPVEYOR_BUILD_FOLDER}/DriverCutsceneTool/bin/${config^}"
    tar -czf "DriverCutsceneTool_${config^}.tar.gz" *
done
