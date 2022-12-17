#version 450

#include "utils.glsl"

// #define DEBUG_NORMAL

layout(location = 0) out vec4 out_color;

layout(location = 0) in vec2 in_uv;

layout(binding = 0) uniform sampler2D in_albedo_texture;
layout(binding = 1) uniform sampler2D in_normal_texture;
layout(binding = 2) uniform sampler2D in_depth_texture;

layout(binding = 0) uniform Data {
    FrameData frame;
};

layout(binding = 1) buffer PointLights {
    PointLight point_lights[];
};

const vec3 ambient = vec3(0.0);

void main() {
    vec3 albedo = texelFetch(in_albedo_texture, ivec2(gl_FragCoord.xy), 0).rgb;
    vec3 normal = texelFetch(in_normal_texture, ivec2(gl_FragCoord.xy), 0).xyz;
    normal = normalize(normal * 2.0 - 1.0);
    float depth = texelFetch(in_depth_texture, ivec2(gl_FragCoord.xy), 0).r;

    vec3 position = unproject(in_uv, depth, inverse(frame.camera.view_proj));

    vec3 acc = frame.sun_color * max(0.0, dot(frame.sun_dir, normal)) + ambient;

    for(uint i = 0; i != frame.point_light_count; ++i) {
        PointLight light = point_lights[i];
        const vec3 to_light = (light.position - position);
        const float dist = length(to_light);
        const vec3 light_vec = to_light / dist;

        const float NoL = dot(light_vec, normal);
        const float att = attenuation(dist, light.radius);
        if(NoL <= 0.0 || att <= 0.0f) {
            continue;
        }

        acc += light.color * (NoL * att);
    }

    out_color = vec4(albedo * acc, 1.0);

#ifdef DEBUG_NORMAL
    out_color = vec4(normal * 0.5 + 0.5, 1.0);
#endif
}
