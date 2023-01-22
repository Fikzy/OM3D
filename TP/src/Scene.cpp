#include "Scene.h"

#include <TypedBuffer.h>

#include <shader_structs.h>
#include <utils.h>

#include <algorithm>
#include <iostream>
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

        mapping[0].shadow_map_levels = 4;
        const float depths[] = { 15.0f, 50.0f, 100.0f, 200.0f};
        for (size_t i = 0; i != 4; ++i)
            mapping[0].depth_levels[i] = depths[i];
        mapping[0].sun_view_projs[0] = get_sun_view_proj(camera, 0.001f, depths[0]);
        mapping[0].sun_view_projs[1] = get_sun_view_proj(camera, depths[0], depths[1]);
        mapping[0].sun_view_projs[2] = get_sun_view_proj(camera, depths[1], depths[2]);
        mapping[0].sun_view_projs[3] = get_sun_view_proj(camera, depths[2], depths[3]);
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

glm::mat4 Scene::get_sun_view_proj(const Camera &camera, const float near, const float far) const {

    auto camera_position = camera.position();
    auto forward = _sun_direction;
    auto right = glm::cross(forward, glm::vec3(0, 1, 0));
    auto up = glm::cross(right, forward);

    // Tight ortho projection for sun shadowmap
    auto frustum = camera.build_frustum();
    auto top_left = glm::cross(frustum._left_normal, frustum._top_normal);
    auto top_left_far = camera_position + top_left * far;

    auto frustum_center = camera.forward() * (near + (far - near) * 0.5f) + camera_position;
    auto radius = glm::length(frustum_center - glm::vec3(top_left_far));

    auto light_space_matrix = glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f) - _sun_direction, up);

    auto light_pos_light_space = light_space_matrix * glm::vec4(frustum_center, 1.0f);

    auto texel_size = 2.0f * radius / 2000.0f;
    light_pos_light_space = glm::floor(light_pos_light_space / texel_size) * texel_size;

    auto light_pos = glm::vec3(glm::inverse(light_space_matrix) * light_pos_light_space);
    light_space_matrix = glm::lookAt(light_pos, light_pos - _sun_direction, up);

    // Setup projection matrix
    auto reverse_z = glm::mat4(1.0f);
    reverse_z[2][2] = -1.0f;
    reverse_z[3][2] = 1.0f;

    auto margin = 1000.0f;
    auto shadow_proj = reverse_z * glm::orthoZO<float>(-radius, radius, -radius, radius, -margin, margin);

    auto sun_view_proj = shadow_proj * light_space_matrix;

    return sun_view_proj;
}

RenderInfo Scene::render_sun_shadowmap(const Camera &camera) const {
    // TODO: frustum culling
    auto map = std::unordered_map<size_t, std::vector<const SceneObject*>>();

    for (const auto &obj : _objects) {
        size_t key = obj.get_mesh()->hash;
        map[key].push_back(&obj);
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

        // auto material = objects[0]->get_material();
        // material->bind();
        auto mesh = objects[0]->get_mesh();
        mesh->draw_instanced(objects.size());
    }

    return RenderInfo{
        _objects.size(),
        map.size(),
    };
}

}
