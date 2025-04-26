#pragma once

//////////////////////////////////////////////////////////////////////////
// PlatformDetection
//////////////////////////////////////////////////////////////////////////

#if defined(_WIN32)
    #if defined(_WIN64)
        #define HE_PLATFORM_WINDOWS
        constexpr const char* c_SharedLibExtension = ".dll";
        constexpr const char* c_System = "Windows";
		constexpr const char* c_Architecture = "x86_64";
		constexpr const char* c_ExecutableExtension = ".exe";
    #else
        #error "x86 Builds are not supported!"
    #endif
#elif defined(__linux__)
    #define HE_PLATFORM_LINUX
    constexpr const char* c_SharedLibExtension = ".so";
    constexpr const char* c_System = "Linux";
    constexpr const char* c_Architecture = "x86_64";
	constexpr const char* c_ExecutableExtension = "";
#else
    #error "Unknown platform!"
#endif


//////////////////////////////////////////////////////////////////////////
// Macros
//////////////////////////////////////////////////////////////////////////

#if defined(HE_BUILD_SHAREDLIB)
#   if defined(_MSC_VER)
#       define HYDRA_API __declspec(dllexport)
#   elif defined(__GNUC__)
#       define HYDRA_API __attribute__((visibility("default")))
#   else
#       define HYDRA_API
#       pragma warning "Unknown dynamic link import/export semantics."
#   endif
#elif defined(HE_IMPORT_SHAREDLIB)
#   if defined(_MSC_VER)
#       define HYDRA_API __declspec(dllimport)
#   else
#       define HYDRA_API
#   endif
#else
#   define HYDRA_API
#endif


#ifdef _MSC_VER
#	define EXPORT extern "C" __declspec(dllexport)
#elif defined(__GNUC__)
#	define EXPORT extern "C" __attribute__((visibility("default")))
#else
#	define EXPORT extern "C"
#endif

#if HE_DEBUG
	constexpr const char* c_BuildConfig = "Debug";
#   define HE_ENABLE_ASSERTS
#   define HE_ENABLE_VERIFY
#	define HE_ENABLE_LOGGING
#elif HE_RELEASE
	constexpr const char* c_BuildConfig = "Release";
#   define HE_ENABLE_VERIFY
#	define HE_ENABLE_LOGGING
#elif HE_PROFILE
	constexpr const char* c_BuildConfig = "Profile";
#   define HE_ENABLE_VERIFY
#	define HE_ENABLE_LOGGING
#else
	constexpr const char* c_BuildConfig = "Dist";
#endif

#define HE_EXPAND_MACRO(x) x
#define HE_STRINGIFY_MACRO(x) #x
#define BIT(x) (1 << x)
#define HE_BIND_EVENT_FN(fn) [this](auto&&... args) -> decltype(auto) { return this->fn(std::forward<decltype(args)>(args)...); }

#define HE_ENUM_CLASS_FLAG_OPERATORS(T)                                     \
    inline T operator | (T a, T b) { return T(uint32_t(a) | uint32_t(b)); } \
    inline T operator & (T a, T b) { return T(uint32_t(a) & uint32_t(b)); } \
    inline T operator ^ (T a, T b) { return T(uint32_t(a) ^ uint32_t(b)); } \
    inline T operator ~ (T a) { return T(~uint32_t(a)); }                   \
    inline T& operator |= (T& a, T b) { a = a | b; return a; }              \
    inline T& operator &= (T& a, T b) { a = a & b; return a; }              \
    inline T& operator ^= (T& a, T b) { a = a ^ b; return a; }              \
    inline bool operator !(T a) { return uint32_t(a) == 0; }                \
    inline bool operator ==(T a, uint32_t b) { return uint32_t(a) == b; }   \
    inline bool operator !=(T a, uint32_t b) { return uint32_t(a) != b; }

#define NOT_YET_IMPLEMENTED() HE_CORE_ERROR("{0}, {1}, {2} not implemented yet\n", __FILE__, __LINE__ ,__func__); HE_CORE_VERIFY(false)

//////////////////////////////////////////////////////////////////////////
// Log
//////////////////////////////////////////////////////////////////////////

#ifdef HE_ENABLE_LOGGING
	#define HE_CORE_TRACE(...)    HE::Log::CoreTrace(std::format(__VA_ARGS__).c_str())
	#define HE_CORE_INFO(...)     HE::Log::CoreInfo(std::format(__VA_ARGS__).c_str())
	#define HE_CORE_WARN(...)     HE::Log::CoreWarn(std::format(__VA_ARGS__).c_str())
	#define HE_CORE_ERROR(...)    HE::Log::CoreError(std::format(__VA_ARGS__).c_str())
	#define HE_CORE_CRITICAL(...) HE::Log::CoreCritical(std::format(__VA_ARGS__).c_str())

	#define HE_TRACE(...)         HE::Log::ClientTrace(std::format(__VA_ARGS__).c_str()) 
	#define HE_INFO(...)          HE::Log::ClientInfo(std::format(__VA_ARGS__).c_str())
	#define HE_WARN(...)          HE::Log::ClientWarn(std::format(__VA_ARGS__).c_str())
	#define HE_ERROR(...)         HE::Log::ClientError(std::format(__VA_ARGS__).c_str())
	#define HE_CRITICAL(...) 	  HE::Log::ClientCritical(std::format(__VA_ARGS__).c_str())
#else
	#define HE_CORE_TRACE(...)    
	#define HE_CORE_INFO(...)     
	#define HE_CORE_WARN(...)     
	#define HE_CORE_ERROR(...)    
	#define HE_CORE_CRITICAL(...) 
	
	#define HE_TRACE(...)         
	#define HE_INFO(...)          
	#define HE_WARN(...)          
	#define HE_ERROR(...)         
	#define HE_CRITICAL(...)   
#endif


//////////////////////////////////////////////////////////////////////////
// Assert && Verify
//////////////////////////////////////////////////////////////////////////

#if defined(HE_PLATFORM_WINDOWS)
#	define HE_DEBUGBREAK() __debugbreak()
#elif defined(HE_PLATFORM_LINUX)
#	include <signal.h>
#	define HE_DEBUGBREAK() raise(SIGTRAP)
#else
#	error "Platform doesn't support debugbreak yet!"
#endif

#define HE_INTERNAL_ASSERT_IMPL(type, check, msg, ...) { if(!(check)) { HE##type##ERROR(msg, __VA_ARGS__); HE_DEBUGBREAK(); } }
#define HE_INTERNAL_ASSERT_WITH_MSG(type, check, ...) HE_INTERNAL_ASSERT_IMPL(type, check, "Check failed: {0}", __VA_ARGS__)
#define HE_INTERNAL_ASSERT_NO_MSG(type, check) HE_INTERNAL_ASSERT_IMPL(type, check, "Check '{0}' failed at {1}:{2}", HE_STRINGIFY_MACRO(check), std::filesystem::path(__FILE__).filename().string(), __LINE__)
#define HE_INTERNAL_ASSERT_GET_MACRO_NAME(arg1, arg2, macro, ...) macro
#define HE_INTERNAL_ASSERT_GET_MACRO(...) HE_EXPAND_MACRO( HE_INTERNAL_ASSERT_GET_MACRO_NAME(__VA_ARGS__, HE_INTERNAL_ASSERT_WITH_MSG, HE_INTERNAL_ASSERT_NO_MSG) )

#ifdef HE_ENABLE_ASSERTS
#	define HE_ASSERT(...) HE_EXPAND_MACRO( HE_INTERNAL_ASSERT_GET_MACRO(__VA_ARGS__)(_, __VA_ARGS__) )
#	define HE_CORE_ASSERT(...) HE_EXPAND_MACRO( HE_INTERNAL_ASSERT_GET_MACRO(__VA_ARGS__)(_CORE_, __VA_ARGS__) )
#else
#	define HE_ASSERT(...)
#	define HE_CORE_ASSERT(...)
#endif

#ifdef HE_ENABLE_VERIFY
#	define HE_VERIFY(...) HE_EXPAND_MACRO( HE_INTERNAL_ASSERT_GET_MACRO(__VA_ARGS__)(_, __VA_ARGS__) )
#	define HE_CORE_VERIFY(...) HE_EXPAND_MACRO( HE_INTERNAL_ASSERT_GET_MACRO(__VA_ARGS__)(_CORE_, __VA_ARGS__) )
#else
#	define HE_VERIFY(...)
#	define HE_CORE_VERIFY(...)
#endif

//////////////////////////////////////////////////////////////////////////
// Profiler
//////////////////////////////////////////////////////////////////////////

#if HE_PROFILE 
#	ifndef TRACY_ENABLE
#		define TRACY_ENABLE
#	endif
#	include "tracy/Tracy.hpp"
#	define HE_PROFILE_SCOPE(name) ZoneScopedN(name)
#	define HE_PROFILE_SCOPE_COLOR(color) ZoneScopedC(color)
#	define HE_PROFILE_SCOPE_NC(name,color) ZoneScopedNC(name,color)
#	define HE_PROFILE_FUNCTION() ZoneScoped
#	define HE_PROFILE_FRAME() FrameMark 
#	define HE_PROFILE_TAG(y, x) ZoneText(x, strlen(x))
#	define HE_PROFILE_LOG(text, size) TracyMessage(text, size)
#	define HE_PROFILE_VALUE(text, value) TracyPlot(text, value)
#	define HE_PROFILE_ALLOC(p, size) TracyAlloc(ptr,size)
#	define HE_PROFILE_FREE(p) TracyFree(ptr);
#else
#	define HE_PROFILE_SCOPE(name)
#	define HE_PROFILE_SCOPE_COLOR(color)
#	define HE_PROFILE_SCOPE_NC(name,color)
#	define HE_PROFILE_FUNCTION()
#	define HE_PROFILE_FRAME()
#	define HE_PROFILE_TAG(y, x) 
#	define HE_PROFILE_LOG(text, size)
#	define HE_PROFILE_VALUE(text, value) 
#	define HE_PROFILE_ALLOC(ptr, size)
#	define HE_PROFILE_FREE(ptr)     
#endif

//////////////////////////////////////////////////////////////////////////
// Shader
//////////////////////////////////////////////////////////////////////////

#ifdef NVRHI_HAS_D3D11
#	define STATIC_SHADER_D3D11(NAME) HE::Buffer{g_##NAME##_dxbc, std::size(g_##NAME##_dxbc)}
#else
#	define STATIC_SHADER_D3D11(NAME) HE::Buffer()
#endif
#ifdef NVRHI_HAS_D3D12
#	define STATIC_SHADER_D3D12(NAME) HE::Buffer{ g_##NAME##_dxil, std::size(g_##NAME##_dxil) }
#else
#	define STATIC_SHADER_D3D12(NAME) HE::Buffer()
#endif
#ifdef NVRHI_HAS_VULKAN
#	define STATIC_SHADER_SPIRV(NAME) HE::Buffer{ g_##NAME##_spirv, std::size(g_##NAME##_spirv) }
#else
#	define STATIC_SHADER_SPIRV(NAME) HE::Buffer()
#endif
#define STATIC_SHADER(NAME) HE::RHI::StaticShader{ STATIC_SHADER_D3D11(NAME) ,STATIC_SHADER_D3D12(NAME) ,STATIC_SHADER_SPIRV(NAME) }
