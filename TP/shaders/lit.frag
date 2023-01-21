#version 450
// lit.frag

#include "utils.glsl"

layout(location = 0) out vec4 out_color;

layout(location = 6) flat in int instanceID;

layout(binding = 0) uniform sampler2D in_albedo_texture;
layout(binding = 1) uniform sampler2D in_normal_texture;
layout(binding = 2) uniform sampler2D in_depth_texture;
layout(binding = 3) uniform sampler2D in_shadow_texture;

layout(binding = 0) uniform Data {
    FrameData frame;
};

layout(binding = 1) buffer PointLights {
    PointLight point_lights[];
};

const vec3 ambient = vec3(0.02);

float light_contribution(PointLight light, vec3 frag_pos, vec3 normal) {
    const vec3 to_light = (light.position - frag_pos);
    const float dist = length(to_light);
    const vec3 light_vec = to_light / dist;

    const float NoL = dot(light_vec, normal);
    const float att = attenuation(dist, light.radius);
    if (NoL > 0.0 && att > 0.0f) {
        return NoL * att;
    }
    return 0.0;
}

void main() {
    vec3 albedo = texelFetch(in_albedo_texture, ivec2(gl_FragCoord.xy), 0).rgb;
    vec3 normal = texelFetch(in_normal_texture, ivec2(gl_FragCoord.xy), 0).xyz;
    normal = normal * 2.0 - 1.0; // [0, 1] -> [-1, 1]
    float depth = texelFetch(in_depth_texture, ivec2(gl_FragCoord.xy), 0).r;

    if (depth == 0.0) {
        discard;
    }

    vec2 uv = gl_FragCoord.xy / frame.window_size;
    vec3 position = unproject(uv, depth, inverse(frame.camera.view_proj));

#ifndef LIGHT_CULL // Compute lighting for the sun & ambient

    // Check if the fragment is in the shadow
    vec3 acc = ambient;

    vec4 shadow_coord = frame.sun_view_proj * vec4(position, 1.0);
    vec2 shadow_texture_coord = (shadow_coord.xy + 1) / 2;
    float shadow_depth = texture(in_shadow_texture, shadow_texture_coord).r;
    if (shadow_depth <= shadow_coord.z) {
        acc += frame.sun_color * max(0.0, dot(frame.sun_dir, normal));
    }

    out_color = vec4(albedo * acc, 1.0);

#else // LIGHT_CULL
    PointLight light = point_lights[instanceID];

    vec3 acc = light.color * light_contribution(light, position, normal);

    out_color = vec4(albedo * acc, 1.0); // Additive blending

#ifdef DEBUG_LIGHT_CULL
    out_color = vec4(light.color, 1.0);
#endif // !DEBUG_LIGHT_CULL

#endif // !LIGHT_CULL

#ifdef DEBUG_ALBEDO
    out_color = vec4(albedo, 1.0);
#elif DEBUG_NORMAL
    if (depth != 0.0)
        out_color = vec4(normal * 0.5 + 0.5, 1.0);
    else
        out_color = vec4(albedo, 1.0);
#elif DEBUG_DEPTH
    out_color = vec4(vec3(depth * 10000.0), 1.0);
#endif
}
