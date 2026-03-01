#import "metal_context.h"
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>

struct MetalContext::Impl {
    id<MTLDevice> device = nil;
    id<MTLCommandQueue> commandQueue = nil;
    std::vector<id<MTLDevice>> allDevices;

    // Metal 4 (macOS 26+)
    bool metal4Supported = false;
    int metalVer = 2;
    id mtl4Queue = nil;       // id<MTL4CommandQueue>
    id mtl4Allocator = nil;   // id<MTL4CommandAllocator>
};

MetalContext::MetalContext() : impl_(std::make_unique<Impl>()) {}

MetalContext::~MetalContext() {
    cleanup();
}

void MetalContext::init(int gpuIndex) {
    @autoreleasepool {
        NSArray<id<MTLDevice>>* devices = MTLCopyAllDevices();
        for (id<MTLDevice> dev in devices) {
            impl_->allDevices.push_back(dev);
        }

        if (impl_->allDevices.empty()) {
            throw std::runtime_error("No Metal-compatible GPU found");
        }

        if (gpuIndex >= 0 && gpuIndex < static_cast<int>(impl_->allDevices.size())) {
            impl_->device = impl_->allDevices[gpuIndex];
        } else {
            impl_->device = MTLCreateSystemDefaultDevice();
        }

        if (!impl_->device) {
            throw std::runtime_error("Failed to create Metal device");
        }

        impl_->commandQueue = [impl_->device newCommandQueue];
        if (!impl_->commandQueue) {
            throw std::runtime_error("Failed to create Metal command queue");
        }

        // Detect Metal version and create Metal 4 objects if supported
        if (@available(macOS 26.0, *)) {
            if ([impl_->device supportsFamily:MTLGPUFamilyMetal4]) {
                impl_->metal4Supported = true;
                impl_->metalVer = 4;

                id<MTL4CommandQueue> q4 = [impl_->device newMTL4CommandQueue];
                id<MTL4CommandAllocator> alloc = [impl_->device newCommandAllocator];
                if (q4 && alloc) {
                    impl_->mtl4Queue = q4;
                    impl_->mtl4Allocator = alloc;
                } else {
                    impl_->metal4Supported = false;
                    impl_->metalVer = 3;
                }
            } else if ([impl_->device supportsFamily:MTLGPUFamilyMetal3]) {
                impl_->metalVer = 3;
            }
        } else {
            if ([impl_->device supportsFamily:MTLGPUFamilyMetal3]) {
                impl_->metalVer = 3;
            }
        }
    }
}

void MetalContext::cleanup() {
    if (impl_) {
        impl_->mtl4Allocator = nil;
        impl_->mtl4Queue = nil;
        impl_->commandQueue = nil;
        impl_->device = nil;
        impl_->allDevices.clear();
    }
}

GPUInfo MetalContext::getGPUInfo() const {
    GPUInfo info{};
    @autoreleasepool {
        info.name = [impl_->device.name UTF8String];
        info.backend = "Metal";
        info.apiVersion = std::to_string(impl_->metalVer);
        info.driverVersion = "N/A";

        if (impl_->device.isLowPower) {
            info.deviceType = "Integrated GPU";
        } else {
            info.deviceType = "Discrete GPU";
        }

        info.maxComputeWorkGroupInvocations =
            static_cast<uint32_t>(impl_->device.maxThreadsPerThreadgroup.width);
        info.maxComputeWorkGroupSize[0] =
            static_cast<uint32_t>(impl_->device.maxThreadsPerThreadgroup.width);
        info.maxComputeWorkGroupSize[1] =
            static_cast<uint32_t>(impl_->device.maxThreadsPerThreadgroup.height);
        info.maxComputeWorkGroupSize[2] =
            static_cast<uint32_t>(impl_->device.maxThreadsPerThreadgroup.depth);

        info.deviceLocalMemoryMB =
            static_cast<uint64_t>(impl_->device.recommendedMaxWorkingSetSize / (1024 * 1024));
        info.hostVisibleMemoryMB = info.deviceLocalMemoryMB;
    }
    return info;
}

std::vector<std::string> MetalContext::listGPUs() const {
    std::vector<std::string> names;
    @autoreleasepool {
        for (const auto& dev : impl_->allDevices) {
            names.push_back([dev.name UTF8String]);
        }
    }
    return names;
}

void* MetalContext::device() const {
    return (__bridge void*)impl_->device;
}

void* MetalContext::commandQueue() const {
    return (__bridge void*)impl_->commandQueue;
}

bool MetalContext::hasMetal4() const {
    return impl_->metal4Supported;
}

int MetalContext::metalVersion() const {
    return impl_->metalVer;
}

void* MetalContext::mtl4CommandQueue() const {
    return (__bridge void*)impl_->mtl4Queue;
}

void* MetalContext::mtl4CommandAllocator() const {
    return (__bridge void*)impl_->mtl4Allocator;
}
