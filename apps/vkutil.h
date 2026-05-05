#pragma once

#include "syntax.h"

#include <vulkan/vulkan_raii.hpp>

#include <array>
#include <mutex>

namespace vulkan {
enum class QueueType : uint8_t {
    kGfx,
    kCompute,
    kTransfer,
    kCount,
};

struct QueueIndex {
    uint32_t family;
    uint32_t queue;
};

class FencePool {
    std::vector<vk::raii::Fence> fences_;
    std::mutex                   mutex_;

public:
    vk::raii::Fence acquire(vk::raii::Device const& device)
    {
        auto lock = std::scoped_lock(mutex_);
        if (fences_.empty()) {
            return vk::raii::Fence(device, {.flags = vk::FenceCreateFlagBits::eSignaled});
        }
        auto fence = std::move(fences_.back());
        fences_.pop_back();
        return fence;
    }

    void recycle(vk::raii::Device const& device, vk::raii::Fence&& fence)
    {
        auto lock = std::scoped_lock(mutex_);
        device.resetFences(*fence);
        fences_.push_back(std::move(fence));
    }
};

/// ---------------------------------------------------------------------------------------------------------
/// Context
/// ---------------------------------------------------------------------------------------------------------
struct Context{
    vk::raii::Context                context;
    vk::raii::Instance               instance        = nullptr;
    vk::raii::DebugUtilsMessengerEXT debug_messenger = nullptr;
};

Context createContext();

/// ---------------------------------------------------------------------------------------------------------
/// Device
/// ---------------------------------------------------------------------------------------------------------
struct Device{
    vk::raii::PhysicalDevice physical = nullptr;
    vk::raii::Device         logical  = nullptr;

    std::array<QueueIndex, +QueueType::kCount> queue_indices;

    [[nodiscard]] vk::Queue getQueue(QueueType queue_type) const
    {
        auto qidx = queue_indices.at(+queue_type);
        return logical.getQueue(qidx.family, qidx.queue);
    }
};

Device createDevice(vk::raii::Instance const& instance, vk::raii::SurfaceKHR const& surface);

/// ---------------------------------------------------------------------------------------------------------
/// SwapChain
/// ---------------------------------------------------------------------------------------------------------
struct SwapChain {
    vk::raii::SwapchainKHR chain          = nullptr;
    vk::SurfaceFormatKHR   surface_format = {};
    vk::Extent2D           extent         = {};

    std::vector<vk::Image>           images;
    std::vector<vk::raii::ImageView> image_views;

    void clear()
    {
        image_views.clear();
        images.clear();
        chain = nullptr;
    }

    // deferred initialization
    vk::ImageView getImageView(
        vk::raii::Device const& device,
        uint32_t                index);
};

SwapChain createSwapchain(
    vk::raii::PhysicalDevice const& physical_device,
    vk::raii::Device const&         device,
    vk::raii::SurfaceKHR const&     surface);
}