@echo off

appveyor DownloadFile %windows_premake_url% -FileName premake5.zip
7z x premake5.zip -o%project_folder% -aoa
