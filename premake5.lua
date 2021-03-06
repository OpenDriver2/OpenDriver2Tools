-- premake5.lua
	
workspace "OpenDriver2Tools"
    configurations { "Debug", "Release" }

    defines { VERSION } 
	
	includedirs {
		"./"
	}
	
	files {
		"math/**.cpp",
		"math/**.h",
		"core/**.cpp",
		"core/**.h",
		"util/**.cpp",
		"util/**.h",
    }

    filter "system:linux"
        buildoptions {
            "-Wno-narrowing",
            "-fpermissive",
        }

	filter "system:Windows"
		disablewarnings { "4996", "4554", "4244", "4101", "4838", "4309" }

    filter "configurations:Debug"
        defines { 
            "DEBUG", 
        }
        symbols "On"

    filter "configurations:Release"
        defines {
            "NDEBUG",
        }

dofile("DriverLevelTool/premake5.lua")
dofile("DriverSoundTool/premake5.lua")
dofile("DriverImageTool/premake5.lua")
dofile("Driver2CutsceneTool/premake5.lua")
dofile("Driver2MissionTool/premake5.lua")