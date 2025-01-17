#include "Material.h"

#include <glad/glad.h>

#include <algorithm>

namespace OM3D {

Material::Material() {
}

void Material::set_program(std::shared_ptr<Program> prog) {
    _program = std::move(prog);
}

void Material::set_blend_mode(BlendMode blend) {
    _blend_mode = blend;
}

void Material::set_depth_test_mode(DepthTestMode depth) {
    _depth_test_mode = depth;
}

void Material::set_depth_writing(bool enabled) {
    _depth_writing = enabled;
}

void Material::set_texture(u32 slot, std::shared_ptr<Texture> tex) {
    if(const auto it = std::find_if(_textures.begin(), _textures.end(), [&](const auto& t) { return t.second == tex; }); it != _textures.end()) {
        it->second = std::move(tex);
    } else {
        _textures.emplace_back(slot, std::move(tex));
    }
}

void Material::bind() const {
    switch(_blend_mode) {
        case BlendMode::None:
            glDisable(GL_BLEND);

            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            glFrontFace(GL_CCW);
        break;

        case BlendMode::Alpha:
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glDisable(GL_CULL_FACE);
        break;

        case BlendMode::Additive:
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);

            glDisable(GL_CULL_FACE);
        break;
    }

    switch(_depth_test_mode) {
        case DepthTestMode::None:
            glDisable(GL_DEPTH_TEST);
        break;

        case DepthTestMode::Equal:
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_EQUAL);
        break;

        case DepthTestMode::Standard:
            glEnable(GL_DEPTH_TEST);
            // We are using reverse-Z
            glDepthFunc(GL_GEQUAL);
        break;

        case DepthTestMode::Reversed:
            glEnable(GL_DEPTH_TEST);
            // We are using reverse-Z
            glDepthFunc(GL_LEQUAL);
        break;
    }

    glDepthMask(_depth_writing ? GL_TRUE : GL_FALSE);

    for(const auto& texture : _textures) {
        texture.second->bind(texture.first);
    }
    _program->bind();
}

std::shared_ptr<Material> Material::material(const std::pair<const char *, const char *> pipeline, Span<const std::string> defines) {
    auto material = std::make_shared<Material>();
    material->_program = Program::from_files(pipeline.first, pipeline.second, defines);
    return material;
}

std::shared_ptr<Material> Material::textured_material(const std::pair<const char *, const char *> pipeline, Span<const std::string> defines) {
    std::vector<std::string> new_defines = {"TEXTURED"};
    new_defines.insert(new_defines.end(), defines.begin(), defines.end());
    return material(pipeline, new_defines);
}

std::shared_ptr<Material> Material::textured_normal_mapped_material(const std::pair<const char *, const char *> pipeline, Span<const std::string> defines) {
    std::vector<std::string> new_defines = {"TEXTURED", "NORMAL_MAPPED"};
    new_defines.insert(new_defines.end(), defines.begin(), defines.end());
    return material(pipeline, new_defines);
}

}
