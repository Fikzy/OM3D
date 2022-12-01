#include "StaticMesh.h"

#include <glad/glad.h>

#include <algorithm>
#include <glm/glm.hpp>

namespace OM3D {

StaticMesh::StaticMesh(const MeshData& data) :
    _vertex_buffer(data.vertices),
    _index_buffer(data.indices) {

    glm::vec3 center = glm::vec3(0.0f);
    
    const auto cmp = [center](const Vertex v1, const Vertex v2) {
        return glm::length(v1.position - center) < glm::length(v2.position - center);
    };
    const auto v = *std::max_element(data.vertices.begin(), data.vertices.end(), cmp);

    radius = glm::length(v.position - center);
}

void StaticMesh::draw() const {
    _vertex_buffer.bind(BufferUsage::Attribute);
    _index_buffer.bind(BufferUsage::Index);

    // Vertex position
    glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(Vertex), nullptr);
    // Vertex normal
    glVertexAttribPointer(1, 3, GL_FLOAT, false, sizeof(Vertex), reinterpret_cast<void*>(3 * sizeof(float)));
    // Vertex uv
    glVertexAttribPointer(2, 2, GL_FLOAT, false, sizeof(Vertex), reinterpret_cast<void*>(6 * sizeof(float)));
    // Tangent / bitangent sign
    glVertexAttribPointer(3, 4, GL_FLOAT, false, sizeof(Vertex), reinterpret_cast<void*>(8 * sizeof(float)));
    // Vertex color
    glVertexAttribPointer(4, 3, GL_FLOAT, false, sizeof(Vertex), reinterpret_cast<void*>(12 * sizeof(float)));

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);
    glEnableVertexAttribArray(4);

    glDrawElements(GL_TRIANGLES, int(_index_buffer.element_count()), GL_UNSIGNED_INT, nullptr);
}

}
