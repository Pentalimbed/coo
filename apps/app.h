#pragma once

#include "vkutil.h"

#include <vulkan/vulkan_raii.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// TODO pack command pool
// TODO Frames-in-flight
// TODO timeline semaphore

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
    GLFWwindow* window_ = nullptr;

    vulkan::Context      context_;
    vulkan::Device       device_;
    vk::raii::SurfaceKHR surface_ = nullptr;
    vulkan::SwapChain    swap_chain_;

    vk::raii::PipelineLayout m_pipeline_layout   = nullptr;
    vk::raii::Pipeline       m_graphics_pipeline = nullptr;

    vk::raii::CommandPool   m_command_pool      = nullptr;
    vk::raii::CommandBuffer m_command_buffer    = nullptr;
    vk::raii::Semaphore     m_acquire_semaphore = nullptr;
    vk::raii::Semaphore     m_submit_semaphore  = nullptr;
    vk::raii::Fence         m_present_fence     = nullptr;

    void initWindow();
    void initVulkan();
    void mainLoop();
    void cleanup();

    void recreateSwapChain();

    void recordCmdBuffer(vk::Image swap_img, vk::ImageView swap_img_view);
    void drawFrame();
};