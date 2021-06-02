@echo off

cd %APPVEYOR_BUILD_FOLDER%
git submodule update --init --recursive

appveyor DownloadFile %windows_premake_url% -FileName premake5.zip
7z x premake5.zip -o%project_folder% -aoa

appveyor DownloadFile %windows_sdl2_url% -FileName SDL2.zip
7z x SDL2.zip -o%dependency_folder%