#version 450

layout(location = 0) out vec4 out_color;

layout(location = 0) in vec2 in_uv;

layout(binding = 0) uniform sampler2D in_texture;

void main() {
    vec3 normal = texelFetch(in_texture, ivec2(gl_FragCoord.xy), 0).xyz;
    normal = normalize(normal * 2.0 - 1.0);
    out_color = vec4(normal, 1.0);
}
