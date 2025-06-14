module;

#include "HydraEngine/Base.h"
#include <ShaderMake/ShaderBlob.h>
#include <GLFW/glfw3.h>

module HE;
import nvrhi;
import std;


namespace HE {

    //////////////////////////////////////////////////////////////////
    //// SwapChain
    //////////////////////////////////////////////////////////////////
    
    void SwapChain::BackBufferResizing()
    {
        HE_PROFILE_FUNCTION();

        swapChainFramebuffers.clear();
    }

    void SwapChain::BackBufferResized()
    {
        HE_PROFILE_FUNCTION();

        uint32_t backBufferCount = GetBackBufferCount();
        swapChainFramebuffers.resize(backBufferCount);
        for (uint32_t index = 0; index < backBufferCount; index++)
        {
            nvrhi::ITexture* texture = GetBackBuffer(index);
            nvrhi::FramebufferDesc& desc = nvrhi::FramebufferDesc().addColorAttachment(texture);
            swapChainFramebuffers[index] = nvrhiDevice->createFramebuffer(desc);
        }
    }

    void SwapChain::UpdateSize()
    {
        HE_PROFILE_FUNCTION();

        int height, width;
        glfwGetWindowSize((GLFWwindow*)windowHandle, &width, &height);
        if (width == 0 || height == 0)
            return;

        if (int(desc.backBufferWidth) != width || int(desc.backBufferHeight) != height || (desc.vsync != isVSync && nvrhiDevice->getGraphicsAPI() == nvrhi::GraphicsAPI::VULKAN))
        {
            BackBufferResizing();

            isVSync = desc.vsync;

            ResizeSwapChain(width, height);
            BackBufferResized();
        }
    }

    nvrhi::IFramebuffer* SwapChain::GetCurrentFramebuffer()
    {
        return GetFramebuffer(GetCurrentBackBufferIndex());
    }

    nvrhi::IFramebuffer* SwapChain::GetFramebuffer(uint32_t index)
    {
        if (index < swapChainFramebuffers.size())
            return swapChainFramebuffers[index];

        return nullptr;
    }

    namespace RHI {

        DeviceContext::~DeviceContext()
        {
            HE_PROFILE_FUNCTION();

            for (auto dm : GetAppContext().deviceContext.managers)
            {
                if (dm)
                {
                    dm->GetDevice()->waitForIdle();
                    delete dm;
                }
            }
        }

        DeviceManager* CreateDeviceManager(const DeviceDesc& desc)
        {
            HE_PROFILE_FUNCTION();

            auto& c = GetAppContext();
            auto& managers = c.deviceContext.managers;

            DeviceManager* dm = nullptr;

            for (size_t i = 0; i < desc.api.size(); i++)
            {
                auto api = desc.api[i];

                if (api == nvrhi::GraphicsAPI(-1))
                    continue;

                HE_CORE_INFO("Trying to create backend API: {}", nvrhi::utils::GraphicsAPIToString(api));

                switch (api)
                {
#if NVRHI_HAS_D3D11
                case nvrhi::GraphicsAPI::D3D11:  dm = CreateD3D11(); break;
#endif
#if NVRHI_HAS_D3D12
                case nvrhi::GraphicsAPI::D3D12:  dm = CreateD3D12(); break;
#endif
#if NVRHI_HAS_VULKAN
                case nvrhi::GraphicsAPI::VULKAN: dm = CreateVULKAN(); break;
#endif
                }

                if (dm)
                {
                    if (dm->CreateDevice(desc))
                    {
                        managers.push_back(dm);
                        break;
                    }

                    delete dm;
                }

                HE_CORE_ERROR("Failed to create backend API: {}", nvrhi::utils::GraphicsAPIToString(api));
            }

            return dm;
        }

        DeviceManager* GetDeviceManager(uint32_t index)
        {
            auto& managers = GetAppContext().deviceContext.managers;

            if (index < managers.size() && managers.size() >= 1)
                return managers[index];

            return nullptr;
        }

        nvrhi::DeviceHandle GetDevice(uint32_t index)
        {
            auto& managers = GetAppContext().deviceContext.managers;

            if (index < managers.size() && managers.size() >= 1)
                return managers[index]->GetDevice();

            return {};
        }

        void TryCreateDefaultDevice()
        {
            HE_PROFILE_FUNCTION();

            auto& c = GetAppContext();

            auto deviceDesc = c.applicatoinDesc.deviceDesc;
            size_t apiCount = deviceDesc.api.size();

            if (deviceDesc.api[0] == nvrhi::GraphicsAPI(-1))
            {
#ifdef HE_PLATFORM_WINDOWS
                deviceDesc.api = {
            #if NVRHI_HAS_D3D11
                    nvrhi::GraphicsAPI::D3D11,
            #endif
            #if NVRHI_HAS_D3D12
                    nvrhi::GraphicsAPI::D3D12,
            #endif
            #if NVRHI_HAS_VULKAN
                    nvrhi::GraphicsAPI::VULKAN
            #endif
                };
#else
                deviceDesc.api = { nvrhi::GraphicsAPI::VULKAN };
#endif
                apiCount = deviceDesc.api.size();
            }

            DeviceManager* dm = CreateDeviceManager(deviceDesc);

            if (!dm)
            {
                HE_CORE_CRITICAL("No graphics backend could be initialized!");
                std::exit(1);
            }
        }

        nvrhi::ShaderHandle CreateStaticShader(nvrhi::IDevice* device, StaticShader staticShader, const std::vector<ShaderMacro>* pDefines, const nvrhi::ShaderDesc& desc)
        {
            HE_PROFILE_FUNCTION();

            nvrhi::ShaderHandle shader;

            Buffer buffer;
            switch (device->getGraphicsAPI())
            {
            case nvrhi::GraphicsAPI::D3D11:  buffer = staticShader.dxbc;  break;
            case nvrhi::GraphicsAPI::D3D12:  buffer = staticShader.dxil;  break;
            case nvrhi::GraphicsAPI::VULKAN: buffer = staticShader.spirv; break;
            }

            const void* permutationBytecode = buffer.data;
            size_t permutationSize = buffer.size;

            if (pDefines)
            {
                std::vector<ShaderMake::ShaderConstant> constants;
                constants.reserve(pDefines->size());
                for (const ShaderMacro& define : *pDefines)
                    constants.emplace_back(define.name.data(), define.definition.data());

                if (!ShaderMake::FindPermutationInBlob(buffer.data, buffer.size, constants.data(), uint32_t(constants.size()), &permutationBytecode, &permutationSize))
                {
                    const std::string message = ShaderMake::FormatShaderNotFoundMessage(buffer.data, buffer.size, constants.data(), uint32_t(constants.size()));
                    HE_CORE_ERROR("CreateStaticShader : {}", message.c_str());
                }
            }

            shader = device->createShader(desc, permutationBytecode, permutationSize);

            return shader;
        }

        nvrhi::ShaderLibraryHandle CreateShaderLibrary(nvrhi::IDevice* device, StaticShader staticShader, const std::vector<ShaderMacro>* pDefines)
        {
            HE_PROFILE_FUNCTION();

            nvrhi::ShaderLibraryHandle shader;

            Buffer buffer;
            switch (device->getGraphicsAPI())
            {
            case nvrhi::GraphicsAPI::D3D11:  buffer = staticShader.dxbc;  break;
            case nvrhi::GraphicsAPI::D3D12:  buffer = staticShader.dxil;  break;
            case nvrhi::GraphicsAPI::VULKAN: buffer = staticShader.spirv; break;
            }

            const void* permutationBytecode = buffer.data;
            size_t permutationSize = buffer.size;

            if (pDefines)
            {
                std::vector<ShaderMake::ShaderConstant> constants;
                constants.reserve(pDefines->size());
                for (const ShaderMacro& define : *pDefines)
                    constants.emplace_back(define.name.data(), define.definition.data());

                if (!ShaderMake::FindPermutationInBlob(buffer.data, buffer.size, constants.data(), uint32_t(constants.size()), &permutationBytecode, &permutationSize))
                {
                    const std::string message = ShaderMake::FormatShaderNotFoundMessage(buffer.data, buffer.size, constants.data(), uint32_t(constants.size()));
                    HE_CORE_ERROR("CreateStaticShader : {}", message.c_str());
                }
            }

            shader = device->createShaderLibrary(permutationBytecode, permutationSize);

            return shader;
        }

        bool DeviceManager::CreateInstance(const DeviceInstanceDesc& pDesc)
        {
            HE_PROFILE_FUNCTION();

            if (instanceCreated)
                return true;

            static_cast<DeviceInstanceDesc&>(desc) = pDesc;

            instanceCreated = CreateInstanceInternal();
            return instanceCreated;
        }

        bool DeviceManager::CreateDevice(const DeviceDesc& pDesc)
        {
            HE_PROFILE_FUNCTION();

            desc = pDesc;

            if (!CreateInstance(desc))
                return false;

            if (!CreateDevice())
                return false;

            HE_CORE_INFO("[Backend API] : {}", nvrhi::utils::GraphicsAPIToString(GetDevice()->getGraphicsAPI()));

            return true;
        }

        DefaultMessageCallback& DefaultMessageCallback::GetInstance()
        {
            static DefaultMessageCallback Instance;
            return Instance;
        }

        void DefaultMessageCallback::message(nvrhi::MessageSeverity severity, const char* messageText)
        {
            switch (severity)
            {
            case nvrhi::MessageSeverity::Info:    HE_CORE_INFO("[DeviceManager] : {}", messageText); break;
            case nvrhi::MessageSeverity::Warning: HE_CORE_WARN("[DeviceManager] : {}", messageText); break;
            case nvrhi::MessageSeverity::Error:   HE_CORE_ERROR("[DeviceManager] : {}", messageText); break;
            case nvrhi::MessageSeverity::Fatal:   HE_CORE_CRITICAL("[DeviceManager] : {}", messageText); break;
            }
        }
    }
}
