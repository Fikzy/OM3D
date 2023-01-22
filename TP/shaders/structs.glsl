struct CameraData {
    mat4 view_proj;
};

struct FrameData {
    vec2 window_size; // 8 bytes
    uint padding_1[2]; // 8 bytes

    CameraData camera; // 64 bytes

    vec3 sun_dir; // 12 bytes
    uint point_light_count; // 4 bytes

    vec3 sun_color; // 12 bytes
    uint shadow_map_levels; // 4 bytes

    float depth_levels[4]; // 4 * 4 bytes

    mat4 sun_view_projs[4]; // 4 * 64 bytes
};

struct PointLight {
    vec3 position;
    float radius;
    vec3 color;
    float padding_1;
};

struct Model {
    mat4 transform;
};
