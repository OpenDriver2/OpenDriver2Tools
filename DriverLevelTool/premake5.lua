-- premake5.lua

project "DriverLevelTool"
    kind "ConsoleApp"
    language "C++"
    targetdir "bin/%{cfg.buildcfg}"

	uses { "ImGui", "libnstd", "frameworkLib" }

	includedirs {
		"./",
	}

    files {
		"**.c",
        "**.cpp",
        "**.h",
		"driver_routines/**.cpp",
		"driver_routines/**.h",
    }
        
    filter "system:linux"
        buildoptions {
            "-Wno-narrowing",
            "-fpermissive",
        }
		links {
			"dl"
        }
        
        cppdialect "C++11"

    filter "configurations:Debug"
		targetsuffix "_dbg"
		 symbols "On"

    filter "configurations:Release"
        optimize "Full"
	