#include "SceneObject.h"

#include <Camera.h>

#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

namespace OM3D {

SceneObject::SceneObject(std::shared_ptr<StaticMesh> mesh, std::shared_ptr<Material> material) :
    _mesh(std::move(mesh)),
    _material(std::move(material)) {
}

void SceneObject::render() const {
    if(!_material || !_mesh) {
        return;
    }

    _material->set_uniform(HASH("model"), transform());
    _material->bind();
    _mesh->draw();
}

void SceneObject::set_transform(const glm::mat4& tr) {
    _transform = tr;
}

const glm::mat4& SceneObject::transform() const {
    return _transform;
}

bool SceneObject::in_frustum(const Frustum& frustum, const Camera& camera) const {

    auto object_positon = glm::vec3(_transform[3]);

    auto scale_x = glm::length(glm::vec3(_transform[0]));
    auto scale_y = glm::length(glm::vec3(_transform[1]));
    auto scale_z = glm::length(glm::vec3(_transform[2]));
    auto scale = std::max(std::max(scale_x, scale_y), scale_z);

    auto vector_to_object = object_positon - camera.position();

    const auto normals = {frustum._near_normal, frustum._top_normal, frustum._bottom_normal, frustum._right_normal, frustum._left_normal};
    
    for (const auto &normal : normals) {
        auto offset_point = vector_to_object + normal * _mesh->radius * scale;

        if (glm::dot(offset_point, normal) < 0) {
            return false;
        }
    }

    return true;
}

}
