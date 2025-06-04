module;

#include "HydraEngine/Base.h"

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

module HE;
import nvrhi;

// Based on NVIDIA Donut framework (MIT License), with slight modifications
// Source: https://github.com/NVIDIA-RTX/Donut/blob/main/src/app/vulkan/DeviceManager_VK.cpp

#define CHECK(a) if (!(a)) { return false; }

// Define the Vulkan dynamic dispatcher - this needs to occur in exactly one cpp file in the program.
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace HE {

    class DeviceManagerVK : public DeviceManager
    {
    public:

        struct VulkanExtensionSet
        {
            std::unordered_set<std::string> instance;
            std::unordered_set<std::string> layers;
            std::unordered_set<std::string> device;
        };

        struct SwapChainImage
        {
            vk::Image image;
            nvrhi::TextureHandle rhiHandle;
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
        int graphicsQueueFamily = -1;
        int computeQueueFamily = -1;
        int transferQueueFamily = -1;
        int presentQueueFamily = -1;

        vk::Device device;
        vk::Queue graphicsQueue;
        vk::Queue computeQueue;
        vk::Queue transferQueue;
        vk::Queue presentQueue;

        vk::SurfaceKHR windowSurface;

        vk::SurfaceFormatKHR swapChainFormat;
        vk::SwapchainKHR swapChain;
        bool swapChainMutableFormatSupported = false;

        std::vector<SwapChainImage> swapChainImages;
        uint32_t swapChainIndex = uint32_t(-1);

        nvrhi::vulkan::DeviceHandle nvrhiDevice;
        nvrhi::DeviceHandle validationLayer;

        std::vector<vk::Semaphore> acquireSemaphores;
        std::vector<vk::Semaphore> presentSemaphores;
        uint32_t acquireSemaphoreIndex = 0;
        uint32_t presentSemaphoreIndex = 0;

        std::queue<nvrhi::EventQueryHandle> framesInFlight;
        std::vector<nvrhi::EventQueryHandle> queryPool;

        bool m_BufferDeviceAddressSupported = false;

#if VK_HEADER_VERSION >= 301
        typedef vk::detail::DynamicLoader VulkanDynamicLoader;
#else
        typedef vk::DynamicLoader VulkanDynamicLoader;
#endif

        std::unique_ptr<VulkanDynamicLoader> dynamicLoader;

        static VKAPI_ATTR VkBool32 VKAPI_CALL vulkanDebugCallback(
            vk::DebugReportFlagsEXT flags,
            vk::DebugReportObjectTypeEXT objType,
            uint64_t obj,
            size_t location,
            int32_t code,
            const char* layerPrefix,
            const char* msg,
            void* userData
        )
        {
            const DeviceManagerVK* manager = static_cast<const DeviceManagerVK*>(userData);

            // Skip ignored messages
            if (manager) {
                const auto& ignored = manager->m_DeviceDesc.ignoredVulkanValidationMessageLocations;
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

        static std::vector<const char*> stringSetToVector(const std::unordered_set<std::string>& set)
        {
            std::vector<const char*> ret;
            for (const auto& s : set)
                ret.push_back(s.c_str());

            return ret;
        }

        template <typename T>
        static std::vector<T> setToVector(const std::unordered_set<T>& set)
        {
            std::vector<T> ret;
            for (const auto& s : set)
            {
                ret.push_back(s);
            }

            return ret;
        }

        nvrhi::IDevice* GetDevice() const override { return validationLayer ? validationLayer : nvrhiDevice; }

        nvrhi::GraphicsAPI GetGraphicsAPI() const override { return nvrhi::GraphicsAPI::VULKAN; }

        bool EnumerateAdapters(std::vector<AdapterInfo>& outAdapters) override
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

                AdapterInfo adapterInfo;
                adapterInfo.name = properties.deviceName.data();
                adapterInfo.vendorID = properties.vendorID;
                adapterInfo.deviceID = properties.deviceID;
                adapterInfo.vkPhysicalDevice = physicalDevice;
                adapterInfo.dedicatedVideoMemory = 0;

                AdapterInfo::UUID uuid;
                static_assert(uuid.size() == idProperties.deviceUUID.size());
                memcpy(uuid.data(), idProperties.deviceUUID.data(), uuid.size());
                adapterInfo.uuid = uuid;

                if (idProperties.deviceLUIDValid)
                {
                    AdapterInfo::LUID luid;
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

        bool CreateInstance()
        {
            HE_PROFILE_FUNCTION();

            if (!m_DeviceDesc.headlessDevice)
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
            for (const std::string& name : m_DeviceDesc.requiredVulkanInstanceExtensions)
            {
                enabledExtensions.instance.insert(name);
            }
            for (const std::string& name : m_DeviceDesc.optionalVulkanInstanceExtensions)
            {
                optionalExtensions.instance.insert(name);
            }

            // add layers requested by the user
            for (const std::string& name : m_DeviceDesc.requiredVulkanLayers)
            {
                enabledExtensions.layers.insert(name);
            }
            for (const std::string& name : m_DeviceDesc.optionalVulkanLayers)
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

        bool CreateInstanceInternal()
        {
            HE_PROFILE_FUNCTION();

            if (m_DeviceDesc.enableDebugRuntime)
            {
                enabledExtensions.instance.insert("VK_EXT_debug_report");
                enabledExtensions.layers.insert("VK_LAYER_KHRONOS_validation");
            }
        
            dynamicLoader = std::make_unique<VulkanDynamicLoader>(m_DeviceDesc.vulkanLibraryName);
            PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = dynamicLoader->getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
            VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
        
            return CreateInstance();
        }

        bool CreateDevice() override
        {
            HE_PROFILE_SCOPE("Create VK Device");

            if (m_DeviceDesc.enableDebugRuntime)
            {
                InstallDebugCallback();
            }

            // add device extensions requested by the user
            for (const std::string& name : m_DeviceDesc.requiredVulkanDeviceExtensions)
            {
                enabledExtensions.device.insert(name);
            }
            for (const std::string& name : m_DeviceDesc.optionalVulkanDeviceExtensions)
            {
                optionalExtensions.device.insert(name);
            }

            if (!m_DeviceDesc.headlessDevice)
            {
                // Need to adjust the swap chain format before creating the device because it affects physical device selection
                if (m_DeviceDesc.swapChainFormat == nvrhi::Format::SRGBA8_UNORM)
                    m_DeviceDesc.swapChainFormat = nvrhi::Format::SBGRA8_UNORM;
                else if (m_DeviceDesc.swapChainFormat == nvrhi::Format::RGBA8_UNORM)
                    m_DeviceDesc.swapChainFormat = nvrhi::Format::BGRA8_UNORM;

                CHECK(CreateWindowSurface())
            }
            CHECK(PickPhysicalDevice())
            CHECK(FindQueueFamilies(vulkanPhysicalDevice))
            CHECK(CreateDeviceImp())

            auto vecInstanceExt = stringSetToVector(enabledExtensions.instance);
            auto vecLayers = stringSetToVector(enabledExtensions.layers);
            auto vecDeviceExt = stringSetToVector(enabledExtensions.device);

            nvrhi::vulkan::DeviceDesc deviceDesc;
            deviceDesc.errorCB = &DefaultMessageCallback::GetInstance();
            deviceDesc.instance = vulkanInstance;
            deviceDesc.physicalDevice = vulkanPhysicalDevice;
            deviceDesc.device = device;
            deviceDesc.graphicsQueue = graphicsQueue;
            deviceDesc.graphicsQueueIndex = graphicsQueueFamily;
            
            if (m_DeviceDesc.enableComputeQueue)
            {
                deviceDesc.computeQueue = computeQueue;
                deviceDesc.computeQueueIndex = computeQueueFamily;
            }
            
            if (m_DeviceDesc.enableCopyQueue)
            {
                deviceDesc.transferQueue = transferQueue;
                deviceDesc.transferQueueIndex = transferQueueFamily;
            }

            deviceDesc.instanceExtensions = vecInstanceExt.data();
            deviceDesc.numInstanceExtensions = vecInstanceExt.size();
            deviceDesc.deviceExtensions = vecDeviceExt.data();
            deviceDesc.numDeviceExtensions = vecDeviceExt.size();
            deviceDesc.bufferDeviceAddressSupported = m_BufferDeviceAddressSupported;

            nvrhiDevice = nvrhi::vulkan::createDevice(deviceDesc);

            if (m_DeviceDesc.enableNvrhiValidationLayer)
            {
                validationLayer = nvrhi::validation::createValidationLayer(nvrhiDevice);
            }

            return true;
        }
        
        bool CreateSwapChain(WindowState windowState) override
        {
            HE_PROFILE_FUNCTION();

            CHECK(CreateSwapChain())

            presentSemaphores.reserve(m_DeviceDesc.maxFramesInFlight + 1);
            acquireSemaphores.reserve(m_DeviceDesc.maxFramesInFlight + 1);
            for (uint32_t i = 0; i < m_DeviceDesc.maxFramesInFlight + 1; ++i)
            {
                presentSemaphores.push_back(device.createSemaphore(vk::SemaphoreCreateInfo()));
                acquireSemaphores.push_back(device.createSemaphore(vk::SemaphoreCreateInfo()));
            }

            return true;
        }
        
        void DestroyDeviceAndSwapChain() override
        {
            HE_PROFILE_FUNCTION();

            DestroySwapChain();

            for (auto& semaphore : presentSemaphores)
            {
                if (semaphore)
                {
                    device.destroySemaphore(semaphore);
                    semaphore = vk::Semaphore();
                }
            }

            for (auto& semaphore : acquireSemaphores)
            {
                if (semaphore)
                {
                    device.destroySemaphore(semaphore);
                    semaphore = vk::Semaphore();
                }
            }

            nvrhiDevice = nullptr;
            validationLayer = nullptr;
            rendererString.clear();

            if (device)
            {
                device.destroy();
                device = nullptr;
            }

            if (windowSurface)
            {
                HE_CORE_ASSERT(vulkanInstance);
                vulkanInstance.destroySurfaceKHR(windowSurface);
                windowSurface = nullptr;
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
        }

        void ResizeSwapChain() override
        {
            HE_PROFILE_FUNCTION();

            if (device)
            {
                CreateSwapChain();
            }
        }

        nvrhi::ITexture* GetCurrentBackBuffer() override { return swapChainImages[swapChainIndex].rhiHandle; }
        nvrhi::ITexture* GetBackBuffer(uint32_t index) override { return (index < swapChainImages.size()) ? swapChainImages[index].rhiHandle : nullptr; }
        uint32_t GetCurrentBackBufferIndex() override { return swapChainIndex; }
        uint32_t GetBackBufferCount() override { return uint32_t(swapChainImages.size()); }

        bool BeginFrame() override
        {
            HE_PROFILE_FUNCTION();

            const auto& semaphore = acquireSemaphores[acquireSemaphoreIndex];

            vk::Result res;

            int const maxAttempts = 3;
            for (int attempt = 0; attempt < maxAttempts; ++attempt)
            {
                res = device.acquireNextImageKHR(swapChain, std::numeric_limits<uint64_t>::max()/*timeout*/, semaphore, vk::Fence(), &swapChainIndex);

                if (res == vk::Result::eErrorOutOfDateKHR && attempt < maxAttempts)
                {
                    BackBufferResizing();
                    auto surfaceCaps = vulkanPhysicalDevice.getSurfaceCapabilitiesKHR(windowSurface);

                    m_DeviceDesc.backBufferWidth = surfaceCaps.currentExtent.width;
                    m_DeviceDesc.backBufferHeight = surfaceCaps.currentExtent.height;

                    ResizeSwapChain();
                    BackBufferResized();
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
                nvrhiDevice->queueWaitForSemaphore(nvrhi::CommandQueue::Graphics, semaphore, 0);
                return true;
            }

            return false;
        }
        
        void Present() override
        {
            HE_PROFILE_FUNCTION();

            const auto& semaphore = presentSemaphores[presentSemaphoreIndex];

            nvrhiDevice->queueSignalSemaphore(nvrhi::CommandQueue::Graphics, semaphore, 0);

            // NVRHI buffers the semaphores and signals them when something is submitted to a queue.
            // Call 'executeCommandLists' with no command lists to actually signal the semaphore.
            nvrhiDevice->executeCommandLists(nullptr, 0);

            vk::PresentInfoKHR info = vk::PresentInfoKHR()
                .setWaitSemaphoreCount(1)
                .setPWaitSemaphores(&semaphore)
                .setSwapchainCount(1)
                .setPSwapchains(&swapChain)
                .setPImageIndices(&swapChainIndex);

            const vk::Result res = presentQueue.presentKHR(&info);
            HE_CORE_ASSERT(res == vk::Result::eSuccess || res == vk::Result::eErrorOutOfDateKHR);

            presentSemaphoreIndex = (presentSemaphoreIndex + 1) % presentSemaphores.size();

#ifndef HE_PLATFORM_WINDOWS
            if (m_DeviceDesc.vsyncEnabled || m_DeviceDesc.enableDebugRuntime)
            {
                // according to vulkan-tutorial.com, "the validation layer implementation expects
                // the application to explicitly synchronize with the GPU"
                presentQueue.waitIdle();
            }
#endif

            while (framesInFlight.size() >= m_DeviceDesc.maxFramesInFlight)
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
        }

        const char* GetRendererString() const override
        {
            return rendererString.c_str();
        }

        bool IsVulkanInstanceExtensionEnabled(const char* extensionName) const override
        {
            return enabledExtensions.instance.contains(extensionName);
        }

        bool IsVulkanDeviceExtensionEnabled(const char* extensionName) const override
        {
            return enabledExtensions.device.contains(extensionName);
        }

        bool IsVulkanLayerEnabled(const char* layerName) const override
        {
            return enabledExtensions.layers.contains(layerName);
        }

        void GetEnabledVulkanInstanceExtensions(std::vector<std::string>& extensions) const override
        {
            for (const auto& ext : enabledExtensions.instance)
                extensions.push_back(ext);
        }

        void GetEnabledVulkanDeviceExtensions(std::vector<std::string>& extensions) const override
        {
            for (const auto& ext : enabledExtensions.device)
                extensions.push_back(ext);
        }

        void GetEnabledVulkanLayers(std::vector<std::string>& layers) const override
        {
            for (const auto& ext : enabledExtensions.layers)
                layers.push_back(ext);
        }
        
        bool CreateWindowSurface()
        {
            HE_PROFILE_FUNCTION();

            const VkResult res = glfwCreateWindowSurface(vulkanInstance, (GLFWwindow*)m_Window, nullptr, (VkSurfaceKHR*)&windowSurface);
            if (res != VK_SUCCESS)
            {
                HE_CORE_ERROR("Failed to create a GLFW window surface, error code = {}", nvrhi::vulkan::resultToString(res));
                return false;
            }

            return true;
        }
        
        void InstallDebugCallback()
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
        
        bool PickPhysicalDevice()
        {
            HE_PROFILE_FUNCTION();

            VkFormat requestedFormat = nvrhi::vulkan::convertFormat(m_DeviceDesc.swapChainFormat);
            vk::Extent2D requestedExtent(m_DeviceDesc.backBufferWidth, m_DeviceDesc.backBufferHeight);

            auto devices = vulkanInstance.enumeratePhysicalDevices();

            int firstDevice = 0;
            int lastDevice = int(devices.size()) - 1;
            if (m_DeviceDesc.adapterIndex >= 0)
            {
                if (m_DeviceDesc.adapterIndex > lastDevice)
                {
                    HE_CORE_ERROR("The specified Vulkan physical device {} does not exist.", m_DeviceDesc.adapterIndex);
                    return false;
                }
                firstDevice = m_DeviceDesc.adapterIndex;
                lastDevice = m_DeviceDesc.adapterIndex;
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

                if (windowSurface)
                {
                    // check that this device supports our intended swap chain creation parameters
                    auto surfaceCaps = dev.getSurfaceCapabilitiesKHR(windowSurface);
                    auto surfaceFmts = dev.getSurfaceFormatsKHR(windowSurface);
                    auto surfacePModes = dev.getSurfacePresentModesKHR(windowSurface);
                    if (surfaceCaps.minImageCount > m_DeviceDesc.swapChainBufferCount ||
                        (surfaceCaps.maxImageCount < m_DeviceDesc.swapChainBufferCount && surfaceCaps.maxImageCount > 0))
                    {
                        errorStream << std::endl << "  - cannot support the requested swap chain image count:";
                        errorStream << " requested " << m_DeviceDesc.swapChainBufferCount << ", available " << surfaceCaps.minImageCount << " - " << surfaceCaps.maxImageCount;
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
        
        bool FindQueueFamilies(vk::PhysicalDevice physicalDevice)
        {
            HE_PROFILE_FUNCTION();

            auto props = physicalDevice.getQueueFamilyProperties();

            for (int i = 0; i < int(props.size()); i++)
            {
                const auto& queueFamily = props[i];

                if (graphicsQueueFamily == -1)
                {
                    if (queueFamily.queueCount > 0 &&
                        (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics))
                    {
                        graphicsQueueFamily = i;
                    }
                }

                if (computeQueueFamily == -1)
                {
                    if (queueFamily.queueCount > 0 &&
                        (queueFamily.queueFlags & vk::QueueFlagBits::eCompute) &&
                        !(queueFamily.queueFlags & vk::QueueFlagBits::eGraphics))
                    {
                        computeQueueFamily = i;
                    }
                }

                if (transferQueueFamily == -1)
                {
                    if (queueFamily.queueCount > 0 &&
                        (queueFamily.queueFlags & vk::QueueFlagBits::eTransfer) &&
                        !(queueFamily.queueFlags & vk::QueueFlagBits::eCompute) &&
                        !(queueFamily.queueFlags & vk::QueueFlagBits::eGraphics))
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
                (presentQueueFamily == -1 && !m_DeviceDesc.headlessDevice) ||
                (computeQueueFamily == -1 && m_DeviceDesc.enableComputeQueue) ||
                (transferQueueFamily == -1 && m_DeviceDesc.enableCopyQueue))
            {
                return false;
            }

            return true;
        }
        
        bool CreateDeviceImp()
        {
            HE_PROFILE_FUNCTION();

            // figure out which optional extensions are supported
            auto deviceExtensions = vulkanPhysicalDevice.enumerateDeviceExtensionProperties();
            for (const auto& ext : deviceExtensions)
            {
                const std::string name = ext.extensionName;
                if (optionalExtensions.device.find(name) != optionalExtensions.device.end())
                {
                    if (name == VK_KHR_SWAPCHAIN_MUTABLE_FORMAT_EXTENSION_NAME && m_DeviceDesc.headlessDevice)
                        continue;

                    enabledExtensions.device.insert(name);
                }

                if (m_DeviceDesc.enableRayTracingExtensions && rayTracingExtensions.find(name) != rayTracingExtensions.end())
                {
                    enabledExtensions.device.insert(name);
                }
            }

            if (!m_DeviceDesc.headlessDevice)
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

#define APPEND_EXTENSION(condition, desc) if (condition) { (desc).pNext = pNext; pNext = &(desc); }  // NOLINT(cppcoreguidelines-macro-usage)
            void* pNext = nullptr;

            vk::PhysicalDeviceFeatures2 physicalDeviceFeatures2;
            // Determine support for Buffer Device Address, the Vulkan 1.2 way
            auto bufferDeviceAddressFeatures = vk::PhysicalDeviceBufferDeviceAddressFeatures();
            // Determine support for maintenance4
            auto maintenance4Features = vk::PhysicalDeviceMaintenance4Features();

            // Put the user-provided extension structure at the end of the chain
            pNext = m_DeviceDesc.physicalDeviceFeatures2Extensions;
            APPEND_EXTENSION(true, bufferDeviceAddressFeatures);
            APPEND_EXTENSION(maintenance4Supported, maintenance4Features);

            physicalDeviceFeatures2.pNext = pNext;
            vulkanPhysicalDevice.getFeatures2(&physicalDeviceFeatures2);

            std::unordered_set<int> uniqueQueueFamilies = {
                graphicsQueueFamily };

            if (!m_DeviceDesc.headlessDevice)
                uniqueQueueFamilies.insert(presentQueueFamily);

            if (m_DeviceDesc.enableComputeQueue)
                uniqueQueueFamilies.insert(computeQueueFamily);

            if (m_DeviceDesc.enableCopyQueue)
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
#undef APPEND_EXTENSION

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

            if (m_DeviceDesc.deviceCreateInfoCallback)
                m_DeviceDesc.deviceCreateInfoCallback(deviceDesc);

            const vk::Result res = vulkanPhysicalDevice.createDevice(&deviceDesc, nullptr, &device);
            if (res != vk::Result::eSuccess)
            {
                HE_CORE_ERROR("Failed to create a Vulkan physical device, error code = %s", nvrhi::vulkan::resultToString(VkResult(res)));
                return false;
            }

            device.getQueue(graphicsQueueFamily, 0, &graphicsQueue);
            if (m_DeviceDesc.enableComputeQueue)
                device.getQueue(computeQueueFamily, 0, &computeQueue);
            if (m_DeviceDesc.enableCopyQueue)
                device.getQueue(transferQueueFamily, 0, &transferQueue);
            if (!m_DeviceDesc.headlessDevice)
                device.getQueue(presentQueueFamily, 0, &presentQueue);

            VULKAN_HPP_DEFAULT_DISPATCHER.init(device);

            // remember the bufferDeviceAddress feature enablement
            m_BufferDeviceAddressSupported = vulkan12features.bufferDeviceAddress;

            HE_CORE_INFO("Created Vulkan device: {}", rendererString.c_str());

            return true;
        }
        
        bool CreateSwapChain()
        {
            HE_PROFILE_FUNCTION();

            DestroySwapChain();

            swapChainFormat = {
                vk::Format(nvrhi::vulkan::convertFormat(m_DeviceDesc.swapChainFormat)),
                vk::ColorSpaceKHR::eSrgbNonlinear
            };

            vk::Extent2D extent = vk::Extent2D(m_DeviceDesc.backBufferWidth, m_DeviceDesc.backBufferHeight);

            std::unordered_set<uint32_t> uniqueQueues = {
                uint32_t(graphicsQueueFamily),
                uint32_t(presentQueueFamily) };

            std::vector<uint32_t> queues = setToVector(uniqueQueues);

            const bool enableSwapChainSharing = queues.size() > 1;

            auto desc = vk::SwapchainCreateInfoKHR()
                .setSurface(windowSurface)
                .setMinImageCount(m_DeviceDesc.swapChainBufferCount)
                .setImageFormat(swapChainFormat.format)
                .setImageColorSpace(swapChainFormat.colorSpace)
                .setImageExtent(extent)
                .setImageArrayLayers(1)
                .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled)
                .setImageSharingMode(enableSwapChainSharing ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive)
                .setFlags(swapChainMutableFormatSupported ? vk::SwapchainCreateFlagBitsKHR::eMutableFormat : vk::SwapchainCreateFlagBitsKHR(0))
                .setQueueFamilyIndexCount(enableSwapChainSharing ? uint32_t(queues.size()) : 0)
                .setPQueueFamilyIndices(enableSwapChainSharing ? queues.data() : nullptr)
                .setPreTransform(vk::SurfaceTransformFlagBitsKHR::eIdentity)
                .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
                .setPresentMode(m_DeviceDesc.vsyncEnabled ? vk::PresentModeKHR::eFifo : vk::PresentModeKHR::eImmediate)
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

            if (swapChainMutableFormatSupported)
                desc.pNext = &imageFormatListCreateInfo;

            const vk::Result res = device.createSwapchainKHR(&desc, nullptr, &swapChain);
            if (res != vk::Result::eSuccess)
            {
                HE_CORE_ERROR("Failed to create a Vulkan swap chain, error code = {}", nvrhi::vulkan::resultToString(VkResult(res)));
                return false;
            }

            // retrieve swap chain images
            auto images = device.getSwapchainImagesKHR(swapChain);
            for (auto image : images)
            {
                SwapChainImage sci;
                sci.image = image;

                nvrhi::TextureDesc textureDesc;
                textureDesc.width = m_DeviceDesc.backBufferWidth;
                textureDesc.height = m_DeviceDesc.backBufferHeight;
                textureDesc.format = m_DeviceDesc.swapChainFormat;
                textureDesc.debugName = "Swap chain image";
                textureDesc.initialState = nvrhi::ResourceStates::Present;
                textureDesc.keepInitialState = true;
                textureDesc.isRenderTarget = true;

                sci.rhiHandle = nvrhiDevice->createHandleForNativeTexture(nvrhi::ObjectTypes::VK_Image, nvrhi::Object(sci.image), textureDesc);
                swapChainImages.push_back(sci);
            }

            swapChainIndex = 0;

            return true;
        }
        
        void DestroySwapChain()
        {
            HE_PROFILE_FUNCTION();

            if (device)
            {
                device.waitIdle();
            }

            if (swapChain)
            {
                device.destroySwapchainKHR(swapChain);
                swapChain = nullptr;
            }

            swapChainImages.clear();
        }
    };

    DeviceManager* DeviceManager::CreateVULKAN() { return new DeviceManagerVK(); }
}
