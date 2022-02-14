-- premake5.lua

project "DriverImageTool"
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
	