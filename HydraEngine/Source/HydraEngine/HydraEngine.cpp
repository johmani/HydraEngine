module;

#define NOMINMAX

#include "HydraEngine/Base.h"
#include <simdjson.h>
#include <nfd.hpp>
#include <ShaderMake/ShaderBlob.h>

#ifdef HE_ENABLE_LOGGING
#define	FMT_UNICODE 0
#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h> 
#endif

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

#define MINIZ_NO_DEFLATE_APIS
#define MINIZ_NO_ARCHIVE_WRITING_APIS
#define MINIZ_NO_ZLIB_COMPATIBLE_NAME
#include <miniz.c>

#if defined(HE_PLATFORM_WINDOWS)
#define GLFW_EXPOSE_NATIVE_WIN32
#include <ShellScalingApi.h>
#pragma comment(lib, "shcore.lib")
#endif
#if defined(HE_PLATFORM_LINUX)
#  define GLFW_EXPOSE_NATIVE_X11
#  include <unistd.h>
#endif
#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"

module HE;
import std;
import nvrhi;
import Math;

#if defined(HE_PLATFORM_WINDOWS) && HE_FORCE_DISCRETE_GPU
extern "C"
{
    // Declaring this symbol makes the OS run the app on the discrete GPU on NVIDIA Optimus laptops by default
    __declspec(dllexport) DWORD NvOptimusEnablement = 1;
    // Same as above, for laptops with AMD GPUs
    __declspec(dllexport) DWORD AmdPowerXpressRequestHighPerformance = 1;
}
#endif

namespace HE {

    //////////////////////////////////////////////////////////////////////////
    // Log
    //////////////////////////////////////////////////////////////////////////

#ifdef HE_ENABLE_LOGGING

    static std::shared_ptr<spdlog::logger> s_CoreLogger;
    static std::shared_ptr<spdlog::logger> s_ClientLogger;
    
    void Log::Init(const std::filesystem::path& client)
    {
        std::vector<spdlog::sink_ptr> logSinks;
        logSinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
        logSinks.emplace_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(client.string(), true));
    
        logSinks[0]->set_pattern("%^[%T] %n: %v%$");
        logSinks[1]->set_pattern("[%T] [%l] %n: %v");
    
        s_CoreLogger = std::make_shared<spdlog::logger>("Core", begin(logSinks), end(logSinks));
        spdlog::register_logger(s_CoreLogger);
        s_CoreLogger->set_level(spdlog::level::trace);
        s_CoreLogger->flush_on(spdlog::level::trace);
    
        std::string loggerName = client.stem().string();
    
        s_ClientLogger = std::make_shared<spdlog::logger>(loggerName, begin(logSinks), end(logSinks));
        spdlog::register_logger(s_ClientLogger);
        s_ClientLogger->set_level(spdlog::level::trace);
        s_ClientLogger->flush_on(spdlog::level::trace);
    }
    
    void Log::Shutdown()
    {
        s_ClientLogger.reset();
        s_CoreLogger.reset();
        spdlog::drop_all();
    }
    
    void Log::CoreTrace(const char* s) { s_CoreLogger->trace(s); }
    void Log::CoreInfo(const char* s) { s_CoreLogger->info(s); }
    void Log::CoreWarn(const char* s) { s_CoreLogger->warn(s); }
    void Log::CoreError(const char* s) { s_CoreLogger->error(s); }
    void Log::CoreCritical(const char* s) { s_CoreLogger->critical(s); }
    
    void Log::ClientTrace(const char* s) { s_ClientLogger->trace(s); }
    void Log::ClientInfo(const char* s) { s_ClientLogger->info(s); }
    void Log::ClientWarn(const char* s) { s_ClientLogger->warn(s); }
    void Log::ClientError(const char* s) { s_ClientLogger->error(s); }
    void Log::ClientCritical(const char* s) { s_ClientLogger->critical(s); }

#endif

    //////////////////////////////////////////////////////////////////////////
    // Layer Stack
    //////////////////////////////////////////////////////////////////////////

    LayerStack::~LayerStack()
    {
        for (Layer* layer : m_Layers)
        {
            layer->OnDetach();
            delete layer;
            layer = nullptr;
        }
    }

    void LayerStack::PushLayer(Layer* layer)
    {
        HE_CORE_ASSERT(layer);

        m_Layers.emplace(m_Layers.begin() + m_LayerInsertIndex, layer);
        m_LayerInsertIndex++;
        layer->OnAttach();
    }

    void LayerStack::PushOverlay(Layer* overlay)
    {
        HE_CORE_ASSERT(overlay);

        m_Layers.emplace_back(overlay);
        overlay->OnAttach();
    }

    void LayerStack::PopLayer(Layer* layer)
    {
        HE_CORE_ASSERT(layer);

        auto it = std::find(m_Layers.begin(), m_Layers.begin() + m_LayerInsertIndex, layer);
        if (it != m_Layers.begin() + m_LayerInsertIndex)
        {
            layer->OnDetach();
            m_Layers.erase(it);
            m_LayerInsertIndex--;
        }
    }

    void LayerStack::PopOverlay(Layer* overlay)
    {
        HE_CORE_ASSERT(overlay)

            auto it = std::find(m_Layers.begin() + m_LayerInsertIndex, m_Layers.end(), overlay);
        if (it != m_Layers.end())
        {
            overlay->OnDetach();
            m_Layers.erase(it);
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // Application
    //////////////////////////////////////////////////////////////////////////

    ApplicationContext& GetAppContext() { return *ApplicationContext::s_Instance; }

    namespace Application {

        void Restart() { GetAppContext().running = false; }
        void Shutdown() { GetAppContext().running = false;  GetAppContext().s_ApplicationRunning = false; }
        bool IsApplicationRunning() { return GetAppContext().s_ApplicationRunning; }
        void PushLayer(Layer* overlay) { GetAppContext().layerStack.PushLayer(overlay); }
        void PushOverlay(Layer* layer) { GetAppContext().layerStack.PushOverlay(layer); }
        void PopLayer(Layer* layer) { GetAppContext().layerStack.PopLayer(layer); }
        void PopOverlay(Layer* overlay) { GetAppContext().layerStack.PopOverlay(overlay); }
        const Stats& GetStats() { return GetAppContext().appStats; }
        const ApplicationDesc& GetApplicationDesc() { return GetAppContext().applicatoinDesc; }
        float GetAverageFrameTimeSeconds() { return GetAppContext().averageFrameTime; }
        float GetLastFrameTimestamp() { return GetAppContext().lastFrameTime; }
        void  SetFrameTimeUpdateInterval(float seconds) { GetAppContext().averageTimeUpdateInterval = seconds; }
        Window& GetWindow() { return  GetAppContext().mainWindow; }
    }

    void OnEvent(Event& e)
    {
        HE_PROFILE_FUNCTION();

        auto& c = GetAppContext();

        DispatchEvent<WindowCloseEvent>(e, [](WindowCloseEvent& e) {

            Application::Shutdown();
            return true;
            });

        for (auto it = c.layerStack.rbegin(); it != c.layerStack.rend(); ++it)
        {
            if (e.handled)
                break;
            (*it)->OnEvent(e);
        }
    }

    void ApplicationContext::Run()
    {
        HE_PROFILE_FUNCTION();

        while (running)
        {
            HE_PROFILE_FRAME();
            HE_PROFILE_SCOPE("Core Loop");

            float time = Application::GetTime();
            Timestep timestep = time - lastFrameTime;
            lastFrameTime = time;

            blockingEventsUntilNextFrame = false;

            {
                HE_PROFILE_SCOPE_NC("ExecuteMainThreadQueue", 0xAA0000);

                std::scoped_lock<std::mutex> lock(mainThreadQueueMutex);
                size_t count = std::min(mainThreadMaxJobsPerFrame, (uint32_t)mainThreadQueue.size());
                for (size_t i = 0; i < count; i++)
                {
                    mainThreadQueue.front()();
                    mainThreadQueue.pop();
                }
            }

            bool headlessDevice = applicatoinDesc.deviceDesc.headlessDevice;

            if (!mainWindow.IsMinimized())
            {
                nvrhi::IFramebuffer* framebuffer = nullptr;
                if (!headlessDevice)
                {
                    auto sc = GetAppContext().mainWindow.swapChain;

                    if (sc)
                    {
                        sc->UpdateSize();
                        if (sc->BeginFrame())
                        {
                            framebuffer = sc->GetCurrentFramebuffer();
                        }
                    }
                }

                FrameInfo info = { timestep, framebuffer };

                {
                    HE_PROFILE_SCOPE("LayerStack OnBegin");
                    for (Layer* layer : layerStack)
                        layer->OnBegin(info);
                }

                {
                    HE_PROFILE_SCOPE("LayerStack OnUpdate");
                    for (Layer* layer : layerStack)
                        layer->OnUpdate(info);
                }

                {
                    HE_PROFILE_SCOPE("LayerStack OnEnd");
                    for (Layer* layer : layerStack)
                        layer->OnEnd(info);
                }

                if (!headlessDevice)
                {
                    auto sc = GetAppContext().mainWindow.swapChain;
                    if (sc)
                    {
                        sc->Present();
                    }
                }
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            if (!headlessDevice)
                mainWindow.UpdateEvent();

            // time
            {
                frameTimeSum += timestep;
                numberOfAccumulatedFrames += 1;

                if (frameTimeSum > averageTimeUpdateInterval && numberOfAccumulatedFrames > 0)
                {
                    averageFrameTime = frameTimeSum / numberOfAccumulatedFrames;
                    numberOfAccumulatedFrames = 0;
                    frameTimeSum = 0.0f;
                }

                appStats.CPUMainTime = averageFrameTime * 1e3f;
                appStats.FPS = (averageFrameTime > 0.0f) ? int(1.0f / averageFrameTime) : 0;
            }

            HE_PROFILE_FRAME();
        }
    }

    ApplicationContext::ApplicationContext(const ApplicationDesc& desc)
        : applicatoinDesc(desc)
        , executor(desc.workersNumber)
    {
        HE_PROFILE_FUNCTION();

#ifdef HE_ENABLE_LOGGING
        Log::Init(desc.logFile);
#endif

        HE_CORE_INFO("Creat Application [{}]", applicatoinDesc.windowDesc.title);

        s_Instance = this;

        auto commandLineArgs = applicatoinDesc.commandLineArgs;
        if (commandLineArgs.count > 1)
        {
            HE_INFO("CommandLineArgs : ");
            for (int i = 0; i < commandLineArgs.count; i++)
            {
                HE_INFO("- [{}] : {}", i, commandLineArgs[i]);
            }
        }

        if (!applicatoinDesc.workingDirectory.empty())
            std::filesystem::current_path(applicatoinDesc.workingDirectory);

        if (!applicatoinDesc.deviceDesc.headlessDevice)
        {
            mainWindow.Init(applicatoinDesc.windowDesc);
            mainWindow.eventCallback = OnEvent;
        }

        if (applicatoinDesc.createDefaultDevice)
            RHI::TryCreateDefaultDevice();

        if (!applicatoinDesc.deviceDesc.headlessDevice)
        {
            mainWindow.swapChain = RHI::GetDeviceManager()->CreateSwapChain(mainWindow.desc.swapChainDesc, mainWindow.handle);
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // Jops
    //////////////////////////////////////////////////////////////////////////

    namespace Jops {

        std::future<void> SubmitTask(const std::function<void()>& function) { return GetAppContext().executor.async(function); }
        
        Future RunTaskflow(Taskflow& taskflow) { return GetAppContext().executor.run(taskflow); }
        
        void WaitForAll() { GetAppContext().executor.wait_for_all(); }
       
        void SetMainThreadMaxJobsPerFrame(uint32_t max) { GetAppContext().mainThreadMaxJobsPerFrame = max; }

        void SubmitToMainThread(const std::function<void()>& function)
        {
            auto& c = GetAppContext();
            std::scoped_lock<std::mutex> lock(c.mainThreadQueueMutex);
            c.mainThreadQueue.push(function);
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // SwapChain
    //////////////////////////////////////////////////////////////////////////

    void SwapChain::ResetBackBuffers()
    {
        HE_PROFILE_FUNCTION();

        swapChainFramebuffers.clear();
    }

    void SwapChain::ResizeBackBuffers()
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
            isVSync = desc.vsync;

            ResizeSwapChain(width, height);
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

    //////////////////////////////////////////////////////////////////////////
    // RHI
    //////////////////////////////////////////////////////////////////////////
    
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

    //////////////////////////////////////////////////////////////////////////
    // Modules
    //////////////////////////////////////////////////////////////////////////

    namespace Modules {

        ModulesContext::~ModulesContext()
        {
            HE_PROFILE_FUNCTION();

            struct ModuleToShutdown
            {
                uint32_t LoadOrder;
                ModuleHandle handle;
            };

            if (modules.size() <= 0)
                return;

            std::vector<ModuleToShutdown> ModulesToShutdone;
            ModulesToShutdone.reserve(modules.size());
            for (auto& [handle, moduleData] : modules)
            {
                ModulesToShutdone.emplace_back(moduleData->loadOrder, handle);
            }

            std::sort(ModulesToShutdone.begin(), ModulesToShutdone.end(), [&](const ModuleToShutdown& a, const ModuleToShutdown& b) { return a.LoadOrder < b.LoadOrder; });
            std::reverse(ModulesToShutdone.begin(), ModulesToShutdone.end());

            for (auto& ModuleToShutdone : ModulesToShutdone)
            {
                UnloadModule(ModuleToShutdone.handle);
            }
        }

        bool LoadModule(const std::filesystem::path& filePath)
        {
            HE_PROFILE_FUNCTION();

            if (!std::filesystem::exists(filePath))
            {
                HE_CORE_ERROR("LoadModule failed: File {} does not exist.", filePath.string());
                return false;
            }

            auto& c = GetAppContext().modulesContext;

            Ref<ModuleData> newModule = CreateRef<ModuleData>(filePath);
            ModuleHandle handle = Hash(filePath);
            if (c.modules.contains(handle))
            {
                HE_CORE_WARN("Module {} has already been loaded.", newModule->name);
                return false;
            }

            if (newModule->lib.IsLoaded())
            {
                auto func = newModule->lib.GetFunction<void()>("OnModuleLoaded");
                if (func)
                {
                    func();
                    c.modules[handle] = newModule;
                    return true;
                }
            }

            HE_CORE_ERROR("LoadModule failed: OnModuleLoaded function not found in module {}.", newModule->name);
            return false;
        }

        bool IsModuleLoaded(ModuleHandle handle)
        {
            HE_PROFILE_FUNCTION();

            auto& c = GetAppContext().modulesContext;

            if (c.modules.contains(handle))
                return true;

            return false;
        }

        bool UnloadModule(ModuleHandle handle)
        {
            HE_PROFILE_FUNCTION();

            auto& c = GetAppContext().modulesContext;

            auto it = c.modules.find(handle);
            if (it == c.modules.end())
            {
                HE_CORE_ERROR("UnloadModule failed: Module with handle {} not found.", handle);
                return false;
            }

            Ref<ModuleData> moduleData = it->second;

            if (auto func = moduleData->lib.GetFunction<void()>("OnModuleShutdown"))
            {
                func();
            }
            else
            {
                HE_CORE_WARN("UnloadModule failed: Module {} does not define an OnModuleShutdown function.", moduleData->name);
            }

            c.modules.erase(it);

            return true;
        }

        Ref<ModuleData> GetModuleData(ModuleHandle handle)
        {
            HE_PROFILE_FUNCTION();

            auto& c = GetAppContext().modulesContext;

            auto it = c.modules.find(handle);
            if (it != c.modules.end())
                return it->second;

            HE_CORE_ERROR("Module with handle {} not found.", handle);
            return nullptr;
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // Plugins
    //////////////////////////////////////////////////////////////////////////

    namespace Plugins {

        bool DeserializePluginDesc(const std::filesystem::path& filePath, PluginDesc& desc)
        {
            HE_PROFILE_FUNCTION();

            static simdjson::dom::parser parser;

            simdjson::dom::element pluginDescriptor;
            auto error = parser.load(filePath.string()).get(pluginDescriptor);
            if (error)
            {
                HE_CORE_ERROR("Failed to load .hplugin file {}\n    {}", filePath.string(), simdjson::error_message(error));
                return false;
            }

            std::string_view name;
            desc.name = (pluginDescriptor["name"].get(name) != simdjson::SUCCESS) ? "" : name;

            std::string_view description;
            desc.description = (pluginDescriptor["description"].get(description) != simdjson::SUCCESS) ? "" : description;

            std::string_view URL;
            desc.URL = (pluginDescriptor["URL"].get(URL) != simdjson::SUCCESS) ? "" : URL;

            pluginDescriptor["reloadable"].get(desc.reloadable);
            pluginDescriptor["enabledByDefault"].get(desc.enabledByDefault);

            simdjson::dom::array modulesArray;
            pluginDescriptor["modules"].get(modulesArray);
            desc.modules.reserve(modulesArray.size());
            for (simdjson::dom::element module : modulesArray)
            {
                std::string_view moduleName;
                module.get(moduleName);
                desc.modules.emplace_back(moduleName);
            }

            simdjson::dom::array pluginsArray;
            pluginDescriptor["plugins"].get(pluginsArray);
            desc.plugins.reserve(pluginsArray.size());
            for (simdjson::dom::element plugin : pluginsArray)
            {
                std::string_view pluginName;
                plugin.get(pluginName);
                desc.plugins.emplace_back(pluginName);
            }

            return true;
        }

        Ref<Plugin> GetOrCreatePluginObject(const std::filesystem::path& descFilePath)
        {
            HE_PROFILE_FUNCTION();

            auto& ctx = GetAppContext().pluginContext;

            PluginDesc desc;
            DeserializePluginDesc(descFilePath, desc);
            PluginHandle handle = Hash(desc.name);

            if (ctx.plugins.contains(handle))
                return ctx.plugins.at(handle);

            Ref<Plugin> plugin = CreateRef<Plugin>(desc);
            plugin->descFilePath = descFilePath;
            ctx.plugins[handle] = plugin;

            return plugin;
        }

        void LoadPlugin(const std::filesystem::path& descriptor)
        {
            HE_PROFILE_FUNCTION();

            auto lexicallyNormal = descriptor.lexically_normal();
            if (!std::filesystem::exists(lexicallyNormal))
            {
                HE_CORE_ERROR("LoadPlugin failed: file {} does not exist.", lexicallyNormal.string());
                return;
            }

            Ref<Plugin> plugin = GetOrCreatePluginObject(lexicallyNormal);
            auto handle = Hash(plugin->desc.name);
            LoadPlugin(handle);
        }

        void LoadPlugin(PluginHandle handle)
        {
            HE_PROFILE_FUNCTION();

            auto& ctx = GetAppContext().pluginContext;

            auto it = ctx.plugins.find(handle);
            if (it == ctx.plugins.end()) return;

            Ref<Plugin> plugin = it->second;
            const auto& dependences = plugin->desc.plugins;

            if (dependences.size() > 0)
            {
                auto pluginsDir = plugin->descFilePath.parent_path().parent_path();

                for (const auto& dependencyPluginName : dependences)
                {
                    auto pluginsDescFilePath = pluginsDir / dependencyPluginName / (dependencyPluginName + c_PluginDescriptorExtension);
                    if (std::filesystem::exists(pluginsDescFilePath))
                        GetOrCreatePluginObject(pluginsDescFilePath);
                }
            }

            for (const auto& dependencyPluginName : dependences)
            {
                PluginHandle dependencyPluginHandle = Hash(dependencyPluginName);

                if (ctx.plugins.contains(dependencyPluginHandle))
                {
                    auto dependencyPlugin = ctx.plugins.at(dependencyPluginHandle);
                    if (!dependencyPlugin->enabled)
                    {
                        LoadPlugin(dependencyPluginHandle);
                    }
                }
            }

            plugin->enabled = true;

            HE_CORE_INFO("Plugins::LoadPlugin {}", plugin->desc.name);

            // Load Modules
            for (const auto& moduleName : plugin->desc.modules)
            {
                auto modulePath = plugin->BinariesDirectory() / std::format("{}-{}", c_System, c_Architecture) / c_BuildConfig / (moduleName + c_SharedLibExtension);
                Modules::LoadModule(modulePath);
            }
        }

        bool UnloadPlugin(PluginHandle handle)
        {
            HE_PROFILE_FUNCTION();

            auto& ctx = GetAppContext().pluginContext;

            if (ctx.plugins.contains(handle))
            {
                const Ref<Plugin>& plugin = ctx.plugins.at(handle);
                if (plugin->enabled)
                {
                    const auto& modulesNames = plugin->desc.modules;

                    // Load Modules
                    bool res = true;
                    for (const auto& name : modulesNames)
                    {
                        auto modulePath = plugin->BinariesDirectory() / std::format("{}-{}", c_System, c_Architecture) / c_BuildConfig / (name + c_SharedLibExtension);
                        auto moduleHandle = Hash(modulePath);
                        res = Modules::UnloadModule(moduleHandle);
                        if (!res) break;
                    }
                    if (res)
                    {
                        plugin->enabled = false;
                    }
                    return true;
                }
                else
                {
                    return true;
                }
            }

            HE_CORE_ERROR("UnloadPlugin : failed to Unload Plugin {}", handle);

            return false;
        }

        void ReloadPlugin(PluginHandle handle)
        {
            HE_PROFILE_FUNCTION();

            auto& ctx = GetAppContext().pluginContext;

            std::filesystem::path pluginDescFilePath;

            if (ctx.plugins.contains(handle))
                pluginDescFilePath = ctx.plugins.at(handle)->descFilePath;

            UnloadPlugin(handle);
            ctx.plugins.erase(handle);
            LoadPlugin(pluginDescFilePath);
        }

        const Ref<Plugin> GetPlugin(PluginHandle handle)
        {
            auto& ctx = GetAppContext().pluginContext;
            if (ctx.plugins.contains(handle))
                return ctx.plugins.at(handle);

            return nullptr;
        }

        void LoadPluginsInDirectory(const std::filesystem::path& directory)
        {
            HE_PROFILE_FUNCTION();

            auto& ctx = GetAppContext().pluginContext;

            if (!std::filesystem::exists(directory))
            {
                HE_CORE_ERROR("LoadPluginsInDirectory failed: directory {} does not exist.", directory.string());
                return;
            }

            PluginHandle discoveredPLugins[4096];
            uint32_t count = 0;

            {
                HE_PROFILE_SCOPE("Find Plugins");

                for (const auto& entry : std::filesystem::directory_iterator(directory))
                {
                    auto pluginsDescFilePath = entry.path() / (entry.path().stem().string() + c_PluginDescriptorExtension);

                    if (std::filesystem::exists(pluginsDescFilePath))
                    {
                        Ref<Plugin> plugin = GetOrCreatePluginObject(pluginsDescFilePath);
                        discoveredPLugins[count] = Hash(plugin->desc.name);
                        count++;
                    }
                }
            }

            {
                HE_PROFILE_SCOPE("Load Plugins");

                for (uint32_t i = 0; i < count; i++)
                {
                    auto handle = discoveredPLugins[i];
                    if (ctx.plugins.at(handle)->desc.enabledByDefault)
                        LoadPlugin(handle);
                }
            }
        }
    }
       
    //////////////////////////////////////////////////////////////////////////
    // Window
    //////////////////////////////////////////////////////////////////////////

    static int ToGLFWKeyCode(KeyCode keyCode)
    {
        switch (keyCode)
        {
        case Key::Space:        return 32;
        case Key::Apostrophe:   return 39;
        case Key::Comma:        return 44;
        case Key::Minus:        return 45;
        case Key::Period:       return 46;
        case Key::Slash:        return 47;
        case Key::D0:           return 48;
        case Key::D1:           return 49;
        case Key::D2:           return 50;
        case Key::D3:           return 51;
        case Key::D4:           return 52;
        case Key::D5:           return 53;
        case Key::D6:           return 54;
        case Key::D7:           return 55;
        case Key::D8:           return 56;
        case Key::D9:           return 57;
        case Key::Semicolon:    return 59;
        case Key::Equal:        return 61;
        case Key::A:            return 65;
        case Key::B:            return 66;
        case Key::C:            return 67;
        case Key::D:            return 68;
        case Key::E:            return 69;
        case Key::F:            return 70;
        case Key::G:            return 71;
        case Key::H:            return 72;
        case Key::I:            return 73;
        case Key::J:            return 74;
        case Key::K:            return 75;
        case Key::L:            return 76;
        case Key::M:            return 77;
        case Key::N:            return 78;
        case Key::O:            return 79;
        case Key::P:            return 80;
        case Key::Q:            return 81;
        case Key::R:            return 82;
        case Key::S:            return 83;
        case Key::T:            return 84;
        case Key::U:            return 85;
        case Key::V:            return 86;
        case Key::W:            return 87;
        case Key::X:            return 88;
        case Key::Y:            return 89;
        case Key::Z:            return 90;
        case Key::LeftBracket:  return 91;
        case Key::Backslash:    return 92;
        case Key::RightBracket: return 93;
        case Key::GraveAccent:  return 96;
        case Key::World1:       return 161;
        case Key::World2:       return 162;
        case Key::Escape:       return 256;
        case Key::Enter:        return 257;
        case Key::Tab:          return 258;
        case Key::Backspace:    return 259;
        case Key::Insert:       return 260;
        case Key::Delete:       return 261;
        case Key::Right:        return 262;
        case Key::Left:         return 263;
        case Key::Down:         return 264;
        case Key::Up:           return 265;
        case Key::PageUp:       return 266;
        case Key::PageDown:     return 267;
        case Key::Home:         return 268;
        case Key::End:          return 269;
        case Key::CapsLock:     return 280;
        case Key::ScrollLock:   return 281;
        case Key::NumLock:      return 282;
        case Key::PrintScreen:  return 283;
        case Key::Pause:        return 284;
        case Key::F1:           return 290;
        case Key::F2:           return 291;
        case Key::F3:           return 292;
        case Key::F4:           return 293;
        case Key::F5:           return 294;
        case Key::F6:           return 295;
        case Key::F7:           return 296;
        case Key::F8:           return 297;
        case Key::F9:           return 298;
        case Key::F10:          return 299;
        case Key::F11:          return 300;
        case Key::F12:          return 301;
        case Key::F13:          return 302;
        case Key::F14:          return 303;
        case Key::F15:          return 304;
        case Key::F16:          return 305;
        case Key::F17:          return 306;
        case Key::F18:          return 307;
        case Key::F19:          return 308;
        case Key::F20:          return 309;
        case Key::F21:          return 310;
        case Key::F22:          return 311;
        case Key::F23:          return 312;
        case Key::F24:          return 313;
        case Key::F25:          return 314;
        case Key::KP0:          return 320;
        case Key::KP1:          return 321;
        case Key::KP2:          return 322;
        case Key::KP3:          return 323;
        case Key::KP4:          return 324;
        case Key::KP5:          return 325;
        case Key::KP6:          return 326;
        case Key::KP7:          return 327;
        case Key::KP8:          return 328;
        case Key::KP9:          return 329;
        case Key::KPDecimal:    return 330;
        case Key::KPDivide:     return 331;
        case Key::KPMultiply:   return 332;
        case Key::KPSubtract:   return 333;
        case Key::KPAdd:        return 334;
        case Key::KPEnter:      return 335;
        case Key::KPEqual:      return 336;
        case Key::LeftShift:    return 340;
        case Key::LeftControl:  return 341;
        case Key::LeftAlt:      return 342;
        case Key::LeftSuper:    return 343;
        case Key::RightShift:   return 344;
        case Key::RightControl: return 345;
        case Key::RightAlt:     return 346;
        case Key::RightSuper:   return 347;
        case Key::Menu:         return 348;
        }

        HE_ASSERT("Unknown Key", false);
        return -1;
    }

    static KeyCode ToHEKeyCode(int keyCode)
    {
        switch (keyCode)
        {
        case 32:  return Key::Space;
        case 39:  return Key::Apostrophe;
        case 44:  return Key::Comma;
        case 45:  return Key::Minus;
        case 46:  return Key::Period;
        case 47:  return Key::Slash;
        case 48:  return Key::D0;
        case 49:  return Key::D1;
        case 50:  return Key::D2;
        case 51:  return Key::D3;
        case 52:  return Key::D4;
        case 53:  return Key::D5;
        case 54:  return Key::D6;
        case 55:  return Key::D7;
        case 56:  return Key::D8;
        case 57:  return Key::D9;
        case 59:  return Key::Semicolon;
        case 61:  return Key::Equal;
        case 65:  return Key::A;
        case 66:  return Key::B;
        case 67:  return Key::C;
        case 68:  return Key::D;
        case 69:  return Key::E;
        case 70:  return Key::F;
        case 71:  return Key::G;
        case 72:  return Key::H;
        case 73:  return Key::I;
        case 74:  return Key::J;
        case 75:  return Key::K;
        case 76:  return Key::L;
        case 77:  return Key::M;
        case 78:  return Key::N;
        case 79:  return Key::O;
        case 80:  return Key::P;
        case 81:  return Key::Q;
        case 82:  return Key::R;
        case 83:  return Key::S;
        case 84:  return Key::T;
        case 85:  return Key::U;
        case 86:  return Key::V;
        case 87:  return Key::W;
        case 88:  return Key::X;
        case 89:  return Key::Y;
        case 90:  return Key::Z;
        case 91:  return Key::LeftBracket;
        case 92:  return Key::Backslash;
        case 93:  return Key::RightBracket;
        case 96:  return Key::GraveAccent;
        case 16:  return Key::World1;
        case 162: return Key::World2;
        case 256: return Key::Escape;
        case 257: return Key::Enter;
        case 258: return Key::Tab;
        case 259: return Key::Backspace;
        case 260: return Key::Insert;
        case 261: return Key::Delete;
        case 262: return Key::Right;
        case 263: return Key::Left;
        case 264: return Key::Down;
        case 265: return Key::Up;
        case 266: return Key::PageUp;
        case 267: return Key::PageDown;
        case 268: return Key::Home;
        case 269: return Key::End;
        case 280: return Key::CapsLock;
        case 281: return Key::ScrollLock;
        case 282: return Key::NumLock;
        case 283: return Key::PrintScreen;
        case 284: return Key::Pause;
        case 290: return Key::F1;
        case 291: return Key::F2;
        case 292: return Key::F3;
        case 293: return Key::F4;
        case 294: return Key::F5;
        case 295: return Key::F6;
        case 296: return Key::F7;
        case 297: return Key::F8;
        case 298: return Key::F9;
        case 299: return Key::F10;
        case 300: return Key::F11;
        case 301: return Key::F12;
        case 302: return Key::F13;
        case 303: return Key::F14;
        case 304: return Key::F15;
        case 305: return Key::F16;
        case 306: return Key::F17;
        case 307: return Key::F18;
        case 308: return Key::F19;
        case 309: return Key::F20;
        case 310: return Key::F21;
        case 311: return Key::F22;
        case 312: return Key::F23;
        case 313: return Key::F24;
        case 314: return Key::F25;
        case 320: return Key::KP0;
        case 321: return Key::KP1;
        case 322: return Key::KP2;
        case 323: return Key::KP3;
        case 324: return Key::KP4;
        case 325: return Key::KP5;
        case 326: return Key::KP6;
        case 327: return Key::KP7;
        case 328: return Key::KP8;
        case 329: return Key::KP9;
        case 330: return Key::KPDecimal;
        case 331: return Key::KPDivide;
        case 332: return Key::KPMultiply;
        case 333: return Key::KPSubtract;
        case 334: return Key::KPAdd;
        case 335: return Key::KPEnter;
        case 336: return Key::KPEqual;
        case 340: return Key::LeftShift;
        case 341: return Key::LeftControl;
        case 342: return Key::LeftAlt;
        case 343: return Key::LeftSuper;
        case 344: return Key::RightShift;
        case 345: return Key::RightControl;
        case 346: return Key::RightAlt;
        case 347: return Key::RightSuper;
        case 348: return Key::Menu;
        }

        HE_ASSERT("Unknown Key", false);
        return -1;
    }

    static int ToGLFWCursorMode(Cursor::Mode mode)
    {
        switch (mode)
        {
        case Cursor::Mode::Normal:   return GLFW_CURSOR_NORMAL;
        case Cursor::Mode::Hidden:   return GLFW_CURSOR_HIDDEN;
        case Cursor::Mode::Disabled: return GLFW_CURSOR_DISABLED;
        }

        return GLFW_CURSOR_NORMAL;
    }

    float Application::GetTime() { return static_cast<float>(glfwGetTime()); }

    static uint8_t s_GLFWWindowCount = 0;

    static const struct
    {
        nvrhi::Format format;
        uint32_t redBits;
        uint32_t greenBits;
        uint32_t blueBits;
        uint32_t alphaBits;
        uint32_t depthBits;
        uint32_t stencilBits;
    } formatInfo[] = {
        { nvrhi::Format::UNKNOWN,            0,  0,  0,  0,  0,  0, },
        { nvrhi::Format::R8_UINT,            8,  0,  0,  0,  0,  0, },
        { nvrhi::Format::RG8_UINT,           8,  8,  0,  0,  0,  0, },
        { nvrhi::Format::RG8_UNORM,          8,  8,  0,  0,  0,  0, },
        { nvrhi::Format::R16_UINT,          16,  0,  0,  0,  0,  0, },
        { nvrhi::Format::R16_UNORM,         16,  0,  0,  0,  0,  0, },
        { nvrhi::Format::R16_FLOAT,         16,  0,  0,  0,  0,  0, },
        { nvrhi::Format::RGBA8_UNORM,        8,  8,  8,  8,  0,  0, },
        { nvrhi::Format::RGBA8_SNORM,        8,  8,  8,  8,  0,  0, },
        { nvrhi::Format::BGRA8_UNORM,        8,  8,  8,  8,  0,  0, },
        { nvrhi::Format::SRGBA8_UNORM,       8,  8,  8,  8,  0,  0, },
        { nvrhi::Format::SBGRA8_UNORM,       8,  8,  8,  8,  0,  0, },
        { nvrhi::Format::R10G10B10A2_UNORM, 10, 10, 10,  2,  0,  0, },
        { nvrhi::Format::R11G11B10_FLOAT,   11, 11, 10,  0,  0,  0, },
        { nvrhi::Format::RG16_UINT,         16, 16,  0,  0,  0,  0, },
        { nvrhi::Format::RG16_FLOAT,        16, 16,  0,  0,  0,  0, },
        { nvrhi::Format::R32_UINT,          32,  0,  0,  0,  0,  0, },
        { nvrhi::Format::R32_FLOAT,         32,  0,  0,  0,  0,  0, },
        { nvrhi::Format::RGBA16_FLOAT,      16, 16, 16, 16,  0,  0, },
        { nvrhi::Format::RGBA16_UNORM,      16, 16, 16, 16,  0,  0, },
        { nvrhi::Format::RGBA16_SNORM,      16, 16, 16, 16,  0,  0, },
        { nvrhi::Format::RG32_UINT,         32, 32,  0,  0,  0,  0, },
        { nvrhi::Format::RG32_FLOAT,        32, 32,  0,  0,  0,  0, },
        { nvrhi::Format::RGB32_UINT,        32, 32, 32,  0,  0,  0, },
        { nvrhi::Format::RGB32_FLOAT,       32, 32, 32,  0,  0,  0, },
        { nvrhi::Format::RGBA32_UINT,       32, 32, 32, 32,  0,  0, },
        { nvrhi::Format::RGBA32_FLOAT,      32, 32, 32, 32,  0,  0, },
    };

    static void GLFWErrorCallback(int error, const char* description)
    {
        HE_CORE_ERROR("[GLFW] : ({}): {}", error, description);
    }

    void Window::Init(const WindowDesc& windowDesc)
    {
        HE_PROFILE_FUNCTION();

        desc = windowDesc;

#ifdef HE_PLATFORM_WINDOWS
        if (!desc.perMonitorDPIAware)
            SetProcessDpiAwareness(PROCESS_DPI_UNAWARE);
#endif
        // Init Hints
        {
            glfwInitHint(GLFW_WIN32_MESSAGES_IN_FIBER, GLFW_TRUE);
        }

        if (s_GLFWWindowCount == 0)
        {
            HE_PROFILE_SCOPE("glfwInit");
            int success = glfwInit();
            HE_CORE_ASSERT(success, "Could not initialize GLFW!");
            glfwSetErrorCallback(GLFWErrorCallback);
        }

        // Window Hints
        {
            bool foundFormat = false;
            for (const auto& info : formatInfo)
            {
                if (info.format == windowDesc.swapChainDesc.swapChainFormat)
                {
                    glfwWindowHint(GLFW_RED_BITS, info.redBits);
                    glfwWindowHint(GLFW_GREEN_BITS, info.greenBits);
                    glfwWindowHint(GLFW_BLUE_BITS, info.blueBits);
                    glfwWindowHint(GLFW_ALPHA_BITS, info.alphaBits);
                    glfwWindowHint(GLFW_DEPTH_BITS, info.depthBits);
                    glfwWindowHint(GLFW_STENCIL_BITS, info.stencilBits);
                    foundFormat = true;
                    break;
                }
            }

            HE_CORE_VERIFY(foundFormat);

            glfwWindowHint(GLFW_SAMPLES, windowDesc.swapChainDesc.swapChainSampleCount);
            glfwWindowHint(GLFW_REFRESH_RATE, windowDesc.swapChainDesc.refreshRate);
            glfwWindowHint(GLFW_SCALE_TO_MONITOR, desc.scaleToMonitor);
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindowHint(GLFW_MAXIMIZED, windowDesc.maximized && !windowDesc.fullScreen);
            glfwWindowHint(GLFW_TITLEBAR, !windowDesc.customTitlebar);
            glfwWindowHint(GLFW_DECORATED, windowDesc.decorated);
            glfwWindowHint(GLFW_VISIBLE, desc.startVisible);
        }

        GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* videoMode = glfwGetVideoMode(primaryMonitor);

        float monitorScaleX, monitorScaleY;
        glfwGetMonitorContentScale(primaryMonitor, &monitorScaleX, &monitorScaleY);

        if (desc.width == 0 || desc.height == 0)
        {
            desc.width = int(videoMode->width * desc.sizeRatio / monitorScaleX);
            desc.height = int(videoMode->height * desc.sizeRatio / monitorScaleY);
        }

        int scaledWidth = int(desc.width * monitorScaleX);
        int scaledHeight = int(desc.height * monitorScaleY);

        if (windowDesc.fullScreen)
        {
            desc.width = videoMode->width;
            desc.height = videoMode->height;
        }

        {
            HE_PROFILE_SCOPE("glfwCreateWindow");

            handle = glfwCreateWindow((int)desc.width, (int)desc.height, desc.title.data(), nullptr, nullptr);
            ++s_GLFWWindowCount;

            // applying fullscreen mode by passing the primaryMonitor to glfwCreateWindow causes some weird behavior, but this works just fine for now.
            if (windowDesc.fullScreen)
            {
                glfwSetWindowMonitor((GLFWwindow*)handle, primaryMonitor, 0, 0, videoMode->width, videoMode->height, videoMode->refreshRate);
            }
        }

        GLFWwindow* glfwWindow = (GLFWwindow*)handle;

        glfwSetWindowSizeLimits(glfwWindow, desc.minWidth, desc.minHeight, desc.maxWidth, desc.maxHeight);

        prevPosX = 0, prevPosY = 0, prevWidth = 0, prevHeight = 0;
        glfwGetWindowSize(glfwWindow, &prevWidth, &prevHeight);
        glfwGetWindowPos(glfwWindow, &prevPosX, &prevPosY);

        if (!windowDesc.maximized && !windowDesc.fullScreen && windowDesc.centered)
        {
            int monitorX, monitorY;
            glfwGetMonitorPos(primaryMonitor, &monitorX, &monitorY);

            glfwSetWindowPos(glfwWindow,
                monitorX + (videoMode->width - scaledWidth) / 2,
                monitorY + (videoMode->height - scaledHeight) / 2
            );
        }

        glfwSetWindowAttrib(glfwWindow, GLFW_RESIZABLE, windowDesc.resizeable);

        if (std::filesystem::exists(windowDesc.iconFilePath))
        {
            HE_PROFILE_SCOPE("Set Window Icon");

            Image image(windowDesc.iconFilePath);
            GLFWimage icon;
            icon.pixels = image.GetData();
            icon.width = image.GetWidth();
            icon.height = image.GetHeight();
            glfwSetWindowIcon(glfwWindow, 1, &icon);
        }

        {
            int w, h;
            glfwGetWindowSize(glfwWindow, &w, &h);
            desc.width = w;
            desc.height = h;
            desc.swapChainDesc.backBufferWidth = w;
            desc.swapChainDesc.backBufferHeight = h;
        }

        if (!desc.setCallbacks)
            return;

        glfwSetWindowUserPointer(glfwWindow, this);

        glfwSetTitlebarHitTestCallback(glfwWindow, [](GLFWwindow* window, int x, int y, int* hit) {

            HE_PROFILE_SCOPE("glfwSetTitlebarHitTestCallback");

            Window* app = (Window*)glfwGetWindowUserPointer(window);
            *hit = app->isTitleBarHit;
            });

        glfwSetWindowSizeCallback(glfwWindow, [](GLFWwindow* window, int width, int height) {

            HE_PROFILE_SCOPE("glfwSetWindowSizeCallback");

            Window& w = *(Window*)glfwGetWindowUserPointer(window);
            w.desc.width = width;
            w.desc.height = height;

            WindowResizeEvent event((uint32_t)width, (uint32_t)height);
            w.eventCallback(event);
            });

        glfwSetWindowCloseCallback(glfwWindow, [](GLFWwindow* window) {

            HE_PROFILE_SCOPE("glfwSetWindowCloseCallback");

            Window& w = *(Window*)glfwGetWindowUserPointer(window);
            WindowCloseEvent event;
            w.eventCallback(event);
            });

        glfwSetWindowContentScaleCallback(glfwWindow, [](GLFWwindow* window, float xscale, float yscale) {

            HE_PROFILE_SCOPE("glfwSetWindowContentScaleCallback");

            Window& w = *(Window*)glfwGetWindowUserPointer(window);

            WindowContentScaleEvent event(xscale, yscale);
            w.eventCallback(event);
            });

        glfwSetWindowMaximizeCallback(glfwWindow, [](GLFWwindow* window, int maximized) {

            HE_PROFILE_SCOPE("glfwSetWindowMaximizeCallback");

            Window& w = *(Window*)glfwGetWindowUserPointer(window);
            static bool isfirstTime = true;
            GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
            const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);

            if (maximized)
            {
                w.desc.maximized = true;
            }
            else
            {
                w.desc.maximized = false;
                if (isfirstTime)
                {
                    float sx, sy;
                    glfwGetMonitorContentScale(primaryMonitor, &sx, &sy);

                    float delta = 100 * sx;
                    glfwSetWindowMonitor(
                        window,
                        nullptr,
                        int(w.prevPosX + delta * 0.5f),
                        int(w.prevPosY + delta * 0.5f),
                        int(w.prevWidth - delta),
                        int(w.prevHeight - delta),
                        0
                    );
                }
            }

            isfirstTime = false;

            WindowMaximizeEvent event(maximized);
            w.eventCallback(event);
            });

        glfwSetKeyCallback(glfwWindow, [](GLFWwindow* window, int key, int scancode, int action, int mods) {

            HE_PROFILE_SCOPE("glfwSetKeyCallback");

            Window& w = *(Window*)glfwGetWindowUserPointer(window);

            switch (action)
            {
            case GLFW_PRESS:
            {
                KeyPressedEvent event(ToHEKeyCode(key), false);
                w.eventCallback(event);
                break;
            }
            case GLFW_RELEASE:
            {
                KeyReleasedEvent event(ToHEKeyCode(key));
                w.eventCallback(event);
                break;
            }
            case GLFW_REPEAT:
            {
                KeyPressedEvent event(ToHEKeyCode(key), true);
                w.eventCallback(event);
                break;
            }
            }
            });

        glfwSetCharCallback(glfwWindow, [](GLFWwindow* window, unsigned int codePoint) {

            HE_PROFILE_SCOPE("glfwSetCharCallback");

            Window& w = *(Window*)glfwGetWindowUserPointer(window);

            KeyTypedEvent event(codePoint);
            w.eventCallback(event);
            });

        glfwSetMouseButtonCallback(glfwWindow, [](GLFWwindow* window, int button, int action, int mods) {

            HE_PROFILE_SCOPE("glfwSetMouseButtonCallback");

            Window& w = *(Window*)glfwGetWindowUserPointer(window);

            switch (action)
            {
            case GLFW_PRESS:
            {
                MouseButtonPressedEvent event(button);
                w.eventCallback(event);
                break;
            }
            case GLFW_RELEASE:
            {
                MouseButtonReleasedEvent event(button);
                w.eventCallback(event);
                break;
            }
            }
            });

        glfwSetScrollCallback(glfwWindow, [](GLFWwindow* window, double xOffset, double yOffset) {

            HE_PROFILE_SCOPE("glfwSetScrollCallback");

            Window& w = *(Window*)glfwGetWindowUserPointer(window);

            MouseScrolledEvent event((float)xOffset, (float)yOffset);
            w.eventCallback(event);
            });

        glfwSetCursorPosCallback(glfwWindow, [](GLFWwindow* window, double xPos, double yPos) {

            HE_PROFILE_SCOPE("glfwSetCursorPosCallback");

            Window& w = *(Window*)glfwGetWindowUserPointer(window);

            MouseMovedEvent event((float)xPos, (float)yPos);
            w.eventCallback(event);
            });

        glfwSetCursorEnterCallback(glfwWindow, [](GLFWwindow* window, int entered) {

            HE_PROFILE_SCOPE("glfwSetCursorEnterCallback");

            Window& w = *(Window*)glfwGetWindowUserPointer(window);

            MouseEnterEvent event((bool)entered);
            w.eventCallback(event);
            });

        glfwSetDropCallback(glfwWindow, [](GLFWwindow* window, int pathCount, const char* paths[]) {

            HE_PROFILE_SCOPE("glfwSetDropCallback");

            Window& w = *(Window*)glfwGetWindowUserPointer(window);

            WindowDropEvent event(paths, pathCount);
            w.eventCallback(event);
            });

        glfwSetJoystickCallback([](int jid, int event) {

            HE_PROFILE_SCOPE("glfwSetJoystickCallback");

            auto& w = GetAppContext().mainWindow;

            if (event == GLFW_CONNECTED)
            {
                GamepadConnectedEvent event(jid, true);
                w.eventCallback(event);
            }
            else if (event == GLFW_DISCONNECTED)
            {
                GamepadConnectedEvent event(jid, false);
                w.eventCallback(event);
            }
            });

        glfwSetCharModsCallback(glfwWindow, [](GLFWwindow* window, unsigned int codepoint, int mods) {

            HE_PROFILE_SCOPE("glfwSetCharModsCallback");


            });

        glfwSetWindowIconifyCallback(glfwWindow, [](GLFWwindow* window, int iconified) {

            HE_PROFILE_SCOPE("glfwSetWindowIconifyCallback");

            Window& w = *(Window*)glfwGetWindowUserPointer(window);
            WindowMinimizeEvent event(iconified);
            w.eventCallback(event);
            });

        glfwSetWindowPosCallback(glfwWindow, [](GLFWwindow* window, int xpos, int ypos) {

            HE_PROFILE_SCOPE("glfwSetWindowPosCallback");

            });

        glfwSetWindowRefreshCallback(glfwWindow, [](GLFWwindow* window) {

            HE_PROFILE_SCOPE("glfwSetWindowRefreshCallback");

            });

        glfwSetWindowFocusCallback(glfwWindow, [](GLFWwindow* window, int focused) {

            HE_PROFILE_SCOPE("glfwSetWindowFocusCallback");

            });
    }

    Window::~Window()
    {
        HE_PROFILE_FUNCTION();

        if (swapChain)
            delete swapChain;

        if (handle)
        {
            glfwDestroyWindow((GLFWwindow*)handle);
            --s_GLFWWindowCount;

            if (s_GLFWWindowCount == 0)
                glfwTerminate();
        }
    }

    void Window::SetTitle(const std::string_view& title)
    {
        if (desc.title == title)
            return;

        glfwSetWindowTitle((GLFWwindow*)handle, title.data());
        desc.title = title;
    }

    void* Window::GetNativeHandle()
    {
#ifdef HE_PLATFORM_WINDOWS
        return (void*)glfwGetWin32Window((GLFWwindow*)handle);
#elif defined(HE_PLATFORM_LINUX)
        return (void*)glfwGetX11Window((GLFWwindow*)handle); // not yet tested
#else
        HE_CORE_VERIFY(false, "unsupported platform");
        return nullptr;
#endif
    }

    void Window::Maximize() { glfwMaximizeWindow((GLFWwindow*)handle); }

    void Window::Minimize() { glfwIconifyWindow((GLFWwindow*)handle); }

    void Window::Restore() { glfwRestoreWindow((GLFWwindow*)handle); }

    bool Window::IsMaximize() { return (bool)glfwGetWindowAttrib((GLFWwindow*)handle, GLFW_MAXIMIZED); }

    bool Window::IsMinimized() { return (bool)glfwGetWindowAttrib((GLFWwindow*)handle, GLFW_ICONIFIED); }

    bool Window::IsFullScreen() { return desc.fullScreen; }

    bool Window::ToggleScreenState()
    {
        GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);

        if (desc.fullScreen)
        {
            // Restore the window size and position
            desc.fullScreen = false;
            glfwSetWindowMonitor((GLFWwindow*)handle, nullptr, prevPosX, prevPosY, prevWidth, prevHeight, 0);
        }
        else
        {
            // Save the window size and position
            desc.fullScreen = true;
            glfwGetWindowSize((GLFWwindow*)handle, &prevWidth, &prevHeight);
            glfwGetWindowPos((GLFWwindow*)handle, &prevPosX, &prevPosY);
            glfwSetWindowMonitor((GLFWwindow*)handle, primaryMonitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        }

        return true;
    }

    void Window::Focus() { glfwFocusWindow((GLFWwindow*)handle); }

    bool Window::IsFocused() { return (bool)glfwGetWindowAttrib((GLFWwindow*)handle, GLFW_FOCUSED); }

    void Window::Show() { glfwShowWindow((GLFWwindow*)handle); }

    void Window::Hide() { glfwHideWindow((GLFWwindow*)handle); }

    std::pair<float, float> Window::GetWindowContentScale()
    {
        float xscale, yscale;
        glfwGetWindowContentScale((GLFWwindow*)handle, &xscale, &yscale);

        return { xscale, yscale };
    }

    void Window::UpdateEvent()
    {
        HE_PROFILE_FUNCTION();

        for (int jid = 0; jid < Joystick::Count; jid++)
        {
            if (glfwJoystickPresent(jid))
            {
                GLFWgamepadstate state;
                if (glfwGetGamepadState(jid, &state))
                {
                    for (int button = 0; button < GamepadButton::Count; button++)
                    {
                        bool isButtonDown = state.buttons[button] == GLFW_PRESS;

                        bool isPressed = isButtonDown && !inputData.gamepadEventButtonDownPrevFrame[jid].test(button);
                        inputData.gamepadEventButtonDownPrevFrame[jid].set(button, isButtonDown);
                        if (isPressed)
                        {
                            GamepadButtonPressedEvent e(jid, button);
                            eventCallback(e);
                        }

                        bool isReleased = !isButtonDown && !inputData.gamepadEventButtonUpPrevFrame[jid].test(button);
                        inputData.gamepadEventButtonUpPrevFrame[jid].set(button, !isButtonDown);
                        if (isReleased)
                        {
                            GamepadButtonReleasedEvent e(jid, button);
                            eventCallback(e);
                        }
                    }

                    // axes
                    {
                        auto createEvent = [](Window& window, int jid, GamepadAxisCode axisCode, Math::vec2 value) {

                            if (Math::length(value) > 0)
                            {
                                GamepadAxisMovedEvent event(jid, axisCode, value.x, value.y);
                                window.eventCallback(event);
                            }
                            };

                        {
                            Math::vec2 v(state.axes[GLFW_GAMEPAD_AXIS_LEFT_X], state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y]);
                            v *= Math::max(Math::length(v) - inputData.deadZoon, 0.0f) / (1.f - inputData.deadZoon);
                            v = Math::clamp(v, Math::vec2(-1.0f), Math::vec2(1.0f));

                            createEvent(*this, jid, GamepadAxis::Left, v);
                        }

                        {
                            Math::vec2 v(state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X], state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y]);
                            v *= Math::max(Math::length(v) - inputData.deadZoon, 0.0f) / (1.f - inputData.deadZoon);
                            v = Math::clamp(v, Math::vec2(-1.0f), Math::vec2(1.0f));

                            createEvent(*this, jid, GamepadAxis::Right, v);
                        }
                    }
                }
            }
        }

        {
            HE_PROFILE_SCOPE("glfwPollEvents");
            glfwPollEvents();
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // Input
    //////////////////////////////////////////////////////////////////////////

    bool Input::IsKeyDown(const KeyCode key)
    {
        auto& w = GetAppContext().mainWindow;
        auto window = static_cast<GLFWwindow*>(w.handle);
        int glfwKey = ToGLFWKeyCode(key);
        auto state = glfwGetKey(window, glfwKey);

        return state == GLFW_PRESS;
    }

    bool Input::IsKeyUp(const KeyCode key)
    {
        auto& w = GetAppContext().mainWindow;
        auto window = static_cast<GLFWwindow*>(w.handle);
        int glfwKey = ToGLFWKeyCode(key);
        auto state = glfwGetKey(window, glfwKey);

        return state == GLFW_RELEASE;
    }

    bool Input::IsKeyPressed(const KeyCode key)
    {
        auto& w = GetAppContext().mainWindow;
        auto window = static_cast<GLFWwindow*>(w.handle);
        int glfwKey = ToGLFWKeyCode(key);
        auto currentState = glfwGetKey(window, glfwKey);

        bool isKeyDown = currentState == GLFW_PRESS && !w.inputData.keyDownPrevFrame.test(key);
        w.inputData.keyDownPrevFrame.set(key, currentState == GLFW_PRESS);

        return isKeyDown;
    }

    bool Input::IsKeyReleased(const KeyCode key)
    {
        auto& w = GetAppContext().mainWindow;
        auto window = static_cast<GLFWwindow*>(w.handle);
        int glfwKey = ToGLFWKeyCode(key);
        auto currentState = glfwGetKey(window, glfwKey);

        bool isKeyUp = currentState == GLFW_RELEASE && !w.inputData.keyUpPrevFrame.test(key);
        w.inputData.keyUpPrevFrame.set(key, currentState == GLFW_RELEASE);

        return isKeyUp;
    }

    bool Input::IsMouseButtonDown(const MouseCode button)
    {
        auto& w = GetAppContext().mainWindow;
        auto window = static_cast<GLFWwindow*>(w.handle);
        auto state = glfwGetMouseButton(window, static_cast<int32_t>(button));
        return state == GLFW_PRESS;
    }

    bool Input::IsMouseButtonUp(const MouseCode button)
    {
        auto& w = GetAppContext().mainWindow;
        auto window = static_cast<GLFWwindow*>(w.handle);
        auto state = glfwGetMouseButton(window, static_cast<int32_t>(button));
        return state == GLFW_RELEASE;
    }

    bool Input::IsMouseButtonPressed(const MouseCode key)
    {
        auto& w = GetAppContext().mainWindow;
        auto window = static_cast<GLFWwindow*>(w.handle);
        auto currentState = glfwGetMouseButton(window, static_cast<int32_t>(key));

        bool isKeyDown = (currentState == GLFW_PRESS) && !w.inputData.mouseButtonDownPrevFrame.test(key);
        w.inputData.mouseButtonDownPrevFrame.set(key, currentState == GLFW_PRESS);

        return isKeyDown;
    }

    bool Input::IsMouseButtonReleased(const MouseCode key)
    {
        auto& w = GetAppContext().mainWindow;
        auto window = static_cast<GLFWwindow*>(w.handle);
        auto currentState = glfwGetMouseButton(window, static_cast<int32_t>(key));

        bool isKeyUp = (currentState == GLFW_RELEASE) && !w.inputData.mouseButtonUpPrevFrame.test(key);
        w.inputData.mouseButtonUpPrevFrame.set(key, currentState == GLFW_RELEASE);

        return isKeyUp;
    }

    std::pair<float, float> Input::GetMousePosition()
    {
        auto& w = GetAppContext().mainWindow;
        auto window = static_cast<GLFWwindow*>(w.handle);
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        return std::make_pair(float(xpos), float(ypos));
    }

    float Input::GetMouseX() { return GetMousePosition().first; }
    float Input::GetMouseY() { return GetMousePosition().second; }

    bool Input::IsGamepadButtonDown(JoystickCode id, GamepadCode code)
    {
        if (glfwJoystickPresent(id))
        {
            GLFWgamepadstate state;
            if (glfwGetGamepadState(id, &state))
                return state.buttons[code] == GLFW_PRESS;
        }

        return false;
    }

    bool Input::IsGamepadButtonUp(JoystickCode id, GamepadCode code)
    {
        if (glfwJoystickPresent(id))
        {
            GLFWgamepadstate state;
            if (glfwGetGamepadState(id, &state))
                return state.buttons[code] == GLFW_RELEASE;
        }

        return false;
    }

    bool Input::IsGamepadButtonPressed(JoystickCode id, GamepadCode code)
    {
        bool b = false;

        if (glfwJoystickPresent(id))
        {
            GLFWgamepadstate state;
            if (glfwGetGamepadState(id, &state))
            {
                auto& w = GetAppContext().mainWindow;

                bool isPressed = state.buttons[code] == GLFW_PRESS;
                b = isPressed && !w.inputData.gamepadButtonDownPrevFrame[id].test(code);
                w.inputData.gamepadButtonDownPrevFrame[id].set(code, isPressed);
            }
        }

        return b;
    }

    bool Input::IsGamepadButtonReleased(JoystickCode id, GamepadCode code)
    {
        bool b = false;

        if (glfwJoystickPresent(id))
        {
            GLFWgamepadstate state;
            if (glfwGetGamepadState(id, &state))
            {
                auto& w = GetAppContext().mainWindow;

                bool isReleased = state.buttons[code] == GLFW_RELEASE;
                b = isReleased && !w.inputData.gamepadButtonUpPrevFrame[id].test(code);
                w.inputData.gamepadButtonUpPrevFrame[id].set(code, isReleased);
            }
        }

        return b;
    }

    std::pair<float, float> Input::GetGamepadLeftAxis(JoystickCode code)
    {
        if (glfwJoystickPresent(code))
        {
            GLFWgamepadstate state;
            if (glfwGetGamepadState(code, &state))
            {
                auto& w = GetAppContext().mainWindow;

                Math::vec2 v(state.axes[GLFW_GAMEPAD_AXIS_LEFT_X], state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y]);
                v *= Math::max(Math::length(v) - w.inputData.deadZoon, 0.0f) / (1.f - w.inputData.deadZoon);
                v = Math::clamp(v, Math::vec2(-1.0f), Math::vec2(1.0f));

                return std::pair<float, float>(v.x, v.y);
            }
        }

        return std::pair<float, float>(0.0f, 0.0f);
    }

    std::pair<float, float> Input::GetGamepadRightAxis(JoystickCode code)
    {
        if (glfwJoystickPresent(code))
        {
            GLFWgamepadstate state;
            if (glfwGetGamepadState(code, &state))
            {
                auto& w = GetAppContext().mainWindow;

                Math::vec2 v(state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X], state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y]);
                v *= Math::max(Math::length(v) - w.inputData.deadZoon, 0.0f) / (1.f - w.inputData.deadZoon);
                v = Math::clamp(v, Math::vec2(-1.0f), Math::vec2(1.0f));

                return std::pair<float, float>(v.x, v.y);
            }
        }

        return std::pair<float, float>(0.0f, 0.0f);
    }

    void Input::SetDeadZoon(float value)
    {
        auto& w = GetAppContext().mainWindow;
        w.inputData.deadZoon = value;
    }

    void Input::SetCursorMode(Cursor::Mode mode)
    {
        auto& w = GetAppContext().mainWindow;
        auto window = static_cast<GLFWwindow*>(w.handle);
        glfwSetInputMode(window, GLFW_CURSOR, ToGLFWCursorMode(mode));
        w.inputData.cursor.CursorMode = mode;
    }

    Cursor::Mode Input::GetCursorMode()
    {
        auto& w = GetAppContext().mainWindow;
        return w.inputData.cursor.CursorMode;
    }

    bool Input::Triggered(const std::string_view& name)
    {
        auto& c = GetAppContext();

        auto hash = Hash(name);

        if (c.blockingEventsUntilNextFrame || !c.keyBindings.contains(hash))
            return false;

        const auto& keysData = c.keyBindings.at(hash);

        for (const auto& m : keysData.modifiers)
            if (m != 0 && !Input::IsKeyDown(m))
                return false;

        if (keysData.eventCategory == EventCategory::Keyboard)
        {
            if (keysData.eventType == EventType::KeyPressed && Input::IsKeyPressed(keysData.code))
            {
                BlockEventsUntilNextFrame();
                return true;
            }

            if (keysData.eventType == EventType::KeyReleased && Input::IsKeyReleased(keysData.code))
            {
                BlockEventsUntilNextFrame();
                return true;
            }
        }
        else if(keysData.eventCategory == EventCategory::Mouse)
        {
            if (keysData.eventType == EventType::MouseButtonPressed && Input::IsMouseButtonPressed(keysData.code))
            {
                BlockEventsUntilNextFrame();
                return true;
            }

            if (keysData.eventType == EventType::MouseButtonReleased && Input::IsMouseButtonReleased(keysData.code))
            {
                BlockEventsUntilNextFrame();
                return true;
            }
        }

        return false;
    }

    void Input::BlockEventsUntilNextFrame()
    {
        GetAppContext().blockingEventsUntilNextFrame = true;
    }

    bool Input::IsEventsBlocked()
    {
        return GetAppContext().blockingEventsUntilNextFrame;
    }

    bool Input::RegisterKeyBinding(const KeyBindingDesc& action)
    {
        auto& c = GetAppContext();

        auto hash = Hash(action.name);

        if (!c.keyBindings.contains(hash))
        {
            c.keyBindings[hash] = action;
            return true;
        }

        HE_CORE_ERROR("Input::RegisterKeyBinding action with name '{}' already regestered", action.name);
        return false;
    }

    std::map<uint64_t, KeyBindingDesc>& Input::GetKeyBindings()
    {
        return GetAppContext().keyBindings;
    }

    std::string_view Input::GetShortCut(std::string_view name)
    {
        auto& keyBindings = GetAppContext().keyBindings;
        auto hash = Hash(name);

        if (keyBindings.contains(hash))
            return keyBindings.at(hash).shortCut;

        return "None";
    }

    //////////////////////////////////////////////////////////////////////////
    // Image
    //////////////////////////////////////////////////////////////////////////

    Image::Image(const std::filesystem::path& filename, int desiredChannels, bool flipVertically)
    {
        bool isHDR = filename.extension() == ".hdr";

        stbi_set_flip_vertically_on_load(flipVertically);

        if (isHDR)
        {
            if (width != height * 2)
            {
                HE_CORE_ERROR("{} is not an equirectangular image!", filename.string());
                return;
            }

            data = (uint8_t*)stbi_loadf(filename.string().c_str(), &width, &height, &channels, 0);
        }
        else
        {
            data = stbi_load(filename.string().c_str(), &width, &height, &channels, desiredChannels);
        }

        if (!data)
        {
            HE_CORE_ERROR("Failed to load image: {}", stbi_failure_reason());
        }
    }

    Image::Image(Buffer buffer, int desiredChannels, bool flipVertically)
    {
        stbi_set_flip_vertically_on_load(flipVertically);
        data = stbi_load_from_memory(buffer.data, (int)buffer.size, &width, &height, &channels, desiredChannels);

        if (!data)
        {
            HE_CORE_ERROR("Failed to load image: {}", stbi_failure_reason());
        }
    }

    Image::Image(int pWidth, int pHeight, int pChannels, uint8_t* pData)
        : width(pWidth)
        , height(pHeight)
        , channels(pChannels)
        , data(pData)
    {
    }

    Image::~Image()
    {
        if (data)
        {
            stbi_image_free(data);
        }
    }

    Image::Image(Image&& other) noexcept
        : data(other.data)
        , width(other.width)
        , height(other.height)
        , channels(other.channels)
    {
        other.data = nullptr;
    }

    Image& Image::operator=(Image&& other) noexcept
    {
        if (this != &other)
        {
            if (data)
            {
                stbi_image_free(data);
            }
            data = other.data;
            width = other.width;
            height = other.height;
            channels = other.channels;
            other.data = nullptr;
        }
        return *this;
    }

    bool Image::GetImageInfo(const std::filesystem::path& filePath, int& outWidth, int& outHeight, int& outChannels)
    {
        return stbi_info(filePath.string().c_str(), &outWidth, &outHeight, &outChannels);
    }

    bool Image::SaveAsPNG(const std::filesystem::path& filePath, int width, int height, int channels, const void* data, int strideInBytes)
    {
        if (!data) return false;
        return stbi_write_png(filePath.string().c_str(), width, height, channels, data, strideInBytes);
    }

    bool Image::SaveAsJPG(const std::filesystem::path& filePath, int width, int height, int channels, const void* data, int quality)
    {
        if (!data) return false;
        return stbi_write_jpg(filePath.string().c_str(), width, height, channels, data, quality);
    }

    bool Image::SaveAsBMP(const std::filesystem::path& filePath, int width, int height, int channels, const void* data)
    {
        if (!data) return false;
        return stbi_write_bmp(filePath.string().c_str(), width, height, channels, data);
    }

    void Image::SetData(uint8_t* pData)
    {
        if (data)
        {
            stbi_image_free(data);
        }
        data = pData;
    }

    uint8_t* Image::ExtractData()
    {
        uint8_t* extracted = data;
        data = nullptr;
        return extracted;
    }

    //////////////////////////////////////////////////////////////////////////
    // Utils
    //////////////////////////////////////////////////////////////////////////

    namespace MouseKey
    {
        constexpr CodeStrPair c_CodeToStringMap[] = {
            { Left,    "Left"    }, { Right,   "Right"   }, { Middle,  "Middle"  },
            { Button3, "Button3" }, { Button4, "Button4" }, { Button5, "Button5" },
            { Button6, "Button6" }, { Button7, "Button7" },
        };

        constexpr std::string_view ToString(MouseCode code) { return c_CodeToStringMap[code].codeStr; }

        constexpr MouseCode FromString(std::string_view code)
        {
            for (auto& pair : c_CodeToStringMap)
                if (pair.codeStr == code)
                    return pair.code;

            HE_CORE_VERIFY(false);
            return -1;
        }

        constexpr std::span<const CodeStrPair> Map()
        {
            return std::span<const CodeStrPair>(c_CodeToStringMap, std::size(c_CodeToStringMap));
        }
    }

    namespace Joystick
    {
        constexpr CodeStrPair c_CodeToStringMap[] = {
            { Joystick0,  "Joystick1"  }, { Joystick1,  "Joystick2"  }, { Joystick2,  "Joystick3"  },
            { Joystick3,  "Joystick4"  }, { Joystick4,  "Joystick5"  }, { Joystick5,  "Joystick6"  },
            { Joystick6,  "Joystick7"  }, { Joystick7,  "Joystick8"  }, { Joystick8,  "Joystick9"  },
            { Joystick9,  "Joystick10" }, { Joystick10, "Joystick11" }, { Joystick11, "Joystick12" },
            { Joystick12, "Joystick13" }, { Joystick13, "Joystick14" },
            { Joystick14, "Joystick15" }, { Joystick15, "Joystick16" }
        };

        constexpr std::string_view ToString(JoystickCode code) { return c_CodeToStringMap[code].codeStr; }

        constexpr JoystickCode FromString(std::string_view codeStr)
        {
            for (auto& pair : c_CodeToStringMap)
                if (pair.codeStr == codeStr)
                    return pair.code;

            HE_CORE_VERIFY(false);
            return -1;
        }

        constexpr std::span<const CodeStrPair> Map()
        {
            return std::span<const CodeStrPair>(c_CodeToStringMap, std::size(c_CodeToStringMap));
        }
    }

    namespace GamepadButton
    {
        constexpr CodeStrPair c_CodeToStringMap[] = {
            { A,          "A"           }, { B,           "B"            }, { X,           "X"           }, { Y,     "Y"     },
            { LeftBumper, "Left Bumper" }, { RightBumper, "Right Bumper" }, { Back,        "Back"        }, { Start, "Start" },
            { Guide,      "Guide"       }, { LeftThumb,   "Left Thumb"   }, { RightThumb,  "Right Thumb" }, { Up,    "Up"    },
            { Right,      "Right"       }, { Down,        "Down"         }, { Left,        "Left"        }
        };

        constexpr std::string_view ToString(GamepadCode code) { return c_CodeToStringMap[code].codeStr; }

        constexpr GamepadCode FromString(std::string_view codeStr)
        {
            for (auto& pair : c_CodeToStringMap)
                if (pair.codeStr == codeStr)
                    return pair.code;

            HE_CORE_VERIFY(false);
            return -1;
        }

        constexpr std::span<const CodeStrPair> Map()
        {
            return std::span<const CodeStrPair>(c_CodeToStringMap, std::size(c_CodeToStringMap));
        }
    }

    namespace GamepadAxis
    {
        constexpr CodeStrPair c_CodeToStringMap[] = {
            { Left, "Left"  }, { Right, "Right" }
        };

        constexpr std::string_view ToString(GamepadAxisCode code) { return c_CodeToStringMap[code].codeStr; }

        constexpr GamepadAxisCode FromString(std::string_view codeStr)
        {
            for (auto& pair : c_CodeToStringMap)
                if (pair.codeStr == codeStr)
                    return pair.code;

            HE_CORE_VERIFY(false);
            return -1;
        }

        constexpr std::span<const CodeStrPair> Map()
        {
            return std::span<const CodeStrPair>(c_CodeToStringMap, std::size(c_CodeToStringMap));
        }
    }

    namespace Key
    {
        constexpr CodeStrPair c_CodeToStringMap[] = {
            { Space,         "Space"           },
            { Apostrophe,    "'"               },
            { Comma,         ","               },
            { Minus,         "-"               },
            { Period,        "."               },
            { Slash,         "/"               },
            { D0,            "0"               },
            { D1,            "1"               },
            { D2,            "2"               },
            { D3,            "3"               },
            { D4,            "4"               },
            { D5,            "5"               },
            { D6,            "6"               },
            { D7,            "7"               },
            { D8,            "8"               },
            { D9,            "9"               },
            { Semicolon,     ";"               },
            { Equal,         "="               },
            { A,             "A"               },
            { B,             "B"               },
            { C,             "C"               },
            { D,             "D"               },
            { E,             "E"               },
            { F,             "F"               },
            { G,             "G"               },
            { H,             "H"               },
            { I,             "I"               },
            { J,             "J"               },
            { K,             "K"               },
            { L,             "L"               },
            { M,             "M"               },
            { N,             "N"               },
            { O,             "O"               },
            { P,             "P"               },
            { Q,             "Q"               },
            { R,             "R"               },
            { S,             "S"               },
            { T,             "T"               },
            { U,             "U"               },
            { V,             "V"               },
            { W,             "W"               },
            { X,             "X"               },
            { Y,             "Y"               },
            { Z,             "Z"               },
            { LeftBracket,   "["               },
            { Backslash,     "\\"              },
            { RightBracket,  "]"               },
            { GraveAccent,   "`"               },
            { World1,        "World1"          },
            { World2,        "World2"          },
            { Escape,        "Escape"          },
            { Enter,         "Enter"           },
            { Tab,           "Tab"             },
            { Backspace,     "Backspace"       },
            { Insert,        "Insert"          },
            { Delete,        "Delete"          },
            { Right,         "Right"           },
            { Left,          "Left"            },
            { Down,          "Down"            },
            { Up,            "Up"              },
            { PageUp,        "PageUp"          },
            { PageDown,      "PageDown"        },
            { Home,          "Home"            },
            { End,           "End"             },
            { CapsLock,      "CapsLock"        },
            { ScrollLock,    "Scroll Lock"     },
            { NumLock,       "Num Lock"        },
            { PrintScreen,   "Print Screen"    },
            { Pause,         "Pause"           },
            { F1,            "F1"              },
            { F2,            "F2"              },
            { F3,            "F3"              },
            { F4,            "F4"              },
            { F5,            "F5"              },
            { F6,            "F6"              },
            { F7,            "F7"              },
            { F8,            "F8"              },
            { F9,            "F9"              },
            { F10,           "F10"             },
            { F11,           "F11"             },
            { F12,           "F12"             },
            { F13,           "F13"             },
            { F14,           "F14"             },
            { F15,           "F15"             },
            { F16,           "F16"             },
            { F17,           "F17"             },
            { F18,           "F18"             },
            { F19,           "F19"             },
            { F20,           "F20"             },
            { F21,           "F21"             },
            { F22,           "F22"             },
            { F23,           "F23"             },
            { F24,           "F24"             },
            { F25,           "F25"             },
            { KP0,           "Keypad 0"        },
            { KP1,           "Keypad 1"        },
            { KP2,           "Keypad 2"        },
            { KP3,           "Keypad 3"        },
            { KP4,           "Keypad 4"        },
            { KP5,           "Keypad 5"        },
            { KP6,           "Keypad 6"        },
            { KP7,           "Keypad 7"        },
            { KP8,           "Keypad 8"        },
            { KP9,           "Keypad 9"        },
            { KPDecimal,     "Keypad ."        },
            { KPDivide,	     "Keypad /"        },
            { KPMultiply,    "Keypad *"        },
            { KPSubtract,    "Keypad -"        },
            { KPAdd,         "Keypad +"        },
            { KPEnter,       "Keypad Enter"    },
            { KPEqual,       "Keypad ="        },
            { LeftShift,     "Left Shift"      },
            { LeftControl,   "Left Control"    },
            { LeftAlt,       "Left Alt"        },
            { LeftSuper,	 "Left Super"      },
            { RightShift,    "Right Shift"     },
            { RightControl,  "Right Control"   },
            { RightAlt,      "Right Alt"       },
            { RightSuper,    "Right Super"     },
            { Menu,          "Menu"            },
        };

        constexpr std::string_view ToString(KeyCode code) { return c_CodeToStringMap[code].codeStr; }

        constexpr KeyCode FromString(std::string_view code)
        {
            for (auto& pair : c_CodeToStringMap)
                if (pair.codeStr == code)
                    return pair.code;

            HE_CORE_VERIFY(false);
            return -1;
        }

        constexpr std::span<const CodeStrPair> Map()
        {
            return std::span<const CodeStrPair>(c_CodeToStringMap, std::size(c_CodeToStringMap));
        }
    }

    constexpr CodeStrPair c_EventTypeMap[] = {
            { (int)EventType::KeyPressed,            "Key Pressed"            },
            { (int)EventType::KeyReleased,           "Key Released"           },
            { (int)EventType::KeyTyped,              "Key Typed"              },
            { (int)EventType::MouseButtonPressed,    "Mouse Button Pressed"   },
            { (int)EventType::MouseButtonReleased,   "Mouse Button Released"  },
            { (int)EventType::MouseMoved,            "Mouse Moved"            },
            { (int)EventType::MouseScrolled,         "Mouse Scrolled"         },
            { (int)EventType::MouseEnter,            "Mouse Enter"            },
            { (int)EventType::GamepadButtonPressed,  "Gamepad Button Pressed" },
            { (int)EventType::GamepadButtonReleased, "Gamepad ButtonReleased" },
            { (int)EventType::GamepadAxisMoved,      "Gamepad Axis Moved"     },
            { (int)EventType::GamepadConnected,      "Gamepad Connected"      },
            { (int)EventType::WindowClose,           "Window Close"           },
            { (int)EventType::WindowResize,          "Window Resize"          },
            { (int)EventType::WindowFocus,           "Window Focus"           },
            { (int)EventType::WindowLostFocus,       "Window LostFocus"       },
            { (int)EventType::WindowMoved,           "Window Moved"           },
            { (int)EventType::WindowDrop,            "Window Drop"            },
            { (int)EventType::WindowContentScale,    "Window ContentScale"    },
            { (int)EventType::WindowMaximize,        "Window Maximize"        },
            { (int)EventType::WindowMinimized,       "Window Minimized"       },
            { (int)EventType::None,                  "None"                   },
    };

    constexpr std::string_view ToString(EventType code) { return c_EventTypeMap[int(code)].codeStr; }

    constexpr EventType FromStringToEventType(std::string_view code)
    {
        for (auto& pair : c_EventTypeMap)
            if (pair.codeStr == code)
                return (EventType)pair.code;

        HE_CORE_VERIFY(false);
        return EventType::None;
    }

    constexpr std::span<const CodeStrPair> EventTypeMap() { return std::span<const CodeStrPair>(c_EventTypeMap, std::size(c_EventTypeMap)); }

    constexpr CodeStrPair c_EventCategoryMap[] = {
        { (int)EventCategory::Keyboard,      "Keyboard"       },
        { (int)EventCategory::Mouse,         "Mouse"          },
        { (int)EventCategory::Gamepad,       "Gamepad"        },
        { (int)EventCategory::Window,        "Window"         },
        { (int)EventCategory::None,          "None"           },
    };

    constexpr std::span<const CodeStrPair> EventCategoryMap() { return std::span<const CodeStrPair>(c_EventCategoryMap, std::size(c_EventCategoryMap)); }

    constexpr std::string_view ToString(EventCategory code) { return c_EventCategoryMap[int(code)].codeStr; }

    constexpr EventCategory FromStringToEventCategory(std::string_view code)
    {
        for (auto& pair : c_EventCategoryMap)
            if (pair.codeStr == code)
                return (EventCategory)pair.code;

        HE_CORE_VERIFY(false);
        return EventCategory::None;
    }

    void Input::SerializeKeyBindings(const std::filesystem::path& filePath)
    {
        std::ofstream file(filePath);
        if (!file.is_open())
        {
            HE_ERROR("Input::SerializeKeyBindings : Unable to open file for writing, {}", filePath.string());
        }

        std::ostringstream os;
        os << "{\n";
        os << "\t\"bindings\" : [\n";


        for (int bindingIndex = 0; auto & [key, desc] : Input::GetKeyBindings())
        {
            if (bindingIndex != 0) os << ",\n"; bindingIndex++;

            os << "\t\t{\n";
            os << "\t\t\t\"name\" : \"" << desc.name << "\",\n";
            os << "\t\t\t\"modifiers\" : [ ";
            for (int i = 0; i < desc.modifiers.size(); i++)
            {
                if (desc.modifiers[i] != 0)
                {
                    if (i > 0) os << ", ";
                    os << "\"" << Key::ToString(desc.modifiers[i]) << "\"";
                }
            }
            os << " ],\n";

            if (desc.eventCategory == EventCategory::Keyboard)
                os << "\t\t\t\"code\" : \"" << Key::ToString(desc.code) << "\",\n";
            if (desc.eventCategory == EventCategory::Mouse)
                os << "\t\t\t\"code\" : \"" << MouseKey::ToString(desc.code) << "\",\n";

            os << "\t\t\t\"eventType\" : \"" << ToString(desc.eventType) << "\",\n";
            os << "\t\t\t\"eventCategory\" : \"" << ToString(desc.eventCategory) << "\",\n";
            os << "\t\t\t\"shortCut\" : \"" << desc.shortCut << "\"\n";
            os << "\t\t}";
        }


        os << "\n\t]\n";
        os << "}\n";

        file << os.str();
    }

    bool Input::DeserializeKeyBindings(const std::filesystem::path& filePath)
    {
        if (!std::filesystem::exists(filePath))
        {
            HE_ERROR("Unable to open file for reaading, {}", filePath.string());
            return false;
        }

        static simdjson::dom::parser parser;
        auto doc = parser.load(filePath.string());

        if (doc["bindings"].error())
            return false;

        auto bindings = doc["bindings"].get_array();
        if (!bindings.error())
        {
            for (auto desc : bindings)
            {
                auto modifiers = desc["modifiers"].get_array();
                std::array<uint16_t, c_MaxModifierCount> arr = {};
                if (!modifiers.error())
                {
                    for (int i = 0; i < modifiers.size(); i++)
                        arr[i] = Key::FromString(modifiers.at(i).get_c_str().value());
                }

                uint16_t code;

                const char* name = !desc["name"].error() ? desc["name"].get_c_str().value() : "None";
                EventType eventType = !desc["eventType"].error() ? FromStringToEventType(desc["eventType"].get_c_str().value()) : EventType::None;
                EventCategory eventCategory = !desc["eventCategory"].error() ? FromStringToEventCategory(desc["eventCategory"].get_c_str().value()) : EventCategory::None;
                std::string shortCut = !desc["shortCut"].error() ? desc["shortCut"].get_c_str().value() : "None";


                if (eventCategory == EventCategory::Keyboard)
                    code = !desc["code"].error() ? Key::FromString(desc["code"].get_c_str().value()) : -1;
                if (eventCategory == EventCategory::Mouse)
                    code = !desc["code"].error() ? MouseKey::FromString(desc["code"].get_c_str().value()) : -1;

                Input::RegisterKeyBinding({
                    .name = name,
                    .modifiers = arr,
                    .code = code,
                    .eventType = eventType,
                    .eventCategory = eventCategory,
                    .shortCut = shortCut
                });
            }
        }

        return true;
    }

    namespace FileSystem {

        bool Delete(const std::filesystem::path& path)
        {
            namespace fs = std::filesystem;

            try
            {
                if (fs::exists(path))
                {
                    if (fs::is_regular_file(path))
                    {
                        fs::remove(path);
                        return true;
                    }
                    else if (fs::is_directory(path))
                    {
                        fs::remove_all(path);
                        return true;
                    }
                    else
                    {
                        HE_CORE_ERROR("Unknown file type");
                        return false;
                    }
                }
                else
                {
                    HE_CORE_ERROR("File or directory {} does not exist ", path.string());
                    return false;
                }
            }
            catch (const std::exception& ex)
            {
                auto& e = ex;
                HE_CORE_ERROR("{}", e.what());
                return false;
            }
        }

        bool Rename(const std::filesystem::path& oldPath, const std::filesystem::path& newPath)
        {
            try
            {
                std::filesystem::rename(oldPath, newPath);
                return true;
            }
            catch (const std::exception& ex)
            {
                auto& e = ex;
                HE_CORE_ERROR("{}", e.what());
            }

            return false;
        }

        bool Copy(const std::filesystem::path& from, const std::filesystem::path& to, std::filesystem::copy_options options)
        {
            try
            {
                std::filesystem::copy(from, to, options);
                return true;
            }
            catch (const std::exception& ex)
            {
                auto& e = ex;
                HE_CORE_ERROR("{}", e.what());
            }

            return false;
        }

        std::vector<uint8_t> ReadBinaryFile(const std::filesystem::path& filePath)
        {
            std::ifstream inputFile(filePath, std::ios::binary | std::ios::ate);

            if (!inputFile)
            {
                HE_CORE_ERROR("Unable to open input file {}", filePath.string());
                return {};
            }

            std::streamsize fileSize = inputFile.tellg();
            inputFile.seekg(0, std::ios::beg);

            std::vector<uint8_t> buffer;
            buffer.resize(static_cast<size_t>(fileSize));
            inputFile.read(reinterpret_cast<char*>(buffer.data()), fileSize);

            return buffer;
        }

        bool ReadBinaryFile(const std::filesystem::path& filePath, Buffer buffer)
        {
            std::ifstream inputFile(filePath, std::ios::binary | std::ios::ate);
            if (!inputFile)
            {
                HE_CORE_ERROR("Unable to open input file {}", filePath.string());
                return false;
            }

            std::streamsize fileSize = inputFile.tellg();
            inputFile.seekg(0, std::ios::beg);

            if (buffer.size < static_cast<size_t>(fileSize))
            {
                HE_CORE_ERROR("Provided buffer is too small. Required size: {}", fileSize);
                return false;
            }

            inputFile.read(reinterpret_cast<char*>(buffer.data), fileSize);

            return true;
        }

        std::string FileSystem::ReadTextFile(const std::filesystem::path& filePath)
        {
            std::ifstream infile(filePath, std::ios::in | std::ios::ate);
            if (!infile)
            {
                HE_CORE_ERROR("Could not open input file: {}", filePath.string());
                return {};
            }

            std::streamsize size = infile.tellg();
            infile.seekg(0, std::ios::beg);

            std::string content(size, '\0');
            infile.read(content.data(), size);

            return content;
        }

        bool ConvertBinaryToHeader(const std::filesystem::path& inputFileName, const std::filesystem::path& outputFileName, const std::string& arrayName)
        {
            std::ifstream inputFile(inputFileName, std::ios::binary);
            if (!inputFile)
            {
                HE_CORE_ERROR("Error: Unable to open input file ", inputFileName.string());
                return false;
            }

            std::vector<uint8_t> buffer(std::istreambuf_iterator<char>(inputFile), {});
            inputFile.close();

            std::ofstream outputFile(outputFileName);
            if (!outputFile)
            {
                HE_CORE_ERROR("Error: Unable to open input file ", inputFileName.string());
                return false;
            }

            outputFile << "#ifndef " << arrayName << "_H" << std::endl;
            outputFile << "#define " << arrayName << "_H" << std::endl;
            outputFile << std::endl;
            outputFile << "unsigned char " << arrayName << "[] = {" << std::endl;

            for (size_t i = 0; i < buffer.size(); ++i)
            {
                outputFile << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(buffer[i]);

                if (i != buffer.size() - 1)
                {
                    outputFile << ", ";
                }

                if ((i + 1) % 12 == 0)  // 12 bytes per line
                {
                    outputFile << std::endl;
                }
            }

            outputFile << std::endl << "};" << std::endl;
            outputFile << std::endl;
            outputFile << "#endif //" << arrayName << "_H" << std::endl;

            outputFile.close();
            return true;
        }

        bool GenerateFileWithReplacements(const std::filesystem::path& input, const std::filesystem::path& output, const std::initializer_list<std::pair<std::string_view, std::string_view>>& replacements)
        {
            std::ifstream infile(input, std::ios::binary | std::ios::ate);
            if (!infile)
            {
                HE_CORE_ERROR("Could not open input file: {}", input.string());
                return false;
            }

            std::streamsize size = infile.tellg();
            infile.seekg(0, std::ios::beg);

            std::string content(size, '\0');
            infile.read(content.data(), size);

            for (const auto& [oldText, newText] : replacements)
            {
                size_t pos = 0;
                while ((pos = content.find(oldText, pos)) != std::string::npos)
                {
                    content.replace(pos, oldText.length(), newText);
                    pos += newText.length();
                }
            }

            std::ofstream outfile(output, std::ios::binary);
            if (!outfile)
            {
                HE_CORE_ERROR("Could not open output file: {}", output.string());
                return false;
            }

            outfile.write(content.data(), content.size());

            return true;
        }

        bool ExtractZip(const std::filesystem::path& zipPath, const std::filesystem::path& outputDir)
        {
            mz_zip_archive zip;
            memset(&zip, 0, sizeof(zip));

            auto pathStr = zipPath.string();
            if (!mz_zip_reader_init_file(&zip, pathStr.c_str(), 0))
                return false;

            int fileCount = (int)mz_zip_reader_get_num_files(&zip);
            for (int i = 0; i < fileCount; i++)
            {
                mz_zip_archive_file_stat file_stat;
                if (!mz_zip_reader_file_stat(&zip, i, &file_stat))
                    continue;

                std::filesystem::path destPath = outputDir / file_stat.m_filename;

                if (file_stat.m_is_directory)
                {
                    std::filesystem::create_directories(destPath);
                }
                else
                {
                    std::filesystem::create_directories(destPath.parent_path());
                    mz_zip_reader_extract_to_file(&zip, i, destPath.string().c_str(), 0);
                }
            }

            mz_zip_reader_end(&zip);
            return true;
        }
    }

    namespace FileDialog {

        static nfdwindowhandle_t GetNDFWindowHandle()
        {
#ifdef HE_PLATFORM_WINDOWS
            return { NFD_WINDOW_HANDLE_TYPE_WINDOWS, HE::Application::GetWindow().GetNativeHandle() };
#elif HE_PLATFORM_LINUX
            return { NFD_WINDOW_HANDLE_TYPE_X11, HE::Application::GetWindow().GetNativeHandle() };
#endif 
        }

        std::filesystem::path OpenFile(std::initializer_list<std::pair<std::string_view, std::string_view>> filters)
        {
            std::array<nfdfilteritem_t, 32> nfdFilters;
            HE_CORE_ASSERT(filters.size() < nfdFilters.size());

            uint32_t filterCount = 0;
            for (const auto& filter : filters)
                nfdFilters[filterCount++] = { filter.first.data(), filter.second.data() };

            NFD::Guard nfdGuard;
            NFD::UniquePath outPath;

            nfdresult_t result = NFD::OpenDialog(outPath, nfdFilters.data(), filterCount, nullptr, GetNDFWindowHandle());
            if (result == NFD_OKAY)        return outPath.get();
            else if (result == NFD_CANCEL) return {};
            else HE_CORE_ERROR("Error: {}", NFD::GetError());
            return {};
        }

        std::filesystem::path SaveFile(std::initializer_list<std::pair<std::string_view, std::string_view>> filters)
        {
            std::array<nfdfilteritem_t, 32> nfdFilters;
            HE_CORE_ASSERT(filters.size() < nfdFilters.size());

            uint32_t filterCount = 0;
            for (const auto& filter : filters)
                nfdFilters[filterCount++] = { filter.first.data(), filter.second.data() };

            NFD::Guard nfdGuard;
            NFD::UniquePath outPath;

            nfdresult_t result = NFD::SaveDialog(outPath, nfdFilters.data(), filterCount, nullptr, nullptr, GetNDFWindowHandle());
            if (result == NFD_OKAY)        return outPath.get();
            else if (result == NFD_CANCEL) return {};

            HE_CORE_ERROR("Error: {}", NFD::GetError());

            return {};
        }

        std::filesystem::path SelectFolder()
        {
            NFD::Guard nfdGuard;
            NFD::UniquePath outPath;

            nfdresult_t result = NFD::PickFolder(outPath, nullptr, GetNDFWindowHandle());
            if (result == NFD_OKAY)        return outPath.get();
            else if (result == NFD_CANCEL) return {};

            HE_CORE_ERROR("Error: {}", NFD::GetError());

            return {};
        }
    }
}
