#include "Scene.h"

#include <TypedBuffer.h>

#include <shader_structs.h>

#include <algorithm>
#include <unordered_map>

namespace OM3D {

Scene::Scene() {
}

void Scene::add_object(SceneObject obj) {
    _objects.emplace_back(std::move(obj));
}

void Scene::add_object(PointLight obj) {
    _point_lights.emplace_back(std::move(obj));
}

void Scene::render(const Camera& camera) const {
    // Fill and bind frame data buffer
    TypedBuffer<shader::FrameData> buffer(nullptr, 1);
    {
        auto mapping = buffer.map(AccessType::WriteOnly);
        mapping[0].camera.view_proj = camera.view_proj_matrix();
        mapping[0].point_light_count = u32(_point_lights.size());
        mapping[0].sun_color = glm::vec3(1.0f, 1.0f, 1.0f);
        mapping[0].sun_dir = glm::normalize(_sun_direction);
    }
    buffer.bind(BufferUsage::Uniform, 0);

    // Fill and bind lights buffer
    TypedBuffer<shader::PointLight> light_buffer(nullptr, std::max(_point_lights.size(), size_t(1)));
    {
        auto mapping = light_buffer.map(AccessType::WriteOnly);
        for(size_t i = 0; i != _point_lights.size(); ++i) {
            const auto& light = _point_lights[i];
            mapping[i] = {
                light.position(),
                light.radius(),
                light.color(),
                0.0f
            };
        }
    }
    light_buffer.bind(BufferUsage::Storage, 1);

    const auto frustum = camera.build_frustum();

    auto map = std::unordered_map<Material*, std::vector<const SceneObject*>>();
    for (const auto &obj : _objects) {
        auto material_ptr = obj.get_material().get();
        
        if (obj.in_frustum(frustum, camera))
            map[material_ptr].push_back(&obj);
    }

    // Render every object
    for (const auto& pair : map) {
        const auto& material = pair.first;
        const auto& objects = pair.second;

        // FIXME: actually call glDrawArraysInstanced
        for (const SceneObject* obj : objects) {
            material->set_uniform(HASH("model"), obj->transform());
            material->bind();

            auto mesh = obj->get_mesh();
            mesh->draw();
        }
    }
}

}
