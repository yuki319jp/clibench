#include "graphics_benchmark.h"
#include "triangle_vert.h"
#include "triangle_frag.h"
#include "logger.h"
#include "progress.h"
#include <array>
#include <cstring>
#include <algorithm>

struct Vertex {
    float pos[2];
    float color[3];
};

static const std::vector<Vertex> triangleVertices = {
    {{ 0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{ 0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},
    {{-0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
};

GraphicsBenchmark::GraphicsBenchmark(VulkanContext& ctx, const BenchmarkConfig& config)
    : ctx_(ctx), config_(config) {}

GraphicsBenchmark::~GraphicsBenchmark() {
    cleanup();
}

void GraphicsBenchmark::cleanup() {
    VkDevice device = ctx_.getDevice();
    if (device == VK_NULL_HANDLE) return;
    vkDeviceWaitIdle(device);

    if (vertexBuffer_ != VK_NULL_HANDLE)
        vkDestroyBuffer(device, vertexBuffer_, nullptr);
    if (vertexBufferMemory_ != VK_NULL_HANDLE)
        vkFreeMemory(device, vertexBufferMemory_, nullptr);
    if (trianglePipeline_ != VK_NULL_HANDLE)
        vkDestroyPipeline(device, trianglePipeline_, nullptr);
    if (trianglePipelineLayout_ != VK_NULL_HANDLE)
        vkDestroyPipelineLayout(device, trianglePipelineLayout_, nullptr);

    for (auto fb : framebuffers_)
        vkDestroyFramebuffer(device, fb, nullptr);
    framebuffers_.clear();

    if (renderPass_ != VK_NULL_HANDLE)
        vkDestroyRenderPass(device, renderPass_, nullptr);

    for (auto iv : swapchain_.imageViews)
        vkDestroyImageView(device, iv, nullptr);
    swapchain_.imageViews.clear();

    if (swapchain_.swapchain != VK_NULL_HANDLE)
        vkDestroySwapchainKHR(device, swapchain_.swapchain, nullptr);

    if (surface_ != VK_NULL_HANDLE)
        vkDestroySurfaceKHR(ctx_.getInstance(), surface_, nullptr);

    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }

    vertexBuffer_ = VK_NULL_HANDLE;
    vertexBufferMemory_ = VK_NULL_HANDLE;
    trianglePipeline_ = VK_NULL_HANDLE;
    trianglePipelineLayout_ = VK_NULL_HANDLE;
    renderPass_ = VK_NULL_HANDLE;
    swapchain_.swapchain = VK_NULL_HANDLE;
    surface_ = VK_NULL_HANDLE;
}

void GraphicsBenchmark::initWindow() {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); // Hidden window for benchmarking
    window_ = glfwCreateWindow(WIDTH, HEIGHT, "CLIBench", nullptr, nullptr);
    if (!window_) {
        throw std::runtime_error("Failed to create GLFW window");
    }
}

void GraphicsBenchmark::createSurface() {
    if (glfwCreateWindowSurface(ctx_.getInstance(), window_, nullptr, &surface_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface");
    }
}

void GraphicsBenchmark::createSwapchain() {
    VkPhysicalDevice physDevice = ctx_.getPhysicalDevice();

    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDevice, surface_, &capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physDevice, surface_, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physDevice, surface_, &formatCount, formats.data());

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physDevice, surface_, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physDevice, surface_, &presentModeCount, presentModes.data());

    // Choose format
    VkSurfaceFormatKHR surfaceFormat = formats[0];
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = f;
            break;
        }
    }

    // Choose present mode (prefer MAILBOX for benchmarking, IMMEDIATE as fallback)
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (const auto& mode : presentModes) {
        if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
            presentMode = mode;
        }
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            presentMode = mode;
            break;
        }
    }

    VkExtent2D extent;
    if (capabilities.currentExtent.width != UINT32_MAX) {
        extent = capabilities.currentExtent;
    } else {
        extent = {WIDTH, HEIGHT};
        extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    }

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface_;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    VkDevice device = ctx_.getDevice();
    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain_.swapchain) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swapchain");
    }

    swapchain_.format = surfaceFormat.format;
    swapchain_.extent = extent;

    vkGetSwapchainImagesKHR(device, swapchain_.swapchain, &imageCount, nullptr);
    swapchain_.images.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapchain_.swapchain, &imageCount, swapchain_.images.data());

    swapchain_.imageViews.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = swapchain_.images[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = swapchain_.format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &viewInfo, nullptr, &swapchain_.imageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image view");
        }
    }
}

void GraphicsBenchmark::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchain_.format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(ctx_.getDevice(), &renderPassInfo, nullptr, &renderPass_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass");
    }
}

void GraphicsBenchmark::createFramebuffers() {
    framebuffers_.resize(swapchain_.imageViews.size());
    for (size_t i = 0; i < swapchain_.imageViews.size(); i++) {
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = renderPass_;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = &swapchain_.imageViews[i];
        fbInfo.width = swapchain_.extent.width;
        fbInfo.height = swapchain_.extent.height;
        fbInfo.layers = 1;

        if (vkCreateFramebuffer(ctx_.getDevice(), &fbInfo, nullptr, &framebuffers_[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer");
        }
    }
}

void GraphicsBenchmark::createTrianglePipeline() {
    VkDevice device = ctx_.getDevice();

    // Shader modules
    VkShaderModuleCreateInfo vertInfo{};
    vertInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertInfo.codeSize = triangle_vert_size;
    vertInfo.pCode = reinterpret_cast<const uint32_t*>(triangle_vert_data);

    VkShaderModule vertModule;
    vkCreateShaderModule(device, &vertInfo, nullptr, &vertModule);

    VkShaderModuleCreateInfo fragInfo{};
    fragInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragInfo.codeSize = triangle_frag_size;
    fragInfo.pCode = reinterpret_cast<const uint32_t*>(triangle_frag_data);

    VkShaderModule fragModule;
    vkCreateShaderModule(device, &fragInfo, nullptr, &fragModule);

    VkPipelineShaderStageCreateInfo shaderStages[2]{};
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vertModule;
    shaderStages[0].pName = "main";
    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fragModule;
    shaderStages[1].pName = "main";

    // Vertex input
    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = sizeof(Vertex);
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 2> attrDescs{};
    attrDescs[0].binding = 0;
    attrDescs[0].location = 0;
    attrDescs[0].format = VK_FORMAT_R32G32_SFLOAT;
    attrDescs[0].offset = offsetof(Vertex, pos);
    attrDescs[1].binding = 0;
    attrDescs[1].location = 1;
    attrDescs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrDescs[1].offset = offsetof(Vertex, color);

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &bindingDesc;
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrDescs.size());
    vertexInput.pVertexAttributeDescriptions = attrDescs.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapchain_.extent.width);
    viewport.height = static_cast<float>(swapchain_.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapchain_.extent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    vkCreatePipelineLayout(device, &layoutInfo, nullptr, &trianglePipelineLayout_);

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = trianglePipelineLayout_;
    pipelineInfo.renderPass = renderPass_;
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &trianglePipeline_) != VK_SUCCESS) {
        vkDestroyShaderModule(device, vertModule, nullptr);
        vkDestroyShaderModule(device, fragModule, nullptr);
        throw std::runtime_error("Failed to create graphics pipeline");
    }

    vkDestroyShaderModule(device, vertModule, nullptr);
    vkDestroyShaderModule(device, fragModule, nullptr);

    // Create vertex buffer
    VkDeviceSize bufferSize = sizeof(Vertex) * triangleVertices.size();

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateBuffer(device, &bufferInfo, nullptr, &vertexBuffer_);

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, vertexBuffer_, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = ctx_.findMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    vkAllocateMemory(device, &allocInfo, nullptr, &vertexBufferMemory_);
    vkBindBufferMemory(device, vertexBuffer_, vertexBufferMemory_, 0);

    void* data;
    vkMapMemory(device, vertexBufferMemory_, 0, bufferSize, 0, &data);
    memcpy(data, triangleVertices.data(), bufferSize);
    vkUnmapMemory(device, vertexBufferMemory_);
}

double GraphicsBenchmark::measureTriangleThroughput() {
    VkDevice device = ctx_.getDevice();
    VkQueue graphicsQueue = ctx_.getGraphicsQueue();
    VkCommandPool cmdPool = ctx_.getGraphicsCommandPool();
    int warmupFrames = static_cast<int>(config_.graphicsWarmupFrames);
    int benchFrames = static_cast<int>(config_.graphicsBenchFrames);
    int drawCalls = static_cast<int>(config_.graphicsDrawCalls);
    bool showProgress = config_.showProgress && Logger::instance().isNormal();

    LOG_DETAIL("Graphics config: warmup=" + std::to_string(warmupFrames) +
               " frames=" + std::to_string(benchFrames) +
               " drawcalls=" + std::to_string(drawCalls));

    // Semaphores and fences
    VkSemaphore imageAvailable, renderFinished;
    VkFence inFlight;

    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    vkCreateSemaphore(device, &semInfo, nullptr, &imageAvailable);
    vkCreateSemaphore(device, &semInfo, nullptr, &renderFinished);

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    vkCreateFence(device, &fenceInfo, nullptr, &inFlight);

    auto renderFrame = [&]() {
        vkWaitForFences(device, 1, &inFlight, VK_TRUE, UINT64_MAX);
        vkResetFences(device, 1, &inFlight);

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(device, swapchain_.swapchain, UINT64_MAX,
                                                 imageAvailable, VK_NULL_HANDLE, &imageIndex);
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) return;

        VkCommandBuffer cmd;
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = cmdPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        vkAllocateCommandBuffers(device, &allocInfo, &cmd);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &beginInfo);

        VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
        VkRenderPassBeginInfo rpBegin{};
        rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpBegin.renderPass = renderPass_;
        rpBegin.framebuffer = framebuffers_[imageIndex];
        rpBegin.renderArea.offset = {0, 0};
        rpBegin.renderArea.extent = swapchain_.extent;
        rpBegin.clearValueCount = 1;
        rpBegin.pClearValues = &clearColor;

        vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipeline_);

        VkBuffer vertexBuffers[] = {vertexBuffer_};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);

        // Draw many triangles to stress test
        for (int i = 0; i < drawCalls; i++) {
            vkCmdDraw(cmd, 3, 1, 0, 0);
        }

        vkCmdEndRenderPass(cmd);
        vkEndCommandBuffer(cmd);

        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &imageAvailable;
        submitInfo.pWaitDstStageMask = &waitStage;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &renderFinished;

        vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlight);

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &renderFinished;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchain_.swapchain;
        presentInfo.pImageIndices = &imageIndex;

        vkQueuePresentKHR(graphicsQueue, &presentInfo);
    };

    // Warmup
    for (int i = 0; i < warmupFrames; i++) {
        renderFrame();
        glfwPollEvents();
    }
    vkDeviceWaitIdle(device);

    // Timed benchmark
    ProgressBar progress("Graphics", benchFrames, showProgress);
    auto start = Clock::now();
    for (int i = 0; i < benchFrames; i++) {
        renderFrame();
        glfwPollEvents();
        if ((i + 1) % 10 == 0) progress.update(i + 1);
    }
    vkDeviceWaitIdle(device);
    auto end = Clock::now();

    double seconds = elapsedSec(start, end);
    double fps = benchFrames / seconds;

    progress.finish(std::to_string(static_cast<int>(fps)) + " FPS");

    LOG_TIME("Graphics benchmark", elapsedMs(start, end));
    LOG_BENCH("Triangle throughput", fps, "FPS");

    vkDestroySemaphore(device, imageAvailable, nullptr);
    vkDestroySemaphore(device, renderFinished, nullptr);
    vkDestroyFence(device, inFlight, nullptr);

    return fps;
}

std::vector<BenchmarkResult> GraphicsBenchmark::run() {
    if (!ctx_.getQueueFamilyIndices().hasGraphics()) {
        return {{"Triangle Throughput", "FPS", 0.0, "No graphics queue available"}};
    }

    try {
        initWindow();
        createSurface();

        // Check present support
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(ctx_.getPhysicalDevice(),
            ctx_.getQueueFamilyIndices().graphicsFamily.value(), surface_, &presentSupport);

        if (!presentSupport) {
            cleanup();
            return {{"Triangle Throughput", "FPS", 0.0, "Present not supported on graphics queue"}};
        }

        createSwapchain();
        createRenderPass();
        createFramebuffers();
        createTrianglePipeline();

        double fps = measureTriangleThroughput();
        double drawCalls = static_cast<double>(config_.graphicsDrawCalls);
        double trianglesPerSec = fps * drawCalls;
        double mTriPerSec = trianglesPerSec / 1e6;

        std::vector<BenchmarkResult> results;
        results.push_back({"Triangle Throughput", "FPS", fps,
            std::to_string(static_cast<int>(drawCalls)) + " draw calls/frame, " +
            BenchmarkConfig::modeToString(config_.mode) + " mode"});
        results.push_back({"Triangle Rate", "M tri/s", mTriPerSec,
            "Million triangles per second"});

        cleanup();
        return results;
    } catch (const std::exception& e) {
        cleanup();
        return {{"Triangle Throughput", "FPS", 0.0, std::string("Error: ") + e.what()}};
    }
}
