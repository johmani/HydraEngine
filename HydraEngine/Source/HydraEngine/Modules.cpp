module;

#include "HydraEngine/Base.h"

#ifdef HE_PLATFORM_WINDOWS
#	ifndef WIN32_LEAN_AND_MEAN
#		define WIN32_LEAN_AND_MEAN
#		include <windows.h>
#		undef WIN32_LEAN_AND_MEAN
#	else
#		include <windows.h>
#	endif
#else
	#include <dlfcn.h>
#endif

module HydraEngine;

namespace HydraEngine {

	namespace Modules {

#ifdef HE_PLATFORM_WINDOWS
		using NativeHandleType = HINSTANCE;
		using NativeSymbolType = FARPROC;
#else
		using NativeHandleType = void*;
		using NativeSymbolType = void*;
#endif

		void* SharedLib::Open(const char* path) noexcept
		{
#		ifdef HE_PLATFORM_WINDOWS
			return LoadLibraryA(path);
#		else
			return dlopen(path, RTLD_NOW | RTLD_LOCAL);
#		endif
		}

		void* SharedLib::GetSymbolAddress(void* handle, const char* name) noexcept
		{
#		ifdef HE_PLATFORM_WINDOWS
			return (NativeSymbolType)GetProcAddress((NativeHandleType)handle, name);
#		else
			return dlsym((NativeHandleType)handle, name);
#		endif
		}

		void SharedLib::Close(void* handle) noexcept
		{
#		ifdef HE_PLATFORM_WINDOWS
			FreeLibrary((NativeHandleType)handle);
#		else
			dlclose((NativeHandleType)lib);
#		endif
		}

		std::string SharedLib::GetError() noexcept
		{
#		ifdef HE_PLATFORM_WINDOWS
			constexpr const size_t bufferSize = 512;
			auto error_code = GetLastError();
			if (!error_code) return "Unknown error (GetLastError failed)";
			char description[512];
			auto lang = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
			const DWORD length = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, error_code, lang, description, bufferSize, nullptr);
			return (length == 0) ? "Unknown error (FormatMessage failed)" : description;
#		else
			auto description = dlerror();
			return (description == nullptr) ? "Unknown error (dlerror failed)" : description;
#		endif
		}

		ModulesContext::~ModulesContext()
		{
			struct ModuleToShutdown
			{
				uint32_t LoadOrder;
				ModuleHandle handle;
			};

			if (modules.size() <= 0)
				return;

			std::vector<ModuleToShutdown> ModulesToShutdone;
			ModulesToShutdone.reserve(modules.size());
			for (auto& [handle, moduleData] : modules)
			{
				ModulesToShutdone.emplace_back(moduleData->loadOrder, handle);
			}

			std::sort(ModulesToShutdone.begin(), ModulesToShutdone.end(), [&](const ModuleToShutdown& a, const ModuleToShutdown& b) { return a.LoadOrder < b.LoadOrder; });
			std::reverse(ModulesToShutdone.begin(), ModulesToShutdone.end());

			for (auto& ModuleToShutdone : ModulesToShutdone)
			{
				UnloadModule(ModuleToShutdone.handle);
			}
		}

		bool LoadModule(const std::filesystem::path& filePath)
		{
			if (!std::filesystem::exists(filePath))
			{
				HE_CORE_ERROR("LoadModule failed: File {} does not exist.", filePath.string());
				return false;
			}

			auto& c = GetAppContext().modulesContext;

			Ref<ModuleData> newModule = CreateRef<ModuleData>(filePath);
			ModuleHandle handle = Hash(filePath);
			if (c.modules.contains(handle))
			{
				HE_CORE_WARN("Module {} has already been loaded.", newModule->name);
				return false;
			}

			if (newModule->lib.IsLoaded())
			{
				auto func = newModule->lib.GetFunction<void()>("OnModuleLoaded");
				if (func)
				{
					func();
					c.modules[handle] = newModule;
					return true;
				}
			}

			HE_CORE_ERROR("LoadModule failed: OnModuleLoaded function not found in module {}.", newModule->name);
			return false;
		}

		bool IsModuleLoaded(ModuleHandle handle)
		{
			auto& c = GetAppContext().modulesContext;

			if (c.modules.contains(handle))
				return true;

			return false;
		}

		bool UnloadModule(ModuleHandle handle)
		{
			auto& c = GetAppContext().modulesContext;

			auto it = c.modules.find(handle);
			if (it == c.modules.end())
			{
				HE_CORE_ERROR("UnloadModule failed: Module with handle {} not found.", handle);
				return false;
			}

			Ref<ModuleData> moduleData = it->second;

			if (auto func = moduleData->lib.GetFunction<void()>("OnModuleShutdown"))
			{
				func();
			}
			else
			{
				HE_CORE_WARN("UnloadModule failed: Module {} does not define an OnModuleShutdown function.", moduleData->name);
			}

			c.modules.erase(it);

			return true;
		}

		Ref<ModuleData> GetModuleData(ModuleHandle handle)
		{
			auto& c = GetAppContext().modulesContext;

			auto it = c.modules.find(handle);
			if (it != c.modules.end())
				return it->second;

			HE_CORE_ERROR("Module with handle {} not found.", handle);
			return nullptr;
		}
	}
}
