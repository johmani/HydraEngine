-------------------------------------------------------------------------------------
-- global variables
-------------------------------------------------------------------------------------
HE = path.getabsolute(".")
if includSourceCode == nil then includSourceCode = true end -- includSourceCode defaults to true, it should be included in client project

if RHI == nil then RHI = {} end
if RHI.enableD3D11  == nil then RHI.enableD3D11 = os.host() == "windows" end 
if RHI.enableD3D12  == nil then RHI.enableD3D12 = os.host() == "windows" end 
if RHI.enableVulkan == nil then RHI.enableVulkan = true end 

outputdir = "%{CapitalizeFirstLetter(cfg.system)}-%{cfg.architecture}/%{cfg.buildcfg}"
binOutputDir = "%{wks.location}/Build/%{outputdir}/Bin"
libOutputDir = "%{wks.location}/Build/%{outputdir}/Lib/%{prj.name}"
IntermediatesOutputDir = "%{wks.location}/Build/Intermediates/%{outputdir}/%{prj.name}"

IncludeDir = {}
IncludeDir["HydraEngine"] = "%{HE}/HydraEngine/Include"
IncludeDir["glfw"] = "%{HE}/ThirdParty/glfw/include"
IncludeDir["spdlog"] = "%{HE}/ThirdParty/spdlog/include"
IncludeDir["glm"] = "%{HE}/ThirdParty/glm"
IncludeDir["stb_image"] = "%{HE}/ThirdParty/stb_image"
IncludeDir["nvrhi"] = "%{HE}/ThirdParty/nvrhi/include"
IncludeDir["NFDE"] = "%{HE}/ThirdParty/nativefiledialog-extended/src/include"
IncludeDir["ShaderMake"] = "%{HE}/ThirdParty/ShaderMake/include"
IncludeDir["tracy"] = "%{HE}/ThirdParty/tracy/public"
IncludeDir["taskflow"] = "%{HE}/ThirdParty/taskflow"
IncludeDir["Vulkan_Headers"] = "%{HE}/ThirdParty/Vulkan-Headers/Include"
IncludeDir["simdjson"] = "%{HE}/ThirdParty/simdjson"
IncludeDir["miniz"] = "%{HE}/ThirdParty/miniz"
IncludeDir["magic_enum"] = "%{HE}/ThirdParty/magic_enum/include"

LibDir = {}
LibDir["HydraEngine"] = includSourceCode and binOutputDir or "%{HE}/Build/%{outputdir}/Bin" 

Tools = {}
Tools["ShaderMake"] = "%{LibDir.HydraEngine}/ShaderMake"

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
    includeDirs = includeDirs or {}
    local exeExt = (os.host() == "windows") and ".exe" or ""
    local sm    = "%{Tools.ShaderMake}" .. exeExt
    local cfg   = path.join(src, "shaders.cfg")
    local dxc   = "%{HE}/ThirdParty/Lib/dxc/bin/x64/dxc" .. exeExt
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
            "\"%s\" --config \"%s\" %s --outputExt .bin --colorize --verbose --out \"%s\" %s %s",
            sm, cfg, inc, path.join(cache, c.out), flags, c.args))
    end

    return table.concat(cmds, sep)
end

function SetupShaders(apiList ,src, cache, flags, includeDirs)
    filter { "files:**.hlsl" }
        buildcommands {
            BuildShaders(apiList ,src, cache, flags, includeDirs)
        }
        buildoutputs { "%{wks.location}/dumy" }
    filter {}
end

-- TODO: Maybe we should reconsider this approach or refine the logic.
function AddProjCppm(projDir, arg1, arg2) --  (name) or (directory, name)
    local isMSVC = _OPTIONS["cc"] == "msc" or os.target() == "windows"
    if not isMSVC then
        print("Error: AddCppm works only for MSVC currently")
        return ""
    end

    local base = path.join(projDir, "Build", "Intermediates", outputdir)

    if arg2 then -- (directory, name)
        return '/reference "' .. path.join(base, arg1, arg2 .. ".cppm.ifc") .. '"'
    else -- (name)
        if arg1 == "std" then
            return '/reference "' .. path.join(base, "HydraEngine", "microsoft", "STL", "std.ixx.ifc") .. '"'
        else
            return '/reference "' .. path.join(base, arg1, arg1 .. ".cppm.ifc") .. '"'
        end
    end
end

function AddCppm(arg1, arg2) --  (name) or (directory, name)
    local isMSVC = _OPTIONS["cc"] == "msc" or os.target() == "windows"
    if not isMSVC then
        print("Error: AddCppm works only for MSVC currently")
        return ""
    end

    local base = path.join("%{wks.location}", "Build", "Intermediates", outputdir)

    if arg2 then -- (directory, name)
        return '/reference "' .. path.join(base, arg1, arg2 .. ".cppm.ifc") .. '"'
    else -- (name)
        if arg1 == "std" then
            return '/reference "' .. path.join(base, "HydraEngine", "microsoft", "STL", "std.ixx.ifc") .. '"'
        else
            return '/reference "' .. path.join(base, arg1, arg1 .. ".cppm.ifc") .. '"'
        end
    end
end

function InitProject()
    architecture "x86_64"
    configurations { "Debug", "Release", "Profile", "Dist" }
    flags {  "MultiProcessorCompile" }
end

function IncludeHydraProject(inc)
    if inc then 
        group "HydraEngine"
            group "HydraEngine/ThirdParty"
                include (HE .. "/ThirdParty/glfw")
                include (HE .. "/ThirdParty/nativefiledialog-extended")
                include (HE .. "/ThirdParty/nvrhi")
                include (HE .. "/ThirdParty/ShaderMake")
            group "HydraEngine"
            include (HE .. "/HydraEngine")
        group ""
    end
end

function LinkHydra(inc, extra)

    if not inc then
        buildoptions {

            AddProjCppm(HE, "std"),
            AddProjCppm(HE, "HydraEngine"),
            AddProjCppm(HE, "nvrhi"),
            AddProjCppm(HE, "HydraEngine", "glm"),
            AddProjCppm(HE, "HydraEngine", "simdjson"),
            AddProjCppm(HE, "HydraEngine", "magic_enum"),
        }

        libdirs {

            "%{LibDir.HydraEngine}",
        }
    else
        buildoptions {

            AddCppm("std"),
            AddCppm("HydraEngine"),
            AddCppm("nvrhi"),
            AddCppm("HydraEngine", "glm"),
            AddCppm("HydraEngine", "simdjson"),
            AddCppm("HydraEngine", "magic_enum"),
        }
    end

    defines {

        "HE_IMPORT_SHAREDLIB",
        "NVRHI_SHARED_LIBRARY_INCLUDE",
        "_CRT_SECURE_NO_WARNINGS",
        "_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING",
    }

    includedirs {

        "%{IncludeDir.HydraEngine}",
        "%{IncludeDir.nvrhi}",
        "%{IncludeDir.glm}",
        "%{IncludeDir.taskflow}",
        "%{IncludeDir.tracy}",
        "%{IncludeDir.magic_enum}",
    }

    links {

        "HydraEngine",
        "nvrhi",
        extra
    }
end

function LinkHydraApp(inc, extra)

    LinkHydra(inc, extra)

    if not inc then
        prebuildcommands {

            "{COPY} \"%{LibDir.HydraEngine}/*.dll\" \"%{binOutputDir}\""
        }
    end

    filter "configurations:Dist"
        kind "WindowedApp"
    filter {}
end

function SetHydraFilters()
    filter "system:windows"
        systemversion "latest"

        if RHI.enableD3D11 then
            defines { "NVRHI_HAS_D3D11" }
        end

        if RHI.enableD3D12 then
            defines { "NVRHI_HAS_D3D12" }
        end

        if RHI.enableVulkan then
            defines { "NVRHI_HAS_VULKAN" }
        end

    filter "system:linux"
        systemversion "latest"

        if RHI.enableVulkan then
            defines { "NVRHI_HAS_VULKAN" }
        end

    filter "configurations:Debug"
        defines "HE_DEBUG"
        runtime "Debug"
        symbols "On"

    filter "configurations:Release"
        defines "HE_RELEASE"
        runtime "Release"
        optimize "On"

    filter "configurations:Profile"
        includedirs { "%{IncludeDir.tracy}" }
        defines { "HE_PROFILE", "TRACY_IMPORTS" }
        runtime "Release"
        optimize "On"

    filter "configurations:Dist"
        defines "HE_DIST"
        runtime "Release"
        optimize "Speed"
        symbols "Off"
    filter {}
end

function AddPlugins()
    group "Plugins"
        local pluginsDir = path.getabsolute("Plugins")
        local plugins = os.matchdirs(pluginsDir .. "/*")
        for _, plugin in ipairs(plugins) do
            include(plugin)
        end
    group ""
end

function CloneLibs(platformRepoURLs, targetDir, branchOrTag)
    local repoURL = platformRepoURLs[os.host()]
    if not repoURL then
        error("No repository URL defined for platform: " .. os.host())
    end

    targetDir = targetDir or "ThirdParty/Lib"

    if not os.isdir(targetDir) then
        local cloneCmd = string.format(
            'git clone --depth 1 %s"%s" "%s"',
            branchOrTag and ('--branch ' .. branchOrTag .. ' ') or '',
            repoURL,
            targetDir
        )

        print("Cloning " .. repoURL)
        os.execute(cloneCmd)
    end

    for _, z in ipairs(os.matchfiles(targetDir .. "/*.zip")) do
        print("Extracting " .. z .. " to " .. targetDir)
        zip.extract(z, targetDir)
        os.remove(z)
    end
end

