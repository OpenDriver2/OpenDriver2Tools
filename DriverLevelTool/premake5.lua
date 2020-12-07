-- premake5.lua

project "DriverLevelTool"
    kind "ConsoleApp"
    language "C++"
    compileas "C++"
    targetdir "bin/%{cfg.buildcfg}"
	
	includedirs {
		"./"
	}

    files {
        "**.cpp",
        "**.h",
		"driver_routines/**.cpp",
		"driver_routines/**.h",
    }
        
    filter "system:linux"
        buildoptions {
            "-Wno-narrowing",
            "-fpermissive",
            "-m32"
        }
        
        linkoptions {
            "-m32"
        }

        cppdialect "C++11"

    filter "configurations:Debug"
		targetsuffix "_dbg"
		 symbols "On"

    filter "configurations:Release"
        optimize "Full"
	