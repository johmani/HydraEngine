module;

#include "HydraEngine/Base.h"

module HE;

namespace HE {

    namespace Modules {

        ModulesContext::~ModulesContext()
        {
            HE_PROFILE_FUNCTION();

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
            HE_PROFILE_FUNCTION();

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
            HE_PROFILE_FUNCTION();

            auto& c = GetAppContext().modulesContext;

            if (c.modules.contains(handle))
                return true;

            return false;
        }

        bool UnloadModule(ModuleHandle handle)
        {
            HE_PROFILE_FUNCTION();

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
            HE_PROFILE_FUNCTION();

            auto& c = GetAppContext().modulesContext;

            auto it = c.modules.find(handle);
            if (it != c.modules.end())
                return it->second;

            HE_CORE_ERROR("Module with handle {} not found.", handle);
            return nullptr;
        }
    }
}
