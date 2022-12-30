#ifndef SCENE_H
#define SCENE_H

#include <SceneObject.h>
#include <PointLight.h>
#include <Camera.h>

#include <shader_structs.h>

#include <vector>
#include <memory>

namespace OM3D {

class Scene : NonMovable {

    public:
        Scene();

        static Result<std::unique_ptr<Scene>> from_gltf(const std::string& file_name, const std::pair<const char *, const char *> pipeline, Span<const std::string> defines = {});

        std::shared_ptr<TypedBuffer<shader::FrameData>> get_framedata_buffer(const Camera& camera) const;
        std::shared_ptr<TypedBuffer<shader::PointLight>> get_lights_buffer(const Camera& camera) const;

        void render(const Camera& camera) const;

        void add_object(SceneObject obj);
        void add_object(PointLight obj);

    private:
        std::vector<SceneObject> _objects;
        std::vector<PointLight> _point_lights;
        glm::vec3 _sun_direction = glm::vec3(0.2f, 1.0f, 0.1f);
};

}

#endif // SCENE_H
