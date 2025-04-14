HE = "%{wks.location}"

include "Dependencies.lua"

-------------------------------------------------------------------------------------
-- Setup
-------------------------------------------------------------------------------------
Setup()

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

    group "Dependencies"
        include "ThirdParty/glfw"
        include "ThirdParty/nativefiledialog-extended"
        include "ThirdParty/nvrhi"
        include "ThirdParty/ShaderMake"

    group "HydraEngine"
        include "HydraEngine"
        