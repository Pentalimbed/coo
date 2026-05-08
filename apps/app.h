#pragma once

#include "vkutil.h"

#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// TODO pack command pool
// TODO Frames-in-flight

struct Vertex
{
    glm::vec2 pos;
    glm::vec3 color;

    static vk::VertexInputBindingDescription getBindingDescription()
    {
      return {.binding = 0, .stride = sizeof(Vertex), .inputRate = vk::VertexInputRate::eVertex};
    }

    static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions()
    {
      return {{{.location = 0, .binding = 0, .format = vk::Format::eR32G32Sfloat, .offset = offsetof(Vertex, pos)},
               {.location = 1, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(Vertex, color)}}};
    }
};

class App{
public:
    void run()
    {
        initWindow();
        initVulkan();

        mainLoop();
        
        cleanup();
    }

private:
    uint64_t frame_;

    GLFWwindow* window_ = nullptr;

    vulkan::Context      context_;
    vulkan::Device       device_;
    vk::raii::SurfaceKHR surface_ = nullptr;
    vulkan::SwapChain    swap_chain_;

    vk::raii::PipelineLayout pipeline_layout_   = nullptr;
    vk::raii::Pipeline       graphics_pipeline_ = nullptr;


    const std::vector<Vertex> vertices = {
        {{0.0f, -0.5f}, {0.0f, 1.0f, 1.0f}},
        {{0.5f, 0.5f}, {1.0f, 0.0f, 1.0f}},
        {{-0.5f, 0.5f}, {1.0f, 1.0f, 0.0f}}};
    vma::raii::Buffer vertex_buffer_ = nullptr;

    constexpr static size_t kFramesInFlight = 2;
    struct PerFrame {
        bool should_close = false;

        vk::raii::CommandPool   command_pool   = nullptr;
        vk::raii::CommandBuffer command_buffer = nullptr;

        vk::raii::Semaphore acquire_semaphore    = nullptr;
        vk::raii::Semaphore submit_semaphore     = nullptr;
        bool                present_fence_in_use = false;
        vk::raii::Fence     present_fence        = nullptr;
    };
    std::array<PerFrame, kFramesInFlight> perframe_;

    void initWindow();
    void initVulkan();
    void mainLoop();
    void cleanup();

    void recreateSwapChain();

    void recordCmdBuffer(vk::raii::CommandBuffer const& cmd_buffer, vk::Image swap_img, vk::ImageView swap_img_view);
    void drawFrame();
};