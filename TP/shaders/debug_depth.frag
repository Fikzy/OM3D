#version 450

#include "utils.glsl"

layout(location = 0) out vec4 out_color;

layout(location = 0) in vec2 in_uv;

layout(binding = 0) uniform sampler2D in_texture;

void main() {
    float depth = texelFetch(in_texture, ivec2(gl_FragCoord.xy), 0).r;
    out_color = vec4(vec3(depth * 10000.0), 1.0);
}
