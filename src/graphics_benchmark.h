#pragma once

#include "benchmark_base.h"
#include "benchmark_config.h"
#include "vulkan_context.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class GraphicsBenchmark : public BenchmarkBase {
public:
    GraphicsBenchmark(VulkanContext& ctx, const BenchmarkConfig& config);
    ~GraphicsBenchmark();

    std::string getName() const override { return "Graphics"; }
    std::vector<BenchmarkResult> run() override;

private:
    struct SwapchainInfo {
        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
        VkFormat format;
        VkExtent2D extent;
        std::vector<VkImage> images;
        std::vector<VkImageView> imageViews;
    };

    void initWindow();
    void createSurface();
    void createSwapchain();
    void createRenderPass();
    void createFramebuffers();
    void createTrianglePipeline();
    void cleanup();

    double measureTriangleThroughput();

    VulkanContext& ctx_;
    BenchmarkConfig config_;
    GLFWwindow* window_ = nullptr;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    SwapchainInfo swapchain_{};
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers_;

    VkPipelineLayout trianglePipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline trianglePipeline_ = VK_NULL_HANDLE;
    VkBuffer vertexBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory_ = VK_NULL_HANDLE;

    static constexpr uint32_t WIDTH = 1280;
    static constexpr uint32_t HEIGHT = 720;
};
