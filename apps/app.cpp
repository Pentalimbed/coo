#include "app.h"

#include "syntax.h"
#include "vkutil.h"

#include <spdlog/spdlog.h>

#include <stdexcept>
#include <array>
#include <vulkan/vulkan.hpp>

namespace {
constexpr auto kHelloTriSpvData = std::to_array<const unsigned char>({
#include "hello_tri.slang.spv.h"
});

[[nodiscard]] vk::raii::ShaderModule createShaderModule(const vk::raii::Device& device, std::span<const unsigned char> code)
{
    vk::ShaderModuleCreateInfo create_info{
        .codeSize = code.size() * sizeof(const unsigned char),
        .pCode    = reinterpret_cast<const uint32_t*>(code.data()),
    };
    return vk::raii::ShaderModule{device, create_info};
}

} // namespace

// ----------------------------------------------------------------------------------------------------------------------------

void App::initWindow()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window_ = glfwCreateWindow(800, 600, "COO", nullptr, nullptr);
}

void App::initVulkan()
{
    context_ = vulkan::createContext();

    VkSurfaceKHR vk_surface = nullptr;
    if (glfwCreateWindowSurface(*context_.instance, window_, nullptr, &vk_surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }
    surface_ = vk::raii::SurfaceKHR(context_.instance, vk_surface);

    device_ = vulkan::createDevice(context_.instance, surface_);
    
    swap_chain_ = vulkan::createSwapchain(device_.physical, device_.logical, surface_);

    // Pipeline
    vk::raii::ShaderModule             shader_module  = createShaderModule(device_.logical, kHelloTriSpvData);
    auto                               shader_stages  = std::to_array<vk::PipelineShaderStageCreateInfo>({
        {
            .stage  = vk::ShaderStageFlagBits::eVertex,
            .module = shader_module,
            .pName  = "vertMain",
        },
        {
            .stage  = vk::ShaderStageFlagBits::eFragment,
            .module = shader_module,
            .pName  = "fragMain",
        },
    });
    std::vector<vk::DynamicState>      dynamic_states = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamic_state  = {
        .dynamicStateCount = static_cast<uint32_t>(dynamic_states.size()),
        .pDynamicStates    = dynamic_states.data(),
    };
    vk::PipelineVertexInputStateCreateInfo   vertex_input_info;
    vk::PipelineInputAssemblyStateCreateInfo input_assembly = {.topology = vk::PrimitiveTopology::eTriangleList};
    vk::Viewport                             viewport{
        .x        = 0.0F,
        .y        = 0.0F,
        .width    = static_cast<float>(swap_chain_.extent.width),
        .height   = static_cast<float>(swap_chain_.extent.height),
        .minDepth = 0.0F,
        .maxDepth = 1.0F,
    };
    vk::Rect2D                          scissor        = {vk::Offset2D{0, 0}, swap_chain_.extent};
    vk::PipelineViewportStateCreateInfo viewport_state = {.viewportCount = 1, .pViewports = &viewport, .scissorCount = 1, .pScissors = &scissor};

    vk::PipelineRasterizationStateCreateInfo rasterizer{
        .depthClampEnable        = vk::False,
        .rasterizerDiscardEnable = vk::False,
        .polygonMode             = vk::PolygonMode::eFill,
        .cullMode                = vk::CullModeFlagBits::eBack,
        .frontFace               = vk::FrontFace::eClockwise,
        .depthBiasEnable         = vk::False,
        .lineWidth               = 1.0F,
    };
    vk::PipelineMultisampleStateCreateInfo multisampling = {.rasterizationSamples = vk::SampleCountFlagBits::e1, .sampleShadingEnable = vk::False};
    vk::PipelineColorBlendAttachmentState  color_blend_attachment{
        .blendEnable    = vk::False,
        .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};
    vk::PipelineColorBlendStateCreateInfo color_blending{
        .logicOpEnable = vk::False, .logicOp = vk::LogicOp::eCopy, .attachmentCount = 1, .pAttachments = &color_blend_attachment};

    vk::PipelineLayoutCreateInfo pipeline_layout_info{.setLayoutCount = 0, .pushConstantRangeCount = 0};
    m_pipeline_layout = vk::raii::PipelineLayout(device_.logical, pipeline_layout_info);

    auto pipeline_create_info_chain = vk::StructureChain{
        vk::GraphicsPipelineCreateInfo{
            .stageCount          = 2,
            .pStages             = shader_stages.data(),
            .pVertexInputState   = &vertex_input_info,
            .pInputAssemblyState = &input_assembly,
            .pViewportState      = &viewport_state,
            .pRasterizationState = &rasterizer,
            .pMultisampleState   = &multisampling,
            .pColorBlendState    = &color_blending,
            .pDynamicState       = &dynamic_state,
            .layout              = m_pipeline_layout,
            .renderPass          = nullptr,
        },
        vk::PipelineRenderingCreateInfo{
            .colorAttachmentCount    = 1,
            .pColorAttachmentFormats = &swap_chain_.surface_format.format,
        },
    };
    m_graphics_pipeline = vk::raii::Pipeline(device_.logical, nullptr, pipeline_create_info_chain.get<vk::GraphicsPipelineCreateInfo>());

    // Cmd
    vk::CommandPoolCreateInfo pool_info{.flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                        .queueFamilyIndex = device_.queue_indices[+vulkan::QueueType::kGfx].family};
    m_command_pool = vk::raii::CommandPool(device_.logical, pool_info);

    vk::CommandBufferAllocateInfo alloc_info{.commandPool = m_command_pool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1};
    m_command_buffer = std::move(vk::raii::CommandBuffers(device_.logical, alloc_info).front());

    // Sync objs
    m_acquire_semaphore = vk::raii::Semaphore(device_.logical, vk::SemaphoreCreateInfo());
    m_submit_semaphore  = vk::raii::Semaphore(device_.logical, vk::SemaphoreCreateInfo());
    m_present_fence     = vk::raii::Fence(device_.logical, {.flags = vk::FenceCreateFlagBits::eSignaled});

    spdlog::info("Vulkan initialized!");
}

void App::mainLoop()
{
    while (glfwWindowShouldClose(window_) == 0) {
        glfwPollEvents();
        drawFrame();
    }

    device_.logical.waitIdle();
}

void App::cleanup()
{
    swap_chain_.clear();

    glfwDestroyWindow(window_);
    glfwTerminate();
}

void App::recreateSwapChain()
{
    int width  = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window_, &width, &height);
        glfwWaitEvents();
    }

    auto fence_result = device_.logical.waitForFences(*m_present_fence, vk::True, UINT64_MAX);
    if (fence_result != vk::Result::eSuccess) {
        throw std::runtime_error("failed to wait for fence!");
    }

    swap_chain_.clear();
    swap_chain_         = vulkan::createSwapchain(device_.physical, device_.logical, surface_);
}

void App::recordCmdBuffer(uint32_t image_index)
{
    m_command_buffer.begin({});

    // Transition the image layout for rendering
    {
        vk::ImageMemoryBarrier2 barrier = {
            .srcStageMask        = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .srcAccessMask       = {},
            .dstStageMask        = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .dstAccessMask       = vk::AccessFlagBits2::eColorAttachmentWrite,
            .oldLayout           = vk::ImageLayout::eUndefined,
            .newLayout           = vk::ImageLayout::eColorAttachmentOptimal,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image               = swap_chain_.images[image_index],
            .subresourceRange    = {
                .aspectMask     = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1,
            },
        };
        vk::DependencyInfo dependency_info = {
            .dependencyFlags         = {},
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers    = &barrier};
        m_command_buffer.pipelineBarrier2(dependency_info);
    }

    // Set up the color attachment
    vk::ClearValue              clear_color     = vk::ClearColorValue(0.0F, 0.0F, 0.0F, 1.0F);
    vk::RenderingAttachmentInfo attachment_info = {
        .imageView   = swap_chain_.getImageView(device_.logical, image_index),
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp      = vk::AttachmentLoadOp::eClear,
        .storeOp     = vk::AttachmentStoreOp::eStore,
        .clearValue  = clear_color};

    // Set up the rendering info
    vk::RenderingInfo rendering_info = {
        .renderArea           = {.offset = {0, 0}, .extent = swap_chain_.extent},
        .layerCount           = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &attachment_info};

    // Begin rendering
    m_command_buffer.beginRendering(rendering_info);

    // Rendering commands will go here
    m_command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_graphics_pipeline);
    m_command_buffer.setViewport(0, vk::Viewport(0.0F, 0.0F, static_cast<float>(swap_chain_.extent.width), static_cast<float>(swap_chain_.extent.height), 0.0F, 1.0F));
    m_command_buffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swap_chain_.extent));
    m_command_buffer.draw(3, 1, 0, 0);

    // End rendering
    m_command_buffer.endRendering();

    // Transition the image layout for presentation
    {
        vk::ImageMemoryBarrier2 barrier = {
            .srcStageMask        = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .srcAccessMask       = vk::AccessFlagBits2::eColorAttachmentWrite,
            .dstStageMask        = vk::PipelineStageFlagBits2::eBottomOfPipe,
            .dstAccessMask       = {},
            .oldLayout           = vk::ImageLayout::eColorAttachmentOptimal,
            .newLayout           = vk::ImageLayout::ePresentSrcKHR,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image               = swap_chain_.images[image_index],
            .subresourceRange    = {
                .aspectMask     = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1,
            },
        };
        vk::DependencyInfo dependency_info = {
            .dependencyFlags         = {},
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers    = &barrier};
        m_command_buffer.pipelineBarrier2(dependency_info);
    }

    m_command_buffer.end();
}

void App::drawFrame()
{
    auto fence_result = device_.logical.waitForFences(*m_present_fence, vk::True, UINT64_MAX);
    if (fence_result != vk::Result::eSuccess) {
        throw std::runtime_error("failed to wait for fence!");
    }

    auto [result, image_index] = swap_chain_.chain.acquireNextImage(UINT64_MAX, m_acquire_semaphore, nullptr);
    if (result == vk::Result::eErrorOutOfDateKHR) {
        recreateSwapChain();
        return;
    }
    if ((result != vk::Result::eSuccess) && (result != vk::Result::eSuboptimalKHR)) {
        assert(result == vk::Result::eTimeout || result == vk::Result::eNotReady);
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    // Render and present
    recordCmdBuffer(image_index);

    vk::PipelineStageFlags wait_destination_stage_mask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    const vk::SubmitInfo   submit_info{
        .waitSemaphoreCount   = 1,
        .pWaitSemaphores      = &*m_acquire_semaphore,
        .pWaitDstStageMask    = &wait_destination_stage_mask,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &*m_command_buffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores    = &*m_submit_semaphore,
    };

    auto gfx_queue = device_.getQueue(vulkan::QueueType::kGfx);
    gfx_queue.submit(submit_info);

    device_.logical.resetFences(*m_present_fence);
    auto present_info = vk::StructureChain{
        vk::PresentInfoKHR{
            .waitSemaphoreCount = 1,
            .pWaitSemaphores    = &*m_submit_semaphore,
            .swapchainCount     = 1,
            .pSwapchains        = &*swap_chain_.chain,
            .pImageIndices      = &image_index,
        },
        vk::SwapchainPresentFenceInfoEXT{
            .swapchainCount = 1,
            .pFences        = &*m_present_fence,
        },
    };
    result = gfx_queue.presentKHR(present_info.get<vk::PresentInfoKHR>());
    if ((result == vk::Result::eSuboptimalKHR) || (result == vk::Result::eErrorOutOfDateKHR)) {
        recreateSwapChain();
    }
}