module;

#include "HydraEngine/Base.h"

module HE;
import std;

namespace HE {

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
                if(!headlessDevice)
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

                if(!headlessDevice)
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
}
