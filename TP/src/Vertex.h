#ifndef VERTEX_H
#define VERTEX_H

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <utils.h>

namespace OM3D {

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec4 tangent_bitangent_sign = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f); // to avoid completly black meshes if no color is present
};

}

namespace std {

template<typename T>
struct hash<glm::vec<2, T>> {
    size_t operator()(const glm::vec<2, T>& v) const {
        size_t h = 0;
        OM3D::hash_combine(h, hash<T>()(v.x));
        OM3D::hash_combine(h, hash<T>()(v.y));
        return h;
    }
};

template<typename T>
struct hash<glm::vec<3, T>> {
    size_t operator()(const glm::vec<3, T>& v) const {
        size_t h = 0;
        OM3D::hash_combine(h, hash<T>()(v.x));
        OM3D::hash_combine(h, hash<T>()(v.y));
        OM3D::hash_combine(h, hash<T>()(v.z));
        return h;
    }
};

template<typename T>
struct hash<glm::vec<4, T>> {
    size_t operator()(const glm::vec<4, T>& v) const {
        size_t h = 0;
        OM3D::hash_combine(h, hash<T>()(v.x));
        OM3D::hash_combine(h, hash<T>()(v.y));
        OM3D::hash_combine(h, hash<T>()(v.z));
        OM3D::hash_combine(h, hash<T>()(v.w));
        return h;
    }
};

template<>
struct hash<OM3D::Vertex> {
    size_t operator()(const OM3D::Vertex& v) const {
        size_t h = 0;
        OM3D::hash_combine(h, hash<glm::vec3>()(v.position));
        OM3D::hash_combine(h, hash<glm::vec3>()(v.normal));
        OM3D::hash_combine(h, hash<glm::vec2>()(v.uv));
        OM3D::hash_combine(h, hash<glm::vec4>()(v.tangent_bitangent_sign));
        OM3D::hash_combine(h, hash<glm::vec3>()(v.color));
        return h;
    }
};

}

#endif // VERTEX_H
