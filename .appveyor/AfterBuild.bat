@echo off

set config=Debug:Release
for %%c in (%config::= %) do (
    cd %project_folder%\bin\%%c

    xcopy /e /v %data_folder% .\ /Y
    7z a "OpenDriver2Tools_%%c.zip" ".\*"
)
