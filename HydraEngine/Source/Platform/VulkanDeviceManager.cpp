module;

#include "HydraEngine/Base.h"

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

module HE;
import nvrhi;

// TODO
// - Investigate device.createSwapchainKHR and device.destroySwapchainKHR take so much time

// Define the Vulkan dynamic dispatcher - this needs to occur in exactly one cpp file in the program.
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#define APPEND_EXTENSION(condition, desc) if (condition) { (desc).pNext = pNext; pNext = &(desc); }  // NOLINT(cppcoreguidelines-macro-usage)

template <typename T>
static std::vector<T> setToVector(const std::unordered_set<T>&set)
{
    std::vector<T> ret;
    for (const auto& s : set)
    {
        ret.push_back(s);
    }

    return ret;
}

struct SwapChainImage
{
    vk::Image image;
    nvrhi::TextureHandle rhiHandle;
};

struct VKDeviceManager : public HE::RHI::DeviceManager
{
    struct VulkanExtensionSet
    {
        std::unordered_set<std::string> instance;
        std::unordered_set<std::string> layers;
        std::unordered_set<std::string> device;
    };

    // minimal set of required extensions
    VulkanExtensionSet enabledExtensions = {
        // instance
        {
            VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
        },
        // layers
        {
        },
        // device
        {
            VK_KHR_MAINTENANCE1_EXTENSION_NAME
        },
    };

    // optional extensions
    VulkanExtensionSet optionalExtensions = {
        // instance
        {
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
            VK_EXT_SAMPLER_FILTER_MINMAX_EXTENSION_NAME,
        },
        // layers
        {
        },
        // device
        {
            VK_EXT_DEBUG_MARKER_EXTENSION_NAME,
            VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
            VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME,
            VK_KHR_MAINTENANCE_4_EXTENSION_NAME,
            VK_KHR_SWAPCHAIN_MUTABLE_FORMAT_EXTENSION_NAME,
            VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
            VK_NV_MESH_SHADER_EXTENSION_NAME,
        },
    };

    std::unordered_set<std::string> rayTracingExtensions = {
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
        VK_KHR_RAY_QUERY_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME
    };

    std::string rendererString;
    vk::Instance vulkanInstance;
    vk::DebugReportCallbackEXT debugReportCallback;
    vk::PhysicalDevice vulkanPhysicalDevice;
    nvrhi::vulkan::DeviceHandle nvrhiDevice;
    nvrhi::DeviceHandle validationLayer;
    bool m_BufferDeviceAddressSupported = false;
    vk::Device device;
    vk::Queue graphicsQueue;
    vk::Queue computeQueue;
    vk::Queue transferQueue;
    vk::Queue presentQueue;
    int graphicsQueueFamily = -1;
    int computeQueueFamily = -1;
    int transferQueueFamily = -1;
    int presentQueueFamily = -1;

    bool swapChainMutableFormatSupported = false;
    HE::Window* tempWindow = nullptr;

#if VK_HEADER_VERSION >= 301
    typedef vk::detail::DynamicLoader VulkanDynamicLoader;
#else
    typedef vk::DynamicLoader VulkanDynamicLoader;
#endif

    HE::Scope<VulkanDynamicLoader> dynamicLoader;

    ~VKDeviceManager();
    nvrhi::IDevice* GetDevice() const override { return validationLayer ? validationLayer : nvrhiDevice; }
    HE::SwapChain* CreateSwapChain(const HE::SwapChainDesc& swapChainDesc, void* windowHandle) override;
    bool EnumerateAdapters(std::vector<HE::RHI::AdapterInfo>& outAdapters) override;
    bool CreateInstance();
    bool CreateInstanceInternal();
    bool CreateDevice() override;
    const char* GetRendererString() const override;
    bool IsVulkanInstanceExtensionEnabled(const char* extensionName) const override;
    bool IsVulkanDeviceExtensionEnabled(const char* extensionName) const override;
    bool IsVulkanLayerEnabled(const char* layerName) const override;
    void GetEnabledVulkanInstanceExtensions(std::vector<std::string>& extensions) const override;
    void GetEnabledVulkanDeviceExtensions(std::vector<std::string>& extensions) const override;
    void GetEnabledVulkanLayers(std::vector<std::string>& layers) const override;
    void InstallDebugCallback();
    bool PickPhysicalDevice();
    bool FindQueueFamilies(vk::PhysicalDevice physicalDevice);
    bool CreateDeviceImp();

    static std::vector<const char*> stringSetToVector(const std::unordered_set<std::string>& set);
    static VKAPI_ATTR VkBool32 VKAPI_CALL vulkanDebugCallback(vk::DebugReportFlagsEXT flags, vk::DebugReportObjectTypeEXT objType, uint64_t obj, size_t location, int32_t code, const char* layerPrefix, const char* msg, void* userData);
};

struct VKSwapChain : public HE::SwapChain
{
    vk::SurfaceKHR windowSurface;
    vk::SurfaceFormatKHR swapChainFormat;
    vk::SwapchainKHR swapChain;
    std::vector<SwapChainImage> swapChainImages;
    uint32_t swapChainIndex = uint32_t(-1);
    std::vector<vk::Semaphore> acquireSemaphores;
    std::vector<vk::Semaphore> presentSemaphores;
    uint32_t acquireSemaphoreIndex = 0;
    uint32_t presentSemaphoreIndex = 0;
    std::queue<nvrhi::EventQueryHandle> framesInFlight;
    std::vector<nvrhi::EventQueryHandle> queryPool;
    VKDeviceManager* vkDeviceManager = nullptr;

    ~VKSwapChain();
    bool CreateWindowSurface();
    bool CreateSwapChain(const HE::SwapChainDesc& swapChainDesc, uint32_t width, uint32_t height);
    void Reset();
    void ResizeSwapChain(uint32_t width, uint32_t height) override;
    bool BeginFrame() override;
    bool Present() override;
    nvrhi::ITexture* GetCurrentBackBuffer() override { return swapChainImages[swapChainIndex].rhiHandle; }
    nvrhi::ITexture* GetBackBuffer(uint32_t index) override { return (index < swapChainImages.size()) ? swapChainImages[index].rhiHandle : nullptr; }
    uint32_t GetCurrentBackBufferIndex() override { return swapChainIndex; }
    uint32_t GetBackBufferCount() override { return uint32_t(swapChainImages.size()); }
};

//////////////////////////////////////////////////////////////////////////
// VKDeviceManager
//////////////////////////////////////////////////////////////////////////

VKDeviceManager::~VKDeviceManager()
{
    HE_PROFILE_FUNCTION();

    nvrhiDevice = nullptr;
    validationLayer = nullptr;
    rendererString.clear();

    if (device)
    {
        device.destroy();
        device = nullptr;
    }

    if (debugReportCallback)
    {
        vulkanInstance.destroyDebugReportCallbackEXT(debugReportCallback);
    }

    if (vulkanInstance)
    {
        vulkanInstance.destroy();
        vulkanInstance = nullptr;
    }

    instanceCreated = false;
}

HE::SwapChain* VKDeviceManager::CreateSwapChain(const HE::SwapChainDesc& swapChainDesc, void* windowHandle)
{
    HE_PROFILE_FUNCTION();

    VKSwapChain* vkSwapChain = new VKSwapChain();

    int w, h;
    glfwGetWindowSize((GLFWwindow*)windowHandle, &w, &h);

    vkSwapChain->isVSync = swapChainDesc.vsync;
    vkSwapChain->desc = swapChainDesc;
    vkSwapChain->desc.backBufferWidth = w;
    vkSwapChain->desc.backBufferHeight = h;
    vkSwapChain->nvrhiDevice = nvrhiDevice;
    vkSwapChain->windowHandle = windowHandle;
    vkSwapChain->vkDeviceManager = this;

    if (!vkSwapChain->CreateWindowSurface())
        return nullptr;

    if (!vkSwapChain->CreateSwapChain(swapChainDesc, w, h))
        return nullptr;

    {
        vkSwapChain->presentSemaphores.reserve(swapChainDesc.maxFramesInFlight + 1);
        vkSwapChain->acquireSemaphores.reserve(swapChainDesc.maxFramesInFlight + 1);
        for (uint32_t i = 0; i < swapChainDesc.maxFramesInFlight + 1; ++i)
        {
            vkSwapChain->presentSemaphores.push_back(device.createSemaphore(vk::SemaphoreCreateInfo()));
            vkSwapChain->acquireSemaphores.push_back(device.createSemaphore(vk::SemaphoreCreateInfo()));
        }
    }

    vkSwapChain->ResizeBackBuffers();

    return vkSwapChain;
}

VKAPI_ATTR VkBool32 VKAPI_CALL VKDeviceManager::vulkanDebugCallback(vk::DebugReportFlagsEXT flags, vk::DebugReportObjectTypeEXT objType, uint64_t obj, size_t location, int32_t code, const char* layerPrefix, const char* msg, void* userData)
{
    const VKDeviceManager* manager = static_cast<const VKDeviceManager*>(userData);

    // Skip ignored messages
    if (manager) 
    {
        const auto& ignored = manager->desc.ignoredVulkanValidationMessageLocations;
        if (std::find(ignored.begin(), ignored.end(), location) != ignored.end())
            return VK_FALSE;
    }

    if (flags & vk::DebugReportFlagBitsEXT::eError)
    {
        HE_CORE_ERROR("[Vulkan] {}", msg);
    }
    else if (flags & (vk::DebugReportFlagBitsEXT::eWarning | vk::DebugReportFlagBitsEXT::ePerformanceWarning))
    {
        HE_CORE_WARN("[Vulkan] {}", msg);
    }
    else if (flags & vk::DebugReportFlagBitsEXT::eInformation)
    {
        HE_CORE_INFO("[Vulkan] {}", msg);
    }
    else if (flags & vk::DebugReportFlagBitsEXT::eDebug)
    {
        HE_CORE_TRACE("[Vulkan] {}", msg);
    }
    else
    {
        HE_CORE_WARN("[Vulkan] {}", msg);
    }
    return VK_FALSE;
}

bool VKDeviceManager::EnumerateAdapters(std::vector<HE::RHI::AdapterInfo>& outAdapters)
{
    HE_PROFILE_FUNCTION();

    if (!vulkanInstance)
        return false;

    std::vector<vk::PhysicalDevice> devices = vulkanInstance.enumeratePhysicalDevices();
    outAdapters.clear();

    for (auto physicalDevice : devices)
    {
        vk::PhysicalDeviceProperties2 properties2;
        vk::PhysicalDeviceIDProperties idProperties;
        properties2.pNext = &idProperties;
        physicalDevice.getProperties2(&properties2);

        auto const& properties = properties2.properties;

        HE::RHI::AdapterInfo adapterInfo;
        adapterInfo.name = properties.deviceName.data();
        adapterInfo.vendorID = properties.vendorID;
        adapterInfo.deviceID = properties.deviceID;
        adapterInfo.vkPhysicalDevice = physicalDevice;
        adapterInfo.dedicatedVideoMemory = 0;

        HE::RHI::AdapterInfo::UUID uuid;
        static_assert(uuid.size() == idProperties.deviceUUID.size());
        memcpy(uuid.data(), idProperties.deviceUUID.data(), uuid.size());
        adapterInfo.uuid = uuid;

        if (idProperties.deviceLUIDValid)
        {
            HE::RHI::AdapterInfo::LUID luid;
            static_assert(luid.size() == idProperties.deviceLUID.size());
            memcpy(luid.data(), idProperties.deviceLUID.data(), luid.size());
            adapterInfo.luid = luid;
        }

        // Go through the memory types to figure out the amount of VRAM on this physical device.
        vk::PhysicalDeviceMemoryProperties memoryProperties = physicalDevice.getMemoryProperties();
        for (uint32_t heapIndex = 0; heapIndex < memoryProperties.memoryHeapCount; ++heapIndex)
        {
            vk::MemoryHeap const& heap = memoryProperties.memoryHeaps[heapIndex];
            if (heap.flags & vk::MemoryHeapFlagBits::eDeviceLocal)
            {
                adapterInfo.dedicatedVideoMemory += heap.size;
            }
        }

        outAdapters.push_back(std::move(adapterInfo));
    }

    return true;
}

bool VKDeviceManager::CreateInstance()
{
    HE_PROFILE_FUNCTION();

    if (!desc.headlessDevice)
    {
        if (!glfwVulkanSupported())
        {
            HE_CORE_ERROR("GLFW reports that Vulkan is not supported. Perhaps missing a call to glfwInit()?");
            return false;
        }

        // add any extensions required by GLFW
        uint32_t glfwExtCount;
        const char** glfwExt = glfwGetRequiredInstanceExtensions(&glfwExtCount);
        HE_CORE_ASSERT(glfwExt);

        for (uint32_t i = 0; i < glfwExtCount; i++)
        {
            enabledExtensions.instance.insert(std::string(glfwExt[i]));
        }
    }

    // add instance extensions requested by the user
    for (const std::string& name : desc.requiredVulkanInstanceExtensions)
    {
        enabledExtensions.instance.insert(name);
    }
    for (const std::string& name : desc.optionalVulkanInstanceExtensions)
    {
        optionalExtensions.instance.insert(name);
    }

    // add layers requested by the user
    for (const std::string& name : desc.requiredVulkanLayers)
    {
        enabledExtensions.layers.insert(name);
    }
    for (const std::string& name : desc.optionalVulkanLayers)
    {
        optionalExtensions.layers.insert(name);
    }

    std::unordered_set<std::string> requiredExtensions = enabledExtensions.instance;

    // figure out which optional extensions are supported
    for (const auto& instanceExt : vk::enumerateInstanceExtensionProperties())
    {
        const std::string name = instanceExt.extensionName;
        if (optionalExtensions.instance.find(name) != optionalExtensions.instance.end())
        {
            enabledExtensions.instance.insert(name);
        }

        requiredExtensions.erase(name);
    }

    if (!requiredExtensions.empty())
    {
        std::stringstream ss;
        ss << "Cannot create a Vulkan instance because the following required extension(s) are not supported:";
        for (const auto& ext : requiredExtensions)
            ss << std::endl << "  - " << ext;

        HE_CORE_ERROR("{}", ss.str().c_str());
        return false;
    }

    HE_CORE_INFO("Enabled Vulkan instance extensions:");
    for (const auto& ext : enabledExtensions.instance)
    {
        HE_CORE_INFO("    {}", ext.c_str());
    }

    std::unordered_set<std::string> requiredLayers = enabledExtensions.layers;

    for (const auto& layer : vk::enumerateInstanceLayerProperties())
    {
        const std::string name = layer.layerName;
        if (optionalExtensions.layers.find(name) != optionalExtensions.layers.end())
        {
            enabledExtensions.layers.insert(name);
        }

        requiredLayers.erase(name);
    }

    if (!requiredLayers.empty())
    {
        std::stringstream ss;
        ss << "Cannot create a Vulkan instance because the following required layer(s) are not supported:";
        for (const auto& ext : requiredLayers)
            ss << std::endl << "  - " << ext;

        HE_CORE_ERROR("{}", ss.str().c_str());
        return false;
    }

    HE_CORE_INFO("Enabled Vulkan layers:");
    for (const auto& layer : enabledExtensions.layers)
    {
        HE_CORE_INFO("    {}", layer.c_str());
    }

    auto instanceExtVec = stringSetToVector(enabledExtensions.instance);
    auto layerVec = stringSetToVector(enabledExtensions.layers);

    auto applicationInfo = vk::ApplicationInfo();

    // Query the Vulkan API version supported on the system to make sure we use at least 1.3 when that's present.
    vk::Result res = vk::enumerateInstanceVersion(&applicationInfo.apiVersion);

    if (res != vk::Result::eSuccess)
    {
        HE_CORE_ERROR("Call to vkEnumerateInstanceVersion failed, error code = {}", nvrhi::vulkan::resultToString(VkResult(res)));
        return false;
    }

    const uint32_t minimumVulkanVersion = VK_MAKE_API_VERSION(0, 1, 3, 0);

    // Check if the Vulkan API version is sufficient.
    if (applicationInfo.apiVersion < minimumVulkanVersion)
    {
        HE_CORE_ERROR("The Vulkan API version supported on the system ({}.{}.{}) is too low, at least {}.{}.{} is required.",
            VK_API_VERSION_MAJOR(applicationInfo.apiVersion), VK_API_VERSION_MINOR(applicationInfo.apiVersion), VK_API_VERSION_PATCH(applicationInfo.apiVersion),
            VK_API_VERSION_MAJOR(minimumVulkanVersion), VK_API_VERSION_MINOR(minimumVulkanVersion), VK_API_VERSION_PATCH(minimumVulkanVersion));
        return false;
    }

    // Spec says: A non-zero variant indicates the API is a variant of the Vulkan API and applications will typically need to be modified to run against it.
    if (VK_API_VERSION_VARIANT(applicationInfo.apiVersion) != 0)
    {
        HE_CORE_ERROR("The Vulkan API supported on the system uses an unexpected variant: {}", VK_API_VERSION_VARIANT(applicationInfo.apiVersion));
        return false;
    }

    // Create the vulkan instance
    vk::InstanceCreateInfo info = vk::InstanceCreateInfo()
        .setEnabledLayerCount(uint32_t(layerVec.size()))
        .setPpEnabledLayerNames(layerVec.data())
        .setEnabledExtensionCount(uint32_t(instanceExtVec.size()))
        .setPpEnabledExtensionNames(instanceExtVec.data())
        .setPApplicationInfo(&applicationInfo);

    res = vk::createInstance(&info, nullptr, &vulkanInstance);
    if (res != vk::Result::eSuccess)
    {
        HE_CORE_ERROR("Failed to create a Vulkan instance, error code = {}", nvrhi::vulkan::resultToString(VkResult(res)));
        return false;
    }

    VULKAN_HPP_DEFAULT_DISPATCHER.init(vulkanInstance);

    return true;
}

bool VKDeviceManager::CreateInstanceInternal()
{
    HE_PROFILE_FUNCTION();

    if (desc.enableDebugRuntime)
    {
        enabledExtensions.instance.insert("VK_EXT_debug_report");
        enabledExtensions.layers.insert("VK_LAYER_KHRONOS_validation");
    }

    dynamicLoader = std::make_unique<VulkanDynamicLoader>(desc.vulkanLibraryName);
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = dynamicLoader->getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

    return CreateInstance();
}

bool VKDeviceManager::CreateDevice()
{
    HE_PROFILE_FUNCTION();

    if(!desc.headlessDevice)
    {
        HE_PROFILE_SCOPE("Create tempWindow");

        tempWindow = new HE::Window();
        auto winDesc = HE::Application::GetApplicationDesc().windowDesc;
        winDesc.startVisible = false;
        winDesc.setCallbacks = false;
        winDesc.title = "tempWindow";
        winDesc.iconFilePath.clear();
        tempWindow->Init(winDesc);
        tempWindow->swapChain = new VKSwapChain();
        ((VKSwapChain*)tempWindow->swapChain)->vkDeviceManager = this;
        ((VKSwapChain*)tempWindow->swapChain)->windowHandle = tempWindow->handle;
    }

    if (desc.enableDebugRuntime)
    {
        InstallDebugCallback();
    }

    // add device extensions requested by the user
    for (const std::string& name : desc.requiredVulkanDeviceExtensions)
    {
        enabledExtensions.device.insert(name);
    }
    for (const std::string& name : desc.optionalVulkanDeviceExtensions)
    {
        optionalExtensions.device.insert(name);
    }

    if (!desc.headlessDevice)
    {
        auto desc = HE::Application::GetApplicationDesc().windowDesc.swapChainDesc;

        // Need to adjust the swap chain format before creating the device because it affects physical device selection
        if (desc.swapChainFormat == nvrhi::Format::SRGBA8_UNORM)
            desc.swapChainFormat = nvrhi::Format::SBGRA8_UNORM;
        else if (desc.swapChainFormat == nvrhi::Format::RGBA8_UNORM)
            desc.swapChainFormat = nvrhi::Format::BGRA8_UNORM;

        if (!((VKSwapChain*)tempWindow->swapChain)->CreateWindowSurface())
            return false;
    }

    if (!PickPhysicalDevice())
        return false;

    if (!FindQueueFamilies(vulkanPhysicalDevice))
        return false;

    if (!CreateDeviceImp())
        return false;

    auto vecInstanceExt = stringSetToVector(enabledExtensions.instance);
    auto vecLayers = stringSetToVector(enabledExtensions.layers);
    auto vecDeviceExt = stringSetToVector(enabledExtensions.device);

    nvrhi::vulkan::DeviceDesc deviceDesc;
    deviceDesc.errorCB = &HE::RHI::DefaultMessageCallback::GetInstance();
    deviceDesc.instance = vulkanInstance;
    deviceDesc.physicalDevice = vulkanPhysicalDevice;
    deviceDesc.device = device;
    deviceDesc.graphicsQueue = graphicsQueue;
    deviceDesc.graphicsQueueIndex = graphicsQueueFamily;

    if (desc.enableComputeQueue)
    {
        deviceDesc.computeQueue = computeQueue;
        deviceDesc.computeQueueIndex = computeQueueFamily;
    }

    if (desc.enableCopyQueue)
    {
        deviceDesc.transferQueue = transferQueue;
        deviceDesc.transferQueueIndex = transferQueueFamily;
    }

    deviceDesc.instanceExtensions = vecInstanceExt.data();
    deviceDesc.numInstanceExtensions = vecInstanceExt.size();
    deviceDesc.deviceExtensions = vecDeviceExt.data();
    deviceDesc.numDeviceExtensions = vecDeviceExt.size();
    deviceDesc.bufferDeviceAddressSupported = m_BufferDeviceAddressSupported;
    deviceDesc.logBufferLifetime = desc.logBufferLifetime;

    nvrhiDevice = nvrhi::vulkan::createDevice(deviceDesc);

    if (desc.enableNvrhiValidationLayer)
    {
        validationLayer = nvrhi::validation::createValidationLayer(nvrhiDevice);
    }

    return true;
}

const char* VKDeviceManager::GetRendererString() const
{
    return rendererString.c_str();
}

bool VKDeviceManager::IsVulkanInstanceExtensionEnabled(const char* extensionName) const
{
    return enabledExtensions.instance.contains(extensionName);
}

bool VKDeviceManager::IsVulkanDeviceExtensionEnabled(const char* extensionName) const
{
    return enabledExtensions.device.contains(extensionName);
}

bool VKDeviceManager::IsVulkanLayerEnabled(const char* layerName) const
{
    return enabledExtensions.layers.contains(layerName);
}

void VKDeviceManager::GetEnabledVulkanInstanceExtensions(std::vector<std::string>& extensions) const
{
    for (const auto& ext : enabledExtensions.instance)
        extensions.push_back(ext);
}

void VKDeviceManager::GetEnabledVulkanDeviceExtensions(std::vector<std::string>& extensions) const
{
    for (const auto& ext : enabledExtensions.device)
        extensions.push_back(ext);
}

void VKDeviceManager::GetEnabledVulkanLayers(std::vector<std::string>& layers) const
{
    for (const auto& ext : enabledExtensions.layers)
        layers.push_back(ext);
}

void VKDeviceManager::InstallDebugCallback()
{
    HE_PROFILE_FUNCTION();

    auto info = vk::DebugReportCallbackCreateInfoEXT()
        .setFlags(
            vk::DebugReportFlagBitsEXT::eError |
            vk::DebugReportFlagBitsEXT::eWarning |
            vk::DebugReportFlagBitsEXT::eInformation |
            vk::DebugReportFlagBitsEXT::eDebug |
            vk::DebugReportFlagBitsEXT::ePerformanceWarning
        )
        .setPfnCallback(vulkanDebugCallback)
        .setPUserData(this);

    vk::Result res = vulkanInstance.createDebugReportCallbackEXT(&info, nullptr, &debugReportCallback);
    HE_CORE_ASSERT(res == vk::Result::eSuccess);
}

bool VKDeviceManager::PickPhysicalDevice()
{
    HE_PROFILE_FUNCTION();


    auto devices = vulkanInstance.enumeratePhysicalDevices();

    int firstDevice = 0;
    int lastDevice = int(devices.size()) - 1;
    if (desc.adapterIndex >= 0)
    {
        if (desc.adapterIndex > lastDevice)
        {
            HE_CORE_ERROR("The specified Vulkan physical device {} does not exist.", desc.adapterIndex);
            return false;
        }

        firstDevice = desc.adapterIndex;
        lastDevice = desc.adapterIndex;
    }

    // Start building an error message in case we cannot find a device.
    std::stringstream errorStream;
    errorStream << "Cannot find a Vulkan device that supports all the required extensions and properties.";

    // build a list of GPUs
    std::vector<vk::PhysicalDevice> discreteGPUs;
    std::vector<vk::PhysicalDevice> otherGPUs;
    for (int deviceIndex = firstDevice; deviceIndex <= lastDevice; ++deviceIndex)
    {
        vk::PhysicalDevice const& dev = devices[deviceIndex];
        vk::PhysicalDeviceProperties prop = dev.getProperties();

        errorStream << std::endl << prop.deviceName.data() << ":";

        // check that all required device extensions are present
        std::unordered_set<std::string> requiredExtensions = enabledExtensions.device;
        auto deviceExtensions = dev.enumerateDeviceExtensionProperties();
        for (const auto& ext : deviceExtensions)
        {
            requiredExtensions.erase(std::string(ext.extensionName.data()));
        }

        bool deviceIsGood = true;

        if (!requiredExtensions.empty())
        {
            // device is missing one or more required extensions
            for (const auto& ext : requiredExtensions)
            {
                errorStream << std::endl << "  - missing " << ext;
            }
            deviceIsGood = false;
        }

        auto deviceFeatures = dev.getFeatures();
        if (!deviceFeatures.samplerAnisotropy)
        {
            // device is a toaster oven
            errorStream << std::endl << "  - does not support samplerAnisotropy";
            deviceIsGood = false;
        }
        if (!deviceFeatures.textureCompressionBC)
        {
            errorStream << std::endl << "  - does not support textureCompressionBC";
            deviceIsGood = false;
        }

        if (!FindQueueFamilies(dev))
        {
            // device doesn't have all the queue families we need
            errorStream << std::endl << "  - does not support the necessary queue types";
            deviceIsGood = false;
        }

        if (!desc.headlessDevice && deviceIsGood)
        {
            auto windowSurface = ((VKSwapChain*)tempWindow->swapChain)->windowSurface;

            VkFormat requestedFormat = nvrhi::vulkan::convertFormat(tempWindow->desc.swapChainDesc.swapChainFormat);
            vk::Extent2D requestedExtent(tempWindow->GetWidth(), tempWindow->GetHeight());

            // check that this device supports our intended swap chain creation parameters
            auto surfaceCaps = dev.getSurfaceCapabilitiesKHR(windowSurface);
            auto surfaceFmts = dev.getSurfaceFormatsKHR(windowSurface);
            auto surfacePModes = dev.getSurfacePresentModesKHR(windowSurface);
            if (surfaceCaps.minImageCount > tempWindow->desc.swapChainDesc.swapChainBufferCount ||
                (surfaceCaps.maxImageCount < tempWindow->desc.swapChainDesc.swapChainBufferCount && surfaceCaps.maxImageCount > 0))
            {
                errorStream << std::endl << "  - cannot support the requested swap chain image count:";
                errorStream << " requested " << tempWindow->desc.swapChainDesc.swapChainBufferCount << ", available " << surfaceCaps.minImageCount << " - " << surfaceCaps.maxImageCount;
                deviceIsGood = false;
            }
            if (surfaceCaps.minImageExtent.width > requestedExtent.width ||
                surfaceCaps.minImageExtent.height > requestedExtent.height ||
                surfaceCaps.maxImageExtent.width < requestedExtent.width ||
                surfaceCaps.maxImageExtent.height < requestedExtent.height)
            {
                errorStream << std::endl << "  - cannot support the requested swap chain size:";
                errorStream << " requested " << requestedExtent.width << "x" << requestedExtent.height << ", ";
                errorStream << " available " << surfaceCaps.minImageExtent.width << "x" << surfaceCaps.minImageExtent.height;
                errorStream << " - " << surfaceCaps.maxImageExtent.width << "x" << surfaceCaps.maxImageExtent.height;
                deviceIsGood = false;
            }

            bool surfaceFormatPresent = false;
            for (const vk::SurfaceFormatKHR& surfaceFmt : surfaceFmts)
            {
                if (surfaceFmt.format == vk::Format(requestedFormat))
                {
                    surfaceFormatPresent = true;
                    break;
                }
            }

            if (!surfaceFormatPresent)
            {
                // can't create a swap chain using the format requested
                errorStream << std::endl << "  - does not support the requested swap chain format";
                deviceIsGood = false;
            }

            // check that we can present from the graphics queue
            uint32_t canPresent = dev.getSurfaceSupportKHR(graphicsQueueFamily, windowSurface);
            if (!canPresent)
            {
                errorStream << std::endl << "  - cannot present";
                deviceIsGood = false;
            }
        }

        if (!deviceIsGood)
            continue;

        if (prop.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
        {
            discreteGPUs.push_back(dev);
        }
        else
        {
            otherGPUs.push_back(dev);
        }
    }

   
    if (!desc.headlessDevice && tempWindow)
    {
        delete tempWindow;
    }


    // pick the first discrete GPU if it exists, otherwise the first integrated GPU
    if (!discreteGPUs.empty())
    {
        vulkanPhysicalDevice = discreteGPUs[0];
        return true;
    }

    if (!otherGPUs.empty())
    {
        vulkanPhysicalDevice = otherGPUs[0];
        return true;
    }

    HE_CORE_ERROR("{}", errorStream.str().c_str());

    return false;
}

bool VKDeviceManager::FindQueueFamilies(vk::PhysicalDevice physicalDevice)
{
    HE_PROFILE_FUNCTION();

    auto props = physicalDevice.getQueueFamilyProperties();

    for (int i = 0; i < int(props.size()); i++)
    {
        const auto& queueFamily = props[i];

        if (graphicsQueueFamily == -1)
        {
            if (
                queueFamily.queueCount > 0 &&
                (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics)
                )
            {
                graphicsQueueFamily = i;
            }
        }

        if (computeQueueFamily == -1)
        {
            if (
                queueFamily.queueCount > 0 &&
                (queueFamily.queueFlags & vk::QueueFlagBits::eCompute) &&
                !(queueFamily.queueFlags & vk::QueueFlagBits::eGraphics)
                )
            {
                computeQueueFamily = i;
            }
        }

        if (transferQueueFamily == -1)
        {
            if (
                queueFamily.queueCount > 0 &&
                (queueFamily.queueFlags & vk::QueueFlagBits::eTransfer) &&
                !(queueFamily.queueFlags & vk::QueueFlagBits::eCompute) &&
                !(queueFamily.queueFlags & vk::QueueFlagBits::eGraphics)
                )
            {
                transferQueueFamily = i;
            }
        }

        if (presentQueueFamily == -1)
        {
            if (queueFamily.queueCount > 0 &&
                glfwGetPhysicalDevicePresentationSupport(vulkanInstance, physicalDevice, i))
            {
                presentQueueFamily = i;
            }
        }
    }

    if (graphicsQueueFamily == -1 ||
        (presentQueueFamily == -1 && !desc.headlessDevice) ||
        (computeQueueFamily == -1 && desc.enableComputeQueue) ||
        (transferQueueFamily == -1 && desc.enableCopyQueue))
    {
        return false;
    }

    return true;
}

bool VKDeviceManager::CreateDeviceImp()
{
    HE_PROFILE_FUNCTION();

    // figure out which optional extensions are supported
    auto deviceExtensions = vulkanPhysicalDevice.enumerateDeviceExtensionProperties();
    for (const auto& ext : deviceExtensions)
    {
        const std::string name = ext.extensionName;
        if (optionalExtensions.device.find(name) != optionalExtensions.device.end())
        {
            if (name == VK_KHR_SWAPCHAIN_MUTABLE_FORMAT_EXTENSION_NAME && desc.headlessDevice)
                continue;

            enabledExtensions.device.insert(name);
        }

        if (desc.enableRayTracingExtensions && rayTracingExtensions.find(name) != rayTracingExtensions.end())
        {
            enabledExtensions.device.insert(name);
        }
    }

    if (!desc.headlessDevice)
    {
        enabledExtensions.device.insert(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }

    const vk::PhysicalDeviceProperties physicalDeviceProperties = vulkanPhysicalDevice.getProperties();
    rendererString = std::string(physicalDeviceProperties.deviceName.data());

    bool accelStructSupported = false;
    bool rayPipelineSupported = false;
    bool rayQuerySupported = false;
    bool meshletsSupported = false;
    bool vrsSupported = false;
    bool interlockSupported = false;
    bool barycentricSupported = false;
    bool storage16BitSupported = false;
    bool synchronization2Supported = false;
    bool maintenance4Supported = false;

    HE_CORE_INFO("Enabled Vulkan device extensions:");
    for (const auto& ext : enabledExtensions.device)
    {
        HE_CORE_INFO("    {}", ext.c_str());

        if (ext == VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME)
            accelStructSupported = true;
        else if (ext == VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME)
            rayPipelineSupported = true;
        else if (ext == VK_KHR_RAY_QUERY_EXTENSION_NAME)
            rayQuerySupported = true;
        else if (ext == VK_NV_MESH_SHADER_EXTENSION_NAME)
            meshletsSupported = true;
        else if (ext == VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME)
            vrsSupported = true;
        else if (ext == VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME)
            interlockSupported = true;
        else if (ext == VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME)
            barycentricSupported = true;
        else if (ext == VK_KHR_16BIT_STORAGE_EXTENSION_NAME)
            storage16BitSupported = true;
        else if (ext == VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME)
            synchronization2Supported = true;
        else if (ext == VK_KHR_MAINTENANCE_4_EXTENSION_NAME)
            maintenance4Supported = true;
        else if (ext == VK_KHR_SWAPCHAIN_MUTABLE_FORMAT_EXTENSION_NAME)
            swapChainMutableFormatSupported = true;
    }

    void* pNext = nullptr;

    vk::PhysicalDeviceFeatures2 physicalDeviceFeatures2;
    // Determine support for Buffer Device Address, the Vulkan 1.2 way
    auto bufferDeviceAddressFeatures = vk::PhysicalDeviceBufferDeviceAddressFeatures();
    // Determine support for maintenance4
    auto maintenance4Features = vk::PhysicalDeviceMaintenance4Features();

    // Put the user-provided extension structure at the end of the chain
    pNext = desc.physicalDeviceFeatures2Extensions;
    APPEND_EXTENSION(true, bufferDeviceAddressFeatures);
    APPEND_EXTENSION(maintenance4Supported, maintenance4Features);

    physicalDeviceFeatures2.pNext = pNext;
    vulkanPhysicalDevice.getFeatures2(&physicalDeviceFeatures2);

    std::unordered_set<int> uniqueQueueFamilies = {
        graphicsQueueFamily };

    if (!desc.headlessDevice)
        uniqueQueueFamilies.insert(presentQueueFamily);

    if (desc.enableComputeQueue)
        uniqueQueueFamilies.insert(computeQueueFamily);

    if (desc.enableCopyQueue)
        uniqueQueueFamilies.insert(transferQueueFamily);

    float priority = 1.f;
    std::vector<vk::DeviceQueueCreateInfo> queueDesc;
    queueDesc.reserve(uniqueQueueFamilies.size());
    for (int queueFamily : uniqueQueueFamilies)
    {
        queueDesc.push_back(vk::DeviceQueueCreateInfo()
            .setQueueFamilyIndex(queueFamily)
            .setQueueCount(1)
            .setPQueuePriorities(&priority));
    }

    auto accelStructFeatures = vk::PhysicalDeviceAccelerationStructureFeaturesKHR()
        .setAccelerationStructure(true);
    auto rayPipelineFeatures = vk::PhysicalDeviceRayTracingPipelineFeaturesKHR()
        .setRayTracingPipeline(true)
        .setRayTraversalPrimitiveCulling(true);
    auto rayQueryFeatures = vk::PhysicalDeviceRayQueryFeaturesKHR()
        .setRayQuery(true);
    auto meshletFeatures = vk::PhysicalDeviceMeshShaderFeaturesNV()
        .setTaskShader(true)
        .setMeshShader(true);
    auto interlockFeatures = vk::PhysicalDeviceFragmentShaderInterlockFeaturesEXT()
        .setFragmentShaderPixelInterlock(true);
    auto barycentricFeatures = vk::PhysicalDeviceFragmentShaderBarycentricFeaturesKHR()
        .setFragmentShaderBarycentric(true);
    auto storage16BitFeatures = vk::PhysicalDevice16BitStorageFeatures()
        .setStorageBuffer16BitAccess(true);
    auto vrsFeatures = vk::PhysicalDeviceFragmentShadingRateFeaturesKHR()
        .setPipelineFragmentShadingRate(true)
        .setPrimitiveFragmentShadingRate(true)
        .setAttachmentFragmentShadingRate(true);
    auto vulkan13features = vk::PhysicalDeviceVulkan13Features()
        .setSynchronization2(synchronization2Supported)
        .setMaintenance4(maintenance4Features.maintenance4);

    pNext = nullptr;
    APPEND_EXTENSION(accelStructSupported, accelStructFeatures)
        APPEND_EXTENSION(rayPipelineSupported, rayPipelineFeatures)
        APPEND_EXTENSION(rayQuerySupported, rayQueryFeatures)
        APPEND_EXTENSION(meshletsSupported, meshletFeatures)
        APPEND_EXTENSION(vrsSupported, vrsFeatures)
        APPEND_EXTENSION(interlockSupported, interlockFeatures)
        APPEND_EXTENSION(barycentricSupported, barycentricFeatures)
        APPEND_EXTENSION(storage16BitSupported, storage16BitFeatures)
        APPEND_EXTENSION(physicalDeviceProperties.apiVersion >= VK_API_VERSION_1_3, vulkan13features)
        APPEND_EXTENSION(physicalDeviceProperties.apiVersion < VK_API_VERSION_1_3 && maintenance4Supported, maintenance4Features);

    auto deviceFeatures = vk::PhysicalDeviceFeatures()
        .setShaderImageGatherExtended(true)
        .setSamplerAnisotropy(true)
        .setTessellationShader(true)
        .setTextureCompressionBC(true)
        .setGeometryShader(true)
        .setImageCubeArray(true)
        .setShaderInt16(true)
        .setFillModeNonSolid(true)
        .setFragmentStoresAndAtomics(true)
        .setDualSrcBlend(true);

    // Add a Vulkan 1.1 structure with default settings to make it easier for apps to modify them
    auto vulkan11features = vk::PhysicalDeviceVulkan11Features()
        .setPNext(pNext);

    auto vulkan12features = vk::PhysicalDeviceVulkan12Features()
        .setDescriptorIndexing(true)
        .setRuntimeDescriptorArray(true)
        .setDescriptorBindingPartiallyBound(true)
        .setDescriptorBindingVariableDescriptorCount(true)
        .setTimelineSemaphore(true)
        .setShaderSampledImageArrayNonUniformIndexing(true)
        .setBufferDeviceAddress(bufferDeviceAddressFeatures.bufferDeviceAddress)
        .setPNext(&vulkan11features);

    auto layerVec = stringSetToVector(enabledExtensions.layers);
    auto extVec = stringSetToVector(enabledExtensions.device);

    auto deviceDesc = vk::DeviceCreateInfo()
        .setPQueueCreateInfos(queueDesc.data())
        .setQueueCreateInfoCount(uint32_t(queueDesc.size()))
        .setPEnabledFeatures(&deviceFeatures)
        .setEnabledExtensionCount(uint32_t(extVec.size()))
        .setPpEnabledExtensionNames(extVec.data())
        .setEnabledLayerCount(uint32_t(layerVec.size()))
        .setPpEnabledLayerNames(layerVec.data())
        .setPNext(&vulkan12features);

    if (desc.deviceCreateInfoCallback)
        desc.deviceCreateInfoCallback(deviceDesc);

    const vk::Result res = vulkanPhysicalDevice.createDevice(&deviceDesc, nullptr, &device);
    if (res != vk::Result::eSuccess)
    {
        HE_CORE_ERROR("Failed to create a Vulkan physical device, error code = %s", nvrhi::vulkan::resultToString(VkResult(res)));
        return false;
    }

    device.getQueue(graphicsQueueFamily, 0, &graphicsQueue);
    if (desc.enableComputeQueue)
        device.getQueue(computeQueueFamily, 0, &computeQueue);
    if (desc.enableCopyQueue)
        device.getQueue(transferQueueFamily, 0, &transferQueue);
    if (!desc.headlessDevice)
        device.getQueue(presentQueueFamily, 0, &presentQueue);

    VULKAN_HPP_DEFAULT_DISPATCHER.init(device);

    // remember the bufferDeviceAddress feature enablement
    m_BufferDeviceAddressSupported = vulkan12features.bufferDeviceAddress;

    HE_CORE_INFO("Created device: {}", rendererString.c_str());

    return true;
}

std::vector<const char*> VKDeviceManager::stringSetToVector(const std::unordered_set<std::string>& set)
{
    std::vector<const char*> ret;
    ret.reserve(set.size());
    for (const auto& s : set)
        ret.emplace_back(s.c_str());

    return ret;
}

HE::RHI::DeviceManager* HE::RHI::CreateVULKAN() { return new VKDeviceManager(); }


//////////////////////////////////////////////////////////////////////////
// VKSwapChain
//////////////////////////////////////////////////////////////////////////

VKSwapChain::~VKSwapChain()
{
    HE_PROFILE_FUNCTION();

    Reset();

    for (auto& semaphore : presentSemaphores)
    {
        if (semaphore)
        {
            vkDeviceManager->device.destroySemaphore(semaphore);
            semaphore = vk::Semaphore();
        }
    }

    for (auto& semaphore : acquireSemaphores)
    {
        if (semaphore)
        {
            vkDeviceManager->device.destroySemaphore(semaphore);
            semaphore = vk::Semaphore();
        }
    }

    nvrhiDevice = nullptr;

    if (windowSurface)
    {
        vkDeviceManager->vulkanInstance.destroySurfaceKHR(windowSurface);
        windowSurface = nullptr;
    }
}

bool VKSwapChain::CreateWindowSurface()
{
    HE_PROFILE_FUNCTION();

    const VkResult res = glfwCreateWindowSurface(vkDeviceManager->vulkanInstance, (GLFWwindow*)windowHandle, nullptr, (VkSurfaceKHR*)&windowSurface);
    if (res != VK_SUCCESS)
    {
        HE_CORE_ERROR("Failed to create a GLFW window surface, error code = {}", nvrhi::vulkan::resultToString(res));
        return false;
    }

    return true;
}

bool VKSwapChain::CreateSwapChain(const HE::SwapChainDesc& swapChainDesc, uint32_t width, uint32_t height)
{
    swapChainFormat = {
        vk::Format(nvrhi::vulkan::convertFormat(swapChainDesc.swapChainFormat)),
        vk::ColorSpaceKHR::eSrgbNonlinear
    };

    vk::Extent2D extent = vk::Extent2D(width, height);

    std::unordered_set<uint32_t> uniqueQueues = { uint32_t(vkDeviceManager->graphicsQueueFamily), uint32_t(vkDeviceManager->presentQueueFamily) };
    std::vector<uint32_t> queues = setToVector(uniqueQueues);

    const bool enableSwapChainSharing = queues.size() > 1;

    auto desc = vk::SwapchainCreateInfoKHR()
        .setSurface(windowSurface)
        .setMinImageCount(swapChainDesc.swapChainBufferCount)
        .setImageFormat(swapChainFormat.format)
        .setImageColorSpace(swapChainFormat.colorSpace)
        .setImageExtent(extent)
        .setImageArrayLayers(1)
        .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled)
        .setImageSharingMode(enableSwapChainSharing ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive)
        .setFlags(vkDeviceManager->swapChainMutableFormatSupported ? vk::SwapchainCreateFlagBitsKHR::eMutableFormat : vk::SwapchainCreateFlagBitsKHR(0))
        .setQueueFamilyIndexCount(enableSwapChainSharing ? uint32_t(queues.size()) : 0)
        .setPQueueFamilyIndices(enableSwapChainSharing ? queues.data() : nullptr)
        .setPreTransform(vk::SurfaceTransformFlagBitsKHR::eIdentity)
        .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
        .setPresentMode(swapChainDesc.vsync ? vk::PresentModeKHR::eFifo : vk::PresentModeKHR::eImmediate)
        .setClipped(true)
        .setOldSwapchain(nullptr);

    std::vector<vk::Format> imageFormats = { swapChainFormat.format };
    switch (swapChainFormat.format)
    {
    case vk::Format::eR8G8B8A8Unorm:
        imageFormats.push_back(vk::Format::eR8G8B8A8Srgb);
        break;
    case vk::Format::eR8G8B8A8Srgb:
        imageFormats.push_back(vk::Format::eR8G8B8A8Unorm);
        break;
    case vk::Format::eB8G8R8A8Unorm:
        imageFormats.push_back(vk::Format::eB8G8R8A8Srgb);
        break;
    case vk::Format::eB8G8R8A8Srgb:
        imageFormats.push_back(vk::Format::eB8G8R8A8Unorm);
        break;
    default:
        break;
    }

    auto imageFormatListCreateInfo = vk::ImageFormatListCreateInfo()
        .setViewFormats(imageFormats);

    if (vkDeviceManager->swapChainMutableFormatSupported)
        desc.pNext = &imageFormatListCreateInfo;

    {
        HE_PROFILE_SCOPE("device.createSwapchainKHR");

        const vk::Result res = vkDeviceManager->device.createSwapchainKHR(&desc, nullptr, &swapChain);
        if (res != vk::Result::eSuccess)
        {
            HE_CORE_ERROR("Failed to create a Vulkan swap chain, error code = {}", nvrhi::vulkan::resultToString(VkResult(res)));
            return false;
        }
    }

    // retrieve swap chain images
    {
        HE_PROFILE_SCOPE("retrieve swap chain images");

        auto images = vkDeviceManager->device.getSwapchainImagesKHR(swapChain);
        for (auto image : images)
        {
            SwapChainImage sci;
            sci.image = image;

            nvrhi::TextureDesc textureDesc;
            textureDesc.width = width;
            textureDesc.height = height;
            textureDesc.format = swapChainDesc.swapChainFormat;
            textureDesc.debugName = "Swap Chain Image";
            textureDesc.initialState = nvrhi::ResourceStates::Present;
            textureDesc.keepInitialState = true;
            textureDesc.isRenderTarget = true;

            sci.rhiHandle = nvrhiDevice->createHandleForNativeTexture(nvrhi::ObjectTypes::VK_Image, nvrhi::Object(sci.image), textureDesc);
            swapChainImages.push_back(sci);
        }
    }

    swapChainIndex = 0;
    
    ResizeBackBuffers();

    return true;
}

void VKSwapChain::Reset()
{
    HE_PROFILE_FUNCTION();

    if (vkDeviceManager->device)
    {
        HE_PROFILE_SCOPE("device.waitIdle");

        vkDeviceManager->device.waitIdle();
    }

    if (swapChain)
    {
        HE_PROFILE_SCOPE("device.destroySwapchainKHR");
        vkDeviceManager->device.destroySwapchainKHR(swapChain);
        swapChain = nullptr;
    }

    swapChainImages.clear();
}

void VKSwapChain::ResizeSwapChain(uint32_t width, uint32_t height)
{
    HE_PROFILE_FUNCTION();

    desc.backBufferWidth = width;
    desc.backBufferHeight = height;

    ResetBackBuffers();
    Reset();
    CreateSwapChain(desc, width, height);
    ResizeBackBuffers();
}

bool VKSwapChain::BeginFrame()
{
    HE_PROFILE_FUNCTION();

    const auto& semaphore = acquireSemaphores[acquireSemaphoreIndex];

    vk::Result res;

    int const maxAttempts = 3;
    for (int attempt = 0; attempt < maxAttempts; ++attempt)
    {
        res = vkDeviceManager->device.acquireNextImageKHR(swapChain, std::numeric_limits<uint64_t>::max()/*timeout*/, semaphore, vk::Fence(), &swapChainIndex);

        if (res == vk::Result::eErrorOutOfDateKHR && attempt < maxAttempts)
        {
            auto surfaceCaps = vkDeviceManager->vulkanPhysicalDevice.getSurfaceCapabilitiesKHR(windowSurface);
            ResizeSwapChain(surfaceCaps.currentExtent.width, surfaceCaps.currentExtent.height);
        }
        else
        {
            break;
        }
    }

    acquireSemaphoreIndex = (acquireSemaphoreIndex + 1) % acquireSemaphores.size();

    if (res == vk::Result::eSuccess)
    {
        // Schedule the wait. The actual wait operation will be submitted when the app executes any command list.

        vkDeviceManager->nvrhiDevice->queueWaitForSemaphore(nvrhi::CommandQueue::Graphics, semaphore, 0);
        return true;
    }

    return false;
}

bool VKSwapChain::Present()
{
    HE_PROFILE_FUNCTION();

    const auto& semaphore = presentSemaphores[presentSemaphoreIndex];

    vkDeviceManager->nvrhiDevice->queueSignalSemaphore(nvrhi::CommandQueue::Graphics, semaphore, 0);

    // NVRHI buffers the semaphores and signals them when something is submitted to a queue.
    // Call 'executeCommandLists' with no command lists to actually signal the semaphore.
    nvrhiDevice->executeCommandLists(nullptr, 0);

    vk::PresentInfoKHR info = vk::PresentInfoKHR()
        .setWaitSemaphoreCount(1)
        .setPWaitSemaphores(&semaphore)
        .setSwapchainCount(1)
        .setPSwapchains(&swapChain)
        .setPImageIndices(&swapChainIndex);

    const vk::Result res = vkDeviceManager->presentQueue.presentKHR(&info);
    HE_CORE_ASSERT(res == vk::Result::eSuccess || res == vk::Result::eErrorOutOfDateKHR);

    presentSemaphoreIndex = (presentSemaphoreIndex + 1) % presentSemaphores.size();

#ifndef HE_PLATFORM_WINDOWS
    if (desc.vsync || vkDeviceManager->desc.enableDebugRuntime)
    {
        // according to vulkan-tutorial.com, "the validation layer implementation expects
        // the application to explicitly synchronize with the GPU"
        vkDeviceManager->presentQueue.waitIdle();
    }
#endif

    while (framesInFlight.size() >= desc.maxFramesInFlight)
    {
        auto query = framesInFlight.front();
        framesInFlight.pop();

        nvrhiDevice->waitEventQuery(query);

        queryPool.push_back(query);
    }

    nvrhi::EventQueryHandle query;
    if (!queryPool.empty())
    {
        query = queryPool.back();
        queryPool.pop_back();
    }
    else
    {
        query = nvrhiDevice->createEventQuery();
    }

    nvrhiDevice->resetEventQuery(query);
    nvrhiDevice->setEventQuery(query, nvrhi::CommandQueue::Graphics);
    framesInFlight.push(query);

    nvrhiDevice->runGarbageCollection();

    return true;
}
