#pragma once

#include <string>
#include <cstdint>

struct GPUInfo {
    std::string name;
    std::string driverVersion;
    std::string apiVersion;
    std::string deviceType;
    std::string backend;  // "Vulkan" or "Metal"
    uint32_t maxComputeWorkGroupSize[3];
    uint32_t maxComputeWorkGroupInvocations;
    uint64_t deviceLocalMemoryMB;
    uint64_t hostVisibleMemoryMB;
};
