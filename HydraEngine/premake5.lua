project "HydraEngine"
    kind "SharedLib"
    language "C++"
    cppdialect "C++latest"
    staticruntime "off"
    location (projectLocation)
    targetdir (binOutputDir)
    objdir (IntermediatesOutputDir)

    files {

        "Source/HydraEngine/**.cpp",
        "Include/**.h",
        "Include/**.cppm",
        "*.lua",

        "%{IncludeDir.glm}/**.cppm",
        "%{IncludeDir.simdjson}/**.cppm",
        "%{IncludeDir.simdjson}/**.cpp",
        "%{IncludeDir.magic_enum}/**.cppm",
    }

    buildoptions {

        AddCppm("nvrhi"),
        AddCppm("std"),
    }

    defines {

        --"HE_FORCE_DISCRETE_GPU",
        "HE_BUILD_SHAREDLIB",
        "GLFW_INCLUDE_NONE",
        "GLFW_DLL",
        "_CRT_SECURE_NO_WARNINGS",
        "NVRHI_SHARED_LIBRARY_INCLUDE",
        "SIMDJSON_BUILDING_WINDOWS_DYNAMIC_LIBRARY",
        "_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING",
    }

    includedirs {

        "%{IncludeDir.HydraEngine}",
        "%{IncludeDir.spdlog}",
        "%{IncludeDir.glfw}",
        "%{IncludeDir.glm}",
        "%{IncludeDir.NFDE}",
        "%{IncludeDir.taskflow}",
        "%{IncludeDir.stb_image}",
        "%{IncludeDir.simdjson}",
        "%{IncludeDir.nvrhi}",
        "%{IncludeDir.Vulkan_Headers}",
        "%{IncludeDir.ShaderMake}",
        "%{IncludeDir.miniz}",
        "%{IncludeDir.magic_enum}",
    }

    links {

        "glfw",
        "nvrhi",
        "NFDE",
        "ShaderMakeBlob",
    }

    filter "system:windows"
        systemversion "latest"
        files { "Source/Platform/WindowsPlatform.cpp" }

        links {

            "DXGI.lib",
            "dxguid.lib",
        }

        if RHI.enableD3D11 then
            links { "D3D11.lib" }
            defines { "NVRHI_HAS_D3D11" }
        end

        if RHI.enableD3D12 then
            links { "D3D12.lib" }
            defines { "NVRHI_HAS_D3D12" }
        end

        if RHI.enableVulkan then
            defines { "NVRHI_HAS_VULKAN" }
            files { "Source/Platform/VulkanDeviceManager.cpp" }
        end

    filter "system:linux"
        systemversion "latest"
        files { "Source/Platform/LinuxPlatform.cpp" }

        if RHI.enableVulkan then
            defines { "NVRHI_HAS_VULKAN" }
            files { "Source/Platform/VulkanDeviceManager.cpp" }
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
        runtime "Release"
        optimize "On"
        files { "%{IncludeDir.tracy}/TracyClient.cpp" }
        includedirs { "%{IncludeDir.tracy}" }
        defines { "HE_PROFILE", "TRACY_EXPORTS" , "TRACY_ENABLE" }

    filter "configurations:Dist"
        defines "HE_DIST"
        runtime "Release"
        optimize "Speed"
        symbols "Off"
