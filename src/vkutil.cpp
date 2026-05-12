#include "vkutil.h"

#include "util.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <stdexcept>

namespace {
VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT      severity,
    vk::DebugUtilsMessageTypeFlagsEXT             type,
    const vk::DebugUtilsMessengerCallbackDataEXT* p_callback_data,
    [[maybe_unused]] void*                        p_user_data)
{
    spdlog::level::level_enum log_level = spdlog::level::off;
    switch (severity) {
        case vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose:
            log_level = spdlog::level::trace;
            break;
        case vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo:
            log_level = spdlog::level::info;
            break;
        case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
            log_level = spdlog::level::warn;
            break;
        case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
            log_level = spdlog::level::err;
            break;
        default:
            std::unreachable();
            break;
    }
    spdlog::log(log_level, "validation layer: type {} msg: {}", vk::to_string(type), p_callback_data->pMessage);

    return vk::False;
}

[[nodiscard]] bool isDeviceSuitable(vk::raii::PhysicalDevice const& physical_device, std::span<const char*> device_exts)
{
    // Check if the physicalDevice supports the Vulkan 1.3 API version
    if (physical_device.getProperties().apiVersion < vk::ApiVersion13)
        return false;

    // Check if any of the queue families support graphics operations
    if (std::ranges::none_of(physical_device.getQueueFamilyProperties(), [](auto const& qfp) { return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics); }))
        return false;

    // Check if all required physicalDevice extensions are available
    const auto ext_props = physical_device.enumerateDeviceExtensionProperties();
    if (std::ranges::any_of(device_exts,
                            [&ext_props](const auto* device_ext) {
                                return std::ranges::none_of(ext_props,
                                                            [device_ext](auto const& extension_property) { return strcmp(extension_property.extensionName, device_ext) == 0; });
                            }))
        return false;


    // Check if the physical device supports the required features (dynamic rendering and extended dynamic state)
    const auto features = physical_device.getFeatures2<
        vk::PhysicalDeviceFeatures2,
        vk::PhysicalDeviceVulkan13Features,
        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
    bool supports_required_features = (features.get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering != 0U) &&
                                      (features.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState != 0U);
    if (!supports_required_features)
        return false;

    // Return true if the physicalDevice meets all the criteria
    return true;
}

[[nodiscard]] vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& available_formats)
{
    const auto format_it = std::ranges::find_if(
        available_formats,
        [](const auto& format) { return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear; });
    return format_it != available_formats.end() ? *format_it : available_formats[0];
}

[[nodiscard]] vk::PresentModeKHR chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const& available_present_modes)
{
    assert(std::ranges::any_of(available_present_modes, [](auto present_mode) { return present_mode == vk::PresentModeKHR::eFifo; }));
    return std::ranges::any_of(available_present_modes,
                               [](const vk::PresentModeKHR value) { return vk::PresentModeKHR::eMailbox == value; }) ?
               vk::PresentModeKHR::eMailbox :
               vk::PresentModeKHR::eFifo;
}

[[nodiscard]] vk::Extent2D chooseSwapExtent(
    vk::SurfaceCapabilitiesKHR const& capabilities)
{
    return (capabilities.currentExtent.width == 0xFFFFFFFF) ? vk::Extent2D(400, 300) : capabilities.currentExtent;
}

[[nodiscard]] uint32_t chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const& surface_capabilities)
{
    auto min_image_count = std::max(3U, surface_capabilities.minImageCount);
    if ((0 < surface_capabilities.maxImageCount) && (surface_capabilities.maxImageCount < min_image_count)) {
        min_image_count = surface_capabilities.maxImageCount;
    }
    return min_image_count;
}
} // namespace

namespace vulkan {
Context createContext()
{
    Context ctx{};

#ifdef NDEBUG
    constexpr bool kEnableValidationLayers = false;
#else
    constexpr bool kEnableValidationLayers = true;
#endif

    // Instance
    {
        constexpr vk::ApplicationInfo kAppInfo{
            .pApplicationName   = "Hello Triangle",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName        = "COO",
            .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion         = vk::ApiVersion14,
        };

        // Get required instance extensions
        std::vector<const char*> req_inst_exts = {
            vk::KHRGetSurfaceCapabilities2ExtensionName, // required by VK_EXT_surface_maintenance1
            vk::EXTSurfaceMaintenance1ExtensionName,     // required by VK_EXT_swapchain_maintenance1
        };
        {
            // Get the required instance extensions from GLFW.
            uint32_t glfw_extension_count = 0;
            auto*    glfw_extension_names = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
            req_inst_exts.append_range(std::span(glfw_extension_names, glfw_extension_count));

            if (kEnableValidationLayers) {
                req_inst_exts.push_back(vk::EXTDebugUtilsExtensionName);
            }
        }
        auto inst_ext_props = ctx.context.enumerateInstanceExtensionProperties();
        for (const auto* inst_ext : req_inst_exts) {
            if (std::ranges::none_of(inst_ext_props,
                                     [inst_ext](auto const& extension_property) { return strcmp(extension_property.extensionName, inst_ext) == 0; })) {
                throw std::runtime_error("Required extension not supported: " + std::string(inst_ext));
            }
        }

        // Get required layers
        std::vector<char const*> req_layers;
        {
            constexpr auto kValidationLayers = std::array{
                "VK_LAYER_KHRONOS_validation"};

            if constexpr (kEnableValidationLayers) {
                req_layers.assign(kValidationLayers.begin(), kValidationLayers.end());
            }
        }
        auto layer_props = ctx.context.enumerateInstanceLayerProperties();
        for (const auto* layer : req_layers) {
            if (std::ranges::none_of(layer_props,
                                     [layer](auto const& layer_property) { return strcmp(layer_property.layerName, layer) == 0; })) {
                throw std::runtime_error("Required layer not supported: " + std::string(layer));
            }
        }

        vk::InstanceCreateInfo instance_create_info{
            .pApplicationInfo        = &kAppInfo,
            .enabledLayerCount       = static_cast<uint32_t>(req_layers.size()),
            .ppEnabledLayerNames     = req_layers.data(),
            .enabledExtensionCount   = static_cast<uint32_t>(req_inst_exts.size()),
            .ppEnabledExtensionNames = req_inst_exts.data()};
        ctx.instance = vk::raii::Instance(ctx.context, instance_create_info);
    }

    // Debug layer
    if constexpr (kEnableValidationLayers) {
        vk::DebugUtilsMessageSeverityFlagsEXT severity_flags =
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
        vk::DebugUtilsMessageTypeFlagsEXT message_type_flags =
            vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
            vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
            vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;
        vk::DebugUtilsMessengerCreateInfoEXT debug_utils_messenger_create_info{
            .messageSeverity = severity_flags,
            .messageType     = message_type_flags,
            .pfnUserCallback = &debugCallback,
        };
        ctx.debug_messenger = ctx.instance.createDebugUtilsMessengerEXT(debug_utils_messenger_create_info);
    }

    return ctx;
}

Device createDevice(vk::raii::Instance const& instance, vk::raii::SurfaceKHR const& surface)
{
    Device device;

    // Fill out device exts
    std::vector<const char*> req_device_exts = {
        vk::KHRSwapchainExtensionName,
        vk::EXTSwapchainMaintenance1ExtensionName,
    };

    // Physical device
    {
        auto const physical_devices = instance.enumeratePhysicalDevices();
        auto const dev_iter         = std::ranges::find_if(physical_devices, [&](auto const& device) { return isDeviceSuitable(device, req_device_exts); });
        if (dev_iter == physical_devices.end()) {
            throw std::runtime_error("failed to find a suitable GPU!");
        }
        device.physical = *dev_iter;
    }

    // Find queues
    auto                                   queue_family_props = device.physical.getQueueFamilyProperties();
    std::vector<std::vector<float>>        queue_priorities(queue_family_props.size());
    std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
    {
        util::HopcroftKarp matcher(+QueueType::eCount, static_cast<uint32_t>(queue_family_props.size()));
        for (uint32_t qfp_index = 0; qfp_index < queue_family_props.size(); qfp_index++) {
            // gfx
            if ((queue_family_props[qfp_index].queueFlags & vk::QueueFlagBits::eGraphics) &&
                (queue_family_props[qfp_index].queueFlags & vk::QueueFlagBits::eCompute) &&
                (device.physical.getSurfaceSupportKHR(qfp_index, surface) == vk::True))
                matcher.addEdge(+QueueType::eGfx, qfp_index);

            // compute
            if (queue_family_props[qfp_index].queueFlags & vk::QueueFlagBits::eCompute)
                matcher.addEdge(+QueueType::eCompute, qfp_index);

            // transfer
            if (queue_family_props[qfp_index].queueFlags & vk::QueueFlagBits::eTransfer)
                matcher.addEdge(+QueueType::eTransfer, qfp_index);
        }
        auto [matched, match_purpose, match_queue] = matcher.match();

        // register matched
        for (uint32_t purpose = 0; purpose < +QueueType::eCount; purpose++) {
            if (auto qfp_index = match_purpose[purpose]; qfp_index != ~0U) {
                device.queue_indices.at(purpose) = {
                    .family = qfp_index,
                    .queue  = static_cast<uint32_t>(queue_priorities[qfp_index].size()),
                };
                queue_priorities[qfp_index].push_back(0.5F);
            }
        }
        // handle the rest
        for (uint32_t purpose = 0; purpose < +QueueType::eCount; purpose++) {
            if (auto qfp_index = match_purpose[purpose]; qfp_index == ~0U) {
                const auto& qfp_candidates      = matcher.getAdj()[purpose];
                auto        available_candidate = std::ranges::find_if(qfp_candidates, [&](uint32_t qfp_idx) {
                    return queue_priorities[qfp_idx].size() < queue_family_props[qfp_idx].queueCount;
                });
                if (available_candidate == qfp_candidates.end()) {
                    throw std::runtime_error(std::format("failed to find a queue family for queue type {}", purpose));
                }
                qfp_index                        = *available_candidate;
                device.queue_indices.at(purpose) = {
                    .family = qfp_index,
                    .queue  = static_cast<uint32_t>(queue_priorities[qfp_index].size()),
                };
                queue_priorities[qfp_index].push_back(0.5F);
            }
        }

        for (uint32_t qfp_index = 0; qfp_index < queue_priorities.size(); qfp_index++)
            if (!queue_priorities[qfp_index].empty())
                queue_create_infos.push_back({
                    .queueFamilyIndex = qfp_index,
                    .queueCount       = static_cast<uint32_t>(queue_priorities[qfp_index].size()),
                    .pQueuePriorities = queue_priorities[qfp_index].data(),
                });
    }

    // Logical device
    {
        auto feature_chain = vk::StructureChain{
            vk::PhysicalDeviceFeatures2{},
            vk::PhysicalDeviceVulkan11Features{.shaderDrawParameters = vk::True},
            vk::PhysicalDeviceVulkan12Features{},
            vk::PhysicalDeviceVulkan13Features{.synchronization2 = vk::True, .dynamicRendering = vk::True},
            vk::PhysicalDeviceVulkan14Features{},
            vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT{.extendedDynamicState = vk::True},
            vk::PhysicalDeviceSwapchainMaintenance1FeaturesEXT{.swapchainMaintenance1 = vk::True},
        };
        vk::DeviceCreateInfo device_create_info{
            .pNext                   = &feature_chain.get(),
            .queueCreateInfoCount    = static_cast<uint32_t>(queue_create_infos.size()),
            .pQueueCreateInfos       = queue_create_infos.data(),
            .enabledExtensionCount   = static_cast<uint32_t>(req_device_exts.size()),
            .ppEnabledExtensionNames = req_device_exts.data(),
        };
        device.logical = vk::raii::Device(device.physical, device_create_info);
    }

    // allocator
    device.allocator = vma::raii::Allocator(
        instance, device.logical,
        vma::AllocatorCreateInfo{.flags = {}, .physicalDevice = device.physical});

    return device;
}

void SwapChain::init(
    vk::raii::PhysicalDevice const& physical_device,
    vk::raii::Device const&         device,
    vk::raii::SurfaceKHR const&     surface)
{
    image_views_.clear();
    images_.clear();
    chain_.clear(); // TODO use old chain

    auto surface_caps            = physical_device.getSurfaceCapabilitiesKHR(*surface);
    auto available_formats       = physical_device.getSurfaceFormatsKHR(*surface);
    auto available_present_modes = physical_device.getSurfacePresentModesKHR(surface);

    surface_format_ = chooseSwapSurfaceFormat(available_formats);
    extent_         = chooseSwapExtent(surface_caps);

    vk::SwapchainCreateInfoKHR create_info = {
        .flags            = vk::SwapchainCreateFlagBitsKHR::eDeferredMemoryAllocationEXT,
        .surface          = surface,
        .minImageCount    = chooseSwapMinImageCount(surface_caps),
        .imageFormat      = surface_format_.format,
        .imageColorSpace  = surface_format_.colorSpace,
        .imageExtent      = extent_,
        .imageArrayLayers = 1,
        .imageUsage       = vk::ImageUsageFlagBits::eColorAttachment,
        .imageSharingMode = vk::SharingMode::eExclusive,
        .preTransform     = surface_caps.currentTransform,
        .compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode      = chooseSwapPresentMode(available_present_modes),
        .clipped          = 1U,
    };
    chain_  = vk::raii::SwapchainKHR(device, create_info);
    images_ = chain_.getImages();
    for (size_t i = 0; i < images_.size(); i++)
        image_views_.emplace_back(nullptr);
}

std::tuple<vk::Result, uint32_t, vk::Image, vk::ImageView> SwapChain::acquireNextImage(
    vk::raii::Device const& device,
    uint64_t                timeout,
    vk::Semaphore           semaphore,
    vk::Fence               fence)
{
    auto [result, index] = chain_.acquireNextImage(timeout, semaphore, fence);
    if ((result != vk::Result::eSuccess) && (result != vk::Result::eSuboptimalKHR))
        return {result, index, nullptr, nullptr};

    if (image_views_.at(index) == nullptr) {
        vk::ImageViewCreateInfo image_view_create_info{
            .image            = images_[index],
            .viewType         = vk::ImageViewType::e2D,
            .format           = surface_format_.format,
            .components       = {},
            .subresourceRange = {
                .aspectMask     = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1,
            },
        };
        image_views_.at(index) = vk::raii::ImageView(device, image_view_create_info);
    }
    return {result, index, images_.at(index), image_views_.at(index)};
}

bool isPresentFenceInUse(vk::Result present_result)
{
    switch (present_result) {
        using enum vk::Result;

        case eSuccess:
        case eSuboptimalKHR:
        case eErrorSurfaceLostKHR:
        case eErrorOutOfDateKHR:
        case eErrorPresentTimingQueueFullEXT:
#if defined(VK_USE_PLATFORM_WIN32_KHR)
        case eErrorFullScreenExclusiveModeLostEXT:
#endif /*VK_USE_PLATFORM_WIN32_KHR*/
            return true;
        case eErrorOutOfHostMemory:
        case eErrorOutOfDeviceMemory:
        case eErrorDeviceLost:
            return false;
        default:
            std::unreachable();
            return false;
    }
}
} // namespace vulkan