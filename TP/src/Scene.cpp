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
        mapping[0].sun_view_proj = get_sun_view_proj(camera, 0.001f, 100.0f);
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
    auto top_right = glm::cross(frustum._top_normal, frustum._right_normal);
    auto bottom_left = glm::cross(frustum._right_normal, frustum._bottom_normal);
    auto bottom_right = glm::cross(frustum._bottom_normal, frustum._left_normal);

    // - compute 8 corners of the frustum in world space
    auto top_left_near = glm::vec4(camera_position + top_left * near, 1.0f);
    auto top_right_near = glm::vec4(camera_position + top_right * near, 1.0f);
    auto bottom_left_near = glm::vec4(camera_position + bottom_left * near, 1.0f);
    auto bottom_right_near = glm::vec4(camera_position + bottom_right * near, 1.0f);
    auto top_left_far = glm::vec4(camera_position + top_left * far, 1.0f);
    auto top_right_far = glm::vec4(camera_position + top_right * far, 1.0f);
    auto bottom_left_far = glm::vec4(camera_position + bottom_left * far, 1.0f);
    auto bottom_right_far = glm::vec4(camera_position + bottom_right * far, 1.0f);

    // - compute axis aligned bounding box in light space
    glm::vec3 min = glm::vec3(std::numeric_limits<float>::max());
    glm::vec3 max = glm::vec3(std::numeric_limits<float>::min());
    for (const auto& corner : {
        top_left_near, top_right_near, bottom_left_near, bottom_right_near,
        top_left_far, top_right_far, bottom_left_far, bottom_right_far
    }) {
        const auto& light_space_corner = glm::vec3(corner);
        min = glm::min(min, light_space_corner);
        max = glm::max(max, light_space_corner);
    }

    // - compute center of the bounding box in light space
    auto light_pos = glm::vec4((min + max) / 2.0f, 1.0f);

    // - transform center of bounding box to world space
    auto light_pos_world = glm::vec3(light_pos);

    // - compute light view matrix
    auto light_view = glm::lookAt(light_pos_world, light_pos_world - _sun_direction, up);

    min = glm::vec3(std::numeric_limits<float>::max());
    max = glm::vec3(std::numeric_limits<float>::min());
    for (const auto& corner : {
        top_left_near, top_right_near, bottom_left_near, bottom_right_near,
        top_left_far, top_right_far, bottom_left_far, bottom_right_far
    }) {
        const auto& light_space_corner = glm::vec3(light_view * corner);
        min = glm::min(min, light_space_corner);
        max = glm::max(max, light_space_corner);
    }

    // Setup projection matrix
    auto height = 1000.0f;
    auto reverse_z = glm::mat4(1.0f);
    reverse_z[2][2] = -1.0f;
    reverse_z[3][2] = 1.0f;

    // Attempt at shimmering shadow fix
    // auto texel_size = (max - min) / 2000.0f;
    // min = glm::floor(min / texel_size) * texel_size;
    // max = glm::floor(max / texel_size) * texel_size;

    // light_pos_world = glm::floor(light_pos_world / texel_size) * texel_size;
    // light_view = glm::lookAt(light_pos_world, light_pos_world - _sun_direction, up);

    // std::cout << "x: " << min.x << " - " << max.x << " | y: " << min.y << " - " << max.y << std::endl;

    auto shadow_proj = reverse_z * glm::orthoZO<float>(min.x, max.x, min.y, max.y, -height, height);

    auto sun_view_proj = shadow_proj * light_view;

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
