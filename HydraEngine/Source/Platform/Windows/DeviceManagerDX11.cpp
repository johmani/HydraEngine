#ifdef NVRHI_HAS_D3D11
module;

#include "HydraEngine/Base.h"
#include <format>

#include <Windows.h>
#include <dxgi1_3.h>
#include <dxgidebug.h>

#include <nvrhi/validation.h>
#include <nvrhi/d3d11.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#ifdef min
#undef min
#endif 

#ifdef max
#undef max
#endif 

module HydraEngine;
import std;

using nvrhi::RefCountPtr;

namespace HydraEngine {

	class DeviceManagerDX11 : public DeviceManager
	{
		RefCountPtr<IDXGIFactory1> m_DxgiFactory;
		RefCountPtr<IDXGIAdapter> m_DxgiAdapter;
		RefCountPtr<ID3D11Device> m_Device;
		RefCountPtr<ID3D11DeviceContext> m_ImmediateContext;
		RefCountPtr<IDXGISwapChain> m_SwapChain;
		DXGI_SWAP_CHAIN_DESC m_SwapChainDesc{};
		HWND m_hWnd = nullptr;

		nvrhi::DeviceHandle m_NvrhiDevice;
		nvrhi::TextureHandle m_RhiBackBuffer;
		RefCountPtr<ID3D11Texture2D> m_D3D11BackBuffer;

		std::string m_RendererString;

	public:
		const char* GetRendererString() const override
		{
			return m_RendererString.c_str();
		}

		nvrhi::IDevice* GetDevice() const override
		{
			return m_NvrhiDevice;
		}

		bool BeginFrame() override;
		void ReportLiveObjects() override;
		bool EnumerateAdapters(std::vector<AdapterInfo>& outAdapters) override;

		nvrhi::GraphicsAPI GetGraphicsAPI() const override
		{
			return nvrhi::GraphicsAPI::D3D11;
		}
	protected:
		bool CreateInstanceInternal() override;
		bool CreateDevice() override;
		bool CreateSwapChain(WindowState windowState) override;
		void DestroyDeviceAndSwapChain() override;
		void ResizeSwapChain() override;
		void Shutdown() override;

		nvrhi::ITexture* GetCurrentBackBuffer() override
		{
			return m_RhiBackBuffer;
		}

		nvrhi::ITexture* GetBackBuffer(uint32_t index) override
		{
			if (index == 0)
				return m_RhiBackBuffer;

			return nullptr;
		}

		uint32_t GetCurrentBackBufferIndex() override
		{
			return 0;
		}

		uint32_t GetBackBufferCount() override
		{
			return 1;
		}

		void Present() override;


	private:
		bool CreateRenderTarget();
		void ReleaseRenderTarget();
	};

	static bool IsNvDeviceID(UINT id)
	{
		return id == 0x10DE;
	}

	bool DeviceManagerDX11::BeginFrame()
	{
		DXGI_SWAP_CHAIN_DESC newSwapChainDesc;
		if (SUCCEEDED(m_SwapChain->GetDesc(&newSwapChainDesc)))
		{
			if (m_SwapChainDesc.Windowed != newSwapChainDesc.Windowed)
			{
				BackBufferResizing();

				m_SwapChainDesc = newSwapChainDesc;
				m_DeviceParams.backBufferWidth = newSwapChainDesc.BufferDesc.Width;
				m_DeviceParams.backBufferHeight = newSwapChainDesc.BufferDesc.Height;

				if (newSwapChainDesc.Windowed)
					glfwSetWindowMonitor((GLFWwindow*)m_Window, nullptr, 50, 50, newSwapChainDesc.BufferDesc.Width, newSwapChainDesc.BufferDesc.Height, 0);

				ResizeSwapChain();
				BackBufferResized();
			}
		}

		return true;
	}

	void DeviceManagerDX11::ReportLiveObjects()
	{
		nvrhi::RefCountPtr<IDXGIDebug> pDebug;
		DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug));

		if (pDebug)
			pDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_DETAIL);
	}

	static std::string GetAdapterName(DXGI_ADAPTER_DESC const& aDesc)
	{
		size_t length = wcsnlen(aDesc.Description, _countof(aDesc.Description));

		std::string name;
		name.resize(length);
		WideCharToMultiByte(CP_ACP, 0, aDesc.Description, int(length), name.data(), int(name.size()), nullptr, nullptr);

		return name;
	}

	bool DeviceManagerDX11::CreateInstanceInternal()
	{
		if (!m_DxgiFactory)
		{
			HRESULT hres = CreateDXGIFactory1(IID_PPV_ARGS(&m_DxgiFactory));
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

	bool DeviceManagerDX11::EnumerateAdapters(std::vector<AdapterInfo>& outAdapters)
	{
		if (!m_DxgiFactory)
			return false;

		outAdapters.clear();

		while (true)
		{
			RefCountPtr<IDXGIAdapter> adapter;
			HRESULT hr = m_DxgiFactory->EnumAdapters(uint32_t(outAdapters.size()), &adapter);
			if (FAILED(hr))
				return true;

			DXGI_ADAPTER_DESC desc;
			hr = adapter->GetDesc(&desc);
			if (FAILED(hr))
				return false;

			AdapterInfo adapterInfo;

			adapterInfo.name = GetAdapterName(desc);
			adapterInfo.dxgiAdapter = adapter;
			adapterInfo.vendorID = desc.VendorId;
			adapterInfo.deviceID = desc.DeviceId;
			adapterInfo.dedicatedVideoMemory = desc.DedicatedVideoMemory;

			AdapterInfo::LUID luid;
			static_assert(luid.size() == sizeof(desc.AdapterLuid));
			memcpy(luid.data(), &desc.AdapterLuid, luid.size());
			adapterInfo.luid = luid;

			outAdapters.push_back(std::move(adapterInfo));
		}
	}

	bool DeviceManagerDX11::CreateDevice()
	{
		int adapterIndex = m_DeviceParams.adapterIndex;
		if (adapterIndex < 0)
			adapterIndex = 0;

		if (FAILED(m_DxgiFactory->EnumAdapters(adapterIndex, &m_DxgiAdapter)))
		{
			if (adapterIndex == 0)
				HE_CORE_ERROR("Cannot find any DXGI adapters in the system.");
			else
				HE_CORE_ERROR("The specified DXGI adapter {} does not exist.", adapterIndex);
			return false;
		}

		{
			DXGI_ADAPTER_DESC aDesc;
			m_DxgiAdapter->GetDesc(&aDesc);

			m_RendererString = GetAdapterName(aDesc);
			m_IsNvidia = IsNvDeviceID(aDesc.VendorId);
		}

		UINT createFlags = 0;
		if (m_DeviceParams.enableDebugRuntime)
			createFlags |= D3D11_CREATE_DEVICE_DEBUG;

		const HRESULT hr = D3D11CreateDevice(
			m_DxgiAdapter, // pAdapter
			D3D_DRIVER_TYPE_UNKNOWN, // DriverType
			nullptr, // Software
			createFlags, // Flags
			&m_DeviceParams.featureLevel, // pFeatureLevels
			1, // FeatureLevels
			D3D11_SDK_VERSION, // SDKVersion
			&m_Device, // ppDevice
			nullptr, // pFeatureLevel
			&m_ImmediateContext // ppImmediateContext
		);

		if (FAILED(hr))
		{
			return false;
		}

		nvrhi::d3d11::DeviceDesc deviceDesc;
		deviceDesc.messageCallback = &DefaultMessageCallback::GetInstance();
		deviceDesc.context = m_ImmediateContext;

		m_NvrhiDevice = nvrhi::d3d11::createDevice(deviceDesc);

		if (m_DeviceParams.enableNvrhiValidationLayer)
		{
			m_NvrhiDevice = nvrhi::validation::createValidationLayer(m_NvrhiDevice);
		}

		return true;
	}

	bool DeviceManagerDX11::CreateSwapChain(WindowState windowState)
	{
		m_hWnd = glfwGetWin32Window((GLFWwindow*)m_Window);

		RECT clientRect;
		GetClientRect(m_hWnd, &clientRect);
		UINT width = clientRect.right - clientRect.left;
		UINT height = clientRect.bottom - clientRect.top;

		ZeroMemory(&m_SwapChainDesc, sizeof(m_SwapChainDesc));
		m_SwapChainDesc.BufferCount = m_DeviceParams.swapChainBufferCount;
		m_SwapChainDesc.BufferDesc.Width = width;
		m_SwapChainDesc.BufferDesc.Height = height;
		m_SwapChainDesc.BufferDesc.RefreshRate.Numerator = m_DeviceParams.refreshRate;
		m_SwapChainDesc.BufferDesc.RefreshRate.Denominator = 0;
		m_SwapChainDesc.BufferUsage = m_DeviceParams.swapChainUsage;
		m_SwapChainDesc.OutputWindow = m_hWnd;
		m_SwapChainDesc.SampleDesc.Count = m_DeviceParams.swapChainSampleCount;
		m_SwapChainDesc.SampleDesc.Quality = m_DeviceParams.swapChainSampleQuality;
		m_SwapChainDesc.Windowed = !windowState.fullscreen;
		m_SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		m_SwapChainDesc.Flags = m_DeviceParams.allowModeSwitch ? DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH : 0;

		// Special processing for sRGB swap chain formats.
		// DXGI will not create a swap chain with an sRGB format, but its contents will be interpreted as sRGB.
		// So we need to use a non-sRGB format here, but store the true sRGB format for later framebuffer creation.
		switch (m_DeviceParams.swapChainFormat)  // NOLINT(clang-diagnostic-switch-enum)
		{
		case nvrhi::Format::SRGBA8_UNORM:
			m_SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			break;
		case nvrhi::Format::SBGRA8_UNORM:
			m_SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
			break;
		default:
			m_SwapChainDesc.BufferDesc.Format = nvrhi::d3d11::convertFormat(m_DeviceParams.swapChainFormat);
			break;
		}

		HRESULT hr = m_DxgiFactory->CreateSwapChain(m_Device, &m_SwapChainDesc, &m_SwapChain);

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

	void DeviceManagerDX11::DestroyDeviceAndSwapChain()
	{
		m_RhiBackBuffer = nullptr;
		m_NvrhiDevice = nullptr;

		if (m_SwapChain)
		{
			m_SwapChain->SetFullscreenState(false, nullptr);
		}

		ReleaseRenderTarget();

		m_SwapChain = nullptr;
		m_ImmediateContext = nullptr;
		m_Device = nullptr;
	}

	bool DeviceManagerDX11::CreateRenderTarget()
	{
		ReleaseRenderTarget();

		const HRESULT hr = m_SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&m_D3D11BackBuffer);  // NOLINT(clang-diagnostic-language-extension-token)
		if (FAILED(hr))
		{
			return false;
		}

		nvrhi::TextureDesc textureDesc;
		textureDesc.width = m_DeviceParams.backBufferWidth;
		textureDesc.height = m_DeviceParams.backBufferHeight;
		textureDesc.sampleCount = m_DeviceParams.swapChainSampleCount;
		textureDesc.sampleQuality = m_DeviceParams.swapChainSampleQuality;
		textureDesc.format = m_DeviceParams.swapChainFormat;
		textureDesc.debugName = "SwapChainBuffer";
		textureDesc.isRenderTarget = true;
		textureDesc.isUAV = false;

		m_RhiBackBuffer = m_NvrhiDevice->createHandleForNativeTexture(nvrhi::ObjectTypes::D3D11_Resource, static_cast<ID3D11Resource*>(m_D3D11BackBuffer.Get()), textureDesc);

		if (FAILED(hr))
		{
			return false;
		}

		return true;
	}

	void DeviceManagerDX11::ReleaseRenderTarget()
	{
		m_RhiBackBuffer = nullptr;
		m_D3D11BackBuffer = nullptr;
	}

	void DeviceManagerDX11::ResizeSwapChain()
	{
		ReleaseRenderTarget();

		if (!m_SwapChain)
			return;

		const HRESULT hr = m_SwapChain->ResizeBuffers(m_DeviceParams.swapChainBufferCount,
			m_DeviceParams.backBufferWidth,
			m_DeviceParams.backBufferHeight,
			m_SwapChainDesc.BufferDesc.Format,
			m_SwapChainDesc.Flags);

		if (FAILED(hr))
		{
			HE_CORE_CRITICAL("ResizeBuffers failed");
		}

		const bool ret = CreateRenderTarget();
		if (!ret)
		{
			HE_CORE_CRITICAL("CreateRenderTarget failed");
		}
	}

	void DeviceManagerDX11::Shutdown()
	{
		DeviceManager::Shutdown();

		if (m_DeviceParams.enableDebugRuntime)
		{
			ReportLiveObjects();
		}
	}

	void DeviceManagerDX11::Present()
	{
		m_SwapChain->Present(m_DeviceParams.vsyncEnabled ? 1 : 0, 0);
	}

	DeviceManager* DeviceManager::CreateD3D11() { return new DeviceManagerDX11(); }
}
#endif
