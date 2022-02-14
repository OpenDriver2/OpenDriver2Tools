-- premake5.lua

project "Driver2CutsceneTool"
    kind "ConsoleApp"
    language "C++"
	
	uses { "frameworkLib", "libnstd" }

    files {
        "**.cpp",
        "**.h",
    }
        
    filter "system:linux"
        buildoptions {
            "-Wno-narrowing",
            "-fpermissive",
        }
        
    filter "configurations:Debug"
		targetsuffix "_dbg"
		 symbols "On"

    filter "configurations:Release"
        optimize "Full"
	