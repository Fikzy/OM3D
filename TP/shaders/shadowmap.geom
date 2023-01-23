#version 450

#include "utils.glsl"

layout(triangles, invocations = 4) in;
layout(triangle_strip, max_vertices = 3) out;

layout(binding = 0) uniform Data {
    FrameData frame;
};

void main() {
    for (int i = 0; i < gl_in.length(); i++) {
        gl_Position = frame.sun_view_projs[gl_InvocationID] * gl_in[i].gl_Position;
        gl_Layer = gl_InvocationID;
        EmitVertex();
    }
    EndPrimitive();
}
