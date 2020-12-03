-- premake5.lua
	
workspace "OpenDriver2Tools"
    configurations { "Debug", "Release" }

    defines { VERSION } 

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
		"exporter/**.cpp",
		"exporter/**.h",
		"math/**.cpp",
		"math/**.h",
		"core/**.cpp",
		"core/**.h",
    }
        
    filter "system:linux"
        buildoptions {
            "-Wno-narrowing",
            "-fpermissive",
            "-m32"
        }
        
        cppdialect "C++11"

    filter "configurations:Debug"
		targetsuffix "_dbg"
		 symbols "On"

    filter "configurations:Release"
        optimize "Full"
	