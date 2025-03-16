module;

#include "HydraEngine/Base.h"

#include <dxgi1_3.h>
#include <dxgidebug.h>
#include <d3d11.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

module HE;
import std;
import nvrhi;

// Based on NVIDIA Donut framework (MIT License), with slight modifications
// Source: https://github.com/NVIDIA-RTX/Donut/blob/main/src/app/dx11/DeviceManager_DX11.cpp

namespace HE {

	class DeviceManagerDX11 : public DeviceManager
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
			nvrhi::RefCountPtr<IDXGIDebug> pDebug;
			DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug));

			if (pDebug)
				pDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_DETAIL);
		}
		
		bool EnumerateAdapters(std::vector<AdapterInfo>& outAdapters) override
		{
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

		bool CreateInstanceInternal() override
		{
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

			nvrhi::d3d11::DeviceDesc deviceDesc;
			deviceDesc.messageCallback = &DefaultMessageCallback::GetInstance();
			deviceDesc.context = immediateContext;

			nvrhiDevice = nvrhi::d3d11::createDevice(deviceDesc);

			if (m_DeviceDesc.enableNvrhiValidationLayer)
			{
				nvrhiDevice = nvrhi::validation::createValidationLayer(nvrhiDevice);
			}

			return true;
		}
		
		bool CreateSwapChain(WindowState windowState) override
		{
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
			ReleaseRenderTarget();

			if (!swapChain)
				return;

			const HRESULT hr = swapChain->ResizeBuffers(m_DeviceDesc.swapChainBufferCount,
				m_DeviceDesc.backBufferWidth,
				m_DeviceDesc.backBufferHeight,
				swapChainDesc.BufferDesc.Format,
				swapChainDesc.Flags);

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
		
		void Shutdown() override
		{
			DeviceManager::Shutdown();

			if (m_DeviceDesc.enableDebugRuntime)
			{
				ReportLiveObjects();
			}
		}

		bool CreateRenderTarget()
		{
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

	DeviceManager* DeviceManager::CreateD3D11() { return new DeviceManagerDX11(); }
}
