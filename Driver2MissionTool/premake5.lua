-- premake5.lua

project "Driver2MissionTool"
    kind "ConsoleApp"
    language "C++"
    targetdir "bin/%{cfg.buildcfg}"

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
	