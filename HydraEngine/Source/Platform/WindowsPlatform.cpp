module;

#define NOMINMAX

#include "HydraEngine/Base.h"

#if defined(NVRHI_HAS_D3D11) | defined(NVRHI_HAS_D3D12)
    #include <dxgidebug.h>
#endif

#ifdef NVRHI_HAS_D3D11
    #include <dxgi1_3.h>
    #include <d3d11.h>
#endif

#ifdef NVRHI_HAS_D3D12
    #include <dxgi1_5.h>
    #include <d3d12.h>
#endif

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

module HE;
import std;
import nvrhi;

//////////////////////////////////////////////////////////////////////////
// D3D11
//////////////////////////////////////////////////////////////////////////
#ifdef NVRHI_HAS_D3D11

// Based on NVIDIA Donut framework (MIT License), with slight modifications
// Source: https://github.com/NVIDIA-RTX/Donut/blob/main/src/app/dx11/DeviceManager_DX11.cpp

class DeviceManagerDX11 : public HE::DeviceManager
{
    nvrhi::RefCountPtr<IDXGIFactory1> dxgiFactory;
    nvrhi::RefCountPtr<IDXGIAdapter> dxgiAdapter;
    nvrhi::RefCountPtr<ID3D11Device> device;
    nvrhi::RefCountPtr<ID3D11DeviceContext> immediateContext;
    nvrhi::RefCountPtr<IDXGISwapChain> swapChain;
    DXGI_SWAP_CHAIN_DESC swapChainDesc{};
    HWND hWnd = nullptr;
    nvrhi::DeviceHandle nvrhiDevice;
    nvrhi::TextureHandle m_RhiBackBuffer;
    nvrhi::RefCountPtr<ID3D11Texture2D> D3D11BackBuffer;
    std::string rendererString;

    static bool IsNvDeviceID(UINT id) { return id == 0x10DE; }

    static std::string GetAdapterName(DXGI_ADAPTER_DESC const& aDesc)
    {
        size_t length = wcsnlen(aDesc.Description, _countof(aDesc.Description));

        std::string name;
        name.resize(length);
        WideCharToMultiByte(CP_ACP, 0, aDesc.Description, int(length), name.data(), int(name.size()), nullptr, nullptr);

        return name;
    }

    void Present() override { swapChain->Present(m_DeviceDesc.vsyncEnabled ? 1 : 0, 0); }
    nvrhi::ITexture* GetCurrentBackBuffer() override { return m_RhiBackBuffer; }
    nvrhi::ITexture* GetBackBuffer(uint32_t index) override { return (index == 0) ? m_RhiBackBuffer : nullptr; }
    uint32_t GetCurrentBackBufferIndex() override { return 0; }
    uint32_t GetBackBufferCount() override { return 1; }

    const char* GetRendererString() const override { return rendererString.c_str(); }
    nvrhi::IDevice* GetDevice() const override { return nvrhiDevice; }
    nvrhi::GraphicsAPI GetGraphicsAPI() const override { return nvrhi::GraphicsAPI::D3D11; }

    bool BeginFrame() override
    {
        HE_PROFILE_FUNCTION();

        DXGI_SWAP_CHAIN_DESC newSwapChainDesc;
        if (SUCCEEDED(swapChain->GetDesc(&newSwapChainDesc)))
        {
            if (swapChainDesc.Windowed != newSwapChainDesc.Windowed)
            {
                BackBufferResizing();

                swapChainDesc = newSwapChainDesc;
                m_DeviceDesc.backBufferWidth = newSwapChainDesc.BufferDesc.Width;
                m_DeviceDesc.backBufferHeight = newSwapChainDesc.BufferDesc.Height;

                if (newSwapChainDesc.Windowed)
                    glfwSetWindowMonitor((GLFWwindow*)m_Window, nullptr, 50, 50, newSwapChainDesc.BufferDesc.Width, newSwapChainDesc.BufferDesc.Height, 0);

                ResizeSwapChain();
                BackBufferResized();
            }
        }

        return true;
    }

    void ReportLiveObjects() override
    {
        HE_PROFILE_FUNCTION();

        nvrhi::RefCountPtr<IDXGIDebug> pDebug;
        DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug));

        if (pDebug)
            pDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_DETAIL);
    }

    bool EnumerateAdapters(std::vector< HE::AdapterInfo>& outAdapters) override
    {
        HE_PROFILE_FUNCTION();

        if (!dxgiFactory)
            return false;

        outAdapters.clear();

        while (true)
        {
            nvrhi::RefCountPtr<IDXGIAdapter> adapter;
            HRESULT hr = dxgiFactory->EnumAdapters(uint32_t(outAdapters.size()), &adapter);
            if (FAILED(hr))
                return true;

            DXGI_ADAPTER_DESC desc;
            hr = adapter->GetDesc(&desc);
            if (FAILED(hr))
                return false;

            HE::AdapterInfo adapterInfo;

            adapterInfo.name = GetAdapterName(desc);
            adapterInfo.dxgiAdapter = adapter;
            adapterInfo.vendorID = desc.VendorId;
            adapterInfo.deviceID = desc.DeviceId;
            adapterInfo.dedicatedVideoMemory = desc.DedicatedVideoMemory;

            HE::AdapterInfo::LUID luid;
            static_assert(luid.size() == sizeof(desc.AdapterLuid));
            memcpy(luid.data(), &desc.AdapterLuid, luid.size());
            adapterInfo.luid = luid;

            outAdapters.push_back(std::move(adapterInfo));
        }
    }

    bool CreateInstanceInternal() override
    {
        HE_PROFILE_FUNCTION();

        if (!dxgiFactory)
        {
            HRESULT hres = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
            if (hres != S_OK)
            {
                HE_CORE_ERROR(
                    "ERROR in CreateDXGIFactory1.\n"
                    "For more info, get log from debug D3D runtime: (1) Install DX SDK, and enable Debug D3D from DX Control Panel Utility. (2) Install and start DbgView. (3) Try running the program again.\n"
                );
                return false;
            }
        }

        return true;
    }

    bool CreateDevice() override
    {
        HE_PROFILE_SCOPE("Create D11 Device");

        int adapterIndex = m_DeviceDesc.adapterIndex;
        if (adapterIndex < 0)
            adapterIndex = 0;

        if (FAILED(dxgiFactory->EnumAdapters(adapterIndex, &dxgiAdapter)))
        {
            if (adapterIndex == 0)
                HE_CORE_ERROR("Cannot find any DXGI adapters in the system.");
            else
                HE_CORE_ERROR("The specified DXGI adapter {} does not exist.", adapterIndex);
            return false;
        }

        {
            DXGI_ADAPTER_DESC aDesc;
            dxgiAdapter->GetDesc(&aDesc);

            rendererString = GetAdapterName(aDesc);
            m_IsNvidia = IsNvDeviceID(aDesc.VendorId);
        }

        UINT createFlags = 0;
        if (m_DeviceDesc.enableDebugRuntime)
            createFlags |= D3D11_CREATE_DEVICE_DEBUG;

        {
            HE_PROFILE_SCOPE("D3D11CreateDevice");

            const HRESULT hr = D3D11CreateDevice(
                dxgiAdapter, // pAdapter
                D3D_DRIVER_TYPE_UNKNOWN, // DriverType
                nullptr, // Software
                createFlags, // Flags
                &m_DeviceDesc.featureLevel, // pFeatureLevels
                1, // FeatureLevels
                D3D11_SDK_VERSION, // SDKVersion
                &device, // ppDevice
                nullptr, // pFeatureLevel
                &immediateContext // ppImmediateContext
            );

            if (FAILED(hr))
            {
                return false;
            }
        }

        nvrhi::d3d11::DeviceDesc deviceDesc;
        deviceDesc.messageCallback = &HE::DefaultMessageCallback::GetInstance();
        deviceDesc.context = immediateContext;

        nvrhiDevice = nvrhi::d3d11::createDevice(deviceDesc);

        if (m_DeviceDesc.enableNvrhiValidationLayer)
        {
            nvrhiDevice = nvrhi::validation::createValidationLayer(nvrhiDevice);
        }

        return true;
    }

    bool CreateSwapChain(HE::WindowState windowState) override
    {
        HE_PROFILE_FUNCTION();

        hWnd = glfwGetWin32Window((GLFWwindow*)m_Window);

        RECT clientRect;
        GetClientRect(hWnd, &clientRect);
        UINT width = clientRect.right - clientRect.left;
        UINT height = clientRect.bottom - clientRect.top;

        ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
        swapChainDesc.BufferCount = m_DeviceDesc.swapChainBufferCount;
        swapChainDesc.BufferDesc.Width = width;
        swapChainDesc.BufferDesc.Height = height;
        swapChainDesc.BufferDesc.RefreshRate.Numerator = m_DeviceDesc.refreshRate;
        swapChainDesc.BufferDesc.RefreshRate.Denominator = 0;
        swapChainDesc.BufferUsage = m_DeviceDesc.swapChainUsage;
        swapChainDesc.OutputWindow = hWnd;
        swapChainDesc.SampleDesc.Count = m_DeviceDesc.swapChainSampleCount;
        swapChainDesc.SampleDesc.Quality = m_DeviceDesc.swapChainSampleQuality;
        swapChainDesc.Windowed = !windowState.fullscreen;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.Flags = m_DeviceDesc.allowModeSwitch ? DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH : 0;

        // Special processing for sRGB swap chain formats.
        // DXGI will not create a swap chain with an sRGB format, but its contents will be interpreted as sRGB.
        // So we need to use a non-sRGB format here, but store the true sRGB format for later framebuffer creation.
        switch (m_DeviceDesc.swapChainFormat)  // NOLINT(clang-diagnostic-switch-enum)
        {
        case nvrhi::Format::SRGBA8_UNORM:
            swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            break;
        case nvrhi::Format::SBGRA8_UNORM:
            swapChainDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            break;
        default:
            swapChainDesc.BufferDesc.Format = nvrhi::d3d11::convertFormat(m_DeviceDesc.swapChainFormat);
            break;
        }

        HRESULT hr = dxgiFactory->CreateSwapChain(device, &swapChainDesc, &swapChain);

        if (FAILED(hr))
        {
            HE_CORE_ERROR("Failed to create a swap chain, HRESULT = 0x{}", hr);
            return false;
        }

        bool ret = CreateRenderTarget();

        if (!ret)
        {
            return false;
        }

        return true;
    }

    void DestroyDeviceAndSwapChain() override
    {
        HE_PROFILE_FUNCTION();

        m_RhiBackBuffer = nullptr;
        nvrhiDevice = nullptr;

        if (swapChain)
        {
            swapChain->SetFullscreenState(false, nullptr);
        }

        ReleaseRenderTarget();

        swapChain = nullptr;
        immediateContext = nullptr;
        device = nullptr;
    }

    void ResizeSwapChain() override
    {
        HE_PROFILE_FUNCTION();

        ReleaseRenderTarget();

        if (!swapChain)
            return;

        {
            HE_PROFILE_SCOPE("swapChain->ResizeBuffers");

            const HRESULT hr = swapChain->ResizeBuffers(m_DeviceDesc.swapChainBufferCount,
                m_DeviceDesc.backBufferWidth,
                m_DeviceDesc.backBufferHeight,
                swapChainDesc.BufferDesc.Format,
                swapChainDesc.Flags);

            if (FAILED(hr))
            {
                HE_CORE_CRITICAL("ResizeBuffers failed");
            }
        }

        const bool ret = CreateRenderTarget();
        if (!ret)
        {
            HE_CORE_CRITICAL("CreateRenderTarget failed");
        }
    }

    void Shutdown() override
    {
        HE_PROFILE_FUNCTION();

        DeviceManager::Shutdown();

        if (m_DeviceDesc.enableDebugRuntime)
        {
            ReportLiveObjects();
        }
    }

    bool CreateRenderTarget()
    {
        HE_PROFILE_FUNCTION();

        ReleaseRenderTarget();

        const HRESULT hr = swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&D3D11BackBuffer);  // NOLINT(clang-diagnostic-language-extension-token)
        if (FAILED(hr))
        {
            return false;
        }

        nvrhi::TextureDesc textureDesc;
        textureDesc.width = m_DeviceDesc.backBufferWidth;
        textureDesc.height = m_DeviceDesc.backBufferHeight;
        textureDesc.sampleCount = m_DeviceDesc.swapChainSampleCount;
        textureDesc.sampleQuality = m_DeviceDesc.swapChainSampleQuality;
        textureDesc.format = m_DeviceDesc.swapChainFormat;
        textureDesc.debugName = "SwapChainBuffer";
        textureDesc.isRenderTarget = true;
        textureDesc.isUAV = false;

        m_RhiBackBuffer = nvrhiDevice->createHandleForNativeTexture(nvrhi::ObjectTypes::D3D11_Resource, static_cast<ID3D11Resource*>(D3D11BackBuffer.Get()), textureDesc);

        if (FAILED(hr))
        {
            return false;
        }

        return true;
    }

    void ReleaseRenderTarget()
    {
        m_RhiBackBuffer = nullptr;
        D3D11BackBuffer = nullptr;
    }
};

HE::DeviceManager* HE::DeviceManager::CreateD3D11() { return new DeviceManagerDX11(); }

#endif

//////////////////////////////////////////////////////////////////////////
// D3D12
//////////////////////////////////////////////////////////////////////////
#ifdef NVRHI_HAS_D3D12

// Based on NVIDIA Donut framework (MIT License), with slight modifications
// Source: https://github.com/NVIDIA-RTX/Donut/blob/main/src/app/dx12/DeviceManager_DX12.cpp

#define HR_RETURN(hr) if(FAILED(hr)) return false

class DeviceManagerDX12 : public HE::DeviceManager
{
public:

    nvrhi::RefCountPtr<IDXGIFactory2>               dxgiFactory2;
    nvrhi::RefCountPtr<ID3D12Device>                device;
    nvrhi::RefCountPtr<ID3D12CommandQueue>          graphicsQueue;
    nvrhi::RefCountPtr<ID3D12CommandQueue>          computeQueue;
    nvrhi::RefCountPtr<ID3D12CommandQueue>          copyQueue;
    nvrhi::RefCountPtr<IDXGISwapChain3>             swapChain;
    DXGI_SWAP_CHAIN_DESC1							swapChainDesc{};
    DXGI_SWAP_CHAIN_FULLSCREEN_DESC					fullScreenDesc{};
    nvrhi::RefCountPtr<IDXGIAdapter>                dxgiAdapter;
    HWND											hWnd = nullptr;
    bool											tearingSupported = false;
    std::vector<nvrhi::RefCountPtr<ID3D12Resource>> swapChainBuffers;
    std::vector<nvrhi::TextureHandle>				rhiSwapChainBuffers;
    nvrhi::RefCountPtr<ID3D12Fence>					frameFence;
    std::vector<HANDLE>								frameFenceEvents;
    UINT64											frameCount = 1;
    nvrhi::DeviceHandle								nvrhiDevice;
    std::string										rendererString;

    static std::string GetAdapterName(DXGI_ADAPTER_DESC const& aDesc)
    {
        size_t length = wcsnlen(aDesc.Description, _countof(aDesc.Description));

        std::string name;
        name.resize(length);
        WideCharToMultiByte(CP_ACP, 0, aDesc.Description, int(length), name.data(), int(name.size()), nullptr, nullptr);

        return name;
    }

    // Adjust window rect so that it is centred on the given adapter.  Clamps to fit if it's too big.
    static bool MoveWindowOntoAdapter(IDXGIAdapter* targetAdapter, RECT& rect)
    {
        HE_PROFILE_FUNCTION();

        HE_CORE_ASSERT(targetAdapter != NULL);

        HRESULT hres = S_OK;
        unsigned int outputNo = 0;
        while (SUCCEEDED(hres))
        {
            nvrhi::RefCountPtr<IDXGIOutput> pOutput;
            hres = targetAdapter->EnumOutputs(outputNo++, &pOutput);

            if (SUCCEEDED(hres) && pOutput)
            {
                DXGI_OUTPUT_DESC OutputDesc;
                pOutput->GetDesc(&OutputDesc);
                const RECT desktop = OutputDesc.DesktopCoordinates;
                const int centreX = (int)desktop.left + (int)(desktop.right - desktop.left) / 2;
                const int centreY = (int)desktop.top + (int)(desktop.bottom - desktop.top) / 2;
                const int winW = rect.right - rect.left;
                const int winH = rect.bottom - rect.top;
                const int left = centreX - winW / 2;
                const int right = left + winW;
                const int top = centreY - winH / 2;
                const int bottom = top + winH;
                rect.left = std::max(left, (int)desktop.left);
                rect.right = std::min(right, (int)desktop.right);
                rect.bottom = std::min(bottom, (int)desktop.bottom);
                rect.top = std::max(top, (int)desktop.top);

                // If there is more than one output, go with the first found.  Multi-monitor support could go here.
                return true;
            }
        }

        return false;
    }

    static bool IsNvDeviceID(UINT id) { return id == 0x10DE; }

    const char* GetRendererString() const override { return rendererString.c_str(); }
    nvrhi::IDevice* GetDevice() const override { return nvrhiDevice; }
    nvrhi::ITexture* GetCurrentBackBuffer() override { return rhiSwapChainBuffers[swapChain->GetCurrentBackBufferIndex()]; }
    uint32_t GetCurrentBackBufferIndex() override { return swapChain->GetCurrentBackBufferIndex(); }
    uint32_t GetBackBufferCount() override { return swapChainDesc.BufferCount; }
    nvrhi::GraphicsAPI GetGraphicsAPI() const override { return nvrhi::GraphicsAPI::D3D12; }

    void ReportLiveObjects() override
    {
        HE_PROFILE_FUNCTION();

        nvrhi::RefCountPtr<IDXGIDebug> pDebug;
        DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug));

        if (pDebug)
        {
            DXGI_DEBUG_RLO_FLAGS flags = (DXGI_DEBUG_RLO_FLAGS)(DXGI_DEBUG_RLO_IGNORE_INTERNAL | DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_DETAIL);
            HRESULT hr = pDebug->ReportLiveObjects(DXGI_DEBUG_ALL, flags);
            if (FAILED(hr))
            {
                HE_CORE_ERROR("ReportLiveObjects failed, HRESULT = 0x{}", hr);
            }
        }
    }

    bool EnumerateAdapters(std::vector<HE::AdapterInfo>& outAdapters) override
    {
        HE_PROFILE_FUNCTION();

        if (!dxgiFactory2)
            return false;

        outAdapters.clear();

        while (true)
        {
            nvrhi::RefCountPtr<IDXGIAdapter> adapter;
            HRESULT hr = dxgiFactory2->EnumAdapters(uint32_t(outAdapters.size()), &adapter);
            if (FAILED(hr))
                return true;

            DXGI_ADAPTER_DESC desc;
            hr = adapter->GetDesc(&desc);
            if (FAILED(hr))
                return false;

            HE::AdapterInfo adapterInfo;

            adapterInfo.name = GetAdapterName(desc);
            adapterInfo.dxgiAdapter = adapter;
            adapterInfo.vendorID = desc.VendorId;
            adapterInfo.deviceID = desc.DeviceId;
            adapterInfo.dedicatedVideoMemory = desc.DedicatedVideoMemory;

            HE::AdapterInfo::LUID luid;
            static_assert(luid.size() == sizeof(desc.AdapterLuid));
            memcpy(luid.data(), &desc.AdapterLuid, luid.size());
            adapterInfo.luid = luid;

            outAdapters.push_back(std::move(adapterInfo));
        }
    }

    bool CreateInstanceInternal() override
    {
        HE_PROFILE_FUNCTION();

        if (!dxgiFactory2)
        {
            HRESULT hres = CreateDXGIFactory2(m_DeviceDesc.enableDebugRuntime ? DXGI_CREATE_FACTORY_DEBUG : 0, IID_PPV_ARGS(&dxgiFactory2));
            if (hres != S_OK)
            {
                HE_CORE_ERROR("ERROR in CreateDXGIFactory2.\n"
                    "For more info, get log from debug D3D runtime: (1) Install DX SDK, and enable Debug D3D from DX Control Panel Utility. (2) Install and start DbgView. (3) Try running the program again.\n");
                return false;
            }
        }

        return true;
    }

    bool CreateDevice() override
    {
        HE_PROFILE_SCOPE("Create D12 Device");

        if (m_DeviceDesc.enableDebugRuntime)
        {
            nvrhi::RefCountPtr<ID3D12Debug> pDebug;
            HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(&pDebug));

            if (SUCCEEDED(hr))
                pDebug->EnableDebugLayer();
            else
                HE_CORE_WARN("Cannot enable DX12 debug runtime, ID3D12Debug is not available.");
        }

        if (m_DeviceDesc.enableGPUValidation)
        {
            nvrhi::RefCountPtr<ID3D12Debug3> debugController3;
            HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debugController3));

            if (SUCCEEDED(hr))
                debugController3->SetEnableGPUBasedValidation(true);
            else
                HE_CORE_WARN("Cannot enable GPU-based validation, ID3D12Debug3 is not available.");
        }

        int adapterIndex = m_DeviceDesc.adapterIndex;
        if (adapterIndex < 0)
            adapterIndex = 0;

        if (FAILED(dxgiFactory2->EnumAdapters(adapterIndex, &dxgiAdapter)))
        {
            if (adapterIndex == 0)
                HE_CORE_ERROR("Cannot find any DXGI adapters in the system.");
            else
                HE_CORE_ERROR("The specified DXGI adapter {} does not exist.", adapterIndex);

            return false;
        }

        {
            DXGI_ADAPTER_DESC aDesc;
            dxgiAdapter->GetDesc(&aDesc);

            rendererString = GetAdapterName(aDesc);
            m_IsNvidia = IsNvDeviceID(aDesc.VendorId);
        }


        HRESULT hr = D3D12CreateDevice(
            dxgiAdapter,
            m_DeviceDesc.featureLevel,
            IID_PPV_ARGS(&device));

        if (FAILED(hr))
        {
            HE_CORE_ERROR("D3D12CreateDevice failed, error code = 0x{}08x", hr);
            return false;
        }

        if (m_DeviceDesc.enableDebugRuntime)
        {
            nvrhi::RefCountPtr<ID3D12InfoQueue> pInfoQueue;
            device->QueryInterface(&pInfoQueue);

            if (pInfoQueue)
            {
#ifdef HE_DEBUG
                if (m_DeviceDesc.enableWarningsAsErrors)
                    pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
                pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
                pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
#endif

                D3D12_MESSAGE_ID disableMessageIDs[] = {
                    D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
                    D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
                    D3D12_MESSAGE_ID_COMMAND_LIST_STATIC_DESCRIPTOR_RESOURCE_DIMENSION_MISMATCH, // descriptor validation doesn't understand acceleration structures
                };

                D3D12_INFO_QUEUE_FILTER filter = {};
                filter.DenyList.pIDList = disableMessageIDs;
                filter.DenyList.NumIDs = sizeof(disableMessageIDs) / sizeof(disableMessageIDs[0]);
                pInfoQueue->AddStorageFilterEntries(&filter);
            }
        }

        D3D12_COMMAND_QUEUE_DESC queueDesc;
        ZeroMemory(&queueDesc, sizeof(queueDesc));
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        queueDesc.NodeMask = 1;
        hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&graphicsQueue));
        HR_RETURN(hr);
        graphicsQueue->SetName(L"Graphics Queue");

        if (m_DeviceDesc.enableComputeQueue)
        {
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
            hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&computeQueue));
            HR_RETURN(hr);
            computeQueue->SetName(L"Compute Queue");
        }

        if (m_DeviceDesc.enableCopyQueue)
        {
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
            hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&copyQueue));
            HR_RETURN(hr);
            copyQueue->SetName(L"Copy Queue");
        }

        nvrhi::d3d12::DeviceDesc deviceDesc;
        deviceDesc.errorCB = &HE::DefaultMessageCallback::GetInstance();
        deviceDesc.pDevice = device;
        deviceDesc.pGraphicsCommandQueue = graphicsQueue;
        deviceDesc.pComputeCommandQueue = computeQueue;
        deviceDesc.pCopyCommandQueue = copyQueue;

        nvrhiDevice = nvrhi::d3d12::createDevice(deviceDesc);

        if (m_DeviceDesc.enableNvrhiValidationLayer)
        {
            nvrhiDevice = nvrhi::validation::createValidationLayer(nvrhiDevice);
        }

        return true;
    }

    bool CreateSwapChain(HE::WindowState windowState) override
    {
        HE_PROFILE_FUNCTION();

        hWnd = glfwGetWin32Window((GLFWwindow*)m_Window);

        HRESULT hr = E_FAIL;

        RECT clientRect;
        GetClientRect(hWnd, &clientRect);
        UINT width = clientRect.right - clientRect.left;
        UINT height = clientRect.bottom - clientRect.top;

        ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
        swapChainDesc.Width = width;
        swapChainDesc.Height = height;
        swapChainDesc.SampleDesc.Count = m_DeviceDesc.swapChainSampleCount;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.BufferUsage = m_DeviceDesc.swapChainUsage;
        swapChainDesc.BufferCount = m_DeviceDesc.swapChainBufferCount;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.Flags = m_DeviceDesc.allowModeSwitch ? DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH : 0;

        // Special processing for sRGB swap chain formats.
        // DXGI will not create a swap chain with an sRGB format, but its contents will be interpreted as sRGB.
        // So we need to use a non-sRGB format here, but store the true sRGB format for later framebuffer creation.
        switch (m_DeviceDesc.swapChainFormat)  // NOLINT(clang-diagnostic-switch-enum)
        {
        case nvrhi::Format::SRGBA8_UNORM:
            swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            break;
        case nvrhi::Format::SBGRA8_UNORM:
            swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            break;
        default:
            swapChainDesc.Format = nvrhi::d3d12::convertFormat(m_DeviceDesc.swapChainFormat);
            break;
        }

        nvrhi::RefCountPtr<IDXGIFactory5> pDxgiFactory5;
        if (SUCCEEDED(dxgiFactory2->QueryInterface(IID_PPV_ARGS(&pDxgiFactory5))))
        {
            BOOL supported = 0;
            if (SUCCEEDED(pDxgiFactory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &supported, sizeof(supported))))
                tearingSupported = (supported != 0);
        }

        if (tearingSupported)
        {
            swapChainDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
        }

        fullScreenDesc = {};
        fullScreenDesc.RefreshRate.Numerator = m_DeviceDesc.refreshRate;
        fullScreenDesc.RefreshRate.Denominator = 1;
        fullScreenDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
        fullScreenDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
        fullScreenDesc.Windowed = !windowState.fullscreen;

        nvrhi::RefCountPtr<IDXGISwapChain1> pSwapChain1;
        hr = dxgiFactory2->CreateSwapChainForHwnd(graphicsQueue, hWnd, &swapChainDesc, &fullScreenDesc, nullptr, &pSwapChain1);
        HR_RETURN(hr);

        hr = pSwapChain1->QueryInterface(IID_PPV_ARGS(&swapChain));
        HR_RETURN(hr);

        if (!CreateRenderTargets())
            return false;

        hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&frameFence));
        HR_RETURN(hr);

        for (UINT bufferIndex = 0; bufferIndex < swapChainDesc.BufferCount; bufferIndex++)
        {
            frameFenceEvents.push_back(CreateEvent(nullptr, false, true, nullptr));
        }

        return true;
    }

    void DestroyDeviceAndSwapChain() override
    {
        HE_PROFILE_FUNCTION();

        rhiSwapChainBuffers.clear();
        rendererString.clear();

        ReleaseRenderTargets();

        nvrhiDevice = nullptr;

        for (auto fenceEvent : frameFenceEvents)
        {
            WaitForSingleObject(fenceEvent, INFINITE);
            CloseHandle(fenceEvent);
        }

        frameFenceEvents.clear();

        if (swapChain)
        {
            swapChain->SetFullscreenState(false, nullptr);
        }

        swapChainBuffers.clear();

        frameFence = nullptr;
        swapChain = nullptr;
        graphicsQueue = nullptr;
        computeQueue = nullptr;
        copyQueue = nullptr;
        device = nullptr;
    }

    void ResizeSwapChain() override
    {
        HE_PROFILE_FUNCTION();

        ReleaseRenderTargets();

        if (!nvrhiDevice)
            return;

        if (!swapChain)
            return;

        {
            HE_PROFILE_SCOPE("swapChain->ResizeBuffers");

            const HRESULT hr = swapChain->ResizeBuffers(
                m_DeviceDesc.swapChainBufferCount,
                m_DeviceDesc.backBufferWidth,
                m_DeviceDesc.backBufferHeight,
                swapChainDesc.Format,
                swapChainDesc.Flags
            );

            if (FAILED(hr))
            {
                HE_CORE_ERROR("ResizeBuffers failed");
            }
        }

        bool ret = CreateRenderTargets();
        if (!ret)
        {
            HE_CORE_ERROR("CreateRenderTarget failed");
        }
    }

    nvrhi::ITexture* GetBackBuffer(uint32_t index) override
    {
        if (index < rhiSwapChainBuffers.size())
            return rhiSwapChainBuffers[index];
        return nullptr;
    }

    bool BeginFrame() override
    {
        HE_PROFILE_FUNCTION();

        DXGI_SWAP_CHAIN_DESC1 newSwapChainDesc;
        DXGI_SWAP_CHAIN_FULLSCREEN_DESC newFullScreenDesc;
        if (SUCCEEDED(swapChain->GetDesc1(&newSwapChainDesc)) && SUCCEEDED(swapChain->GetFullscreenDesc(&newFullScreenDesc)))
        {
            if (fullScreenDesc.Windowed != newFullScreenDesc.Windowed)
            {
                BackBufferResizing();

                fullScreenDesc = newFullScreenDesc;
                swapChainDesc = newSwapChainDesc;
                m_DeviceDesc.backBufferWidth = newSwapChainDesc.Width;
                m_DeviceDesc.backBufferHeight = newSwapChainDesc.Height;

                if (newFullScreenDesc.Windowed)
                    glfwSetWindowMonitor((GLFWwindow*)m_Window, nullptr, 50, 50, newSwapChainDesc.Width, newSwapChainDesc.Height, 0);

                ResizeSwapChain();
                BackBufferResized();
            }
        }

        auto bufferIndex = swapChain->GetCurrentBackBufferIndex();

        WaitForSingleObject(frameFenceEvents[bufferIndex], INFINITE);

        return true;
    }

    void Present() override
    {
        HE_PROFILE_FUNCTION();

        auto bufferIndex = swapChain->GetCurrentBackBufferIndex();

        UINT presentFlags = 0;
        if (!m_DeviceDesc.vsyncEnabled && fullScreenDesc.Windowed && tearingSupported)
            presentFlags |= DXGI_PRESENT_ALLOW_TEARING;

        swapChain->Present(m_DeviceDesc.vsyncEnabled ? 1 : 0, presentFlags);

        frameFence->SetEventOnCompletion(frameCount, frameFenceEvents[bufferIndex]);
        graphicsQueue->Signal(frameFence, frameCount);
        frameCount++;
    }

    void Shutdown() override
    {
        HE_PROFILE_FUNCTION();

        DeviceManager::Shutdown();

        dxgiAdapter = nullptr;
        dxgiFactory2 = nullptr;

        if (m_DeviceDesc.enableDebugRuntime)
        {
            ReportLiveObjects();
        }
    }

    bool CreateRenderTargets()
    {
        HE_PROFILE_FUNCTION();

        swapChainBuffers.resize(swapChainDesc.BufferCount);
        rhiSwapChainBuffers.resize(swapChainDesc.BufferCount);

        for (UINT n = 0; n < swapChainDesc.BufferCount; n++)
        {
            const HRESULT hr = swapChain->GetBuffer(n, IID_PPV_ARGS(&swapChainBuffers[n]));
            HR_RETURN(hr);

            nvrhi::TextureDesc textureDesc;
            textureDesc.width = m_DeviceDesc.backBufferWidth;
            textureDesc.height = m_DeviceDesc.backBufferHeight;
            textureDesc.sampleCount = m_DeviceDesc.swapChainSampleCount;
            textureDesc.sampleQuality = m_DeviceDesc.swapChainSampleQuality;
            textureDesc.format = m_DeviceDesc.swapChainFormat;
            textureDesc.debugName = "SwapChainBuffer";
            textureDesc.isRenderTarget = true;
            textureDesc.isUAV = false;
            textureDesc.initialState = nvrhi::ResourceStates::Present;
            textureDesc.keepInitialState = true;

            rhiSwapChainBuffers[n] = nvrhiDevice->createHandleForNativeTexture(nvrhi::ObjectTypes::D3D12_Resource, nvrhi::Object(swapChainBuffers[n]), textureDesc);
        }

        return true;
    }

    void ReleaseRenderTargets()
    {
        HE_PROFILE_FUNCTION();

        if (nvrhiDevice)
        {
            // Make sure that all frames have finished rendering
            nvrhiDevice->waitForIdle();

            // Release all in-flight references to the render targets
            nvrhiDevice->runGarbageCollection();
        }


        // Set the events so that WaitForSingleObject in OneFrame will not hang later
        for (auto e : frameFenceEvents)
            SetEvent(e);

        // Release the old buffers because ResizeBuffers requires that
        rhiSwapChainBuffers.clear();
        swapChainBuffers.clear();
    }
};

HE::DeviceManager* HE::DeviceManager::CreateD3D12() { return new DeviceManagerDX12(); }

#undef HR_RETURN

#endif

//////////////////////////////////////////////////////////////////////////
// OS
//////////////////////////////////////////////////////////////////////////
#pragma region OS

using NativeHandleType = HINSTANCE;
using NativeSymbolType = FARPROC;

void* HE::Modules::SharedLib::Open(const char* path) noexcept
{
    HE_PROFILE_FUNCTION();

    return LoadLibraryA(path);
}

void* HE::Modules::SharedLib::GetSymbolAddress(void* handle, const char* name) noexcept
{
    HE_PROFILE_FUNCTION();

    return (NativeSymbolType)GetProcAddress((NativeHandleType)handle, name);
}

void HE::Modules::SharedLib::Close(void* handle) noexcept
{
    HE_PROFILE_FUNCTION();

    FreeLibrary((NativeHandleType)handle);
}

std::string HE::Modules::SharedLib::GetError() noexcept
{
    constexpr const size_t bufferSize = 512;
    auto error_code = GetLastError();
    if (!error_code) return "Unknown error (GetLastError failed)";
    char description[512];
    auto lang = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
    const DWORD length = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, error_code, lang, description, bufferSize, nullptr);
    return (length == 0) ? "Unknown error (FormatMessage failed)" : description;
}


bool HE::FileSystem::Open(const std::filesystem::path& path)
{
    return (INT_PTR)::ShellExecuteW(NULL, L"open", path.c_str(), NULL, NULL, SW_SHOWDEFAULT) > 32;
}


void HE::OS::SetEnvVar(const char* var, const char* value)
{
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Environment", 0, KEY_WRITE, &hKey) == ERROR_SUCCESS)
    {
        if (RegSetValueExA(hKey, var, 0, REG_SZ, (const BYTE*)value, (DWORD)strlen(value) + 1) != ERROR_SUCCESS)
        {
            HE_CORE_ERROR("Failed to set environment variable {}", var);
        }
        RegCloseKey(hKey);
    }
    else
    {
        HE_CORE_ERROR("Failed to open registry key!");
    }
}

void HE::OS::RemoveEnvVar(const char* var)
{
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Environment", 0, KEY_WRITE, &hKey) == ERROR_SUCCESS)
    {
        if (RegDeleteValueA(hKey, var) != ERROR_SUCCESS)
        {
            HE_CORE_ERROR("Failed to remove environment variable {}", var);
        }
        RegCloseKey(hKey);
    }
    else
    {
        HE_CORE_ERROR("Failed to open registry key!");
    }
}

#pragma endregion
