-- premake5.lua

-- you can redefine dependencies
SDL2_DIR = os.getenv("SDL2_DIR") or "../dependencies/SDL2"

project "DriverLevelTool"
    kind "ConsoleApp"
    language "C++"
    compileas "C++"
    targetdir "bin/%{cfg.buildcfg}"
	
	includedirs {
		"./"
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
            "GL",
            "SDL2",
        }
        
        cppdialect "C++11"
		
	filter "system:windows"
		includedirs {
			SDL2_DIR.."/include"
		}
		links { 
            "SDL2", 
        }
		libdirs { 
			SDL2_DIR.."/lib/x86",
		}


    filter "configurations:Debug"
		targetsuffix "_dbg"
		 symbols "On"

    filter "configurations:Release"
        optimize "Full"
	