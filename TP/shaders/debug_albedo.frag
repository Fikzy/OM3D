#version 450

layout(location = 0) out vec4 out_color;

layout(location = 0) in vec2 in_uv;

layout(binding = 0) uniform sampler2D in_texture;

void main() {
    vec3 albedo = texelFetch(in_texture, ivec2(gl_FragCoord.xy), 0).rgb;
    out_color = vec4(albedo, 1.0);
}
