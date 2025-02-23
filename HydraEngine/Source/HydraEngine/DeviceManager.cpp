module;

#include "HydraEngine/Base.h"
#include <ShaderMake/ShaderBlob.h>
#include <GLFW/glfw3.h>

module HydraEngine;
import nvrhi;
import std;

namespace HydraEngine {

	namespace RHI {

		nvrhi::DeviceHandle GetDevice() { return HydraEngine::GetAppContext().mainWindow.GetDeviceManager()->GetDevice(); }
		nvrhi::IFramebuffer* GetCurrentFramebuffer() { return HydraEngine::GetAppContext().mainWindow.GetDeviceManager()->GetCurrentFramebuffer(); }

		nvrhi::ShaderHandle CreateStaticShader(nvrhi::IDevice* device, StaticShader staticShader, const std::vector<ShaderMacro>* pDefines, const nvrhi::ShaderDesc& desc)
		{
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
					constants.emplace_back(define.Name.data(), define.Definition.data());

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

	bool DeviceManager::CreateInstance(const InstanceParameters& params)
	{
		HE_PROFILE_FUNCTION();

		if (m_InstanceCreated)
			return true;

		static_cast<InstanceParameters&>(m_DeviceParams) = params;

		m_InstanceCreated = CreateInstanceInternal();
		return m_InstanceCreated;
	}

	bool DeviceManager::CreateHeadlessDevice(const DeviceDesc& params)
	{
		HE_PROFILE_FUNCTION();

		m_DeviceParams = params;
		m_DeviceParams.headlessDevice = true;

		if (!CreateInstance(m_DeviceParams))
			return false;

		return CreateDevice();
	}

	bool DeviceManager::CreateWindowDeviceAndSwapChain(const DeviceDesc& params, WindowState windowState, void* windowHandle)
	{
		HE_PROFILE_FUNCTION();

		m_Window = windowHandle;
		HE_CORE_VERIFY(m_Window, "invalid window handle");

		m_DeviceParams = params;
		m_DeviceParams.headlessDevice = false;
		m_RequestedVSync = params.vsyncEnabled;

		if (!CreateInstance(m_DeviceParams))
			return false;

		{
			int fbWidth = 0, fbHeight = 0;
			glfwGetFramebufferSize((GLFWwindow*)m_Window, &fbWidth, &fbHeight);
			m_DeviceParams.backBufferWidth = fbWidth;
			m_DeviceParams.backBufferHeight = fbHeight;
		}

		if (!CreateDevice())
			return false;

		if (!CreateSwapChain(windowState))
			return false;
		
		m_DeviceParams.backBufferWidth = 0;
		m_DeviceParams.backBufferHeight = 0;
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
		++m_FrameIndex;
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
			int(m_DeviceParams.backBufferWidth) != width ||
			int(m_DeviceParams.backBufferHeight) != height ||
			(m_DeviceParams.vsyncEnabled != m_RequestedVSync && GetGraphicsAPI() == nvrhi::GraphicsAPI::VULKAN)
		)
		{
			BackBufferResizing();

			m_DeviceParams.backBufferWidth = width;
			m_DeviceParams.backBufferHeight = height;
			m_DeviceParams.vsyncEnabled = m_RequestedVSync;

			ResizeSwapChain();
			BackBufferResized();
		}

		m_DeviceParams.vsyncEnabled = m_RequestedVSync;
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
		case nvrhi::GraphicsAPI::VULKAN: return CreateVK();
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
