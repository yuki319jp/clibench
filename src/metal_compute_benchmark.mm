#import "metal_compute_benchmark.h"
#import "metal_context.h"
#import "logger.h"
#import "progress.h"
#import <Metal/Metal.h>


static const char* kComputeShaderSource = R"(
#include <metal_stdlib>
using namespace metal;

kernel void compute_fma(device float4* data [[buffer(0)]],
                       constant uint& iterations [[buffer(1)]],
                       uint gid [[thread_position_in_grid]]) {
    float4 acc = data[gid];
    float4 b = float4(1.0001f, 0.9999f, 1.0001f, 0.9999f);
    float4 c = float4(0.5f, 0.5f, 0.5f, 0.5f);

    for (uint i = 0; i < iterations; i++) {
        acc = fma(acc, b, c);
        acc = fma(acc, b, c);
        acc = fma(acc, b, c);
        acc = fma(acc, b, c);
    }

    data[gid] = acc;
}
)";

// Tight spin-wait for GPU completion (no dispatch overhead)
static inline void waitForEvent(id<MTLSharedEvent> event, uint64_t value) {
    while (event.signaledValue < value) { }
}

MetalComputeBenchmark::MetalComputeBenchmark(MetalContext& ctx, const BenchmarkConfig& config)
    : ctx_(ctx), config_(config) {}

MetalComputeBenchmark::~MetalComputeBenchmark() = default;

// Metal 4 compute path (macOS 26+)
static std::vector<BenchmarkResult> runMetal4Compute(
    MetalContext& ctx, const BenchmarkConfig& config,
    id<MTLDevice> device, id<MTLComputePipelineState> pipeline,
    uint32_t numElements, uint32_t iterations, uint32_t runs,
    NSUInteger threadGroupSize, NSUInteger numGroups, const std::string& name)
{
    if (@available(macOS 26.0, *)) {
        id<MTL4CommandQueue> queue4 = (__bridge id<MTL4CommandQueue>)ctx.mtl4CommandQueue();
        id<MTL4CommandAllocator> allocator = (__bridge id<MTL4CommandAllocator>)ctx.mtl4CommandAllocator();
        id<MTLSharedEvent> event = [device newSharedEvent];

        NSUInteger bufferSize = sizeof(float) * 4 * numElements;
        id<MTLBuffer> dataBuffer = [device newBufferWithLength:bufferSize
                                                       options:MTLResourceStorageModePrivate];
        id<MTLBuffer> itersBuf = [device newBufferWithBytes:&iterations
                                                     length:sizeof(uint32_t)
                                                    options:MTLResourceStorageModeShared];

        // Create argument table for buffer bindings
        MTL4ArgumentTableDescriptor* atDesc = [[MTL4ArgumentTableDescriptor alloc] init];
        atDesc.maxBufferBindCount = 2;
        NSError* error = nil;
        id<MTL4ArgumentTable> argTable = [device newArgumentTableWithDescriptor:atDesc error:&error];
        if (!argTable) {
            return {{name, "GFLOPS", 0.0, "Metal 4 argument table creation failed"}};
        }
        [argTable setAddress:dataBuffer.gpuAddress atIndex:0];
        [argTable setAddress:itersBuf.gpuAddress atIndex:1];

        // Warmup
        {
            uint32_t warmupIters = 16;
            id<MTLBuffer> warmupBuf = [device newBufferWithBytes:&warmupIters
                                                          length:sizeof(uint32_t)
                                                         options:MTLResourceStorageModeShared];
            MTL4ArgumentTableDescriptor* warmupAtDesc = [[MTL4ArgumentTableDescriptor alloc] init];
            warmupAtDesc.maxBufferBindCount = 2;
            id<MTL4ArgumentTable> warmupArgTable = [device newArgumentTableWithDescriptor:warmupAtDesc error:nil];
            [warmupArgTable setAddress:dataBuffer.gpuAddress atIndex:0];
            [warmupArgTable setAddress:warmupBuf.gpuAddress atIndex:1];

            id<MTL4CommandBuffer> cmdBuf = [device newCommandBuffer];
            [cmdBuf beginCommandBufferWithAllocator:allocator];
            id<MTL4ComputeCommandEncoder> enc = [cmdBuf computeCommandEncoder];
            [enc setComputePipelineState:pipeline];
            [enc setArgumentTable:warmupArgTable];
            [enc dispatchThreadgroups:MTLSizeMake(numGroups, 1, 1)
                threadsPerThreadgroup:MTLSizeMake(threadGroupSize, 1, 1)];
            [enc endEncoding];
            [cmdBuf endCommandBuffer];

            id<MTL4CommandBuffer> cmds[] = { cmdBuf };
            [queue4 commit:cmds count:1];
            [queue4 signalEvent:event value:1];
            waitForEvent(event, 1);
            [allocator reset];
        }

        double bestGflops = 0.0;
        bool showProgress = config.showProgress && Logger::instance().isNormal();
        ProgressBar progress("Compute", static_cast<int>(runs), showProgress);

        for (uint32_t r = 0; r < runs; r++) {
            id<MTL4CommandBuffer> cmdBuf = [device newCommandBuffer];
            [cmdBuf beginCommandBufferWithAllocator:allocator];
            id<MTL4ComputeCommandEncoder> enc = [cmdBuf computeCommandEncoder];
            [enc setComputePipelineState:pipeline];
            [enc setArgumentTable:argTable];
            [enc dispatchThreadgroups:MTLSizeMake(numGroups, 1, 1)
                threadsPerThreadgroup:MTLSizeMake(threadGroupSize, 1, 1)];
            [enc endEncoding];
            [cmdBuf endCommandBuffer];

            uint64_t signalVal = r + 2; // +2 because warmup used 1
            auto start = std::chrono::high_resolution_clock::now();
            id<MTL4CommandBuffer> cmds[] = { cmdBuf };
            [queue4 commit:cmds count:1];
            [queue4 signalEvent:event value:signalVal];
            waitForEvent(event, signalVal);
            auto end = std::chrono::high_resolution_clock::now();

            [allocator reset];

            double seconds = std::chrono::duration<double>(end - start).count();
            double totalFlops = static_cast<double>(numElements) * iterations * 4.0 * 4.0 * 2.0;
            double gflops = (totalFlops / seconds) / 1e9;

            double ms = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;
            LOG_TIME("Compute run " + std::to_string(r + 1), ms);
            LOG_BENCH("Run " + std::to_string(r + 1), gflops, "GFLOPS");

            if (gflops > bestGflops) bestGflops = gflops;
            progress.increment();
        }

        progress.finish(std::to_string(static_cast<int>(bestGflops)) + " GFLOPS");

        return {
            {name, "GFLOPS", bestGflops, "FMA compute throughput (Metal 4, " +
             BenchmarkConfig::modeToString(config.mode) + " mode)"}
        };
    }
    return {{name, "GFLOPS", 0.0, "Metal 4 not available"}};
}

std::vector<BenchmarkResult> MetalComputeBenchmark::run() {
    @autoreleasepool {
        id<MTLDevice> device = (__bridge id<MTLDevice>)ctx_.device();

        NSError* error = nil;
        NSString* src = [NSString stringWithUTF8String:kComputeShaderSource];
        id<MTLLibrary> library = [device newLibraryWithSource:src options:nil error:&error];
        if (!library) {
            std::string errMsg = error ? [[error localizedDescription] UTF8String] : "Unknown";
            return {{getName(), "GFLOPS", 0.0, "Metal shader compilation failed: " + errMsg}};
        }

        id<MTLFunction> function = [library newFunctionWithName:@"compute_fma"];
        id<MTLComputePipelineState> pipeline =
            [device newComputePipelineStateWithFunction:function error:&error];
        if (!pipeline) {
            return {{getName(), "GFLOPS", 0.0, "Metal pipeline creation failed"}};
        }

        uint32_t numElements = config_.computeElements;
        uint32_t iterations = config_.computeIterations;
        uint32_t runs = config_.computeRuns;

        LOG_DETAIL("Compute config (Metal " + std::to_string(ctx_.metalVersion()) +
                   "): elements=" + std::to_string(numElements) +
                   " iterations=" + std::to_string(iterations) + " runs=" + std::to_string(runs));

        NSUInteger threadGroupSize = MIN(256, (NSUInteger)pipeline.maxTotalThreadsPerThreadgroup);
        NSUInteger numGroups = numElements / threadGroupSize;

        // Use Metal 4 path if available
        if (ctx_.hasMetal4()) {
            return runMetal4Compute(ctx_, config_, device, pipeline,
                                   numElements, iterations, runs,
                                   threadGroupSize, numGroups, getName());
        }

        // Metal 3 fallback
        id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)ctx_.commandQueue();

        NSUInteger bufferSize = sizeof(float) * 4 * numElements;
        id<MTLBuffer> dataBuffer = [device newBufferWithLength:bufferSize
                                                        options:MTLResourceStorageModePrivate];

        // Warmup
        {
            id<MTLCommandBuffer> cmdBuf = [queue commandBuffer];
            id<MTLComputeCommandEncoder> enc = [cmdBuf computeCommandEncoder];
            [enc setComputePipelineState:pipeline];
            [enc setBuffer:dataBuffer offset:0 atIndex:0];
            uint32_t warmupIters = 16;
            [enc setBytes:&warmupIters length:sizeof(uint32_t) atIndex:1];
            [enc dispatchThreadgroups:MTLSizeMake(numGroups, 1, 1)
                threadsPerThreadgroup:MTLSizeMake(threadGroupSize, 1, 1)];
            [enc endEncoding];
            [cmdBuf commit];
            [cmdBuf waitUntilCompleted];
        }

        double bestGflops = 0.0;
        bool showProgress = config_.showProgress && Logger::instance().isNormal();
        ProgressBar progress("Compute", static_cast<int>(runs), showProgress);

        for (uint32_t r = 0; r < runs; r++) {
            id<MTLCommandBuffer> cmdBuf = [queue commandBuffer];
            id<MTLComputeCommandEncoder> enc = [cmdBuf computeCommandEncoder];
            [enc setComputePipelineState:pipeline];
            [enc setBuffer:dataBuffer offset:0 atIndex:0];
            [enc setBytes:&iterations length:sizeof(uint32_t) atIndex:1];
            [enc dispatchThreadgroups:MTLSizeMake(numGroups, 1, 1)
                threadsPerThreadgroup:MTLSizeMake(threadGroupSize, 1, 1)];
            [enc endEncoding];

            auto start = Clock::now();
            [cmdBuf commit];
            [cmdBuf waitUntilCompleted];
            auto end = Clock::now();

            double seconds = elapsedSec(start, end);
            double totalFlops = static_cast<double>(numElements) * iterations * 4.0 * 4.0 * 2.0;
            double gflops = (totalFlops / seconds) / 1e9;

            LOG_TIME("Compute run " + std::to_string(r + 1), elapsedMs(start, end));
            LOG_BENCH("Run " + std::to_string(r + 1), gflops, "GFLOPS");

            if (gflops > bestGflops) bestGflops = gflops;
            progress.increment();
        }

        progress.finish(std::to_string(static_cast<int>(bestGflops)) + " GFLOPS");

        return {
            {getName(), "GFLOPS", bestGflops, "FMA compute throughput (Metal, " +
             BenchmarkConfig::modeToString(config_.mode) + " mode)"}
        };
    }
}
