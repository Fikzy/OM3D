#include "Scene.h"

#include <TypedBuffer.h>

#include <shader_structs.h>
#include <utils.h>

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

std::shared_ptr<TypedBuffer<shader::FrameData>> Scene::get_framedata_buffer(const glm::uvec2& window_size, const Camera& camera) const {
    const auto buffer = std::make_shared<TypedBuffer<shader::FrameData>>(nullptr, 1);
    {
        auto mapping = buffer->map(AccessType::WriteOnly);
        mapping[0].window_size = window_size;
        mapping[0].camera.view_proj = camera.view_proj_matrix();
        mapping[0].point_light_count = u32(_point_lights.size());
        mapping[0].sun_color = glm::vec3(1.0f, 1.0f, 1.0f);
        mapping[0].sun_dir = glm::normalize(_sun_direction);
    }
    return buffer;
}

std::vector<const PointLight*> Scene::get_in_frustum_lights(const Camera& camera) const {
    const auto frustum = camera.build_frustum();

    auto lights = std::vector<const PointLight*>();
    for (const auto &light : _point_lights) {
        if (camera.in_frustum(frustum, light.position(), light.radius()))
            lights.push_back(&light);
    }

    return lights;
}

std::shared_ptr<TypedBuffer<shader::PointLight>> Scene::get_lights_buffer(std::vector<const PointLight*> lights) const {
    const auto light_buffer = std::make_shared<TypedBuffer<shader::PointLight>>(nullptr, std::max(lights.size(), size_t(1)));
    {
        auto mapping = light_buffer->map(AccessType::WriteOnly);
        for(size_t i = 0; i != lights.size(); ++i) {
            const auto& light = lights[i];
            mapping[i] = {
                light->position(),
                light->radius(),
                light->color(),
                0.0f
            };
        }
    }
    return light_buffer;
}

RenderInfo Scene::render(const Camera& camera) const {

    const auto frustum = camera.build_frustum();

    auto map = std::unordered_map<size_t, std::vector<const SceneObject*>>();

    for (const auto &obj : _objects) {
        size_t key = std::hash<Material *>()(obj.get_material().get());
        hash_combine(key, obj.get_mesh()->hash);
        if (obj.in_frustum(frustum, camera)) {
            map[key].push_back(&obj);
        }
    }

    // Render every object
    for (const auto& pair : map) {
        const auto& objects = pair.second;

        const auto transform_buffer = std::make_shared<TypedBuffer<shader::Model>>(nullptr, std::max(objects.size(), size_t(1)));
        {
            auto mapping = transform_buffer->map(AccessType::WriteOnly);
            for(size_t i = 0; i != objects.size(); ++i) {
                const auto& obj = objects[i];
                mapping[i] = {
                    obj->transform()
                };
            }
        }
        transform_buffer->bind(BufferUsage::Storage, 2);

        auto material = objects[0]->get_material();
        material->bind();
        auto mesh = objects[0]->get_mesh();
        mesh->draw_instanced(objects.size());
    }

    return RenderInfo{
        _objects.size(),
        map.size(),
    };
}

RenderInfo Scene::render_sun_shadowmap(const Camera &camera) const {
    // TODO: frustum culling
    auto map = std::unordered_map<size_t, std::vector<const SceneObject*>>();

    for (const auto &obj : _objects) {
        size_t key = obj.get_mesh()->hash;
        map[key].push_back(&obj);
    }

    // Setup projection matrix
    auto depthProjectionMatrix = glm::ortho<float>(-100, 100, -100, 100, -100, 200);
    // TODO: move view matrix according to camera
    auto camera_position = camera.position();
    auto forward = _sun_direction;
    auto right = glm::cross(forward, glm::vec3(0, 1, 0));
    auto up = glm::cross(right, forward);
    auto depthViewMatrix = glm::lookAt(_sun_direction * 100.0f, glm::vec3(0, 0, 0), up);

    auto shadow_camera = Camera();
    shadow_camera.set_view(depthViewMatrix);
    shadow_camera.set_proj(depthProjectionMatrix);
    
    const auto framedata_buffer = get_framedata_buffer(glm::vec2(0, 0), shadow_camera);
    framedata_buffer->bind(BufferUsage::Uniform, 0);

    // Render every object
    for (const auto& pair : map) {
        const auto& objects = pair.second;

        const auto transform_buffer = std::make_shared<TypedBuffer<shader::Model>>(nullptr, std::max(objects.size(), size_t(1)));
        {
            auto mapping = transform_buffer->map(AccessType::WriteOnly);
            for(size_t i = 0; i != objects.size(); ++i) {
                const auto& obj = objects[i];
                mapping[i] = {
                    obj->transform()
                };
            }
        }
        transform_buffer->bind(BufferUsage::Storage, 2);

        // auto material = objects[0]->get_material();
        // material->bind();
        auto mesh = objects[0]->get_mesh();
        mesh->draw_instanced(objects.size());
    }
}

}
