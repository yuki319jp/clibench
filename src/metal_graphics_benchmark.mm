#import "metal_graphics_benchmark.h"
#import "metal_context.h"
#import "logger.h"
#import "progress.h"
#import <Metal/Metal.h>

#import <cstring>

static const char* kGraphicsShaderSource = R"(
#include <metal_stdlib>
using namespace metal;

struct VertexIn {
    float2 position [[attribute(0)]];
    float3 color [[attribute(1)]];
};

struct VertexOut {
    float4 position [[position]];
    float3 color;
};

vertex VertexOut triangle_vertex(VertexIn in [[stage_in]]) {
    VertexOut out;
    out.position = float4(in.position, 0.0, 1.0);
    out.color = in.color;
    return out;
}

fragment float4 triangle_fragment(VertexOut in [[stage_in]]) {
    return float4(in.color, 1.0);
}
)";

struct Vertex {
    float pos[2];
    float color[3];
};

static const Vertex kTriangleVertices[] = {
    {{ 0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{ 0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},
    {{-0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
};

// Tight spin-wait for GPU completion (no dispatch overhead)
static inline void waitForEvent(id<MTLSharedEvent> event, uint64_t value) {
    while (event.signaledValue < value) { }
}

MetalGraphicsBenchmark::MetalGraphicsBenchmark(MetalContext& ctx, const BenchmarkConfig& config)
    : ctx_(ctx), config_(config) {}

MetalGraphicsBenchmark::~MetalGraphicsBenchmark() = default;

std::vector<BenchmarkResult> MetalGraphicsBenchmark::run() {
    @autoreleasepool {
        id<MTLDevice> device = (__bridge id<MTLDevice>)ctx_.device();

        // Compile shaders
        NSError* error = nil;
        NSString* src = [NSString stringWithUTF8String:kGraphicsShaderSource];
        id<MTLLibrary> library = [device newLibraryWithSource:src options:nil error:&error];
        if (!library) {
            std::string errMsg = error ? [[error localizedDescription] UTF8String] : "Unknown";
            return {{"Triangle Throughput", "FPS", 0.0, "Metal shader error: " + errMsg}};
        }

        id<MTLFunction> vertexFunc = [library newFunctionWithName:@"triangle_vertex"];
        id<MTLFunction> fragmentFunc = [library newFunctionWithName:@"triangle_fragment"];

        // Vertex descriptor
        MTLVertexDescriptor* vertexDesc = [[MTLVertexDescriptor alloc] init];
        vertexDesc.attributes[0].format = MTLVertexFormatFloat2;
        vertexDesc.attributes[0].offset = 0;
        vertexDesc.attributes[0].bufferIndex = 0;
        vertexDesc.attributes[1].format = MTLVertexFormatFloat3;
        vertexDesc.attributes[1].offset = sizeof(float) * 2;
        vertexDesc.attributes[1].bufferIndex = 0;
        vertexDesc.layouts[0].stride = sizeof(Vertex);
        vertexDesc.layouts[0].stepRate = 1;
        vertexDesc.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;

        // Render pipeline (shared between Metal 3 and 4)
        MTLRenderPipelineDescriptor* pipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];
        pipelineDesc.vertexFunction = vertexFunc;
        pipelineDesc.fragmentFunction = fragmentFunc;
        pipelineDesc.vertexDescriptor = vertexDesc;
        pipelineDesc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

        id<MTLRenderPipelineState> pipeline =
            [device newRenderPipelineStateWithDescriptor:pipelineDesc error:&error];
        if (!pipeline) {
            return {{"Triangle Throughput", "FPS", 0.0, "Metal pipeline creation failed"}};
        }

        // Offscreen render target
        MTLTextureDescriptor* texDesc =
            [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                              width:WIDTH
                                                             height:HEIGHT
                                                          mipmapped:NO];
        texDesc.usage = MTLTextureUsageRenderTarget;
        texDesc.storageMode = MTLStorageModePrivate;
        id<MTLTexture> renderTarget = [device newTextureWithDescriptor:texDesc];

        // Vertex buffer
        id<MTLBuffer> vertexBuffer = [device newBufferWithBytes:kTriangleVertices
                                                         length:sizeof(kTriangleVertices)
                                                        options:MTLResourceStorageModeShared];

        int warmupFrames = static_cast<int>(config_.graphicsWarmupFrames);
        int benchFrames = static_cast<int>(config_.graphicsBenchFrames);
        int drawCalls = static_cast<int>(config_.graphicsDrawCalls);
        bool showProgress = config_.showProgress && Logger::instance().isNormal();

        LOG_DETAIL("Graphics config (Metal " + std::to_string(ctx_.metalVersion()) +
                   "): warmup=" + std::to_string(warmupFrames) +
                   " frames=" + std::to_string(benchFrames) +
                   " drawcalls=" + std::to_string(drawCalls));

        // Choose Metal 4 or Metal 3 render path
        if (ctx_.hasMetal4()) {
            if (@available(macOS 26.0, *)) {
                id<MTL4CommandQueue> queue4 = (__bridge id<MTL4CommandQueue>)ctx_.mtl4CommandQueue();
                id<MTL4CommandAllocator> allocator = (__bridge id<MTL4CommandAllocator>)ctx_.mtl4CommandAllocator();
                id<MTLSharedEvent> event = [device newSharedEvent];
                uint64_t signalVal = 0;

                // Argument table for vertex buffer binding
                MTL4ArgumentTableDescriptor* atDesc = [[MTL4ArgumentTableDescriptor alloc] init];
                atDesc.maxBufferBindCount = 1;
                atDesc.supportAttributeStrides = YES;
                id<MTL4ArgumentTable> argTable = [device newArgumentTableWithDescriptor:atDesc error:nil];
                [argTable setAddress:vertexBuffer.gpuAddress attributeStride:sizeof(Vertex) atIndex:0];

                auto renderFrame4 = [&]() {
                    id<MTL4CommandBuffer> cmdBuf = [device newCommandBuffer];
                    [cmdBuf beginCommandBufferWithAllocator:allocator];

                    MTL4RenderPassDescriptor* rpDesc = [[MTL4RenderPassDescriptor alloc] init];
                    rpDesc.colorAttachments[0].texture = renderTarget;
                    rpDesc.colorAttachments[0].loadAction = MTLLoadActionClear;
                    rpDesc.colorAttachments[0].storeAction = MTLStoreActionStore;
                    rpDesc.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);

                    id<MTL4RenderCommandEncoder> enc =
                        [cmdBuf renderCommandEncoderWithDescriptor:rpDesc];
                    [enc setRenderPipelineState:pipeline];
                    [enc setArgumentTable:argTable atStages:MTLRenderStageVertex];

                    for (int i = 0; i < drawCalls; i++) {
                        [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
                    }

                    [enc endEncoding];
                    [cmdBuf endCommandBuffer];

                    id<MTL4CommandBuffer> cmds[] = { cmdBuf };
                    [queue4 commit:cmds count:1];
                    [queue4 signalEvent:event value:++signalVal];
                    waitForEvent(event, signalVal);
                    [allocator reset];
                };

                // Warmup
                for (int i = 0; i < warmupFrames; i++) {
                    renderFrame4();
                }

                // Timed benchmark
                ProgressBar progress("Graphics", benchFrames, showProgress);
                auto start = std::chrono::high_resolution_clock::now();
                for (int i = 0; i < benchFrames; i++) {
                    renderFrame4();
                    if ((i + 1) % 10 == 0) progress.update(i + 1);
                }
                auto end = std::chrono::high_resolution_clock::now();

                double seconds = std::chrono::duration<double>(end - start).count();
                double fps = benchFrames / seconds;
                double mTriPerSec = fps * drawCalls / 1e6;

                progress.finish(std::to_string(static_cast<int>(fps)) + " FPS");

                double ms = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;
                LOG_TIME("Graphics benchmark (Metal 4)", ms);
                LOG_BENCH("Triangle throughput", fps, "FPS");

                return {
                    {"Triangle Throughput", "FPS", fps,
                     std::to_string(drawCalls) + " draw calls/frame, Metal 4, " +
                     BenchmarkConfig::modeToString(config_.mode) + " mode"},
                    {"Triangle Rate", "M tri/s", mTriPerSec,
                     "Million triangles per second"}
                };
            }
        }

        // Metal 3 fallback
        id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)ctx_.commandQueue();

        auto renderFrame = [&]() {
            id<MTLCommandBuffer> cmdBuf = [queue commandBuffer];

            MTLRenderPassDescriptor* rpDesc = [MTLRenderPassDescriptor renderPassDescriptor];
            rpDesc.colorAttachments[0].texture = renderTarget;
            rpDesc.colorAttachments[0].loadAction = MTLLoadActionClear;
            rpDesc.colorAttachments[0].storeAction = MTLStoreActionStore;
            rpDesc.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);

            id<MTLRenderCommandEncoder> enc =
                [cmdBuf renderCommandEncoderWithDescriptor:rpDesc];
            [enc setRenderPipelineState:pipeline];
            [enc setVertexBuffer:vertexBuffer offset:0 atIndex:0];

            for (int i = 0; i < drawCalls; i++) {
                [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
            }

            [enc endEncoding];
            [cmdBuf commit];
            [cmdBuf waitUntilCompleted];
        };

        // Warmup
        for (int i = 0; i < warmupFrames; i++) {
            renderFrame();
        }

        // Timed benchmark
        ProgressBar progress("Graphics", benchFrames, showProgress);
        auto start = Clock::now();
        for (int i = 0; i < benchFrames; i++) {
            renderFrame();
            if ((i + 1) % 10 == 0) progress.update(i + 1);
        }
        auto end = Clock::now();

        double seconds = elapsedSec(start, end);
        double fps = benchFrames / seconds;
        double mTriPerSec = fps * drawCalls / 1e6;

        progress.finish(std::to_string(static_cast<int>(fps)) + " FPS");

        LOG_TIME("Graphics benchmark (Metal)", elapsedMs(start, end));
        LOG_BENCH("Triangle throughput", fps, "FPS");

        return {
            {"Triangle Throughput", "FPS", fps,
             std::to_string(drawCalls) + " draw calls/frame, Metal, " +
             BenchmarkConfig::modeToString(config_.mode) + " mode"},
            {"Triangle Rate", "M tri/s", mTriPerSec,
             "Million triangles per second"}
        };
    }
}
