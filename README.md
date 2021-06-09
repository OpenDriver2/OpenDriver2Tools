# OpenDriver2Tools - Driver 1 and Driver 2 tools

Project targets to provide most accurate and clean code for Driver 1/2 format loading and manipulation.

[![Build status](https://ci.appveyor.com/api/projects/status/59t58wq3etym653n/branch/master?svg=true)](https://ci.appveyor.com/project/SoapyMan/opendriver2tools/branch/master)

### Features:
#### DriverLevelTool
 - ability to view Driver 1 (PS1) and Driver 2 LEV files
 - exporting models to OBJ
 - exporting whole city to OBJ
 - exporting texture pages to TGA files with original transparency key
 - exporting texture pages to separate TIM files
 - exporting level overhead map to TGA file
 - model compiler for REDRIVER2

#### DriverImageTool
 - convert sky images of Driver 2 (SKYn.RAW) to TIMs and TGA
 - convert frontend background images of Driver 2

#### DriverSoundTool
- convert MUSIC.BIN files of Driver 2 to separate FastTracker 2 XM files
- convert PSX (super-packed) XMs and SBK files of Driver 1 demos to FastTracker 2 XM files
- convert BLK files of retail Driver 1 and Driver 2 to separate WAV files
- convert SBK files of Driver 1 demo to separate WAV files

#### Driver2CutsceneTool
- allows to assemble Driver 2 cutscene file using chase replays saved by REDRIVER2
 
### Planned features:
 - OpenDriver2 integration
 
### Supported games
- Driver: You Are The Wheelman
- Driver 2 Demo (1.6 alpha - Aug 11 2000)
- Driver 2 Back On the Streets (Wheelman is Back)
