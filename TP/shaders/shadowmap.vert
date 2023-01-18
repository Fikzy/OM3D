#version 450

#include "utils.glsl"

layout(location = 0) in vec3 in_pos;

layout(binding = 0) uniform Data {
    FrameData frame;
};

layout(binding = 2) buffer Models {
    Model models[];
};

void main() {
    const mat4 model = models[gl_InstanceID].transform;
    const vec4 position = model * vec4(in_pos, 1.0);

    gl_Position = frame.camera.view_proj * position;
}
