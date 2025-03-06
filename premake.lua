HE = "%{wks.location}"

include "Dependencies.lua"

function SetupDxc()
    local dxcDir = "ThirdParty/dxc"
    local host = os.host()
    local dxcUrl, arch, bin
    if host=="windows" then
        dxcUrl = "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.7.2212.1/dxc_2023_03_01.zip"
        arch   = path.join(dxcDir, "dxc.zip")
        bin    = path.join(dxcDir, "bin", "x64", "dxc.exe")
    else
        dxcUrl = "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.7.2212.1/linux_dxc_2023_03_01.x86_64.tar.gz"
        arch   = path.join(dxcDir, "dxc.tar.gz")
        bin    = path.join(dxcDir, "bin", "dxc")
    end
    if os.isfile(bin) then return bin end
    os.mkdir(dxcDir)
    print("Downloading DXC...")
    Download(dxcUrl, arch)
    print("Extracting DXC...")
    Extract(arch, dxcDir)
    if host~="windows" then os.execute("chmod +x "..bin) end
    print("DXC setup complete! Binary located at: "..bin)
end

-------------------------------------------------------------------------------------
-- Setup
-------------------------------------------------------------------------------------
SetupDxc()

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
        