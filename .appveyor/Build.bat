@echo off

cd %project_folder%

set SDL2_DIR=%windows_sdl2_dir%

premake5 vs2019

cd project_vs2019_windows

set config=Debug:Release
for %%c in (%config::= %) do (
    msbuild .\OpenDriver2Tools.sln /p:Configuration="%%c" /p:Platform=Win32 /m ^
        /logger:"C:\Program Files\AppVeyor\BuildAgent\Appveyor.MSBuildLogger.dll" ^
        /nologo /ConsoleLoggerParameters:NoSummary;Verbosity=quiet
)
