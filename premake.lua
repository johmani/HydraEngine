-------------------------------------------------------------------------------------
-- global variables
-------------------------------------------------------------------------------------
outputdir = "%{CapitalizeFirstLetter(cfg.system)}-%{cfg.architecture}/%{cfg.buildcfg}"
binOutputDir = "%{wks.location}/Build/" .. outputdir .. "/Bin"
libOutputDir = "%{wks.location}/Build/" .. outputdir .. "/Lib/%{prj.name}"
IntermediatesOutputDir = "%{wks.location}/Build/Intermediates/%{outputdir}/%{prj.name}"

IncludeDir = {}
IncludeDir["HydraEngine"] = "%{wks.location}/HydraEngine/Include"
IncludeDir["glfw"] = "%{wks.location}/ThirdParty/glfw/include"
IncludeDir["spdlog"] = "%{wks.location}/ThirdParty/spdlog/include"
IncludeDir["glm"] = "%{wks.location}/ThirdParty/glm"
IncludeDir["stb_image"] = "%{wks.location}/ThirdParty/stb_image"
IncludeDir["yaml_cpp"] = "%{wks.location}/ThirdParty/yaml-cpp/include"
IncludeDir["nvrhi"] = "%{wks.location}/ThirdParty/nvrhi/include"
IncludeDir["NFDE"] = "%{wks.location}/ThirdParty/nativefiledialog-extended/src/include"
IncludeDir["ShaderMake"] = "%{wks.location}/ThirdParty/ShaderMake/include"
IncludeDir["tracy"] = "%{wks.location}/ThirdParty/tracy/public/tracy"
IncludeDir["taskflow"] = "%{wks.location}/ThirdParty/taskflow"
IncludeDir["Vulkan_Headers"] = "%{wks.location}/ThirdParty/Vulkan-Headers/Include"

-------------------------------------------------------------------------------------
-- helper functions
-------------------------------------------------------------------------------------
function CapitalizeFirstLetter(str)
    return str:sub(1,1):upper() .. str:sub(2)
end

function Download(url, output)
    if os.host() == "windows" then
        os.execute("powershell -Command \"Invoke-WebRequest -Uri '" .. url .. "' -OutFile '" .. output .. "'\"")
    else
        os.execute("curl -L -o " .. output .. " " .. url)
    end
end

function Extract(archive, outputDir)
    if os.host() == "windows" then
        os.execute("powershell -Command \"Expand-Archive -Path '" .. archive .. "' -DestinationPath '" .. outputDir .. "' -Force\"")
    else
        os.execute("tar -xzf " .. archive .. " -C " .. outputDir)
    end
end

function FindFxc()
    local sdkRoot = "C:/Program Files (x86)/Windows Kits/10/bin/"
    local versions = os.matchdirs(sdkRoot .. "*")
    table.sort(versions, function(a, b) return a > b end)
    for _, dir in ipairs(versions) do 
        local path = dir .. "/x64/fxc.exe" 
        if os.isfile(path) then  return path end
    end
    return nil
end

function BuildShaders(apiList ,src, cache, flags, includeDirs)
    local exeExt = (os.host() == "windows") and ".exe" or ""
    local sm    = "%{binOutputDir}/ShaderMake" .. exeExt
    local cfg   = path.join(src, "shaders.cfg")
    local dxc   = "%{wks.location}/ThirdParty/dxc/bin/x64/dxc" .. exeExt
    local fxc   = FindFxc()
    local inc   = table.concat(includeDirs, "\" -I \"", 1, -1, "-I \"")
    local sep   = (os.host() == "windows") and " && " or " ; "
    apiList = apiList or { D3D11 = false, D3D12 = false,  VULKAN = false, METAL = false }

    local conf = {}
    if apiList.VULKAN then table.insert(conf, { out = "spirv", args = "--platform SPIRV --vulkanVersion 1.2 --tRegShift 0 --sRegShift 128 --bRegShift 256 --uRegShift 384 -D SPIRV --compiler \"" .. dxc .. "\"" }) end
    if os.host() == "windows" then
        if apiList.D3D11 then table.insert(conf, { out = "dxbc", args = "--platform DXBC --shaderModel 6_5 --compiler \"" .. fxc .. "\"" }) end
        if apiList.D3D12 then table.insert(conf, { out = "dxil", args = "--platform DXIL --shaderModel 6_5 --compiler \"" .. dxc .. "\"" }) end
    end

    local cmds = {}
    for _, c in ipairs(conf) do
        table.insert(cmds, string.format(
            "\"%s\" --config \"%s\" %s --outputExt .bin --colorize --verbose --out %s %s %s",
            sm, cfg, inc, path.join(cache, c.out), flags, c.args))
    end

    return table.concat(cmds, sep)
end

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

-- TODO: Maybe we should reconsider this approach or refine the logic.
function AddCppm(arg1, arg2) --  (name) or (directory, name)
    local isMSVC = _OPTIONS["cc"] == "msc" or os.target() == "windows"
    if not isMSVC then
        print("Error: AddCppm works only for MSVC currently")
        return ""
    end

    if arg2 then -- (directory, name)
        return "/reference " .. path.join("%{wks.location}", "Build", "Intermediates", outputdir, arg1, (arg2 .. ".cppm.ifc"))
    else -- (name)
        if arg1 == "std" then
            return "/reference " .. path.join("%{wks.location}", "Build", "Intermediates", outputdir, "HydraEngine", "microsoft", "STL", "std.ixx.ifc")
        else
            return "/reference " .. path.join("%{wks.location}", "Build", "Intermediates", outputdir, arg1, (arg1 .. ".cppm.ifc"))
        end
    end
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
        include "ThirdParty/yaml-cpp"
        include "ThirdParty/nvrhi"
        include "ThirdParty/ShaderMake"

    group "HydraEngine"
        include "HydraEngine"
        