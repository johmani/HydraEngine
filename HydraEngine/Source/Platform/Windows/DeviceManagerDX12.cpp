module;

#define NOMINMAX

#include "HydraEngine/Base.h"

#include <dxgi1_5.h>
#include <dxgidebug.h>
#include <d3d12.h>

#ifdef HE_PLATFORM_WINDOWS
#define GLFW_EXPOSE_NATIVE_WIN32
#endif 
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#define HR_RETURN(hr) if(FAILED(hr)) return false

module HE;
import std;
import nvrhi;

// Based on NVIDIA Donut framework (MIT License), with slight modifications
// Source: https://github.com/NVIDIA-RTX/Donut/blob/main/src/app/dx12/DeviceManager_DX12.cpp

namespace HE {

	class DeviceManagerDX12 : public DeviceManager
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

		bool EnumerateAdapters(std::vector<AdapterInfo>& outAdapters) override
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
			deviceDesc.errorCB = &DefaultMessageCallback::GetInstance();
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

		bool CreateSwapChain(WindowState windowState) override
		{
			HE_PROFILE_FUNCTION();

			UINT windowStyle = windowState.fullscreen
				? (WS_POPUP | WS_SYSMENU | WS_VISIBLE)
				: windowState.maximized
				? (WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_MAXIMIZE)
				: (WS_OVERLAPPEDWINDOW | WS_VISIBLE);

			RECT rect = { 0, 0, LONG(m_DeviceDesc.backBufferWidth), LONG(m_DeviceDesc.backBufferHeight) };
			AdjustWindowRect(&rect, windowStyle, FALSE);

			if (MoveWindowOntoAdapter(dxgiAdapter, rect))
			{
				glfwSetWindowPos((GLFWwindow*)m_Window, rect.left, rect.top);
			}

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

			const HRESULT hr = swapChain->ResizeBuffers(m_DeviceDesc.swapChainBufferCount,
				m_DeviceDesc.backBufferWidth,
				m_DeviceDesc.backBufferHeight,
				swapChainDesc.Format,
				swapChainDesc.Flags);

			if (FAILED(hr))
			{
				HE_CORE_ERROR("ResizeBuffers failed");
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

	DeviceManager* DeviceManager::CreateD3D12() { return new DeviceManagerDX12(); }
}
