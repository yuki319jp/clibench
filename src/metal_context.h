#pragma once

#include "gpu_info.h"
#include <memory>
#include <vector>
#include <string>

class MetalContext {
public:
    MetalContext();
    ~MetalContext();

    void init(int gpuIndex = -1);
    void cleanup();

    GPUInfo getGPUInfo() const;
    std::vector<std::string> listGPUs() const;

    // Returns opaque Metal pointers (id<MTLDevice>, id<MTLCommandQueue>)
    void* device() const;
    void* commandQueue() const;

    // Metal 4 support (macOS 26+)
    bool hasMetal4() const;
    int metalVersion() const;
    void* mtl4CommandQueue() const;   // id<MTL4CommandQueue>
    void* mtl4CommandAllocator() const; // id<MTL4CommandAllocator>

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
