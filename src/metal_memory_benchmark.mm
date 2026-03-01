#import "metal_memory_benchmark.h"
#import "metal_context.h"
#import "logger.h"
#import "progress.h"
#import <Metal/Metal.h>
#import <cstring>
#import <algorithm>

MetalMemoryBenchmark::MetalMemoryBenchmark(MetalContext& ctx, const BenchmarkConfig& config)
    : ctx_(ctx), config_(config) {}

MetalMemoryBenchmark::~MetalMemoryBenchmark() = default;

// Memory benchmarks use Metal 3 command queue even on Metal 4 hardware
// because waitUntilCompleted provides more accurate timing for short blit
// operations than event-based synchronization.

static double measureHostToDevice(id<MTLDevice> device, id<MTLCommandQueue> queue,
                                  NSUInteger size, int numIterations) {
    id<MTLBuffer> srcBuffer = [device newBufferWithLength:size options:MTLResourceStorageModeShared];
    memset(srcBuffer.contents, 0xAB, size);
    id<MTLBuffer> dstBuffer = [device newBufferWithLength:size options:MTLResourceStorageModePrivate];

    // Warmup
    {
        id<MTLCommandBuffer> cmdBuf = [queue commandBuffer];
        id<MTLBlitCommandEncoder> blit = [cmdBuf blitCommandEncoder];
        [blit copyFromBuffer:srcBuffer sourceOffset:0 toBuffer:dstBuffer destinationOffset:0 size:size];
        [blit endEncoding];
        [cmdBuf commit];
        [cmdBuf waitUntilCompleted];
    }

    double bestBW = 0.0;
    for (int i = 0; i < numIterations; i++) {
        id<MTLCommandBuffer> cmdBuf = [queue commandBuffer];
        id<MTLBlitCommandEncoder> blit = [cmdBuf blitCommandEncoder];
        [blit copyFromBuffer:srcBuffer sourceOffset:0 toBuffer:dstBuffer destinationOffset:0 size:size];
        [blit endEncoding];

        auto start = std::chrono::high_resolution_clock::now();
        [cmdBuf commit];
        [cmdBuf waitUntilCompleted];
        auto end = std::chrono::high_resolution_clock::now();

        double seconds = std::chrono::duration<double>(end - start).count();
        double bw = (static_cast<double>(size) / (1024.0 * 1024.0 * 1024.0)) / seconds;
        bestBW = std::max(bestBW, bw);
    }
    return bestBW;
}

static double measureDeviceToHost(id<MTLDevice> device, id<MTLCommandQueue> queue,
                                  NSUInteger size, int numIterations) {
    id<MTLBuffer> srcBuffer = [device newBufferWithLength:size options:MTLResourceStorageModePrivate];
    id<MTLBuffer> dstBuffer = [device newBufferWithLength:size options:MTLResourceStorageModeShared];

    // Warmup
    {
        id<MTLCommandBuffer> cmdBuf = [queue commandBuffer];
        id<MTLBlitCommandEncoder> blit = [cmdBuf blitCommandEncoder];
        [blit copyFromBuffer:srcBuffer sourceOffset:0 toBuffer:dstBuffer destinationOffset:0 size:size];
        [blit endEncoding];
        [cmdBuf commit];
        [cmdBuf waitUntilCompleted];
    }

    double bestBW = 0.0;
    for (int i = 0; i < numIterations; i++) {
        id<MTLCommandBuffer> cmdBuf = [queue commandBuffer];
        id<MTLBlitCommandEncoder> blit = [cmdBuf blitCommandEncoder];
        [blit copyFromBuffer:srcBuffer sourceOffset:0 toBuffer:dstBuffer destinationOffset:0 size:size];
        [blit endEncoding];

        auto start = std::chrono::high_resolution_clock::now();
        [cmdBuf commit];
        [cmdBuf waitUntilCompleted];
        auto end = std::chrono::high_resolution_clock::now();

        double seconds = std::chrono::duration<double>(end - start).count();
        double bw = (static_cast<double>(size) / (1024.0 * 1024.0 * 1024.0)) / seconds;
        bestBW = std::max(bestBW, bw);
    }
    return bestBW;
}

static double measureDeviceToDevice(id<MTLDevice> device, id<MTLCommandQueue> queue,
                                    NSUInteger size, int numIterations) {
    id<MTLBuffer> srcBuffer = [device newBufferWithLength:size options:MTLResourceStorageModePrivate];
    id<MTLBuffer> dstBuffer = [device newBufferWithLength:size options:MTLResourceStorageModePrivate];

    // Initialize source
    {
        id<MTLCommandBuffer> cmdBuf = [queue commandBuffer];
        id<MTLBlitCommandEncoder> blit = [cmdBuf blitCommandEncoder];
        [blit fillBuffer:srcBuffer range:NSMakeRange(0, size) value:0xAB];
        [blit endEncoding];
        [cmdBuf commit];
        [cmdBuf waitUntilCompleted];
    }

    // Warmup
    {
        id<MTLCommandBuffer> cmdBuf = [queue commandBuffer];
        id<MTLBlitCommandEncoder> blit = [cmdBuf blitCommandEncoder];
        [blit copyFromBuffer:srcBuffer sourceOffset:0 toBuffer:dstBuffer destinationOffset:0 size:size];
        [blit endEncoding];
        [cmdBuf commit];
        [cmdBuf waitUntilCompleted];
    }

    double bestBW = 0.0;
    for (int i = 0; i < numIterations; i++) {
        id<MTLCommandBuffer> cmdBuf = [queue commandBuffer];
        id<MTLBlitCommandEncoder> blit = [cmdBuf blitCommandEncoder];
        [blit copyFromBuffer:srcBuffer sourceOffset:0 toBuffer:dstBuffer destinationOffset:0 size:size];
        [blit endEncoding];

        auto start = std::chrono::high_resolution_clock::now();
        [cmdBuf commit];
        [cmdBuf waitUntilCompleted];
        auto end = std::chrono::high_resolution_clock::now();

        double seconds = std::chrono::duration<double>(end - start).count();
        double bw = (static_cast<double>(size) / (1024.0 * 1024.0 * 1024.0)) / seconds;
        bestBW = std::max(bestBW, bw);
    }
    return bestBW;
}

std::vector<BenchmarkResult> MetalMemoryBenchmark::run() {
    @autoreleasepool {
        id<MTLDevice> device = (__bridge id<MTLDevice>)ctx_.device();
        id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)ctx_.commandQueue();

        NSUInteger bufferSize = static_cast<NSUInteger>(config_.memoryBufferSizeMB) * 1024 * 1024;
        int iterations = static_cast<int>(config_.memoryIterations);
        bool showProgress = config_.showProgress && Logger::instance().isNormal();

        LOG_DETAIL("Memory config (Metal " + std::to_string(ctx_.metalVersion()) +
                   "): buffer=" + std::to_string(config_.memoryBufferSizeMB) +
                   "MB iterations=" + std::to_string(iterations));

        ProgressBar progress("Memory", 3, showProgress);

        double h2d = measureHostToDevice(device, queue, bufferSize, iterations);
        progress.increment();

        double d2h = measureDeviceToHost(device, queue, bufferSize, iterations);
        progress.increment();

        double d2d = measureDeviceToDevice(device, queue, bufferSize, iterations);
        progress.increment();

        std::string apiStr = "Metal " + std::to_string(ctx_.metalVersion());
        std::string desc = std::to_string(config_.memoryBufferSizeMB) + " MB, " + apiStr + ", " +
                           BenchmarkConfig::modeToString(config_.mode) + " mode";

        LOG_BENCH("Host->Device", h2d, "GB/s");
        LOG_BENCH("Device->Host", d2h, "GB/s");
        LOG_BENCH("VRAM Bandwidth", d2d, "GB/s");

        progress.finish(std::to_string(static_cast<int>(h2d)) + "/" +
                        std::to_string(static_cast<int>(d2h)) + "/" +
                        std::to_string(static_cast<int>(d2d)) + " GB/s");

        return {
            {"Host -> Device", "GB/s", h2d, "Host to device transfer (" + desc + ")"},
            {"Device -> Host", "GB/s", d2h, "Device to host transfer (" + desc + ")"},
            {"VRAM Bandwidth", "GB/s", d2d, "Device-local memory bandwidth (" + desc + ")"}
        };
    }
}
