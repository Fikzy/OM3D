#ifndef MATERIAL_H
#define MATERIAL_H

#include <Program.h>
#include <Texture.h>

#include <memory>
#include <vector>

namespace OM3D {

enum class BlendMode {
    None,
    Alpha,
    Additive,
};

enum class DepthTestMode {
    Standard,
    Reversed,
    Equal,
    None
};

class Material {

    public:
        Material();

        void set_program(std::shared_ptr<Program> prog);
        void set_blend_mode(BlendMode blend);
        void set_depth_test_mode(DepthTestMode depth);
        void set_depth_writing(bool enabled);
        void set_texture(u32 slot, std::shared_ptr<Texture> tex);

        template<typename... Args>
        void set_uniform(Args&&... args) {
            _program->set_uniform(FWD(args)...);
        }


        void bind() const;

        static std::shared_ptr<Material> material(const std::pair<const char *, const char *> pipeline, Span<const std::string> defines);
        static std::shared_ptr<Material> textured_material(const std::pair<const char *, const char *> pipeline, Span<const std::string> defines);
        static std::shared_ptr<Material> textured_normal_mapped_material(const std::pair<const char *, const char *> pipeline, Span<const std::string> defines);


    private:
        std::shared_ptr<Program> _program;
        std::vector<std::pair<u32, std::shared_ptr<Texture>>> _textures;

        BlendMode _blend_mode = BlendMode::None;
        DepthTestMode _depth_test_mode = DepthTestMode::Standard;
        bool _depth_writing = true;
};

}

#endif // MATERIAL_H
