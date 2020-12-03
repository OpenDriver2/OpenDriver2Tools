@echo off

set config=Debug:Release
for %%c in (%config::= %) do (
    cd %project_folder%\DriverLevelTool\bin\%%c

    xcopy /e /v %data_folder% .\ /Y
    7z a "DriverLevelTool_%%c.zip" ".\*"
	
	cd %project_folder%\DriverSoundTool\bin\%%c

    xcopy /e /v %data_folder% .\ /Y
    7z a "DriverSoundTool_%%c.zip" ".\*"
)
