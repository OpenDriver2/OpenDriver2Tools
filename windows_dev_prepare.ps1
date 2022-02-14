$windows_premake_url = 'https://github.com/premake/premake-core/releases/download/v5.0.0-beta1/premake-5.0.0-beta1-windows.zip'
$windows_sdl2_url = 'https://www.libsdl.org/release/SDL2-devel-2.0.20-VC.zip'

$project_folder = '.\\'
$dependency_folder = $project_folder + '\\dependencies'

# Download required dependencies
Invoke-WebRequest -Uri $windows_premake_url -OutFile PREMAKE.zip
Expand-Archive PREMAKE.zip -DestinationPath $project_folder

Invoke-WebRequest -Uri $windows_sdl2_url -OutFile SDL2.zip
Expand-Archive SDL2.zip -DestinationPath $dependency_folder


# Generate project files
$windows_sdl2_dir = '.\\dependencies\\SDL2-2.0.20'

$env:SDL2_DIR = $windows_sdl2_dir

Set-Location -Path $project_folder

& .\\premake5 vs2019

# Open solution
& .\\project_vs2019_windows\\OpenDriver2Tools.sln