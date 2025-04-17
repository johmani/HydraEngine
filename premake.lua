HE = "%{wks.location}"

include "Dependencies.lua"

-------------------------------------------------------------------------------------
-- Setup
-------------------------------------------------------------------------------------
CloneLibs(
    {
        windows = "https://github.com/johmani/HydraEngineLibs_Windows_x64",
    },
    "ThirdParty/Lib"
)

-------------------------------------------------------------------------------------
-- workspace
-------------------------------------------------------------------------------------
workspace "HydraEngine"
    architecture "x86_64"
    configurations { "Debug", "Release", "Profile", "Dist" }
    flags
    {
      "MultiProcessorCompile",
	}

    group "ThirdParty"
        include "ThirdParty/glfw"
        include "ThirdParty/nativefiledialog-extended"
        include "ThirdParty/nvrhi"
        include "ThirdParty/ShaderMake"

    group "HydraEngine"
        include "HydraEngine"
        