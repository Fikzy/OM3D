#version 450
// gbuffer.frag

#include "utils.glsl"

layout(location = 0) out vec4 out_albedo;
layout(location = 1) out vec4 out_normal;

layout(location = 0) in vec3 in_normal;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec3 in_color;
// layout(location = 3) in vec3 in_position; ??
layout(location = 4) in vec3 in_tangent;
layout(location = 5) in vec3 in_bitangent;

layout(binding = 0) uniform sampler2D in_texture;
layout(binding = 1) uniform sampler2D in_normal_texture;

const vec3 ambient = vec3(0.0);

void main() {
#ifdef NORMAL_MAPPED
    const vec3 normal_map = unpack_normal_map(texture(in_normal_texture, in_uv).xy);
    const vec3 normal = normal_map.x * in_tangent +
                        normal_map.y * in_bitangent +
                        normal_map.z * in_normal;
#else
    const vec3 normal = in_normal;
#endif

    out_albedo = vec4(in_color, 1.0);
    out_normal = vec4((normal + 1.0) / 2.0 , 1.0); // [-1; 1] -> [0; 1]

#ifdef TEXTURED
    out_albedo *= texture(in_texture, in_uv);
#endif
}
