#version 450

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D texSampler;

layout(push_constant) uniform PushConstants {
    uint frameIndex;
};

void main() {
    vec2 uv = gl_FragCoord.xy / vec2(1920.0, 1080.0);
    // Multiple texture samples to stress the texture units
    vec4 color = vec4(0.0);
    color += texture(texSampler, uv);
    color += texture(texSampler, uv + vec2(0.001, 0.001));
    color += texture(texSampler, uv + vec2(-0.001, 0.001));
    color += texture(texSampler, uv + vec2(0.001, -0.001));
    outColor = color * 0.25;
}
