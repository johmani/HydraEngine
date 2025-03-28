module;

#include "HydraEngine/Base.h"
#include <ShaderMake/ShaderBlob.h>
#include <GLFW/glfw3.h>

module HE;
import nvrhi;
import std;

namespace HE {

	namespace RHI {

		DeviceContext::~DeviceContext()
		{
			HE_PROFILE_FUNCTION();

			for (auto dm : GetAppContext().deviceContext.managers)
			{
				if (dm)
				{
					dm->GetDevice()->waitForIdle();
					dm->Shutdown();
					delete dm;
				}
			}
		}

		void DeviceContext::TryCreateDefaultDevice()
		{
			HE_PROFILE_FUNCTION();

			auto& c = GetAppContext();
			auto& dm = c.deviceContext.managers.emplace_back();

			auto deviceDesc = c.applicatoinDesc.deviceDesc;
			auto windowDesc = c.applicatoinDesc.windowDesc;
			size_t apiCount = deviceDesc.api.size();
			if (deviceDesc.api[0] == nvrhi::GraphicsAPI(-1))
			{
#ifdef HE_PLATFORM_WINDOWS
				deviceDesc.api = {
			#if NVRHI_HAS_D3D12
					nvrhi::GraphicsAPI::D3D12,
			#endif
			#if NVRHI_HAS_VULKAN
					nvrhi::GraphicsAPI::VULKAN,
			#endif
			#if NVRHI_HAS_D3D11
					nvrhi::GraphicsAPI::D3D11
			#endif
				};
#else
				deviceDesc.api = { nvrhi::GraphicsAPI::VULKAN };
#endif
				apiCount = deviceDesc.api.size();
			}

			for (size_t i = 0; i < apiCount; i++)
			{
				auto api = deviceDesc.api[i];

				if (api == nvrhi::GraphicsAPI(-1))
					continue;

				HE_CORE_INFO("Trying to create backend API: {}", nvrhi::utils::GraphicsAPIToString(api));

				dm = DeviceManager::Create(api);
				if (dm)
				{
					bool deviceCreated = false;
					if (deviceDesc.headlessDevice)
						deviceCreated = dm->CreateHeadlessDevice(deviceDesc);
					else
						deviceCreated = dm->CreateWindowDeviceAndSwapChain(deviceDesc, { windowDesc.fullScreen, windowDesc.maximized }, c.mainWindow.GetWindowHandle());

					if (deviceCreated)
						break;

					dm->Shutdown();
					delete dm;
				}

				HE_CORE_ERROR("Failed to create backend API: {}", nvrhi::utils::GraphicsAPIToString(api));
			}

			if (!dm)
			{
				HE_CORE_CRITICAL("No graphics backend could be initialized!");
				std::exit(1);
			}
		}

		DeviceManager* GetDeviceManager(uint32_t index) 
		{ 
			auto& managers = GetAppContext().deviceContext.managers;
			
			if(managers.size() >= 1)
				return managers[index];

			return nullptr;
		}

		nvrhi::DeviceHandle GetDevice(uint32_t index) 
		{
			auto& managers = GetAppContext().deviceContext.managers;

			if(managers.size() >= 1)
				return managers[index]->GetDevice();
			
			return {};
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
	}

	bool DeviceManager::CreateInstance(const DeviceInstanceDesc& desc)
	{
		HE_PROFILE_FUNCTION();

		if (m_InstanceCreated)
			return true;

		static_cast<DeviceInstanceDesc&>(m_DeviceDesc) = desc;

		m_InstanceCreated = CreateInstanceInternal();
		return m_InstanceCreated;
	}

	bool DeviceManager::CreateHeadlessDevice(const DeviceDesc& desc)
	{
		HE_PROFILE_FUNCTION();

		m_DeviceDesc = desc;
		m_DeviceDesc.headlessDevice = true;

		if (!CreateInstance(m_DeviceDesc))
			return false;

		if (!CreateDevice())
			return false;

		HE_CORE_INFO("[Backend API] : {}", nvrhi::utils::GraphicsAPIToString(GetDevice()->getGraphicsAPI()));

		return true;
	}

	bool DeviceManager::CreateWindowDeviceAndSwapChain(const DeviceDesc& desc, WindowState windowState, void* windowHandle)
	{
		HE_PROFILE_FUNCTION();

		m_Window = windowHandle;
		HE_CORE_VERIFY(m_Window, "invalid window handle");

		m_DeviceDesc = desc;
		m_DeviceDesc.headlessDevice = false;
		m_RequestedVSync = desc.vsyncEnabled;

		if (!CreateInstance(m_DeviceDesc))
			return false;

		{
			int fbWidth = 0, fbHeight = 0;
			glfwGetFramebufferSize((GLFWwindow*)m_Window, &fbWidth, &fbHeight);
			m_DeviceDesc.backBufferWidth = fbWidth;
			m_DeviceDesc.backBufferHeight = fbHeight;
		}

		if (!CreateDevice())
			return false;

		HE_CORE_INFO("[Backend API] : {}", nvrhi::utils::GraphicsAPIToString(GetDevice()->getGraphicsAPI()));


		if (!CreateSwapChain(windowState))
			return false;
		
		m_DeviceDesc.backBufferWidth = 0;
		m_DeviceDesc.backBufferHeight = 0;
		UpdateWindowSize();

		return true;
	}

	void DeviceManager::BackBufferResizing()
	{
		m_SwapChainFramebuffers.clear();
	}

	void DeviceManager::BackBufferResized()
	{
		HE_PROFILE_FUNCTION();

		uint32_t backBufferCount = GetBackBufferCount();
		m_SwapChainFramebuffers.resize(backBufferCount);
		for (uint32_t index = 0; index < backBufferCount; index++)
		{
			nvrhi::ITexture* texture = GetBackBuffer(index);
			nvrhi::FramebufferDesc& desc = nvrhi::FramebufferDesc().addColorAttachment(texture);
			m_SwapChainFramebuffers[index] = GetDevice()->createFramebuffer(desc);
		}
	}

	void DeviceManager::PresentResult()
	{
		HE_PROFILE_FUNCTION();

		Present();
		GetDevice()->runGarbageCollection();
	}

	void DeviceManager::UpdateWindowSize()
	{
		HE_PROFILE_FUNCTION();

		int width;
		int height;
		glfwGetWindowSize((GLFWwindow*)m_Window, &width, &height);
		if (width == 0 || height == 0)
			return;

		if (
			int(m_DeviceDesc.backBufferWidth) != width ||
			int(m_DeviceDesc.backBufferHeight) != height ||
			(m_DeviceDesc.vsyncEnabled != m_RequestedVSync && GetGraphicsAPI() == nvrhi::GraphicsAPI::VULKAN)
		)
		{
			BackBufferResizing();

			m_DeviceDesc.backBufferWidth = width;
			m_DeviceDesc.backBufferHeight = height;
			m_DeviceDesc.vsyncEnabled = m_RequestedVSync;

			ResizeSwapChain();
			BackBufferResized();
		}

		m_DeviceDesc.vsyncEnabled = m_RequestedVSync;
	}

	void DeviceManager::Shutdown()
	{
		HE_PROFILE_FUNCTION();

		m_SwapChainFramebuffers.clear();
		DestroyDeviceAndSwapChain();
		m_InstanceCreated = false;
	}

	nvrhi::IFramebuffer* DeviceManager::GetCurrentFramebuffer()
	{
		return GetFramebuffer(GetCurrentBackBufferIndex());
	}

	nvrhi::IFramebuffer* DeviceManager::GetFramebuffer(uint32_t index)
	{
		if (index < m_SwapChainFramebuffers.size())
			return m_SwapChainFramebuffers[index];

		return nullptr;
	}

	DeviceManager* DeviceManager::Create(nvrhi::GraphicsAPI api)
	{
		HE_PROFILE_FUNCTION();

		switch (api)
		{
#if NVRHI_HAS_D3D11
		case nvrhi::GraphicsAPI::D3D11:  return CreateD3D11();
#endif
#if NVRHI_HAS_D3D12
		case nvrhi::GraphicsAPI::D3D12:  return CreateD3D12();
#endif
#if NVRHI_HAS_VULKAN
		case nvrhi::GraphicsAPI::VULKAN: return CreateVULKAN();
#endif
		}

		HE_CORE_ERROR("DeviceManager::Create: Unsupported Graphics API ({})", (int)api);
		return nullptr;
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
