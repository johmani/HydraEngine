module;

#define NOMINMAX

#include "HydraEngine/Base.h"

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
#include "stb_image.h"

module HydraEngine;
import Math;
import nvrhi;
import std;

#if defined(HE_PLATFORM_WINDOWS) && HE_FORCE_DISCRETE_GPU
extern "C"
{
	// Declaring this symbol makes the OS run the app on the discrete GPU on NVIDIA Optimus laptops by default
	__declspec(dllexport) DWORD NvOptimusEnablement = 1;
	// Same as above, for laptops with AMD GPUs
	__declspec(dllexport) DWORD AmdPowerXpressRequestHighPerformance = 1;
}
#endif

namespace HydraEngine {

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

	void Window::Init(const WindowDesc& windowDesc, DeviceDesc& deviceDesc)
	{
		desc = windowDesc;

		HE_PROFILE_FUNCTION();
		
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
				if (info.format == deviceDesc.swapChainFormat)
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

			glfwWindowHint(GLFW_SAMPLES, deviceDesc.swapChainSampleCount);
			glfwWindowHint(GLFW_REFRESH_RATE, deviceDesc.refreshRate);
			glfwWindowHint(GLFW_SCALE_TO_MONITOR, desc.scaleToMonitor);
			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
			glfwWindowHint(GLFW_MAXIMIZED, windowDesc.maximized && !windowDesc.fullScreen);
			glfwWindowHint(GLFW_TITLEBAR, !windowDesc.customTitlebar);
			glfwWindowHint(GLFW_DECORATED, windowDesc.decorated);
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
			desc.width  = videoMode->width;
			desc.height = videoMode->height;
		}
		
		{
			HE_PROFILE_SCOPE("glfwCreateWindow");

			m_WindowHandle = glfwCreateWindow((int)desc.width, (int)desc.height, desc.title.data(), nullptr, nullptr);
			++s_GLFWWindowCount;

			// applying fullscreen mode by passing the primaryMonitor to glfwCreateWindow causes some weird behavior, but this works just fine for now.
			if (windowDesc.fullScreen)
			{			
				glfwSetWindowMonitor((GLFWwindow*)m_WindowHandle, primaryMonitor, 0, 0, videoMode->width, videoMode->height, videoMode->refreshRate);
			}
		}

		GLFWwindow* glfwWindow = (GLFWwindow*)m_WindowHandle;
	
		m_PrevPosX = 0, m_PrevPosY = 0, m_PrevWidth = 0, m_PrevHeight = 0;
		glfwGetWindowSize(glfwWindow, &m_PrevWidth, &m_PrevHeight);
		glfwGetWindowPos(glfwWindow, &m_PrevPosX, &m_PrevPosY);

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
			GLFWimage icon;
			int channels;
			icon.pixels = stbi_load((const char*)windowDesc.iconFilePath.data(), &icon.width, &icon.height, &channels, 4);
			glfwSetWindowIcon(glfwWindow, 1, &icon);
			stbi_image_free(icon.pixels);
		}

		// create device
		{
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

				m_DeviceManager = DeviceManager::Create(api);
				if (m_DeviceManager)
				{
					bool deviceCreated = false;
					if (deviceDesc.headlessDevice)
						deviceCreated = m_DeviceManager->CreateHeadlessDevice(deviceDesc);
					else
						deviceCreated = m_DeviceManager->CreateWindowDeviceAndSwapChain(deviceDesc, { windowDesc.fullScreen, windowDesc.maximized }, m_WindowHandle);

					if (deviceCreated)
						break;

					m_DeviceManager->Shutdown();
					delete m_DeviceManager;
				}

				HE_CORE_ERROR("Failed to create backend API: {}", nvrhi::utils::GraphicsAPIToString(api));
			}

			if (!m_DeviceManager)
			{
				HE_CORE_CRITICAL("No graphics backend could be initialized!");
				std::exit(EXIT_FAILURE);
			}
		}

		glfwShowWindow(glfwWindow);

		glfwSetWindowUserPointer(glfwWindow, this);

		glfwSetTitlebarHitTestCallback(glfwWindow, [](GLFWwindow* window, int x, int y, int* hit)
		{
			Window* app = (Window*)glfwGetWindowUserPointer(window);
			*hit = app->m_isTitleBarHovered;
		});

		glfwSetWindowSizeCallback(glfwWindow, [](GLFWwindow* window, int width, int height)
		{
			Window& w = *(Window*)glfwGetWindowUserPointer(window);
			w.desc.width = width;
			w.desc.height = height;

			WindowResizeEvent event((uint32_t)width, (uint32_t)height);
			w.eventCallback(event);
		});

		glfwSetWindowCloseCallback(glfwWindow, [](GLFWwindow* window)
		{
			Window& w = *(Window*)glfwGetWindowUserPointer(window);
			WindowCloseEvent event;
			w.eventCallback(event);
		});

		glfwSetWindowContentScaleCallback(glfwWindow, [](GLFWwindow* window, float xscale, float yscale)
		{
			Window& w = *(Window*)glfwGetWindowUserPointer(window);

			WindowContentScaleEvent event(xscale, yscale);
			w.eventCallback(event);
		});

		glfwSetWindowMaximizeCallback(glfwWindow, [](GLFWwindow* window, int maximized)
		{
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
						int(w.m_PrevPosX + delta * 0.5f), 
						int(w.m_PrevPosY + delta * 0.5f), 
						int(w.m_PrevWidth - delta), 
						int(w.m_PrevHeight - delta), 
						0
					);
				}
			}

			isfirstTime = false;
			
			WindowMaximizeEvent event(maximized);
			w.eventCallback(event);
		});

		glfwSetKeyCallback(glfwWindow, [](GLFWwindow* window, int key, int scancode, int action, int mods)
		{
			Window& w = *(Window*)glfwGetWindowUserPointer(window);

			switch (action)
			{
			case GLFW_PRESS:
			{
				KeyPressedEvent event(key, false);
				w.eventCallback(event);
				break;
			}
			case GLFW_RELEASE:
			{
				KeyReleasedEvent event(key);
				w.eventCallback(event);
				break;
			}
			case GLFW_REPEAT:
			{
				KeyPressedEvent event(key, true);
				w.eventCallback(event);
				break;
			}
			}
		});

		glfwSetCharCallback(glfwWindow, [](GLFWwindow* window, unsigned int keycode)
		{
			Window& w = *(Window*)glfwGetWindowUserPointer(window);

			KeyTypedEvent event(keycode);
			w.eventCallback(event);
		});

		glfwSetMouseButtonCallback(glfwWindow, [](GLFWwindow* window, int button, int action, int mods)
		{
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

		glfwSetScrollCallback(glfwWindow, [](GLFWwindow* window, double xOffset, double yOffset)
		{
			Window& w = *(Window*)glfwGetWindowUserPointer(window);

			MouseScrolledEvent event((float)xOffset, (float)yOffset);
			w.eventCallback(event);
		});

		glfwSetCursorPosCallback(glfwWindow, [](GLFWwindow* window, double xPos, double yPos)
		{
			Window& w = *(Window*)glfwGetWindowUserPointer(window);

			MouseMovedEvent event((float)xPos, (float)yPos);
			w.eventCallback(event);
		});

		glfwSetCursorEnterCallback(glfwWindow, [](GLFWwindow* window, int entered)
		{
			Window& w = *(Window*)glfwGetWindowUserPointer(window);

			MouseEnterEvent event((bool)entered);
			w.eventCallback(event);
		});

		glfwSetDropCallback(glfwWindow, [](GLFWwindow* window, int pathCount, const char* paths[])
		{
			Window& w = *(Window*)glfwGetWindowUserPointer(window);

			WindowDropEvent event(paths, pathCount);
			w.eventCallback(event);
		});

		glfwSetJoystickCallback([](int jid, int event)
		{
			auto& w = GetAppContext().mainWindow;

			if (event == GLFW_CONNECTED)
			{
				GamepadConnectedEvent event(jid, true);
				w.CallEvent(event);
			}
			else if (event == GLFW_DISCONNECTED)
			{
				GamepadConnectedEvent event(jid, false);
				w.CallEvent(event);
			}
		});

		glfwSetCharModsCallback(glfwWindow, [](GLFWwindow* window, unsigned int codepoint, int mods)
		{

		});

		glfwSetWindowIconifyCallback(glfwWindow, [](GLFWwindow* window, int iconified)
		{

		});

		glfwSetWindowPosCallback(glfwWindow, [](GLFWwindow* window, int xpos, int ypos)
		{
	
		});

		glfwSetWindowRefreshCallback(glfwWindow, [](GLFWwindow* window)
		{
		});

		glfwSetWindowFocusCallback(glfwWindow, [](GLFWwindow* window, int focused)
		{
		});
	}

	Window::~Window()
	{
		HE_PROFILE_FUNCTION();

		if (m_DeviceManager)
		{
			m_DeviceManager->GetDevice()->waitForIdle();
			m_DeviceManager->Shutdown();

			delete m_DeviceManager;
		}

		if(m_WindowHandle)
		{
			glfwDestroyWindow((GLFWwindow*)m_WindowHandle);
			--s_GLFWWindowCount;

			if (s_GLFWWindowCount == 0)
				glfwTerminate();
		}
	}

	void Window::SetVSync(bool enabled) { if (m_DeviceManager) m_DeviceManager->SetVsyncEnabled(enabled); }

	bool Window::IsVSync() const { return m_DeviceManager ? m_DeviceManager->IsVsyncEnabled() : false; }

	void Window::SetWindowTitle(const std::string_view& title)
	{
		if (desc.title == title)
			return;

		glfwSetWindowTitle((GLFWwindow*)m_WindowHandle, title.data());
		desc.title = title;
	}

	void* Window::GetNativeWindow()
	{
#ifdef HE_PLATFORM_WINDOWS
		return (void*)glfwGetWin32Window((GLFWwindow*)m_WindowHandle);
#elif defined(HE_PLATFORM_LINUX)
		return (void*)glfwGetX11Window((GLFWwindow*)m_WindowHandle); // not yet tested
#else
		HE_CORE_VERIFY(false, "unsupported platform");
		return nullptr;
#endif
	}

	void Window::MaximizeWindow() { glfwMaximizeWindow((GLFWwindow*)m_WindowHandle); }

	void Window::MinimizeWindow() { glfwIconifyWindow((GLFWwindow*)m_WindowHandle); }

	void Window::RestoreWindow() { glfwRestoreWindow((GLFWwindow*)m_WindowHandle); }

	bool Window::IsMaximize() { return (bool)glfwGetWindowAttrib((GLFWwindow*)m_WindowHandle, GLFW_MAXIMIZED); }

	bool Window::IsFullScreen() { return desc.fullScreen; }

	bool Window::ToggleScreenState()
	{
		GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
		const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);
		
		if (desc.fullScreen)
		{
			// Restore the window size and position
			desc.fullScreen = false;
			glfwSetWindowMonitor((GLFWwindow*)m_WindowHandle, nullptr, m_PrevPosX, m_PrevPosY, m_PrevWidth, m_PrevHeight, 0);
		}
		else
		{
			// Save the window size and position
			desc.fullScreen = true;
			glfwGetWindowSize((GLFWwindow*)m_WindowHandle, &m_PrevWidth, &m_PrevHeight);
			glfwGetWindowPos((GLFWwindow*)m_WindowHandle, &m_PrevPosX, &m_PrevPosY);
			glfwSetWindowMonitor((GLFWwindow*)m_WindowHandle, primaryMonitor, 0, 0, mode->width, mode->height, mode->refreshRate);
		}

		return true;
	}

	void Window::FocusMainWindow() { glfwFocusWindow((GLFWwindow*)m_WindowHandle); }

	bool Window::IsMainWindowFocused() { return (bool)glfwGetWindowAttrib((GLFWwindow*)m_WindowHandle, GLFW_FOCUSED); }

	std::pair<float, float> Window::GetWindowContentScale()
	{	
		float xscale, yscale;
		glfwGetWindowContentScale((GLFWwindow*)m_WindowHandle, &xscale, &yscale);
		
		return { xscale, yscale };
	}

	void Window::UpdateEvent()
	{
		HE_PROFILE_FUNCTION();

		GLFWwindow* windowHandle = static_cast<GLFWwindow*>(GetWindowHandle());

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
							CallEvent(e);
						}

						bool isReleased = !isButtonDown && !inputData.gamepadEventButtonUpPrevFrame[jid].test(button);
						inputData.gamepadEventButtonUpPrevFrame[jid].set(button, !isButtonDown);
						if (isReleased)
						{
							GamepadButtonReleasedEvent e(jid, button);
							CallEvent(e);
						}
					}

					// axes
					{
						auto createEvent = [](Window& window, int jid, GamepadAxisCode axisCode, Math::vec2 value)
							{
								if (Math::length(value) > 0)
								{
									GamepadAxisMovedEvent event(jid, axisCode, value.x, value.y);
									window.CallEvent(event);
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

		glfwPollEvents();
	}

	nvrhi::IFramebuffer* Window::BeginFrame()
	{
		if (m_DeviceManager)
		{
			m_DeviceManager->UpdateWindowSize();
			if (m_DeviceManager->BeginFrame())
			{
				return m_DeviceManager->GetCurrentFramebuffer();
			}
		}

		return nullptr;
	}

	void Window::EndFrame()
	{
		if (m_DeviceManager)
		{
			m_DeviceManager->PresentResult();
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Input
	//////////////////////////////////////////////////////////////////////////

	static int ToGLFWKeyCode(KeyCode keyCode)
	{
		switch (keyCode) {
		case Key::Space: return 32;
		case Key::Apostrophe: return 39; /* ' */
		case Key::Comma: return 44; /* , */
		case Key::Minus: return 45; /* - */
		case Key::Period: return 46; /* . */
		case Key::Slash: return 47; /* / */

		case Key::D0: return 48; /* 0 */
		case Key::D1: return 49; /* 1 */
		case Key::D2: return 50; /* 2 */
		case Key::D3: return 51; /* 3 */
		case Key::D4: return 52; /* 4 */
		case Key::D5: return 53; /* 5 */
		case Key::D6: return 54; /* 6 */
		case Key::D7: return 55; /* 7 */
		case Key::D8: return 56; /* 8 */
		case Key::D9: return 57; /* 9 */

		case Key::Semicolon: return 59; /* ; */
		case Key::Equal: return 61; /* = */

		case Key::A: return 65;
		case Key::B: return 66;
		case Key::C: return 67;
		case Key::D: return 68;
		case Key::E: return 69;
		case Key::F: return 70;
		case Key::G: return 71;
		case Key::H: return 72;
		case Key::I: return 73;
		case Key::J: return 74;
		case Key::K: return 75;
		case Key::L: return 76;
		case Key::M: return 77;
		case Key::N: return 78;
		case Key::O: return 79;
		case Key::P: return 80;
		case Key::Q: return 81;
		case Key::R: return 82;
		case Key::S: return 83;
		case Key::T: return 84;
		case Key::U: return 85;
		case Key::V: return 86;
		case Key::W: return 87;
		case Key::X: return 88;
		case Key::Y: return 89;
		case Key::Z: return 90;

		case Key::LeftBracket: return 91; /* [ */
		case Key::Backslash: return 92; /* \ */
		case Key::RightBracket: return 93; /* ] */
		case Key::GraveAccent: return 96; /* ` */

		case Key::World1: return 161; /* non-US #1 */
		case Key::World2: return 162; /* non-US #2 */

			/* Function keys */
		case Key::Escape: return 256;
		case Key::Enter: return 257;
		case Key::Tab: return 258;
		case Key::Backspace: return 259;
		case Key::Insert: return 260;
		case Key::Delete: return 261;
		case Key::Right: return 262;
		case Key::Left: return 263;
		case Key::Down: return 264;
		case Key::Up: return 265;
		case Key::PageUp: return 266;
		case Key::PageDown: return 267;
		case Key::Home: return 268;
		case Key::End: return 269;
		case Key::CapsLock: return 280;
		case Key::ScrollLock: return 281;
		case Key::NumLock: return 282;
		case Key::PrintScreen: return 283;
		case Key::Pause: return 284;
		case Key::F1: return 290;
		case Key::F2: return 291;
		case Key::F3: return 292;
		case Key::F4: return 293;
		case Key::F5: return 294;
		case Key::F6: return 295;
		case Key::F7: return 296;
		case Key::F8: return 297;
		case Key::F9: return 298;
		case Key::F10: return 299;
		case Key::F11: return 300;
		case Key::F12: return 301;
		case Key::F13: return 302;
		case Key::F14: return 303;
		case Key::F15: return 304;
		case Key::F16: return 305;
		case Key::F17: return 306;
		case Key::F18: return 307;
		case Key::F19: return 308;
		case Key::F20: return 309;
		case Key::F21: return 310;
		case Key::F22: return 311;
		case Key::F23: return 312;
		case Key::F24: return 313;
		case Key::F25: return 314;

			/* Keypad */
		case Key::KP0: return 320;
		case Key::KP1: return 321;
		case Key::KP2: return 322;
		case Key::KP3: return 323;
		case Key::KP4: return 324;
		case Key::KP5: return 325;
		case Key::KP6: return 326;
		case Key::KP7: return 327;
		case Key::KP8: return 328;
		case Key::KP9: return 329;
		case Key::KPDecimal: return 330;
		case Key::KPDivide: return 331;
		case Key::KPMultiply: return 332;
		case Key::KPSubtract: return 333;
		case Key::KPAdd: return 334;
		case Key::KPEnter: return 335;
		case Key::KPEqual: return 336;

			/* Modifiers and others */
		case Key::LeftShift: return 340;
		case Key::LeftControl: return 341;
		case Key::LeftAlt: return 342;
		case Key::LeftSuper: return 343;
		case Key::RightShift: return 344;
		case Key::RightControl: return 345;
		case Key::RightAlt: return 346;
		case Key::RightSuper: return 347;
		case Key::Menu: return 348;


		default: return -1; // Unknown key
		}
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

	bool Input::IsKeyDown(const KeyCode key)
	{
		auto& w = GetAppContext().mainWindow;
		auto window = static_cast<GLFWwindow*>(w.GetWindowHandle());
		int glfwKey = ToGLFWKeyCode(key);
		auto state = glfwGetKey(window, glfwKey);

		return state == GLFW_PRESS;
	}

	bool Input::IsKeyUp(const KeyCode key)
	{
		auto& w = GetAppContext().mainWindow;
		auto window = static_cast<GLFWwindow*>(w.GetWindowHandle());
		int glfwKey = ToGLFWKeyCode(key);
		auto state = glfwGetKey(window, glfwKey);

		return state == GLFW_RELEASE;
	}

	bool Input::IsKeyPressed(const KeyCode key)
	{
		auto& w = GetAppContext().mainWindow;
		auto window = static_cast<GLFWwindow*>(w.GetWindowHandle());
		int glfwKey = ToGLFWKeyCode(key);
		auto currentState = glfwGetKey(window, glfwKey);

		bool isKeyDown = currentState == GLFW_PRESS && !w.inputData.keyDownPrevFrame.test(key);
		w.inputData.keyDownPrevFrame.set(key, currentState == GLFW_PRESS);

		return isKeyDown;
	}

	bool Input::IsKeyReleased(const KeyCode key)
	{
		auto& w = GetAppContext().mainWindow;
		auto window = static_cast<GLFWwindow*>(w.GetWindowHandle());
		int glfwKey = ToGLFWKeyCode(key);
		auto currentState = glfwGetKey(window, glfwKey);

		bool isKeyUp = currentState == GLFW_RELEASE && !w.inputData.keyUpPrevFrame.test(key);
		w.inputData.keyUpPrevFrame.set(key, currentState == GLFW_RELEASE);

		return isKeyUp;
	}

	bool Input::IsMouseButtonDown(const MouseCode button)
	{
		auto& w = GetAppContext().mainWindow;
		auto window = static_cast<GLFWwindow*>(w.GetWindowHandle());
		auto state = glfwGetMouseButton(window, static_cast<int32_t>(button));
		return state == GLFW_PRESS;
	}

	bool Input::IsMouseButtonUp(const MouseCode button)
	{
		auto& w = GetAppContext().mainWindow;
		auto window = static_cast<GLFWwindow*>(w.GetWindowHandle());
		auto state = glfwGetMouseButton(window, static_cast<int32_t>(button));
		return state == GLFW_RELEASE;
	}

	bool Input::IsMouseButtonPressed(const MouseCode key)
	{
		auto& w = GetAppContext().mainWindow;
		auto window = static_cast<GLFWwindow*>(w.GetWindowHandle());
		auto currentState = glfwGetMouseButton(window, static_cast<int32_t>(key));

		bool isKeyDown = (currentState == GLFW_PRESS) && !w.inputData.mouseButtonDownPrevFrame.test(key);
		w.inputData.mouseButtonDownPrevFrame.set(key, currentState == GLFW_PRESS);

		return isKeyDown;
	}

	bool Input::IsMouseButtonReleased(const MouseCode key)
	{
		auto& w = GetAppContext().mainWindow;
		auto window = static_cast<GLFWwindow*>(w.GetWindowHandle());
		auto currentState = glfwGetMouseButton(window, static_cast<int32_t>(key));

		bool isKeyUp = (currentState == GLFW_RELEASE) && !w.inputData.mouseButtonUpPrevFrame.test(key);
		w.inputData.mouseButtonUpPrevFrame.set(key, currentState == GLFW_RELEASE);

		return isKeyUp;
	}

	std::pair<float, float> Input::GetMousePosition()
	{
		auto& w = GetAppContext().mainWindow;
		auto window = static_cast<GLFWwindow*>(w.GetWindowHandle());
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
		auto window = static_cast<GLFWwindow*>(w.GetWindowHandle());
		glfwSetInputMode(window, GLFW_CURSOR, ToGLFWCursorMode(mode));
		w.inputData.cursor.CursorMode = mode;
	}

	Cursor::Mode Input::GetCursorMode()
	{
		auto& w = GetAppContext().mainWindow;
		return w.inputData.cursor.CursorMode;
	}
}
