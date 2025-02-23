project "HydraEngine"
    kind "SharedLib"
    language "C++"
    cppdialect "C++latest"
    staticruntime "off"
    targetdir (binOutputDir)
    objdir (IntermediatesOutputDir)

    files 
    { 
        "Source/HydraEngine/**.cpp",
        "Include/**.h",
        "Include/**.cppm",
        
        "%{IncludeDir.HydraEngine}/**.cppm",
        "%{IncludeDir.nvrhi}/**.cppm",
        "%{IncludeDir.yaml_cpp}/**.cppm",
        "%{IncludeDir.glm}/**.cppm",
    }

    defines 
    {
        --"HE_FORCE_DISCRETE_GPU",
        "HE_BUILD_SHAREDLIB",
        "GLFW_INCLUDE_NONE",
        "GLFW_DLL",
        "_CRT_SECURE_NO_WARNINGS",
        "NVRHI_SHARED_LIBRARY_INCLUDE",
        "YAML_CPP_STATIC_DEFINE",
        "_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING",
    }
    
    includedirs
    {
        "%{IncludeDir.HydraEngine}",
        "%{IncludeDir.spdlog}",
        "%{IncludeDir.glfw}",
        "%{IncludeDir.glm}",
        "%{IncludeDir.NFDE}",
        "%{IncludeDir.taskflow}",
        "%{IncludeDir.stb_image}",
        "%{IncludeDir.yaml_cpp}",
        
        "%{IncludeDir.nvrhi}",
        "%{IncludeDir.Vulkan_Headers}",
        "%{IncludeDir.ShaderMake}",
    }
    
    libdirs 
    {
    
    }

    links
    {
        "glfw",
        "nvrhi",
        "NFDE",
        "yaml-cpp",
        "ShaderMakeBlob",
    }

    filter "system:windows"
        systemversion "latest"
        files 
        {  
            "Source/Platform/Windows/**.cpp" 
        }
        
        links
        {
            "DXGI.lib",
            "dxguid.lib",
            "D3D11.lib",
            "D3D12.lib",
        }
        
        defines
        {
            "NVRHI_HAS_D3D11",
            "NVRHI_HAS_D3D12",
            "NVRHI_HAS_VULKAN",
        }
    
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
        files { "%{wks.location}/ThirdParty/tracy/public/TracyClient.cpp" }
        includedirs { "%{IncludeDir.tracy}" }
        defines { "HE_PROFILE", "TRACY_EXPORTS" }
    
    filter "configurations:Dist"
    	defines "HE_DIST"
    	runtime "Release"
    	optimize "Speed"
        symbols "Off"
