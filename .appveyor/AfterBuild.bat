@echo off

set config=Debug:Release
for %%c in (%config::= %) do (
    cd %project_folder%\bin\%%c
    
    copy %windows_sdl2_dir%\lib\x86\SDL2.dll SDL2.dll /Y

    xcopy /e /v %data_folder% .\ /Y
    7z a "OpenDriverTools_%%c.zip" ".\*"
)
