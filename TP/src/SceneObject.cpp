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

    const auto object_positon = glm::vec3(_transform[3]);

    auto scale_x = glm::length(glm::vec3(_transform[0]));
    auto scale_y = glm::length(glm::vec3(_transform[1]));
    auto scale_z = glm::length(glm::vec3(_transform[2]));
    auto max_scale = std::max(std::max(scale_x, scale_y), scale_z);

    return camera.in_frustum(frustum, object_positon, _mesh->radius * max_scale);
}

}
