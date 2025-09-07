module;

#include "HydraEngine/Base.h"
#include <simdjson.h>

module HE;

namespace HE::Plugins {

    bool DeserializePluginDesc(const std::filesystem::path& filePath, PluginDesc& desc)
    {
        HE_PROFILE_FUNCTION();

        static simdjson::dom::parser parser;

        simdjson::dom::element pluginDescriptor;
        auto error = parser.load(filePath.string()).get(pluginDescriptor);
        if (error)
        {
            HE_CORE_ERROR("Failed to load .hplugin file {}\n    {}", filePath.string(), simdjson::error_message(error));
            return false;
        }

        std::string_view name;
        desc.name = (pluginDescriptor["name"].get(name) != simdjson::SUCCESS) ? "" : name;

        std::string_view description;
        desc.description = (pluginDescriptor["description"].get(description) != simdjson::SUCCESS) ? "" : description;

        std::string_view URL;
        desc.URL = (pluginDescriptor["URL"].get(URL) != simdjson::SUCCESS) ? "" : URL;

        pluginDescriptor["reloadable"].get(desc.reloadable);
        pluginDescriptor["enabledByDefault"].get(desc.enabledByDefault);

        simdjson::dom::array modulesArray;
        pluginDescriptor["modules"].get(modulesArray);
        desc.modules.reserve(modulesArray.size());
        for (simdjson::dom::element module : modulesArray)
        {
            std::string_view moduleName;
            module.get(moduleName);
            desc.modules.emplace_back(moduleName);
        }

        simdjson::dom::array pluginsArray;
        pluginDescriptor["plugins"].get(pluginsArray);
        desc.plugins.reserve(pluginsArray.size());
        for (simdjson::dom::element plugin : pluginsArray)
        {
            std::string_view pluginName;
            plugin.get(pluginName);
            desc.plugins.emplace_back(pluginName);
        }

        return true;
    }

    Ref<Plugin> GetOrCreatePluginObject(const std::filesystem::path& descFilePath)
    {
        HE_PROFILE_FUNCTION();

        auto& ctx = GetAppContext().pluginContext;

        PluginDesc desc;
        DeserializePluginDesc(descFilePath, desc);
        PluginHandle handle = Hash(desc.name);

        if (ctx.plugins.contains(handle))
            return ctx.plugins.at(handle);

        Ref<Plugin> plugin = CreateRef<Plugin>(desc);
        plugin->descFilePath = descFilePath;
        ctx.plugins[handle] = plugin;

        return plugin;
    }

    void LoadPlugin(const std::filesystem::path& descriptor)
    {
        HE_PROFILE_FUNCTION();

        auto lexicallyNormal = descriptor.lexically_normal();
        if (!std::filesystem::exists(lexicallyNormal))
        {
            HE_CORE_ERROR("LoadPlugin failed: file {} does not exist.", lexicallyNormal.string());
            return;
        }

        Ref<Plugin> plugin = GetOrCreatePluginObject(lexicallyNormal);
        auto handle = Hash(plugin->desc.name);
        LoadPlugin(handle);
    }

    void LoadPlugin(PluginHandle handle)
    {
        HE_PROFILE_FUNCTION();

        auto& ctx = GetAppContext().pluginContext;

        auto it = ctx.plugins.find(handle);
        if (it == ctx.plugins.end()) return;

        Ref<Plugin> plugin = it->second;
        const auto& dependences = plugin->desc.plugins;

        if(dependences.size() > 0)
        {
            auto pluginsDir = plugin->descFilePath.parent_path().parent_path();

            for (const auto& dependencyPluginName : dependences)
            {
                auto pluginsDescFilePath = pluginsDir / dependencyPluginName / (dependencyPluginName + c_PluginDescriptorExtension);
                if (std::filesystem::exists(pluginsDescFilePath))
                    GetOrCreatePluginObject(pluginsDescFilePath);
            }
        }

        for (const auto& dependencyPluginName : dependences)
        {
            PluginHandle dependencyPluginHandle = Hash(dependencyPluginName);

            if (ctx.plugins.contains(dependencyPluginHandle))
            {
                auto dependencyPlugin = ctx.plugins.at(dependencyPluginHandle);
                if (!dependencyPlugin->enabled)
                {
                    LoadPlugin(dependencyPluginHandle);
                }
            }
        }

        plugin->enabled = true;

        HE_CORE_INFO("Plugins::LoadPlugin {}", plugin->desc.name);

        // Load Modules
        for (const auto& moduleName : plugin->desc.modules)
        {
            auto modulePath = plugin->BinariesDirectory() / std::format("{}-{}", c_System, c_Architecture) / c_BuildConfig / (moduleName + c_SharedLibExtension);
            Modules::LoadModule(modulePath);
        }
    }

    bool UnloadPlugin(PluginHandle handle)
    {
        HE_PROFILE_FUNCTION();

        auto& ctx = GetAppContext().pluginContext;

        if (ctx.plugins.contains(handle))
        {
            const Ref<Plugin>& plugin = ctx.plugins.at(handle);
            if (plugin->enabled)
            {
                const auto& modulesNames = plugin->desc.modules;

                // Load Modules
                bool res = true;
                for (const auto& name : modulesNames)
                {
                    auto modulePath = plugin->BinariesDirectory() / std::format("{}-{}", c_System, c_Architecture) / c_BuildConfig / (name + c_SharedLibExtension);
                    auto moduleHandle = Hash(modulePath);
                    res = Modules::UnloadModule(moduleHandle);
                    if (!res) break;
                }
                if (res)
                {
                    plugin->enabled = false;
                }
                return true;
            }
            else
            {
                return true;
            }
        }

        HE_CORE_ERROR("UnloadPlugin : failed to Unload Plugin {}", handle);

        return false;
    }

    void ReloadPlugin(PluginHandle handle)
    {
        HE_PROFILE_FUNCTION();

        auto& ctx = GetAppContext().pluginContext;

        std::filesystem::path pluginDescFilePath;

        if (ctx.plugins.contains(handle))
            pluginDescFilePath = ctx.plugins.at(handle)->descFilePath;

        UnloadPlugin(handle);
        ctx.plugins.erase(handle);
        LoadPlugin(pluginDescFilePath);
    }

    const Ref<Plugin> GetPlugin(PluginHandle handle)
    {
        auto& ctx = GetAppContext().pluginContext;
        if (ctx.plugins.contains(handle))
            return ctx.plugins.at(handle);

        return nullptr;
    }

    void LoadPluginsInDirectory(const std::filesystem::path& directory)
    {
        HE_PROFILE_FUNCTION();

        auto& ctx = GetAppContext().pluginContext;

        if (!std::filesystem::exists(directory))
        {
            HE_CORE_ERROR("LoadPluginsInDirectory failed: directory {} does not exist.", directory.string());
            return;
        }

        PluginHandle discoveredPLugins[4096];
        uint32_t count = 0;

        {
            HE_PROFILE_SCOPE("Find Plugins");

            for (const auto& entry : std::filesystem::directory_iterator(directory))
            {
                auto pluginsDescFilePath = entry.path() / (entry.path().stem().string() + c_PluginDescriptorExtension);

                if (std::filesystem::exists(pluginsDescFilePath))
                {
                    Ref<Plugin> plugin = GetOrCreatePluginObject(pluginsDescFilePath);
                    discoveredPLugins[count] = Hash(plugin->desc.name);
                    count++;
                }
            }
        }

        {
            HE_PROFILE_SCOPE("Load Plugins");

            for (uint32_t i = 0; i < count; i++)
            {
                auto handle = discoveredPLugins[i];
                if (ctx.plugins.at(handle)->desc.enabledByDefault)
                    LoadPlugin(handle);
            }
        }
    }
}
