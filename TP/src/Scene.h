#ifndef SCENE_H
#define SCENE_H

#include <SceneObject.h>
#include <PointLight.h>
#include <Camera.h>

#include <shader_structs.h>

#include <vector>
#include <memory>

namespace OM3D {

struct RenderInfo {
    size_t scene_objects = 0;
    size_t draw_instanced_calls = 0;
};

class Scene : NonMovable {

    public:
        Scene();

        static Result<std::unique_ptr<Scene>> from_gltf(const std::string& file_name, const std::pair<const char *, const char *> pipeline, Span<const std::string> defines = {});

        std::shared_ptr<TypedBuffer<shader::FrameData>> get_framedata_buffer(const glm::uvec2& window_size, const Camera& camera) const;

        std::vector<const PointLight*> get_in_frustum_lights(const Camera& camera) const;
        std::shared_ptr<TypedBuffer<shader::PointLight>> get_lights_buffer(std::vector<const PointLight*> lights) const;

        size_t get_point_light_count() const { return _point_lights.size(); }

        const std::vector<SceneObject>& get_objects() { return _objects; }
        const std::vector<PointLight>& get_point_lights() { return _point_lights; }

        glm::mat4 get_sun_view_proj(const Camera &camera, const float near, const float far) const;

        RenderInfo render(const Camera& camera) const;
        RenderInfo render_sun_shadowmap(const Camera& camera) const;

        void add_object(SceneObject obj);
        void add_object(PointLight obj);

        void set_ambient(const glm::vec3& ambient) { _ambient = ambient; }
        void set_sun_direction(const glm::vec3& dir) { _sun_direction = dir; }
        void set_sun_color(const glm::vec3& color) { _sun_color = color; }

    private:
        std::vector<SceneObject> _objects;
        std::vector<PointLight> _point_lights;
        glm::vec3 _ambient = glm::vec3(0.1f, 0.1f, 0.1f);
        glm::vec3 _sun_direction = glm::vec3(0.2f, 1.0f, 0.1f);
        glm::vec3 _sun_color = glm::vec3(1.0f, 1.0f, 1.0f);
        glm::mat4 _sun_view_proj;
};

}

#endif // SCENE_H
