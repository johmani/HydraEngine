module;

#define NOMINMAX

#include "HydraEngine/Base.h"
#include <taskflow/taskflow.hpp>

#if defined(NVRHI_HAS_D3D11) | defined(NVRHI_HAS_D3D12)
    #include <dxgi.h>
#endif

#if NVRHI_HAS_D3D11
    #include <d3d11.h>
#endif

#if NVRHI_HAS_D3D12
    #include <d3d12.h>
#endif

#if NVRHI_HAS_VULKAN
    #include <vulkan/vulkan.h>
#endif

export module HE;

import std;
import nvrhi;

export {

    using std::uint8_t;
    using std::uint16_t;
    using std::uint32_t;
    using std::uint64_t;
    using std::size_t;
}

export namespace HE {

    //////////////////////////////////////////////////////////////////////////
    // Basic
    //////////////////////////////////////////////////////////////////////////

#ifdef HE_ENABLE_LOGGING

    namespace Log {

        HYDRA_API void Init(const char* client);
        HYDRA_API void Shutdown();

        HYDRA_API void CoreTrace(const char* s);
        HYDRA_API void CoreInfo(const char* s);
        HYDRA_API void CoreWarn(const char* s);
        HYDRA_API void CoreError(const char* s);
        HYDRA_API void CoreCritical(const char* s);

        HYDRA_API void ClientTrace(const char* s);
        HYDRA_API void ClientInfo(const char* s);
        HYDRA_API void ClientWarn(const char* s);
        HYDRA_API void ClientError(const char* s);
        HYDRA_API void ClientCritical(const char* s);
    }

#endif

    template <typename EnumType>
    constexpr bool HasFlags(EnumType value, EnumType group)
    {
        using UnderlyingType = typename std::underlying_type<EnumType>::type;
        return (static_cast<UnderlyingType>(value) & static_cast<UnderlyingType>(group)) != 0;
    }

    template <typename... Args>
    constexpr size_t Hash(const Args&... args)
    {
        size_t seed = 0;
        (..., (seed ^= std::hash<std::decay_t<Args>>{}(args)+0x9e3779b9 + (seed << 6) + (seed >> 2)));
        return seed;
    }

    // Aligns 'size' up to the next multiple of 'alignment' (power of two).
    template<typename T>
    constexpr T AlignUp(T size, T alignment)
    {
        static_assert(std::is_integral<T>::value, "AlignUp() requires an integral type");
        HE_CORE_ASSERT(size >= 0, "'size' must be non-negative");
        HE_CORE_ASSERT(alignment != 0 && (alignment & (alignment - 1)) == 0, "Alignment must be a power of two");

        return (size + alignment - 1) & ~(alignment - 1);
    }

    template<typename T>
    using Scope = std::unique_ptr<T>;
    template<typename T, typename ... Args>
    constexpr Scope<T> CreateScope(Args&& ... args)
    {
        return std::make_unique<T>(std::forward<Args>(args)...);
    }

    template<typename T>
    using Ref = std::shared_ptr<T>;
    template<typename T, typename ... Args>
    constexpr Ref<T> CreateRef(Args&& ... args)
    {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }

    class Timestep
    {
    public:
        Timestep(float time = 0.0f) : m_Time(time) {}

        operator float() const { return m_Time; }

        float Seconds() const { return m_Time; }
        float Milliseconds() const { return m_Time * 1000.0f; }
    private:
        float m_Time;
    };
    
    class Timer
    {
    public:
        Timer() { Reset(); }
        void Reset() { m_Start = std::chrono::high_resolution_clock::now(); }
        float ElapsedSeconds() const { return std::chrono::duration_cast<std::chrono::duration<float>>(std::chrono::high_resolution_clock::now() - m_Start).count(); }
        float ElapsedMilliseconds() const { return (float)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - m_Start).count(); }
        float ElapsedMicroseconds() const { return (float)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - m_Start).count(); }
        float ElapsedNanoseconds() const { return (float)std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - m_Start).count();  }

    private:
        std::chrono::time_point<std::chrono::high_resolution_clock> m_Start;
    };

    struct Random
    {
        static bool Bool() 
        {
            static thread_local std::uniform_int_distribution<int> dist(0, 1);
            return dist(GetGenerator()) == 1;
        }

        static int Int() 
        {
            static thread_local std::uniform_int_distribution<int> dist(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
            return dist(GetGenerator());
        }

        static int Int(int min, int max) 
        {
            std::uniform_int_distribution<int> dist(min, max);
            return dist(GetGenerator());
        }

        // Float ranges (0.0f, 1.0)
        static float Float()
        { 
            static thread_local std::uniform_real_distribution<float> dist(0.0f, 1.0f);
            return dist(GetGenerator());
        }

        static float Float(float min, float max) 
        {
            std::uniform_real_distribution<float> dist(min, max);
            return dist(GetGenerator());
        }

        // Float ranges (0.0f, 1.0)
        static double Double() 
        {   
            static thread_local std::uniform_real_distribution<double> dist(0.0, 1.0);
            return dist(GetGenerator());
        }

        static double Double(double min, double max) 
        {
            std::uniform_real_distribution<double> dist(min, max);
            return dist(GetGenerator());
        }

        HYDRA_API static std::mt19937& GetGenerator() 
        {
            static thread_local std::mt19937 generator = CreateSeededGenerator();
            return generator;
        }

    private:
        static std::mt19937 CreateSeededGenerator() 
        {
            std::random_device rd;
            std::seed_seq seeds{
                rd(),
                rd(),
                static_cast<unsigned>(std::chrono::system_clock::now().time_since_epoch().count()),
                static_cast<unsigned>(reinterpret_cast<uintptr_t>(&seeds))
            };
            return std::mt19937(seeds);
        }
    };

    // Non-owning raw buffer
    struct Buffer
    {
        uint8_t* data = nullptr;
        uint64_t size = 0;

        Buffer() = default;

        Buffer(uint64_t size) { Allocate(size); }
        Buffer(const void* data, uint64_t pSize) : data((uint8_t*)data), size(pSize) {}
        Buffer(const Buffer&) = default;

        static Buffer Copy(Buffer other)
        {
            Buffer result(other.size);
            memcpy(result.data, other.data, other.size);
            return result;
        }

        void Allocate(uint64_t pSize)
        {
            Release();
            data = (uint8_t*)malloc(pSize);
            size = pSize;
        }

        void Release()
        {
            free(data);
            data = nullptr;
            size = 0;
        }

        template<typename T> T* As() { return (T*)data; }
        operator bool() const { return (bool)data; }
    };

    struct ScopedBuffer
    {
        ScopedBuffer(Buffer buffer) : m_Buffer(buffer) {}
        ScopedBuffer(uint64_t size) : m_Buffer(size) {}
        ~ScopedBuffer() { m_Buffer.Release(); }

        uint8_t* Data() { return m_Buffer.data; }
        uint64_t Size() { return m_Buffer.size; }

        template<typename T> T* As() { return m_Buffer.As<T>(); }
        operator bool() const { return m_Buffer; }
    private:
        Buffer m_Buffer;
    };

    class HYDRA_API Image
    {
    public:
        Image(const std::filesystem::path& filename, int desiredChannels = 4, bool flipVertically = false);
        Image(Buffer buffer, int desiredChannels = 4, bool flipVertically = false);
        Image(int width, int height, int channels, uint8_t* data);
        ~Image();

        Image(const Image&) = delete;
        Image& operator=(const Image&) = delete;

        Image(Image&& other) noexcept;
        Image& operator=(Image&& other) noexcept;


        static bool GetImageInfo(const std::filesystem::path& filePath, int& outWidth, int& outWeight, int& outChannels);
        static bool SaveAsPNG(const std::filesystem::path& filePath, int width, int height, int channels, const void* data, int strideInBytes);
        static bool SaveAsJPG(const std::filesystem::path& filePath, int width, int height, int channels, const void* data, int quality = 90);
        static bool SaveAsBMP(const std::filesystem::path& filePath, int width, int height, int channels, const void* data);

        bool isValid() const { return data != nullptr; }
        int GetWidth() const { return width; }
        int GetHeight() const { return height; }
        int GetChannels() const { return channels; }
        unsigned char* GetData() const { return data; }
        void SetData(uint8_t* data);
        uint8_t* ExtractData();

    private:
        uint8_t* data = nullptr;
        int width = 0;
        int height = 0;
        int channels = 0;
    };

    //////////////////////////////////////////////////////////////////////////
    // Event
    //////////////////////////////////////////////////////////////////////////

    struct CodeStrPair
    {
        uint16_t code;
        std::string_view codeStr;
    };

    using MouseCode = uint16_t;
    namespace MouseKey
    {
        enum : MouseCode
        {
            Button0, Button1, Button2, Button3, Button4, Button5, Button6, Button7,

            Count,

            Left = Button0,
            Right = Button1,
            Middle = Button2
        };

        HYDRA_API constexpr std::string_view ToString(MouseCode code);
        HYDRA_API constexpr MouseCode FromString(std::string_view code);
    }

    using JoystickCode = uint16_t;
    namespace Joystick
    {
        enum : JoystickCode
        {
            Joystick0,  Joystick1,  Joystick2,  Joystick3, Joystick4,  Joystick5, 
            Joystick6,  Joystick7,  Joystick8,  Joystick9, Joystick10, Joystick11, 
            Joystick12, Joystick13, Joystick14, Joystick15,

            Count
        };

        HYDRA_API constexpr std::string_view ToString(JoystickCode code);
        HYDRA_API constexpr JoystickCode FromString(std::string_view codeStr);
    }

    using GamepadCode = uint16_t;
    namespace GamepadButton
    {
        enum : GamepadCode
        {
            A, B, X, Y,
            LeftBumper, RightBumper, Back, 
            Start, Guide, LeftThumb, RightThumb,
            Up, Right, Down, Left,

            Count,

            Cross = A, Circle = B, Square = X, Triangle = Y
        };

        HYDRA_API constexpr std::string_view ToString(GamepadCode code);
        HYDRA_API constexpr GamepadCode FromString(std::string_view codeStr);
    }

    using GamepadAxisCode = uint16_t;
    namespace GamepadAxis
    {
        enum : GamepadAxisCode
        {
            Left, Right,

            Count
        };

        HYDRA_API constexpr std::string_view ToString(GamepadAxisCode code);
        HYDRA_API constexpr GamepadAxisCode FromString(std::string_view codeStr);
    }

    using KeyCode = uint16_t;
    namespace Key
    {
        enum : KeyCode 
        {
            Space, Apostrophe, Comma, Minus, Period, Slash,
            D0, D1, D2, D3, D4, D5, D6, D7, D8, D9,
            Semicolon, Equal,
            A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
            LeftBracket, Backslash, RightBracket, GraveAccent,
            World1, World2,
            Escape, Enter, Tab, Backspace, Insert, Delete, Right, Left, Down, Up, PageUp, PageDown, Home, End, CapsLock, ScrollLock, NumLock, PrintScreen, Pause,
            F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12, F13, F14, F15, F16, F17, F18, F19, F20, F21, F22, F23, F24, F25,
            KP0, KP1, KP2, KP3, KP4, KP5, KP6, KP7, KP8, KP9,
            KPDecimal, KPDivide, KPMultiply, KPSubtract, KPAdd, KPEnter, KPEqual,
            LeftShift, LeftControl, LeftAlt, LeftSuper, RightShift, RightControl, RightAlt, RightSuper, Menu,

            Count
        };

        HYDRA_API constexpr std::string_view ToString(KeyCode code);
        HYDRA_API constexpr KeyCode FromString(std::string_view code);
    }

    struct Cursor
    {
        enum class Mode : uint8_t
        {
            Normal,     // the regular arrow cursor
            Hidden,     // the cursor to become hidden when it is over a window but still want it to behave normally,
            Disabled    // unlimited mouse movement,This will hide the cursor and lock it to the specified window
        };

        Mode CursorMode;
    };

    enum class EventType : uint8_t
    {
        None = 0,
        WindowClose, WindowResize, WindowFocus, WindowLostFocus, WindowMoved, WindowDrop, WindowContentScale, WindowMaximize, WindowMinimized,
        KeyPressed, KeyReleased, KeyTyped,
        MouseButtonPressed, MouseButtonReleased, MouseMoved, MouseScrolled, MouseEnter,
        GamepadButtonPressed, GamepadButtonReleased, GamepadAxisMoved, GamepadConnected
    };

    enum EventCategory : uint8_t
    {
        EventCategoryNone = 0,
        EventCategoryApplication = BIT(0),
        EventCategoryInput = BIT(1),
        EventCategoryKeyboard = BIT(2),
        EventCategoryMouse = BIT(3),
        EventCategoryMouseButton = BIT(4),
        EventCategoryJoystick = BIT(5),
        EventCategoryGamepadButton = BIT(6),
        EventCategoryGamepadAxis = BIT(7),
    };

    HYDRA_API constexpr std::string_view ToString(EventType code);
    HYDRA_API constexpr EventType FromStringToEventType(std::string_view code);
    HYDRA_API constexpr std::string_view ToString(EventCategory code);
    HYDRA_API constexpr EventCategory FromStringToEventCategory(std::string_view code);

#define EVENT_CLASS_TYPE(type)  static EventType GetStaticType() { return EventType::type; }\
                                virtual EventType GetEventType() const override { return GetStaticType(); }\
                                virtual const char* GetName() const override { return #type; }
#define EVENT_CLASS_CATEGORY(category) virtual int GetCategoryFlags() const override { return category; }

    struct Event
    {
        virtual ~Event() = default;
       
        bool handled = false;

        virtual EventType GetEventType() const = 0;
        virtual const char* GetName() const = 0;
        virtual int GetCategoryFlags() const = 0;
        virtual std::string ToString() const { return GetName(); }

        bool IsInCategory(EventCategory category) { return GetCategoryFlags() & category; }
    };

    template<typename T, typename F>
    bool DispatchEvent(Event& event, const F& func)
    {
        if (event.GetEventType() == T::GetStaticType())
        {
            event.handled |= func(static_cast<T&>(event));
            return true;
        }
        return false;
    }

    inline std::ostream& operator<<(std::ostream& os, const Event& e) { return os << e.ToString(); }

    //////////////////////////////////////////////////////////////////////////
    // Application Events
    //////////////////////////////////////////////////////////////////////////

    struct WindowResizeEvent : public Event
    {
        uint32_t width, height;

        WindowResizeEvent(uint32_t pWidth, uint32_t pHeight) : width(pWidth), height(pHeight) {}

        std::string ToString() const override
        {
            std::stringstream ss;
            ss << "WindowResizeEvent: " << width << ", " << height;
            return ss.str();
        }

        EVENT_CLASS_TYPE(WindowResize)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)
    };

    struct WindowCloseEvent : public Event
    {
        WindowCloseEvent() = default;

        EVENT_CLASS_TYPE(WindowClose)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)
    };

    struct WindowDropEvent : public Event
    {
        const char** paths;
        int count;

        WindowDropEvent(const char** pPaths, int pPathCount) : paths(pPaths), count(pPathCount) {}

        std::string ToString() const override
        {
            std::stringstream ss;

            ss << "WindowDropEvent: " << paths << "\n";
            for (int i = 0; i < count; i++)
                ss << paths[i] << "\n";

            return ss.str();
        }

        EVENT_CLASS_TYPE(WindowDrop)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)
    };

    struct WindowContentScaleEvent : public Event
    {
        float scaleX, scaleY;

        WindowContentScaleEvent(float sx, float sy) : scaleX(sx), scaleY(sy) {}

        std::string ToString() const override
        {
            std::stringstream ss;
            ss << "WindowContentScaleEvent: " << scaleX << ", " << scaleY;
            return ss.str();
        }

        EVENT_CLASS_TYPE(WindowContentScale)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)
    };

    struct WindowMaximizeEvent : public Event
    {
        bool maximized;

        WindowMaximizeEvent(bool pMaximized) : maximized(pMaximized) {}

        std::string ToString() const override
        {
            std::stringstream ss;
            ss << "WindowMaximizeEvent: " << (maximized ? "maximized" : "restored");
            return ss.str();
        }

        EVENT_CLASS_TYPE(WindowMaximize)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)
    };

    struct WindowMinimizeEvent : public Event
    {
        bool minimized;

        WindowMinimizeEvent(bool minimized) : minimized(minimized) {}

        std::string ToString() const override
        {
            std::stringstream ss;
            ss << "WindowMinimizeEvent: " << (minimized ? "true" : "false");
            return ss.str();
        }

        EVENT_CLASS_TYPE(WindowMinimized)
        EVENT_CLASS_CATEGORY(EventCategoryApplication)
    };

    //////////////////////////////////////////////////////////////////////////
    // Keybord Events
    //////////////////////////////////////////////////////////////////////////

    struct KeyPressedEvent : public Event
    {
        KeyCode keyCode;
        bool isRepeat;

        KeyPressedEvent(const KeyCode pKeycode, bool pIsRepeat = false) : keyCode(pKeycode), isRepeat(pIsRepeat) {}

        std::string ToString() const override
        {
            std::stringstream ss;
            ss << "KeyPressedEvent: " << Key::ToString(keyCode) << " (repeat = " << (isRepeat ? "true" : "false") << ")";
            return ss.str();
        }

        EVENT_CLASS_TYPE(KeyPressed)
        EVENT_CLASS_CATEGORY(EventCategoryKeyboard | EventCategoryInput)
    };

    struct KeyReleasedEvent : public Event
    {
        KeyCode keyCode;

        KeyReleasedEvent(const KeyCode pKeyCode) : keyCode(pKeyCode) {}

        std::string ToString() const override
        {
            std::stringstream ss;
            ss << "KeyReleasedEvent: " << Key::ToString(keyCode);
            return ss.str();
        }

        EVENT_CLASS_CATEGORY(EventCategoryKeyboard | EventCategoryInput)
        EVENT_CLASS_TYPE(KeyReleased)
    };

    struct KeyTypedEvent : public Event
    {
        uint32_t codePoint;

        KeyTypedEvent(const uint32_t pCodePoint) : codePoint(pCodePoint) {}

        std::string ToString() const override
        {
            std::stringstream ss;
            ss << "KeyTypedEvent: " << static_cast<char>(codePoint);
            return ss.str();
        }

        EVENT_CLASS_TYPE(KeyTyped)
        EVENT_CLASS_CATEGORY(EventCategoryKeyboard | EventCategoryInput)
    };

    //////////////////////////////////////////////////////////////////////////
    // Mouse Events
    //////////////////////////////////////////////////////////////////////////	

    struct MouseMovedEvent : public Event
    {
        float x, y;

        MouseMovedEvent(const float pX, const float pY) : x(pX), y(pY) {}

        std::string ToString() const override
        {
            std::stringstream ss;
            ss << "MouseMovedEvent: " << x << ", " << y;
            return ss.str();
        }

        EVENT_CLASS_TYPE(MouseMoved)
        EVENT_CLASS_CATEGORY(EventCategoryInput | EventCategoryMouse)
    };

    struct MouseEnterEvent : public Event
    {
        bool entered;

        MouseEnterEvent(const bool pEntered) : entered(pEntered) {}

        std::string ToString() const override
        {
            std::stringstream ss;
            ss << "MouseEnterEvent: " << entered;
            return ss.str();
        }

        EVENT_CLASS_TYPE(MouseEnter)
        EVENT_CLASS_CATEGORY(EventCategoryInput | EventCategoryMouse)
    };

    struct MouseScrolledEvent : public Event
    {
        float xOffset, yOffset;

        MouseScrolledEvent(const float pXOffset, const float pYOffset) : xOffset(pXOffset), yOffset(pYOffset) {}

        std::string ToString() const override
        {
            std::stringstream ss;
            ss << "MouseScrolledEvent: " << xOffset << ", " << yOffset;
            return ss.str();
        }

        EVENT_CLASS_TYPE(MouseScrolled)
        EVENT_CLASS_CATEGORY(EventCategoryInput | EventCategoryMouse)
    };

    struct MouseButtonPressedEvent : public Event
    {
        MouseCode button;

        MouseButtonPressedEvent(const MouseCode pButton) : button(pButton) {}

        std::string ToString() const override
        {
            std::stringstream ss;
            ss << "MouseButtonPressedEvent: " << MouseKey::ToString(button);
            return ss.str();
        }

        EVENT_CLASS_CATEGORY(EventCategoryInput | EventCategoryMouse | EventCategoryMouseButton)
        EVENT_CLASS_TYPE(MouseButtonPressed)
    };

    struct MouseButtonReleasedEvent : public Event
    {
        MouseCode button;

        MouseButtonReleasedEvent(const MouseCode pButton) : button(pButton) {}

        std::string ToString() const override
        {
            std::stringstream ss;
            ss << "MouseButtonReleasedEvent: " << MouseKey::ToString(button);
            return ss.str();
        }

        EVENT_CLASS_CATEGORY(EventCategoryInput | EventCategoryMouse | EventCategoryMouseButton)
        EVENT_CLASS_TYPE(MouseButtonReleased)
    };

    //////////////////////////////////////////////////////////////////////////
    // Gamepad Events
    //////////////////////////////////////////////////////////////////////////

    struct GamepadAxisMovedEvent : public Event
    {
        JoystickCode joystickId = 0;
        GamepadAxisCode axisCode;
        float x, y;

        GamepadAxisMovedEvent(JoystickCode joystickCode, GamepadAxisCode axisCode, const float pX, const float pY) : joystickId(joystickCode), axisCode(axisCode), x(pX), y(pY) {}

        std::string ToString() const override
        {
            std::stringstream ss;
            ss << "GamepadAxisMovedEvent: Joystick : " << joystickId << ", value : " << x << "," << y;
            return ss.str();
        }

        EVENT_CLASS_TYPE(GamepadAxisMoved)
        EVENT_CLASS_CATEGORY(EventCategoryInput | EventCategoryJoystick | EventCategoryGamepadAxis)
    };

    struct GamepadButtonPressedEvent : public Event
    {
        JoystickCode joystickCode;
        GamepadCode button;

        GamepadButtonPressedEvent(JoystickCode pJoystickCode, const GamepadCode pButton) : joystickCode(pJoystickCode), button(pButton) {}

        virtual std::string ToString() const override
        {
            std::stringstream ss;
            ss << "GamepadButtonPressedEvent: Joystick : " << joystickCode << ", button : " << button;
            return ss.str();
        }

        EVENT_CLASS_CATEGORY(EventCategoryInput | EventCategoryJoystick | EventCategoryGamepadButton)
        EVENT_CLASS_TYPE(GamepadButtonPressed)
    };

    struct GamepadButtonReleasedEvent : public Event
    {
        JoystickCode joystickCode;
        GamepadCode button;

        GamepadButtonReleasedEvent(JoystickCode joystickCode, const GamepadCode button) : joystickCode(joystickCode), button(button) {}

        virtual std::string ToString() const override
        {
            std::stringstream ss;
            ss << "GamepadButtonReleasedEvent: Joystick : " << joystickCode << ", button : " << GamepadButton::ToString(button);
            return ss.str();
        }

        EVENT_CLASS_CATEGORY(EventCategoryInput | EventCategoryJoystick | EventCategoryGamepadButton)
        EVENT_CLASS_TYPE(GamepadButtonReleased)
    };

    struct GamepadConnectedEvent : public Event
    {
        JoystickCode joystickCode;
        bool connected;

        GamepadConnectedEvent(JoystickCode joystickCode, bool connected) : joystickCode(joystickCode), connected(connected) {}

        virtual std::string ToString() const override
        {
            std::stringstream ss;
            ss << "GamepadConnectedEvent: Joystick : " << joystickCode << ", state : " << (connected ? "Connected" : "Disconnected");
            return ss.str();
        }

        EVENT_CLASS_TYPE(GamepadConnected)
        EVENT_CLASS_CATEGORY(EventCategoryInput | EventCategoryJoystick)
    };

    //////////////////////////////////////////////////////////////////////////
    // Input
    //////////////////////////////////////////////////////////////////////////

    constexpr int c_MaxModifierCount = 4;

    struct KeyBindingDesc
    {
        std::string name;
        std::array<uint16_t, c_MaxModifierCount> modifiers;
        uint16_t code;
        EventType eventType;
        EventCategory eventCategory;
    };

    namespace Input {
    
        HYDRA_API bool IsKeyPressed(KeyCode key);
        HYDRA_API bool IsKeyReleased(KeyCode key);
        HYDRA_API bool IsKeyDown(KeyCode key);
        HYDRA_API bool IsKeyUp(KeyCode key);

        HYDRA_API bool IsMouseButtonPressed(MouseCode button);
        HYDRA_API bool IsMouseButtonReleased(MouseCode button);
        HYDRA_API bool IsMouseButtonDown(MouseCode button);
        HYDRA_API bool IsMouseButtonUp(MouseCode button);
        HYDRA_API std::pair<float, float> GetMousePosition();
        HYDRA_API float GetMouseX();
        HYDRA_API float GetMouseY();

        HYDRA_API bool IsGamepadButtonPressed(JoystickCode code, GamepadCode key);
        HYDRA_API bool IsGamepadButtonReleased(JoystickCode code, GamepadCode key);
        HYDRA_API bool IsGamepadButtonDown(JoystickCode code, GamepadCode key);
        HYDRA_API bool IsGamepadButtonUp(JoystickCode code, GamepadCode key);
        HYDRA_API std::pair<float, float> GetGamepadLeftAxis(JoystickCode code);
        HYDRA_API std::pair<float, float> GetGamepadRightAxis(JoystickCode code);
        HYDRA_API void SetDeadZoon(float value);

        HYDRA_API void SetCursorMode(Cursor::Mode mode);
        HYDRA_API Cursor::Mode GetCursorMode();

        HYDRA_API bool Triggered(const std::string_view& name);
        HYDRA_API bool RegisterKeyBinding(const KeyBindingDesc& action);
        HYDRA_API const std::map<uint64_t, KeyBindingDesc>& GetKeyBindings();
        HYDRA_API void SerializeKeyBindings(const std::filesystem::path& filePath);
        HYDRA_API bool DeserializeKeyBindings(const std::filesystem::path& filePath);
    }

    //////////////////////////////////////////////////////////////////////////
    // Window
    //////////////////////////////////////////////////////////////////////////

    struct SwapChainDesc
    {
        nvrhi::Format swapChainFormat = nvrhi::Format::RGBA8_UNORM;
        uint32_t refreshRate = 0;
        uint32_t swapChainBufferCount = 3;
        uint32_t swapChainSampleCount = 1;
        uint32_t swapChainSampleQuality = 0;
        uint32_t maxFramesInFlight = 2;
        uint32_t backBufferWidth = 0;
        uint32_t backBufferHeight = 0;
        bool allowModeSwitch = true;
        bool vsync = true;

#if NVRHI_HAS_D3D11 || NVRHI_HAS_D3D12
        DXGI_USAGE swapChainUsage = DXGI_USAGE_SHADER_INPUT | DXGI_USAGE_RENDER_TARGET_OUTPUT;
#endif
    };

    struct WindowDesc
    {
        std::string title = "Hydra Engine";
        std::string iconFilePath;
        uint32_t width = 0, height = 0;
        int minWidth = -1, minHeight = -1;
        int maxWidth = -1, maxHeight = -1;
        float sizeRatio = 0.7f;				    // Percentage of screen size to use when width/height is 0
        bool resizeable = true;
        bool customTitlebar = false;
        bool decorated = true;
        bool centered = true;
        bool fullScreen = false;
        bool maximized = false;
        bool perMonitorDPIAware = true;
        bool scaleToMonitor = true;
        bool startVisible = true;
        bool setCallbacks = true;

        SwapChainDesc swapChainDesc;
    };

    // internal
    struct InputState
    {
        InputState()
        {
            keyDownPrevFrame.set();
            keyUpPrevFrame.set();
            mouseButtonDownPrevFrame.set();
            mouseButtonUpPrevFrame.set();

            for (auto& gamepad : gamepadButtonDownPrevFrame) gamepad.set();
            for (auto& gamepad : gamepadButtonUpPrevFrame) gamepad.set();
        }

        Cursor cursor;

        std::bitset<Key::Count> keyDownPrevFrame;
        std::bitset<Key::Count> keyUpPrevFrame;

        std::bitset<MouseKey::Count> mouseButtonDownPrevFrame;
        std::bitset<MouseKey::Count> mouseButtonUpPrevFrame;

        std::bitset<GamepadButton::Count> gamepadButtonDownPrevFrame[Joystick::Count];
        std::bitset<GamepadButton::Count> gamepadButtonUpPrevFrame[Joystick::Count];

        std::bitset<GamepadButton::Count> gamepadEventButtonDownPrevFrame[Joystick::Count];
        std::bitset<GamepadButton::Count> gamepadEventButtonUpPrevFrame[Joystick::Count];

        float deadZoon = 0.1f;
    };

    struct SwapChain
    {
        SwapChainDesc desc;
        void* windowHandle = nullptr;
        std::vector<nvrhi::FramebufferHandle> swapChainFramebuffers;
        nvrhi::DeviceHandle nvrhiDevice;
        bool isVSync = false;

        virtual ~SwapChain() = default;

        HYDRA_API nvrhi::IFramebuffer* GetCurrentFramebuffer();
        HYDRA_API nvrhi::IFramebuffer* GetFramebuffer(uint32_t index);
        HYDRA_API void UpdateSize();

        HYDRA_API void ResetBackBuffers();
        HYDRA_API void ResizeBackBuffers();

        virtual nvrhi::ITexture* GetCurrentBackBuffer() = 0;
        virtual nvrhi::ITexture* GetBackBuffer(uint32_t index) = 0;
        virtual uint32_t GetCurrentBackBufferIndex() = 0;
        virtual uint32_t GetBackBufferCount() = 0;
        virtual void ResizeSwapChain(uint32_t width, uint32_t height) = 0;
        virtual bool Present() = 0;
        virtual bool BeginFrame() = 0;
    };
    
    using WindowEventCallback = std::function<void(Event&)>;

    struct Window
    {
        void* handle = nullptr;
        WindowDesc desc;
        WindowEventCallback eventCallback = 0;
        InputState inputData;
        bool isTitleBarHit = false;
        int prevPosX = 0, prevPosY = 0, prevWidth = 0, prevHeight = 0;
        SwapChain* swapChain = nullptr;

        HYDRA_API ~Window();
        HYDRA_API void Init(const WindowDesc& windowDesc);
        HYDRA_API void* GetNativeHandle();
        HYDRA_API void SetTitle(const std::string_view& title);
        HYDRA_API void Maximize();
        HYDRA_API void Minimize();
        HYDRA_API void Restore();
        HYDRA_API bool IsMaximize();
        HYDRA_API bool IsMinimized();
        HYDRA_API bool IsFullScreen();
        HYDRA_API bool ToggleScreenState();
        HYDRA_API void Focus();
        HYDRA_API bool IsFocused();
        HYDRA_API void Show();
        HYDRA_API void Hide();
        HYDRA_API std::pair<float, float> GetWindowContentScale();
        HYDRA_API void UpdateEvent();
        uint32_t GetWidth() const { return desc.width; }
        uint32_t GetHeight() const { return desc.height; }
    };

    //////////////////////////////////////////////////////////////////////////
    // RHI
    //////////////////////////////////////////////////////////////////////////

    namespace RHI {

        consteval uint8_t BackendCount()
        {
            uint8_t backendCount = 0;
#if NVRHI_HAS_D3D11
            backendCount++;
#endif
#if NVRHI_HAS_D3D12
            backendCount++;
#endif
#if NVRHI_HAS_VULKAN
            backendCount++;
#endif
            return backendCount;
        }

        struct DeviceInstanceDesc
        {
            bool enableDebugRuntime = false;
            bool enableWarningsAsErrors = false;
            bool enableGPUValidation = false; // DX12 only 
            bool headlessDevice = false;
            bool logBufferLifetime = false;
            bool enableHeapDirectlyIndexed = false; // Allows ResourceDescriptorHeap on DX12

#if NVRHI_HAS_VULKAN
            std::string vulkanLibraryName;
            std::vector<std::string> requiredVulkanInstanceExtensions;
            std::vector<std::string> requiredVulkanLayers;
            std::vector<std::string> optionalVulkanInstanceExtensions;
            std::vector<std::string> optionalVulkanLayers;
#endif
        };

        struct DeviceDesc : public DeviceInstanceDesc
        {
            std::array<nvrhi::GraphicsAPI, BackendCount()> api;

            bool enableNvrhiValidationLayer = false;
            bool enableRayTracingExtensions = false;
            bool enableComputeQueue = false;
            bool enableCopyQueue = false;
            int adapterIndex = -1;

#if NVRHI_HAS_D3D11 || NVRHI_HAS_D3D12
            D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_1;
#endif

#if NVRHI_HAS_VULKAN
            std::vector<std::string> requiredVulkanDeviceExtensions;
            std::vector<std::string> optionalVulkanDeviceExtensions;
            std::vector<size_t> ignoredVulkanValidationMessageLocations;
            std::function<void(VkDeviceCreateInfo&)> deviceCreateInfoCallback;
            void* physicalDeviceFeatures2Extensions = nullptr;
#endif

            DeviceDesc() { api.fill(nvrhi::GraphicsAPI(-1)); }
        };

        struct AdapterInfo
        {
            using UUID = std::array<uint8_t, 16>;
            using LUID = std::array<uint8_t, 8>;

            std::string name;
            uint32_t vendorID = 0;
            uint32_t deviceID = 0;
            uint64_t dedicatedVideoMemory = 0;

            std::optional<UUID> uuid;
            std::optional<LUID> luid;

#if NVRHI_HAS_D3D11 || NVRHI_HAS_D3D12
            nvrhi::RefCountPtr<IDXGIAdapter> dxgiAdapter;
#endif

#if NVRHI_HAS_VULKAN
            VkPhysicalDevice vkPhysicalDevice = nullptr;
#endif
        };

        struct DefaultMessageCallback : public nvrhi::IMessageCallback
        {
            static DefaultMessageCallback& GetInstance();

            void message(nvrhi::MessageSeverity severity, const char* messageText) override;
        };

        struct DeviceManager
        {
            DeviceDesc desc;
            bool isNvidia = false;
            bool instanceCreated = false;

            HYDRA_API virtual ~DeviceManager() = default;
            HYDRA_API bool CreateDevice(const DeviceDesc& desc);
            HYDRA_API bool CreateInstance(const DeviceInstanceDesc& desc);
            virtual SwapChain* CreateSwapChain(const SwapChainDesc& swapChainDesc, void* windowHandle) = 0;
            virtual bool EnumerateAdapters(std::vector<AdapterInfo>& outAdapters) = 0;
            virtual nvrhi::IDevice* GetDevice() const = 0;
            virtual const char* GetRendererString() const = 0;
            virtual bool CreateInstanceInternal() = 0;
            virtual bool CreateDevice() = 0;
            virtual void ReportLiveObjects() {}

#if NVRHI_HAS_VULKAN

            virtual bool IsVulkanInstanceExtensionEnabled(const char* extensionName) const { return false; }
            virtual bool IsVulkanDeviceExtensionEnabled(const char* extensionName) const { return false; }
            virtual bool IsVulkanLayerEnabled(const char* layerName) const { return false; }
            virtual void GetEnabledVulkanInstanceExtensions(std::vector<std::string>& extensions) const {}
            virtual void GetEnabledVulkanDeviceExtensions(std::vector<std::string>& extensions) const {}
            virtual void GetEnabledVulkanLayers(std::vector<std::string>& layers) const {}
#endif
        };

        struct DeviceContext
        {
            std::vector<DeviceManager*> managers;

            HYDRA_API ~DeviceContext();
        };

        HYDRA_API DeviceManager* CreateDeviceManager(const DeviceDesc& desc);
        HYDRA_API DeviceManager* GetDeviceManager(uint32_t index = 0);
        HYDRA_API nvrhi::DeviceHandle GetDevice(uint32_t index = 0);
        HYDRA_API void TryCreateDefaultDevice();

#if NVRHI_HAS_D3D11
        HYDRA_API DeviceManager* CreateD3D11();
#endif
#if NVRHI_HAS_D3D12
        HYDRA_API DeviceManager* CreateD3D12();
#endif
#if NVRHI_HAS_VULKAN

        HYDRA_API DeviceManager* CreateVULKAN();
#endif

        struct StaticShader
        {
            Buffer dxbc;
            Buffer dxil;
            Buffer spirv;
        };

        struct ShaderMacro
        {
            std::string_view name;
            std::string_view definition;
        };

        HYDRA_API nvrhi::ShaderHandle CreateStaticShader(nvrhi::IDevice* device, StaticShader staticShader, const std::vector<ShaderMacro>* pDefines, const nvrhi::ShaderDesc& desc);
        HYDRA_API nvrhi::ShaderLibraryHandle CreateShaderLibrary(nvrhi::IDevice* device, StaticShader staticShader, const std::vector<ShaderMacro>* pDefines);
    }


    //////////////////////////////////////////////////////////////////////////
    // Modules
    //////////////////////////////////////////////////////////////////////////

    // Use these callbacks in your module:
    // EXPORT void OnModuleLoaded() {}
    // EXPORT void OnModuleShutdown() {}

    namespace Modules {

        class SharedLib
        {
        public:
            void* handle{ nullptr };

            SharedLib(const SharedLib&) = delete;
            SharedLib& operator=(const SharedLib&) = delete;

            SharedLib(SharedLib&& other) noexcept : handle(other.handle) { other.handle = nullptr; }
            SharedLib& operator=(SharedLib&& other) noexcept { if (this != &other) std::swap(handle, other.handle); return *this; }

            explicit SharedLib(const std::filesystem::path& filePath, bool decorations = false)
            {
                std::string finalPath = decorations ? filePath.string() + c_SharedLibExtension : filePath.string();
                handle = Open(finalPath.c_str());
                if (!handle)
                {
                    HE_CORE_ERROR("SharedLib : Could not load library {} : {}", finalPath, GetError());
                }
            }

            ~SharedLib() { if (handle) Close(handle); }

            bool IsLoaded() { return handle != nullptr; }
            bool HasSymbol(const std::string_view& symbol) const noexcept { return !handle || !symbol.empty() ? false : GetSymbolAddress(handle, symbol.data()) != nullptr; }
            template<typename T> T& GetVariable(const std::string_view& symbolName) const { return *reinterpret_cast<T*>(GetSymbol(symbolName)); }
            void* GetSymbol(const std::string_view& symbolName) const
            {
                HE_ASSERT(handle, "Modules::SharedLib::GetSymbol failed : The dynamic library handle is null");

                auto symbol = GetSymbolAddress(handle, symbolName.data());

                if (!symbol)
                {
                    HE_ERROR("SharedLib::GetSymbol : Could not get symbol {} : {}", symbolName, GetError());
                }

                return symbol;
            }

            template<typename T> 
            T* GetFunction(const std::string_view& symbolName) const 
            {
#           if (defined(__GNUC__) && __GNUC__ >= 8)
#               pragma GCC diagnostic push
#               pragma GCC diagnostic ignored "-Wcast-function-type"
#           endif
                return reinterpret_cast<T*>(GetSymbol(symbolName.data()));
#           if (defined(__GNUC__) && __GNUC__ >= 8)
#               pragma GCC diagnostic pop
#           endif
            }
            
        private:
            static void* Open(const char* path) noexcept;
            static void* GetSymbolAddress(void* handle, const char* name) noexcept;
            static void Close(void* handle) noexcept;
            static std::string GetError() noexcept;
        };

        using ModuleHandle = uint64_t;

        struct ModuleData
        {
            std::string name;
            SharedLib lib;

            uint32_t loadOrder;
            inline static uint32_t currentLoadOrder = 0;

            ModuleData() = delete;
            ModuleData(const std::filesystem::path& filePath) : name(filePath.stem().string()), lib(filePath), loadOrder(currentLoadOrder++) {}
        };

        struct ModulesContext
        {
            std::unordered_map<ModuleHandle, Ref<ModuleData>> modules;

            HYDRA_API ~ModulesContext();
        };

        HYDRA_API bool LoadModule(const std::filesystem::path& filePath);
        HYDRA_API bool IsModuleLoaded(ModuleHandle handle);
        HYDRA_API bool UnloadModule(ModuleHandle handle);
        HYDRA_API Ref<ModuleData> GetModuleData(ModuleHandle handle);

        // TODO:
        // - Optimizing memory usage with compact data structures.
        // - Is using a hash of the relative file path a good approach for generating ModuleHandle?  
    }

    //////////////////////////////////////////////////////////////////////////
    // Plugin
    //////////////////////////////////////////////////////////////////////////
    
    namespace Plugins {

        inline constexpr const char* c_PluginDescriptorExtension = ".hplugin";
        
        using PluginHandle = uint64_t;

        struct PluginDesc
        {
            std::string name;
            std::string description;
            std::string URL;
            bool reloadable = false;
            bool enabledByDefault = false;
            std::vector<std::string> modules;	// the base name of module without extension
            std::vector<std::string> plugins;	// Plugins used by this plugin 
        };

        struct Plugin
        {
            PluginDesc desc;
            std::filesystem::path descFilePath;
            bool enabled = false;		  

            Plugin(PluginDesc pDesc) : desc(pDesc) {}

            std::filesystem::path BaseDirectory() const { return descFilePath.parent_path(); }
            std::filesystem::path BinariesDirectory() const { return BaseDirectory() / "Binaries"; }
            std::filesystem::path AssetsDirectory() const { return BaseDirectory() / "Assets"; }
            std::filesystem::path SourceDirectory() const { return BaseDirectory() / "Source"; }
        };

        struct PluginContext
        {
            std::unordered_map<PluginHandle, Ref<Plugin>> plugins;
        };

        HYDRA_API void LoadPluginsInDirectory(const std::filesystem::path& directory);
        HYDRA_API void LoadPlugin(const std::filesystem::path& descriptor);
        HYDRA_API void LoadPlugin(PluginHandle handle);
        HYDRA_API bool UnloadPlugin(PluginHandle handle);
        HYDRA_API void ReloadPlugin(PluginHandle handle);
        HYDRA_API const Ref<Plugin> GetPlugin(PluginHandle handle);

        // TODO:
        // - Handling circular dependencies to avoid infinite recursion.
        // - Optimizing memory usage with compact data structures.
        // - Find a better way to generate PluginHandle. The current approach hashes the plugin name,  
        //   which does not scale well and may lead to collisions.
    }

    //////////////////////////////////////////////////////////////////////////
    // Layer
    //////////////////////////////////////////////////////////////////////////
    
    struct FrameInfo
    {
        Timestep ts;
        nvrhi::IFramebuffer* fb;
    };

    class Layer
    {
    public:
        virtual ~Layer() = default;

        virtual void OnAttach() {}
        virtual void OnDetach() {}
        virtual void OnEvent(Event& event) {}
        virtual void OnBegin(const FrameInfo& info) {}
        virtual void OnUpdate(const FrameInfo& info) {}
        virtual void OnEnd(const FrameInfo& info) {}
    };

    class LayerStack
    {
    public:
        LayerStack() = default;
        HYDRA_API ~LayerStack();

        void PushLayer(Layer* layer);
        void PushOverlay(Layer* overlay);
        
        void PopLayer(Layer* layer);
        void PopOverlay(Layer* overlay);

        std::vector<Layer*>::iterator begin() { return m_Layers.begin(); }
        std::vector<Layer*>::iterator end() { return m_Layers.end(); }
        std::vector<Layer*>::reverse_iterator rbegin() { return m_Layers.rbegin(); }
        std::vector<Layer*>::reverse_iterator rend() { return m_Layers.rend(); }

        std::vector<Layer*>::const_iterator begin() const { return m_Layers.begin(); }
        std::vector<Layer*>::const_iterator end()	const { return m_Layers.end(); }
        std::vector<Layer*>::const_reverse_iterator rbegin() const { return m_Layers.rbegin(); }
        std::vector<Layer*>::const_reverse_iterator rend() const { return m_Layers.rend(); }
    private:
        std::vector<Layer*> m_Layers;
        uint32_t m_LayerInsertIndex = 0;
    };

    //////////////////////////////////////////////////////////////////////////
    // Application
    //////////////////////////////////////////////////////////////////////////

    struct ApplicationCommandLineArgs
    {
        char** args = nullptr;
        int count = 0;

        const char* operator[](int index) const { HE_CORE_ASSERT(index < count); return args[index]; }
    };

    struct ApplicationDesc
    {
        WindowDesc windowDesc;
        RHI::DeviceDesc deviceDesc;
        ApplicationCommandLineArgs commandLineArgs;
        std::filesystem::path workingDirectory;
        bool createDefaultDevice = true;
        uint32_t workersNumber = std::thread::hardware_concurrency() - 1;
        const char* logFile = "HE";
    };

    struct Stats
    {
        float CPUMainTime;
        uint32_t FPS;
    };

    struct ApplicationContext
    {
        ApplicationDesc applicatoinDesc;
        RHI::DeviceContext deviceContext;
        Window mainWindow;
        std::map<uint64_t, KeyBindingDesc> keyBindings;
        LayerStack layerStack;
        Modules::ModulesContext modulesContext;
        Plugins::PluginContext pluginContext;

        Stats appStats;
        bool running = true;
        float lastFrameTime = 0.0f;
        float averageFrameTime = 0.0;
        float averageTimeUpdateInterval = 0.5;
        float frameTimeSum = 0.0;
        int numberOfAccumulatedFrames = 0;
    
        tf::Executor executor;
        uint32_t mainThreadMaxJobsPerFrame = 1;
        std::queue<std::function<void()>> mainThreadQueue;
        std::mutex mainThreadQueueMutex;

        inline static bool s_ApplicationRunning = true;
        inline static ApplicationContext* s_Instance = nullptr;
        
        HYDRA_API ApplicationContext(const ApplicationDesc& desc);
        HYDRA_API void Run();
    };

    HYDRA_API ApplicationContext& GetAppContext();

    namespace Application {

        HYDRA_API void Restart();
        HYDRA_API void Shutdown();
        HYDRA_API bool IsApplicationRunning();
        HYDRA_API void PushLayer(Layer* overlay);
        HYDRA_API void PushOverlay(Layer* layer);
        HYDRA_API void PopLayer(Layer* layer);
        HYDRA_API void PopOverlay(Layer* overlay);
        HYDRA_API float GetTime();
        HYDRA_API const Stats& GetStats();
        HYDRA_API const ApplicationDesc& GetApplicationDesc();
        HYDRA_API float GetAverageFrameTimeSeconds();
        HYDRA_API float GetLastFrameTimestamp();
        HYDRA_API void SetFrameTimeUpdateInterval(float seconds);
        HYDRA_API Window& GetWindow();
    }

    ApplicationContext* CreateApplication(ApplicationCommandLineArgs args);
    int Main(int argc, char** argv);

    //////////////////////////////////////////////////////////////////////////
    // Jops
    //////////////////////////////////////////////////////////////////////////

    namespace Jops {

        using Taskflow = tf::Taskflow;
        using Task = tf::Task;
        using Executor = tf::Executor;
        using Future = tf::Future<void>;

        HYDRA_API std::future<void> SubmitTask(const std::function<void()>& function) { return GetAppContext().executor.async(function);  }
        HYDRA_API Future RunTaskflow(Taskflow& taskflow)                              { return GetAppContext().executor.run(taskflow);    }
        HYDRA_API void WaitForAll()                                                   { GetAppContext().executor.wait_for_all();          }
        HYDRA_API void SetMainThreadMaxJobsPerFrame(uint32_t max)                     { GetAppContext().mainThreadMaxJobsPerFrame = max; }
        HYDRA_API void SubmitToMainThread(const std::function<void()>& function)
        {
            auto& c = GetAppContext();
            std::scoped_lock<std::mutex> lock(c.mainThreadQueueMutex);
            c.mainThreadQueue.push(function);
        }
    }
    
    //////////////////////////////////////////////////////////////////////////
    // Utils
    //////////////////////////////////////////////////////////////////////////

    namespace FileSystem {

        enum class AppDataType
        {
            Roaming,
            Local
        };

        HYDRA_API bool Delete(const std::filesystem::path& path);
        HYDRA_API bool Rename(const std::filesystem::path& oldPath, const std::filesystem::path& newPath);
        HYDRA_API bool Copy(const std::filesystem::path& from, const std::filesystem::path& to, std::filesystem::copy_options options = std::filesystem::copy_options::recursive);
        HYDRA_API bool Open(const std::filesystem::path& path);
        HYDRA_API std::vector<uint8_t> ReadBinaryFile(const std::filesystem::path& filePath);
        HYDRA_API bool ReadBinaryFile(const std::filesystem::path& filePath, Buffer buffer);
        HYDRA_API std::string ReadTextFile(const std::filesystem::path& filePath);
        HYDRA_API bool ConvertBinaryToHeader(const std::filesystem::path& inputFileName, const std::filesystem::path& outputFileName, const std::string& arrayName);
        HYDRA_API bool GenerateFileWithReplacements(const std::filesystem::path& input, const std::filesystem::path& output, const std::initializer_list<std::pair<std::string_view, std::string_view>>& replacements);
        HYDRA_API bool ExtractZip(const std::filesystem::path& zipPath, const std::filesystem::path& outputDir);
        HYDRA_API std::filesystem::path GetAppDataPath(const std::string& appName, AppDataType type = AppDataType::Roaming);
    }

    namespace FileDialog {

        HYDRA_API std::filesystem::path OpenFile(std::initializer_list<std::pair<std::string_view, std::string_view>> filters);
        HYDRA_API std::filesystem::path SaveFile(std::initializer_list<std::pair<std::string_view, std::string_view>> filters);
        HYDRA_API std::filesystem::path SelectFolder();
    }

    namespace OS {

        HYDRA_API void SetEnvVar(const char* var, const char* value);
        HYDRA_API void RemoveEnvVar(const char* var);
    }
}
