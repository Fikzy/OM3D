#version 450

#include "utils.glsl"

layout(location = 0) in vec3 in_pos;

layout(binding = 2) buffer Models {
    Model models[];
};

void main() {
    const mat4 model = models[gl_InstanceID].transform;
    gl_Position = model * vec4(in_pos, 1.0);
}
