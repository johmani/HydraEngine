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

struct DX11SwapChain : public HE::SwapChain
{
    nvrhi::RefCountPtr<IDXGISwapChain>  swapChain;
    DXGI_SWAP_CHAIN_DESC                swapChainDesc{};
    nvrhi::TextureHandle                rhiBackBuffer;
    nvrhi::RefCountPtr<ID3D11Texture2D> D3D11BackBuffer;

    ~DX11SwapChain()
    {
        HE_PROFILE_FUNCTION();
        HE_TRACE("DestroySwapChain");

        rhiBackBuffer = nullptr;

        if (swapChain)
        {
            swapChain->SetFullscreenState(false, nullptr);
        }

        ReleaseRenderTarget();

        swapChain = nullptr;
    }

    nvrhi::ITexture* GetCurrentBackBuffer() override        { return rhiBackBuffer;                          }
    nvrhi::ITexture* GetBackBuffer(uint32_t index) override { return (index == 0) ? rhiBackBuffer : nullptr; }
    uint32_t GetCurrentBackBufferIndex() override           { return 0;                                      }
    uint32_t GetBackBufferCount() override                  { return 1;                                      }

    bool Present() override;

    void ResizeSwapChain(uint32_t width, uint32_t height) override;

    bool BeginFrame() override;

    bool CreateRenderTarget(uint32_t width, uint32_t height);

    void ReleaseRenderTarget();
};

struct DX11DeviceManager : public HE::RHI::DeviceManager
{
    nvrhi::RefCountPtr<IDXGIFactory1>       dxgiFactory;
    nvrhi::RefCountPtr<IDXGIAdapter>        dxgiAdapter;
    nvrhi::RefCountPtr<ID3D11Device>        device;
    nvrhi::RefCountPtr<ID3D11DeviceContext> immediateContext;
    nvrhi::DeviceHandle                     nvrhiDevice;
    std::string                             rendererString;

    ~DX11DeviceManager();
    const char* GetRendererString() const override { return rendererString.c_str(); }
    nvrhi::IDevice* GetDevice() const override { return nvrhiDevice; }
    HE::SwapChain* CreateSwapChain(const HE::SwapChainDesc& swapChainDesc, void* windowHandle) override;
    void ReportLiveObjects() override;
    bool EnumerateAdapters(std::vector<HE::RHI::AdapterInfo>& outAdapters) override;
    bool CreateInstanceInternal() override;
    bool CreateDevice() override;

    static std::string GetAdapterName(DXGI_ADAPTER_DESC const& aDesc);
    static bool IsNvDeviceID(UINT id) { return id == 0x10DE; }
};


//////////////////////////////////////////////////////////////////////////
// DX11DeviceManager
//////////////////////////////////////////////////////////////////////////

DX11DeviceManager::~DX11DeviceManager()
{
    HE_PROFILE_FUNCTION();

    immediateContext = nullptr;
    nvrhiDevice = nullptr;
    device = nullptr;
    instanceCreated = false;

    if (desc.enableDebugRuntime)
    {
        ReportLiveObjects();
    }
}

HE::SwapChain* DX11DeviceManager::CreateSwapChain(const HE::SwapChainDesc& swapChainDesc, void* windowHandle)
{
    HE_PROFILE_FUNCTION();

    auto hWnd = glfwGetWin32Window((GLFWwindow*)windowHandle);

    RECT clientRect;
    GetClientRect(hWnd, &clientRect);
    UINT width = clientRect.right - clientRect.left;
    UINT height = clientRect.bottom - clientRect.top;

    DX11SwapChain* dx11SwapChain = new DX11SwapChain();
    ZeroMemory(&dx11SwapChain->swapChainDesc, sizeof(dx11SwapChain->swapChainDesc));

    dx11SwapChain->desc = swapChainDesc;
    dx11SwapChain->desc.backBufferWidth = width;
    dx11SwapChain->desc.backBufferHeight = height;
    dx11SwapChain->windowHandle = windowHandle;
    dx11SwapChain->nvrhiDevice = nvrhiDevice;
    dx11SwapChain->swapChainDesc.BufferCount = swapChainDesc.swapChainBufferCount;
    dx11SwapChain->swapChainDesc.BufferDesc.Width = width;
    dx11SwapChain->swapChainDesc.BufferDesc.Height = height;
    dx11SwapChain->swapChainDesc.BufferDesc.RefreshRate.Numerator = swapChainDesc.refreshRate;
    dx11SwapChain->swapChainDesc.BufferDesc.RefreshRate.Denominator = 0;
    dx11SwapChain->swapChainDesc.BufferUsage = swapChainDesc.swapChainUsage;
    dx11SwapChain->swapChainDesc.OutputWindow = hWnd;
    dx11SwapChain->swapChainDesc.SampleDesc.Count = swapChainDesc.swapChainSampleCount;
    dx11SwapChain->swapChainDesc.SampleDesc.Quality = swapChainDesc.swapChainSampleQuality;
    dx11SwapChain->swapChainDesc.Windowed = !glfwGetWindowMonitor((GLFWwindow*)windowHandle);
    dx11SwapChain->swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    dx11SwapChain->swapChainDesc.Flags = swapChainDesc.allowModeSwitch ? DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH : 0;

    // Special processing for sRGB swap chain formats.
    // DXGI will not create a swap chain with an sRGB format, but its contents will be interpreted as sRGB.
    // So we need to use a non-sRGB format here, but store the true sRGB format for later framebuffer creation.
    switch (swapChainDesc.swapChainFormat)  // NOLINT(clang-diagnostic-switch-enum)
    {
    case nvrhi::Format::SRGBA8_UNORM:
        dx11SwapChain->swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        break;
    case nvrhi::Format::SBGRA8_UNORM:
        dx11SwapChain->swapChainDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        break;
    default:
        dx11SwapChain->swapChainDesc.BufferDesc.Format = nvrhi::d3d11::convertFormat(swapChainDesc.swapChainFormat);
        break;
    }

    HRESULT hr = dxgiFactory->CreateSwapChain(device, &dx11SwapChain->swapChainDesc, &dx11SwapChain->swapChain);

    if (FAILED(hr))
    {
        HE_CORE_ERROR("Failed to create a swap chain, HRESULT = 0x{}", hr);
        return nullptr;
    }

    bool ret = dx11SwapChain->CreateRenderTarget(width, height);

    if (!ret)
    {
        return nullptr;
    }

    dx11SwapChain->ResizeBackBuffers();

    return dx11SwapChain;
}

void DX11DeviceManager::ReportLiveObjects()
{
    HE_PROFILE_FUNCTION();

    nvrhi::RefCountPtr<IDXGIDebug> pDebug;
    DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug));

    if (pDebug)
        pDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_DETAIL);
}

bool DX11DeviceManager::EnumerateAdapters(std::vector<HE::RHI::AdapterInfo>& outAdapters)
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

        HE::RHI::AdapterInfo adapterInfo;

        adapterInfo.name = GetAdapterName(desc);
        HE_TRACE("adapterInfo {}", adapterInfo.name);
        adapterInfo.dxgiAdapter = adapter;
        adapterInfo.vendorID = desc.VendorId;
        adapterInfo.deviceID = desc.DeviceId;
        adapterInfo.dedicatedVideoMemory = desc.DedicatedVideoMemory;

        HE::RHI::AdapterInfo::LUID luid;
        static_assert(luid.size() == sizeof(desc.AdapterLuid));
        memcpy(luid.data(), &desc.AdapterLuid, luid.size());
        adapterInfo.luid = luid;

        outAdapters.push_back(std::move(adapterInfo));
    }
}

bool DX11DeviceManager::CreateInstanceInternal()
{
    HE_PROFILE_FUNCTION();

    if (!dxgiFactory)
    {
        HRESULT hres = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
        if (hres != S_OK)
        {
            HE_CORE_ERROR("CreateInstanceInternal : CreateDXGIFactory1 : For more info, get log from debug D3D runtime: (1) Install DX SDK, and enable Debug D3D from DX Control Panel Utility. (2) Install and start DbgView. (3) Try running the program again.");
            return false;
        }
    }

    return true;
}

bool DX11DeviceManager::CreateDevice()
{
    HE_PROFILE_SCOPE("Create D11 Device");

    int adapterIndex = desc.adapterIndex;
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
        isNvidia = IsNvDeviceID(aDesc.VendorId);
        HE_CORE_INFO("Adapter {}", rendererString);
    }

    UINT createFlags = 0;
    if (desc.enableDebugRuntime)
        createFlags |= D3D11_CREATE_DEVICE_DEBUG;

    {
        HE_PROFILE_SCOPE("D3D11CreateDevice");

        const HRESULT hr = D3D11CreateDevice(
            dxgiAdapter,             // pAdapter
            D3D_DRIVER_TYPE_UNKNOWN, // DriverType
            nullptr,                 // Software
            createFlags,             // Flags
            &desc.featureLevel,      // pFeatureLevels
            1,                       // FeatureLevels
            D3D11_SDK_VERSION,       // SDKVersion
            &device,                 // ppDevice
            nullptr,                 // pFeatureLevel
            &immediateContext        // ppImmediateContext
        );

        if (FAILED(hr))
        {
            return false;
        }
    }

    nvrhi::d3d11::DeviceDesc deviceDesc;
    deviceDesc.messageCallback = &HE::RHI::DefaultMessageCallback::GetInstance();
    deviceDesc.context = immediateContext;

    nvrhiDevice = nvrhi::d3d11::createDevice(deviceDesc);

    if (desc.enableNvrhiValidationLayer)
    {
        nvrhiDevice = nvrhi::validation::createValidationLayer(nvrhiDevice);
    }

    return true;
}

std::string DX11DeviceManager::GetAdapterName(DXGI_ADAPTER_DESC const& aDesc)
{
    size_t length = wcsnlen(aDesc.Description, _countof(aDesc.Description));

    std::string name;
    name.resize(length);
    WideCharToMultiByte(CP_ACP, 0, aDesc.Description, int(length), name.data(), int(name.size()), nullptr, nullptr);

    return name;
}

HE::RHI::DeviceManager* HE::RHI::CreateD3D11() { return new DX11DeviceManager(); }

//////////////////////////////////////////////////////////////////////////
// DX11SwapChain
//////////////////////////////////////////////////////////////////////////

bool DX11SwapChain::Present()
{
    HE_PROFILE_FUNCTION();

    HRESULT result = swapChain->Present(desc.vsync ? 1 : 0, 0);
    nvrhiDevice->runGarbageCollection();

    return SUCCEEDED(result);
}

void DX11SwapChain::ResizeSwapChain(uint32_t width, uint32_t height)
{
    HE_PROFILE_FUNCTION();

    ResetBackBuffers();
    ReleaseRenderTarget();

    if (!swapChain)
        return;

    {
        HE_PROFILE_SCOPE("swapChain->ResizeBuffers");

        const HRESULT hr = swapChain->ResizeBuffers(
            desc.swapChainBufferCount,
            width,
            height,
            swapChainDesc.BufferDesc.Format,
            swapChainDesc.Flags
        );

        desc.backBufferWidth = width;
        desc.backBufferHeight = height;

        if (FAILED(hr))
        {
            HE_CORE_CRITICAL("ResizeBuffers failed, {}, {}", width, height);
        }
    }

    const bool ret = CreateRenderTarget(width, height);
    if (!ret)
    {
        HE_CORE_CRITICAL("CreateRenderTarget failed");
    }

    ResizeBackBuffers();
}

bool DX11SwapChain::BeginFrame()
{
    HE_PROFILE_FUNCTION();

    DXGI_SWAP_CHAIN_DESC newSwapChainDesc;
    if (SUCCEEDED(swapChain->GetDesc(&newSwapChainDesc)))
    {
        if (swapChainDesc.Windowed != newSwapChainDesc.Windowed)
        {
            swapChainDesc = newSwapChainDesc;
            ResizeSwapChain(desc.backBufferWidth, desc.backBufferHeight);
        }
    }

    return true;
}

bool DX11SwapChain::CreateRenderTarget(uint32_t width, uint32_t height)
{
    HE_PROFILE_FUNCTION();

    ReleaseRenderTarget();

    const HRESULT hr = swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&D3D11BackBuffer);  // NOLINT(clang-diagnostic-language-extension-token)
    if (FAILED(hr))
    {
        return false;
    }

    nvrhi::TextureDesc textureDesc;
    textureDesc.width = width;
    textureDesc.height = height;
    textureDesc.sampleCount = desc.swapChainSampleCount;
    textureDesc.sampleQuality = desc.swapChainSampleQuality;
    textureDesc.format = desc.swapChainFormat;
    textureDesc.debugName = "SwapChainBuffer";
    textureDesc.isRenderTarget = true;
    textureDesc.isUAV = false;

    rhiBackBuffer = nvrhiDevice->createHandleForNativeTexture(nvrhi::ObjectTypes::D3D11_Resource, static_cast<ID3D11Resource*>(D3D11BackBuffer.Get()), textureDesc);

    if (FAILED(hr))
    {
        return false;
    }

    return true;
}

void DX11SwapChain::ReleaseRenderTarget()
{
    rhiBackBuffer = nullptr;
    D3D11BackBuffer = nullptr;
}

#endif

//////////////////////////////////////////////////////////////////////////
// D3D12
//////////////////////////////////////////////////////////////////////////
#ifdef NVRHI_HAS_D3D12

struct DX12DeviceManager : public HE::RHI::DeviceManager
{
    nvrhi::RefCountPtr<IDXGIFactory2>      dxgiFactory2;
    nvrhi::RefCountPtr<ID3D12Device>       device;
    nvrhi::RefCountPtr<ID3D12CommandQueue> graphicsQueue;
    nvrhi::RefCountPtr<ID3D12CommandQueue> computeQueue;
    nvrhi::RefCountPtr<ID3D12CommandQueue> copyQueue;
    nvrhi::RefCountPtr<IDXGIAdapter>       dxgiAdapter;
    nvrhi::DeviceHandle                    nvrhiDevice;
    std::string                            rendererString;

    ~DX12DeviceManager();
    HE::SwapChain* CreateSwapChain(const HE::SwapChainDesc& swapChainDesc, void* windowHandle) override;
    const char* GetRendererString() const override;
    nvrhi::IDevice* GetDevice() const override;
    void ReportLiveObjects() override;
    bool EnumerateAdapters(std::vector<HE::RHI::AdapterInfo>& outAdapters) override;
    bool CreateInstanceInternal() override;
    bool CreateDevice() override;
    
    static bool IsNvDeviceID(UINT id);
    static std::string GetAdapterName(DXGI_ADAPTER_DESC const& aDesc);
};

struct DX12SwapChain : public HE::SwapChain
{
    nvrhi::RefCountPtr<IDXGISwapChain3>             swapChain;
    DXGI_SWAP_CHAIN_DESC1                           swapChainDesc{};
    DXGI_SWAP_CHAIN_FULLSCREEN_DESC                 fullScreenDesc{};
    HWND                                            hWnd = nullptr;
    std::vector<nvrhi::RefCountPtr<ID3D12Resource>> swapChainBuffers;
    std::vector<nvrhi::TextureHandle>               rhiSwapChainBuffers;
    nvrhi::RefCountPtr<ID3D12Fence>	                frameFence;
    std::vector<HANDLE>                             frameFenceEvents;
    UINT64                                          frameCount = 1;
    bool                                            tearingSupported = false;
    DX12DeviceManager*                              dx12deviceManager = nullptr;

    ~DX12SwapChain();
    nvrhi::ITexture* GetCurrentBackBuffer() override { return rhiSwapChainBuffers[swapChain->GetCurrentBackBufferIndex()]; }
    uint32_t GetCurrentBackBufferIndex() override { return swapChain->GetCurrentBackBufferIndex(); }
    uint32_t GetBackBufferCount() override { return swapChainDesc.BufferCount; }
    void ResizeSwapChain(uint32_t width, uint32_t height) override;
    nvrhi::ITexture* GetBackBuffer(uint32_t index) override;
    bool BeginFrame() override;
    bool Present() override;
    bool CreateRenderTargets(uint32_t width, uint32_t height);
    void ReleaseRenderTargets();
};

//////////////////////////////////////////////////////////////////////////
// DX12DeviceManager
//////////////////////////////////////////////////////////////////////////

DX12DeviceManager::~DX12DeviceManager()
{
    HE_PROFILE_FUNCTION();

    dxgiAdapter = nullptr;
    dxgiFactory2 = nullptr;

    if (desc.enableDebugRuntime)
    {
        ReportLiveObjects();
    }

    rendererString.clear();
    nvrhiDevice = nullptr;
    graphicsQueue = nullptr;
    computeQueue = nullptr;
    copyQueue = nullptr;
    device = nullptr;
    instanceCreated = false;
}

HE::SwapChain* DX12DeviceManager::CreateSwapChain(const HE::SwapChainDesc& swapChainDesc, void* windowHandle)
{
    HE_PROFILE_FUNCTION();

    auto hWnd = glfwGetWin32Window((GLFWwindow*)windowHandle);

    DX12SwapChain* dx12SwapChain = new DX12SwapChain();

    HRESULT hr = E_FAIL;

    RECT clientRect;
    GetClientRect(hWnd, &clientRect);
    UINT width = clientRect.right - clientRect.left;
    UINT height = clientRect.bottom - clientRect.top;

    dx12SwapChain->desc = swapChainDesc;
    dx12SwapChain->desc.backBufferWidth = width;
    dx12SwapChain->desc.backBufferHeight = height;
    dx12SwapChain->windowHandle = windowHandle;
    dx12SwapChain->nvrhiDevice = nvrhiDevice;
    dx12SwapChain->dx12deviceManager = this;
    dx12SwapChain->fullScreenDesc = {};
    dx12SwapChain->fullScreenDesc.RefreshRate.Numerator = swapChainDesc.refreshRate;
    dx12SwapChain->fullScreenDesc.RefreshRate.Denominator = 1;
    dx12SwapChain->fullScreenDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
    dx12SwapChain->fullScreenDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    dx12SwapChain->fullScreenDesc.Windowed = !glfwGetWindowMonitor((GLFWwindow*)windowHandle);

    ZeroMemory(&dx12SwapChain->swapChainDesc, sizeof(dx12SwapChain->swapChainDesc));
    dx12SwapChain->swapChainDesc.Width = width;
    dx12SwapChain->swapChainDesc.Height = height;
    dx12SwapChain->swapChainDesc.SampleDesc.Count = swapChainDesc.swapChainSampleCount;
    dx12SwapChain->swapChainDesc.SampleDesc.Quality = 0;
    dx12SwapChain->swapChainDesc.BufferUsage = swapChainDesc.swapChainUsage;
    dx12SwapChain->swapChainDesc.BufferCount = swapChainDesc.swapChainBufferCount;
    dx12SwapChain->swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    dx12SwapChain->swapChainDesc.Flags = swapChainDesc.allowModeSwitch ? DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH : 0;

    // Special processing for sRGB swap chain formats.
    // DXGI will not create a swap chain with an sRGB format, but its contents will be interpreted as sRGB.
    // So we need to use a non-sRGB format here, but store the true sRGB format for later framebuffer creation.
    switch (swapChainDesc.swapChainFormat)  // NOLINT(clang-diagnostic-switch-enum)
    {
    case nvrhi::Format::SRGBA8_UNORM:
        dx12SwapChain->swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        break;
    case nvrhi::Format::SBGRA8_UNORM:
        dx12SwapChain->swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        break;
    default:
        dx12SwapChain->swapChainDesc.Format = nvrhi::d3d12::convertFormat(swapChainDesc.swapChainFormat);
        break;
    }

    nvrhi::RefCountPtr<IDXGIFactory5> pDxgiFactory5;
    if (SUCCEEDED(dxgiFactory2->QueryInterface(IID_PPV_ARGS(&pDxgiFactory5))))
    {
        BOOL supported = 0;
        if (SUCCEEDED(pDxgiFactory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &supported, sizeof(supported))))
            dx12SwapChain->tearingSupported = (supported != 0);
    }

    if (dx12SwapChain->tearingSupported)
    {
        dx12SwapChain->swapChainDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    }

    nvrhi::RefCountPtr<IDXGISwapChain1> pSwapChain1;
    {
        HE_PROFILE_SCOPE("dxgiFactory2->CreateSwapChainForHwnd");

        hr = dxgiFactory2->CreateSwapChainForHwnd(graphicsQueue, hWnd, &dx12SwapChain->swapChainDesc, &dx12SwapChain->fullScreenDesc, nullptr, &pSwapChain1);
        if ((HRESULT)hr < 0)
            return nullptr;
    }

    {
        HE_PROFILE_SCOPE("pSwapChain1->QueryInterface");

        hr = pSwapChain1->QueryInterface(IID_PPV_ARGS(&dx12SwapChain->swapChain));
        if ((HRESULT)hr < 0)
            return nullptr;
    }

    if (!dx12SwapChain->CreateRenderTargets(width, height))
        return nullptr;

    {
        HE_PROFILE_SCOPE("device->CreateFence");

        hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&dx12SwapChain->frameFence));
        if ((HRESULT)hr < 0)
            return nullptr;
    }

    for (UINT bufferIndex = 0; bufferIndex < dx12SwapChain->swapChainDesc.BufferCount; bufferIndex++)
    {
        dx12SwapChain->frameFenceEvents.push_back(CreateEvent(nullptr, false, true, nullptr));
    }

    dx12SwapChain->ResizeBackBuffers();

    return dx12SwapChain;
}

const char* DX12DeviceManager::GetRendererString() const { return rendererString.c_str(); }

nvrhi::IDevice* DX12DeviceManager::GetDevice() const { return nvrhiDevice; }

void DX12DeviceManager::ReportLiveObjects()
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
            HE_CORE_ERROR("[DX12DeviceManager::ReportLiveObjects] failed, HRESULT = 0x{}", hr);
        }
    }
}

bool DX12DeviceManager::EnumerateAdapters(std::vector<HE::RHI::AdapterInfo>& outAdapters)
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

        HE::RHI::AdapterInfo adapterInfo;

        adapterInfo.name = GetAdapterName(desc);
        adapterInfo.dxgiAdapter = adapter;
        adapterInfo.vendorID = desc.VendorId;
        adapterInfo.deviceID = desc.DeviceId;
        adapterInfo.dedicatedVideoMemory = desc.DedicatedVideoMemory;

        HE::RHI::AdapterInfo::LUID luid;
        static_assert(luid.size() == sizeof(desc.AdapterLuid));
        memcpy(luid.data(), &desc.AdapterLuid, luid.size());
        adapterInfo.luid = luid;

        outAdapters.push_back(std::move(adapterInfo));
    }
}

bool DX12DeviceManager::CreateInstanceInternal()
{
    HE_PROFILE_FUNCTION();

    if (!dxgiFactory2)
    {
        HRESULT hres = CreateDXGIFactory2(desc.enableDebugRuntime ? DXGI_CREATE_FACTORY_DEBUG : 0, IID_PPV_ARGS(&dxgiFactory2));
        if (hres != S_OK)
        {
            HE_CORE_ERROR("[CreateInstanceInternal][CreateDXGIFactory2] : For more info, get log from debug D3D runtime: (1) Install DX SDK, and enable Debug D3D from DX Control Panel Utility. (2) Install and start DbgView. (3) Try running the program again.\n");
            return false;
        }
    }

    return true;
}

bool DX12DeviceManager::CreateDevice()
{
    HE_PROFILE_FUNCTION();

    if (desc.enableDebugRuntime)
    {
        nvrhi::RefCountPtr<ID3D12Debug> pDebug;
        HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(&pDebug));

        if (SUCCEEDED(hr))
            pDebug->EnableDebugLayer();
        else
            HE_CORE_WARN("Cannot enable DX12 debug runtime, ID3D12Debug is not available.");
    }

    if (desc.enableGPUValidation)
    {
        nvrhi::RefCountPtr<ID3D12Debug3> debugController3;
        HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debugController3));

        if (SUCCEEDED(hr))
            debugController3->SetEnableGPUBasedValidation(true);
        else
            HE_CORE_WARN("Cannot enable GPU-based validation, ID3D12Debug3 is not available.");
    }

    int adapterIndex = desc.adapterIndex;
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
        isNvidia = IsNvDeviceID(aDesc.VendorId);

        HE_CORE_INFO("Adapter {}", rendererString);
    }


    {
        HE_PROFILE_SCOPE("D3D12CreateDevice");

        HRESULT hr = D3D12CreateDevice(
            dxgiAdapter,
            desc.featureLevel,
            IID_PPV_ARGS(&device)
        );

        if (FAILED(hr))
        {
            HE_CORE_ERROR("D3D12CreateDevice failed, error code = 0x{}08x", hr);
            return false;
        }
    }

    if (desc.enableDebugRuntime)
    {
        HE_PROFILE_SCOPE("enableDebugRuntime");

        nvrhi::RefCountPtr<ID3D12InfoQueue> pInfoQueue;
        device->QueryInterface(&pInfoQueue);

        if (pInfoQueue)
        {
#ifdef HE_DEBUG
            if (desc.enableWarningsAsErrors)
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
    HRESULT hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&graphicsQueue));
    
    if ((HRESULT)hr < 0) 
        return false;

    graphicsQueue->SetName(L"Graphics Queue");

    if (desc.enableComputeQueue)
    {
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
        hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&computeQueue));
        
        if ((HRESULT)hr < 0)
            return false;

        computeQueue->SetName(L"Compute Queue");
    }

    if (desc.enableCopyQueue)
    {
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
        hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&copyQueue));
        
        if ((HRESULT)hr < 0)
            return false;
        
        copyQueue->SetName(L"Copy Queue");
    }

    nvrhi::d3d12::DeviceDesc deviceDesc;
    deviceDesc.errorCB = &HE::RHI::DefaultMessageCallback::GetInstance();
    deviceDesc.pDevice = device;
    deviceDesc.pGraphicsCommandQueue = graphicsQueue;
    deviceDesc.pComputeCommandQueue = computeQueue;
    deviceDesc.pCopyCommandQueue = copyQueue;
    deviceDesc.logBufferLifetime = desc.logBufferLifetime;
    deviceDesc.enableHeapDirectlyIndexed = desc.enableHeapDirectlyIndexed;

    nvrhiDevice = nvrhi::d3d12::createDevice(deviceDesc);

    if (desc.enableNvrhiValidationLayer)
    {
        nvrhiDevice = nvrhi::validation::createValidationLayer(nvrhiDevice);
    }

    return true;
}

bool DX12DeviceManager::IsNvDeviceID(UINT id) { return id == 0x10DE; }

std::string DX12DeviceManager::GetAdapterName(DXGI_ADAPTER_DESC const& aDesc)
{
    size_t length = wcsnlen(aDesc.Description, _countof(aDesc.Description));

    std::string name;
    name.resize(length);
    WideCharToMultiByte(CP_ACP, 0, aDesc.Description, int(length), name.data(), int(name.size()), nullptr, nullptr);

    return name;
}

HE::RHI::DeviceManager* HE::RHI::CreateD3D12() { return new DX12DeviceManager(); }

//////////////////////////////////////////////////////////////////////////
// DX12SwapChain
//////////////////////////////////////////////////////////////////////////

DX12SwapChain::~DX12SwapChain()
{
    HE_PROFILE_FUNCTION();

    rhiSwapChainBuffers.clear();
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
}

void DX12SwapChain::ResizeSwapChain(uint32_t width, uint32_t height)
{
    HE_PROFILE_FUNCTION();

    ResetBackBuffers();
    ReleaseRenderTargets();

    if (!nvrhiDevice)
        return;

    if (!swapChain)
        return;

    {
        HE_PROFILE_SCOPE("swapChain->ResizeBuffers");

        const HRESULT hr = swapChain->ResizeBuffers(
            desc.swapChainBufferCount,
            width,
            height,
            swapChainDesc.Format,
            swapChainDesc.Flags
        );

        if (FAILED(hr))
        {
            HE_CORE_ERROR("ResizeBuffers failed");
        }

        desc.backBufferWidth = width;
        desc.backBufferHeight = height;
    }

    bool ret = CreateRenderTargets(width, height);
    if (!ret)
    {
        HE_CORE_ERROR("CreateRenderTarget failed");
    }

    ResizeBackBuffers();
}

nvrhi::ITexture* DX12SwapChain::GetBackBuffer(uint32_t index)
{
    if (index < rhiSwapChainBuffers.size())
        return rhiSwapChainBuffers[index];
    return nullptr;
}

bool DX12SwapChain::BeginFrame()
{
    HE_PROFILE_FUNCTION();

    DXGI_SWAP_CHAIN_DESC1 newSwapChainDesc;
    DXGI_SWAP_CHAIN_FULLSCREEN_DESC newFullScreenDesc;
    if (SUCCEEDED(swapChain->GetDesc1(&newSwapChainDesc)) && SUCCEEDED(swapChain->GetFullscreenDesc(&newFullScreenDesc)))
    {
        if (fullScreenDesc.Windowed != newFullScreenDesc.Windowed)
        {
            fullScreenDesc = newFullScreenDesc;
            swapChainDesc = newSwapChainDesc;

            ResizeSwapChain(newSwapChainDesc.Width, newSwapChainDesc.Height);
        }
    }

    auto bufferIndex = swapChain->GetCurrentBackBufferIndex();

    WaitForSingleObject(frameFenceEvents[bufferIndex], INFINITE);

    return true;
}

bool DX12SwapChain::Present()
{
    HE_PROFILE_FUNCTION();

    auto bufferIndex = swapChain->GetCurrentBackBufferIndex();

    UINT presentFlags = 0;
    if (!desc.vsync && fullScreenDesc.Windowed && tearingSupported)
        presentFlags |= DXGI_PRESENT_ALLOW_TEARING;

    HRESULT result = swapChain->Present(desc.vsync ? 1 : 0, presentFlags);

    frameFence->SetEventOnCompletion(frameCount, frameFenceEvents[bufferIndex]);
    dx12deviceManager->graphicsQueue->Signal(frameFence, frameCount);
    frameCount++;

    nvrhiDevice->runGarbageCollection();

    return SUCCEEDED(result);
}

bool DX12SwapChain::CreateRenderTargets(uint32_t width, uint32_t height)
{
    HE_PROFILE_FUNCTION();

    swapChainBuffers.resize(swapChainDesc.BufferCount);
    rhiSwapChainBuffers.resize(swapChainDesc.BufferCount);

    for (UINT n = 0; n < swapChainDesc.BufferCount; n++)
    {
        const HRESULT hr = swapChain->GetBuffer(n, IID_PPV_ARGS(&swapChainBuffers[n]));
        
        if ((HRESULT)hr < 0) 
            return false;

        nvrhi::TextureDesc textureDesc;
        textureDesc.width = width;
        textureDesc.height = height;
        textureDesc.sampleCount = desc.swapChainSampleCount;
        textureDesc.sampleQuality = desc.swapChainSampleQuality;
        textureDesc.format = desc.swapChainFormat;
        textureDesc.debugName = "SwapChainBuffer";
        textureDesc.isRenderTarget = true;
        textureDesc.isUAV = false;
        textureDesc.initialState = nvrhi::ResourceStates::Present;
        textureDesc.keepInitialState = true;

        rhiSwapChainBuffers[n] = nvrhiDevice->createHandleForNativeTexture(nvrhi::ObjectTypes::D3D12_Resource, nvrhi::Object(swapChainBuffers[n]), textureDesc);
    }

    return true;
}

void DX12SwapChain::ReleaseRenderTargets()
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
