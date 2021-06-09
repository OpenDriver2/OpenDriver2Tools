-- premake5.lua

require "premake_modules/usage"

-- you can redefine dependencies
SDL2_DIR = os.getenv("SDL2_DIR") or "dependencies/SDL2"

workspace "OpenDriver2Tools"
    language "C++"
    configurations { "Debug", "Release" }
	linkgroups 'On'
	characterset "ASCII"
	
	includedirs {
		"./"
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
		characterset "ASCII"

-- NoSTD
project "libnstd"
    kind "StaticLib"
	targetdir "bin/%{cfg.buildcfg}"
	
	filter "system:Windows"
		defines { "_CRT_SECURE_NO_WARNINGS", "__PLACEMENT_NEW_INLINE" }
    
	includedirs {
		"dependencies/libnstd/include"
	}
	
    files {
        "dependencies/libnstd/src/**.cpp",
        "dependencies/libnstd/src/**.h",
    }
	
usage "libnstd"
	includedirs {
		"dependencies/libnstd/include"
	}
	links "libnstd"
	
-- little framework
project "frameworkLib"
    kind "StaticLib"
	targetdir "bin/%{cfg.buildcfg}"
	
	uses "libnstd"

	filter "system:Windows"
		defines { "_CRT_SECURE_NO_WARNINGS" }
	
	files {
		"math/**.cpp",
		"math/**.h",
		"core/**.cpp",
		"core/**.h",
		"util/**.cpp",
		"util/**.h",
    }
	
-- GLAD
project "glad"
    kind "StaticLib"
	targetdir "bin/%{cfg.buildcfg}"
	
	filter "system:Windows"
		defines { "_CRT_SECURE_NO_WARNINGS" }
	
    files {
        "dependencies/glad/*.c",
        "dependencies/glad/*.h",
    }
	
usage "glad"
	includedirs {
        "dependencies/glad/*.h",
    }
	
-- ImGui
project "ImGui"
    kind "StaticLib"
	targetdir "bin/%{cfg.buildcfg}"
	
	filter "system:Windows"
		defines { "_CRT_SECURE_NO_WARNINGS", "IMGUI_IMPL_OPENGL_LOADER_GLAD" }
    
	includedirs {
		"dependencies/imgui",
		"dependencies",
		SDL2_DIR.."/include"
	}
	
    files {
        "dependencies/imgui/*.cpp",
        "dependencies/imgui/*.h",
		"dependencies/imgui/backends/imgui_impl_opengl3.cpp",
		"dependencies/imgui/backends/imgui_impl_opengl3.h",
		"dependencies/imgui/backends/imgui_impl_sdl.cpp",
        "dependencies/imgui/backends/imgui_impl_sdl.h",
    }
	
usage "ImGui"
	includedirs {
		"dependencies/imgui",
	}
	links { "ImGui" }

include "DriverLevelTool/premake5.lua"
include "DriverSoundTool/premake5.lua"
include "DriverImageTool/premake5.lua"
include "Driver2CutsceneTool/premake5.lua"
include "Driver2MissionTool/premake5.lua"