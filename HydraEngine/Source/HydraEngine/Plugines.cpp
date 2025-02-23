module;

#include "HydraEngine/Base.h"

module HydraEngine;
import YAML;

namespace HydraEngine::Plugins {

	void SerializePluginDesc(const std::filesystem::path& filePath, const PluginDesc& desc)
	{
		YAML::Emitter out;
		out << YAML::BeginMap << YAML::Key << "PluginDescriptor";

		out << YAML::BeginMap;
		{
			out << YAML::Key << "name" << YAML::Value << desc.name;
			out << YAML::Key << "description" << YAML::Value << desc.description;
			out << YAML::Key << "reloadable" << YAML::Value << desc.reloadable;
			out << YAML::Key << "enabledByDefault" << YAML::Value << desc.enabledByDefault;
			out << YAML::Key << "modules" << YAML::Value << desc.modules;
			out << YAML::Key << "plugins" << YAML::Value << desc.plugins;
		}
		out << YAML::EndMap;
		out << YAML::EndMap;

		std::ofstream fout(filePath);
		if (!fout)
		{
			HE_CORE_ERROR("Failed to open file {} for writing plugin descriptor.", filePath.string());
			return;
		}

		fout << out.c_str();
	}

	bool DeserializePluginDesc(const std::filesystem::path& filePath, PluginDesc& desc)
	{
		YAML::Node data;

		try
		{
			data = YAML::LoadFile(filePath.string());
		}
		catch (const std::exception& e)
		{
			auto& ex = e;
			HE_CORE_ERROR("Failed to load .hplugin file {}\n    {}", filePath.string(), ex.what());
			return false;
		}

		if (!data["PluginDescriptor"])
		{
			HE_CORE_ERROR("Failed to load .hplugin file {} : missing key PluginDescriptor", filePath.string());
			return false;
		}

		auto pluginDescriptor = data["PluginDescriptor"];
		if (pluginDescriptor)
		{
			desc.name = pluginDescriptor["name"].as<std::string>("");
			desc.description = pluginDescriptor["description"].as<std::string>("");
			desc.reloadable = pluginDescriptor["reloadable"].as<bool>(false);
			desc.enabledByDefault = pluginDescriptor["enabledByDefault"].as<bool>(false);
			desc.modules = pluginDescriptor["modules"].as<std::vector<std::string>>();
			desc.plugins = pluginDescriptor["plugins"].as<std::vector<std::string>>();
		}

		return true;
	}
				
	Ref<Plugin> CreatePluginObject(const std::filesystem::path& descFilePath)
	{
		auto& c = GetAppContext().pluginContext;

		Ref<Plugin> plugin = CreateRef<Plugin>();
		DeserializePluginDesc(descFilePath, plugin->desc);
		PluginHandle handle = Hash(plugin->desc.name);

		plugin->descFilePath = descFilePath;
		if (c.plugins.contains(handle))
		{
			HE_CORE_TRACE("CreatePluginObject : plugin {} is already loaded", plugin->desc.name);
			return c.plugins.at(handle);
		}
		c.plugins[handle] = plugin;

		return plugin;
	}

	void LoadPlugin(const std::filesystem::path& descriptor)
	{
		auto lexicallyNormal = descriptor.lexically_normal();
		if (!std::filesystem::exists(lexicallyNormal))
		{
			HE_CORE_ERROR("LoadPlugin failed: file {} does not exist.", lexicallyNormal.string());
			return;
		}

		Ref<Plugin> plugin = CreatePluginObject(lexicallyNormal);
		auto handle = Hash(plugin->desc.name);
		LoadPlugin(handle);
	}

	void LoadPlugin(PluginHandle handle)
	{
		auto& c = GetAppContext().pluginContext;

		auto it = c.plugins.find(handle);
		if (it == c.plugins.end()) return;

		Ref<Plugin> plugin = it->second;
		const auto& dependences = plugin->desc.plugins;

		for (const auto& dependencyPluginName : dependences)
		{
			PluginHandle dependencyPluginHandle = Hash(dependencyPluginName);

			if (c.plugins.contains(dependencyPluginHandle))
			{
				auto dependencyPlugin = c.plugins.at(dependencyPluginHandle);
				if (!dependencyPlugin->enabled)
				{
					LoadPlugin(dependencyPluginHandle);
				}
			}
		}

		plugin->enabled = true;

		// Load Modules
		for (const auto& moduleName : plugin->desc.modules)
		{
			auto modulePath = plugin->BinariesDirectory() / std::format("{}-{}", c_System, c_Architecture) / c_BuildConfig / (moduleName + c_SharedLibExtension);
			Modules::LoadModule(modulePath);
		}
	}

	bool UnloadPlugin(PluginHandle handle)
	{
		auto& c = GetAppContext().pluginContext;

		if (c.plugins.contains(handle))
		{
			const Ref<Plugin>& plugin = c.plugins.at(handle);
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
		auto& c = GetAppContext().pluginContext;

		std::filesystem::path pluginDescFilePath;

		if (c.plugins.contains(handle))
			pluginDescFilePath = c.plugins.at(handle)->descFilePath;

		UnloadPlugin(handle);
		c.plugins.erase(handle);
		LoadPlugin(pluginDescFilePath);
	}

	const Ref<Plugin> GetPlugin(PluginHandle handle)
	{
		auto& c = GetAppContext().pluginContext;
		if (c.plugins.contains(handle))
			return c.plugins.at(handle);

		return nullptr;
	}

	void LoadPluginsInDirectory(const std::filesystem::path& directory)
	{
		if (!std::filesystem::exists(directory))
		{
			HE_CORE_ERROR("LoadPluginsInDirectory failed: directory {} does not exist.", directory.string());
			return;
		}

		PluginHandle discoveredPLugins[4096];
		uint32_t count = 0;

		// (optimization) we know once we find one .hplugin file that there shouldn't be anymore in the same folder hierarchy.
		for (const auto& entry : std::filesystem::recursive_directory_iterator(directory))
		{
			if (entry.is_regular_file() && entry.path().extension() == c_PluginDescriptorExtension)
			{
				Ref<Plugin> plugin = CreatePluginObject(entry.path());
				discoveredPLugins[count] = Hash(plugin->desc.name);
				count++;
			}
		}

		auto& c = GetAppContext().pluginContext;

		for (uint32_t i = 0; i < count; i++)
		{
			auto handle = discoveredPLugins[i];
			if(c.plugins.at(handle)->desc.enabledByDefault)
				LoadPlugin(handle);
		}
	}
}
