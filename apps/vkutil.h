#pragma once

#include "syntax.h"

#include <vulkan/vulkan_raii.hpp>

#include <array>
#include <memory>
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

/// ---------------------------------------------------------------------------------------------------------
/// FencePool
/// ---------------------------------------------------------------------------------------------------------
class FencePool {
    std::vector<vk::raii::Fence> pool_;
    std::mutex                   mutex_;

public:
    vk::raii::Fence acquire(vk::raii::Device const& device)
    {
        auto lock = std::scoped_lock(mutex_);
        if (pool_.empty())
            return vk::raii::Fence(device, {.flags = vk::FenceCreateFlagBits::eSignaled});
        auto fence = std::move(pool_.back());
        device.resetFences(*fence);
        pool_.pop_back();
        return fence;
    }

    void recycle(vk::raii::Fence&& fence)
    {
        if (fence != nullptr) {
            auto lock = std::scoped_lock(mutex_);
            pool_.push_back(std::move(fence));
        }
    }
};

/// ---------------------------------------------------------------------------------------------------------
/// SemaphorePool
/// ---------------------------------------------------------------------------------------------------------
class SemaphorePool {
    std::vector<vk::raii::Semaphore> pool_;
    std::mutex                       mutex_;

public:
    vk::raii::Semaphore acquire(vk::raii::Device const& device)
    {
        auto lock     = std::scoped_lock(mutex_);
        if (pool_.empty()) {
            return vk::raii::Semaphore(device, {});
        }
        auto semaphore = std::move(pool_.back());
        pool_.pop_back();
        return semaphore;
    }

    void recycle(vk::raii::Semaphore&& sempahore)
    {
        if (sempahore != nullptr) {
            auto lock = std::scoped_lock(mutex_);
            pool_.push_back(std::move(sempahore));
        }
    }
};

/// ---------------------------------------------------------------------------------------------------------
/// Context
/// ---------------------------------------------------------------------------------------------------------
struct Context {
    vk::raii::Context                context;
    vk::raii::Instance               instance        = nullptr;
    vk::raii::DebugUtilsMessengerEXT debug_messenger = nullptr;
};

Context createContext();

/// ---------------------------------------------------------------------------------------------------------
/// Device
/// ---------------------------------------------------------------------------------------------------------
struct Device {
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
class SwapChain {
    vk::raii::SwapchainKHR chain_          = nullptr;
    vk::SurfaceFormatKHR   surface_format_ = {};
    vk::Extent2D           extent_         = {};

    std::vector<vk::Image>           images_;
    std::vector<vk::raii::ImageView> image_views_;

public:
    void init(
        vk::raii::PhysicalDevice const& physical_device,
        vk::raii::Device const&         device,
        vk::raii::SurfaceKHR const&     surface);

    void clear()
    {
        image_views_.clear();
        images_.clear();
        chain_.clear();
    }

    [[nodiscard]] vk::raii::SwapchainKHR const& get() const { return chain_; }
    [[nodiscard]] vk::SurfaceFormatKHR const&   getFormat() const { return surface_format_; }
    [[nodiscard]] vk::Extent2D const&           getExtent() const { return extent_; }

    [[nodiscard]] std::tuple<vk::Result, uint32_t, vk::Image, vk::ImageView> acquireNextImage(
        vk::raii::Device const& device,
        uint64_t                timeout,
        vk::Semaphore           semaphore,
        vk::Fence               fence);
};

/// ---------------------------------------------------------------------------------------------------------
/// Helper Functions
/// ---------------------------------------------------------------------------------------------------------

[[nodiscard]] inline vk::raii::ShaderModule createShaderModule(
    const vk::raii::Device&        device,
    std::span<const unsigned char> code)
{
    vk::ShaderModuleCreateInfo create_info{
        .codeSize = code.size() * sizeof(const unsigned char),
        .pCode    = reinterpret_cast<const uint32_t*>(code.data()),
    };
    return vk::raii::ShaderModule{device, create_info};
}
} // namespace vulkan