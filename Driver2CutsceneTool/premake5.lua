-- premake5.lua

project "Driver2CutsceneTool"
    kind "ConsoleApp"
    language "C++"
    compileas "C++"
    targetdir "bin/%{cfg.buildcfg}"
	
	-- framework link
	dependson { "frameworkLib", "libnstd" }
	links { "frameworkLib", "libnstd" }
	includedirs {
		"../dependencies/libnstd/include",
	}
	--

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
	