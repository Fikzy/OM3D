#include <glad/glad.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <iostream>
#include <filesystem>
#include <optional>
#include <vector>

#include <graphics.h>
#include <SceneView.h>
#include <Texture.h>
#include <Framebuffer.h>
#include <ImGuiRenderer.h>
#include <Material.h>

#include <imgui/imgui.h>


using namespace OM3D;

static float delta_time = 0.0f;
const glm::uvec2 window_size(1600, 900);

const auto FORWARD_PIPELINE = std::pair{"basic_lit.frag", "basic.vert"};
const auto DEFERRED_PIPELINE = std::pair{"gbuffer.frag", "basic.vert"};

auto current_scene = std::optional<std::filesystem::path>();
auto scene_paths = std::vector<std::filesystem::path>();
auto current_pipeline = DEFERRED_PIPELINE;


void glfw_check(bool cond) {
    if(!cond) {
        const char* err = nullptr;
        glfwGetError(&err);
        std::cerr << "GLFW error: " << err << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

void update_delta_time() {
    static double time = 0.0;
    const double new_time = program_time();
    delta_time = float(new_time - time);
    time = new_time;
}

void process_inputs(GLFWwindow* window, Camera& camera) {
    static glm::dvec2 mouse_pos;

    glm::dvec2 new_mouse_pos;
    glfwGetCursorPos(window, &new_mouse_pos.x, &new_mouse_pos.y);

    const auto world_up = glm::vec3(0.0f, 1.0f, 0.0f);
    const auto camera_forward = glm::cross(-camera.right(), world_up);

    {
        glm::vec3 movement = {};
        if(glfwGetKey(window, 'W') == GLFW_PRESS) {
            movement += camera_forward;
        }
        if(glfwGetKey(window, 'S') == GLFW_PRESS) {
            movement -= camera_forward;
        }
        if(glfwGetKey(window, 'D') == GLFW_PRESS) {
            movement += camera.right();
        }
        if(glfwGetKey(window, 'A') == GLFW_PRESS) {
            movement -= camera.right();
        }
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            movement += world_up;
        }
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
            movement -= world_up;
        }

        float speed = 10.0f;
        if(glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
            speed *= 10.0f;
        }

        if(movement.length() > 0.0f) {
            const glm::vec3 new_pos = camera.position() + movement * delta_time * speed;
            camera.set_view(glm::lookAt(new_pos, new_pos + camera.forward(), camera.up()));
        }
    }

    if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        const glm::vec2 delta = glm::vec2(mouse_pos - new_mouse_pos) * 0.005f;
        if(delta.length() > 0.0f) {
            glm::mat4 rot = glm::rotate(glm::mat4(1.0f), delta.x, glm::vec3(0.0f, 1.0f, 0.0f));
            rot = glm::rotate(rot, delta.y, camera.right());
            camera.set_view(glm::lookAt(camera.position(), camera.position() + (glm::mat3(rot) * camera.forward()), (glm::mat3(rot) * camera.up())));
        }
    } else {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    mouse_pos = new_mouse_pos;
}


std::unique_ptr<Scene> create_default_scene() {
    auto scene = std::make_unique<Scene>();

    // Load default cube model
    const auto cube_scene_path = std::string(data_path) + "scenes/cube_lights.glb";
    auto result = Scene::from_gltf(cube_scene_path, current_pipeline);
    if (result.is_ok) {
        current_scene.emplace(cube_scene_path);
    }

    ALWAYS_ASSERT(result.is_ok, "Unable to load default scene");

    return std::move(result.value);
}


int main(int, char**) {
    DEBUG_ASSERT([] { std::cout << "Debug asserts enabled" << std::endl; return true; }());

    glfw_check(glfwInit());
    DEFER(glfwTerminate());

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(window_size.x, window_size.y, "TP window", nullptr, nullptr);
    glfw_check(window);
    DEFER(glfwDestroyWindow(window));

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync
    init_graphics();

    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    std::cout << "Scenes:" << std::endl;
    for (const auto& entry : std::filesystem::directory_iterator(std::string(data_path) + "scenes/")) {
        if (entry.path().extension() == ".glb") {
            std::cout << "  - " << entry.path().filename().string() << std::endl;
            scene_paths.push_back(entry.path());
        }
    }

    ImGuiRenderer imgui(window);
    bool debug = false;
    bool debug_updated = false;
    int debug_shader = 0;
    bool debug_light_cull = false;
    bool deferred_rendering = true;
    bool tonemapping = true;

    RenderInfo render_info;
    size_t rendered_point_lights = 0;

    std::unique_ptr<Scene> scene = create_default_scene();
    SceneView scene_view(scene.get());

    auto sphere_scene = Scene::from_gltf(std::string(data_path) + "meshes/sphere.glb", current_pipeline);
    ALWAYS_ASSERT(sphere_scene.is_ok, "Unable to load sphere");
    auto sphere = sphere_scene.value->get_objects()[0].get_mesh();

    auto tonemap_program = Program::from_file("tonemap.comp");
    auto color = std::make_shared<Texture>(window_size, ImageFormat::RGBA8_UNORM);
    Framebuffer tonemap_framebuffer(nullptr, std::array{color.get()});

    auto albedo = std::make_shared<Texture>(window_size, ImageFormat::RGBA8_UNORM); // do not use RGBA8_sRGB!
    auto normal = std::make_shared<Texture>(window_size, ImageFormat::RGBA8_UNORM);
    auto depth = std::make_shared<Texture>(window_size, ImageFormat::Depth32_FLOAT);
    Framebuffer gbuffer(depth.get(), std::array{albedo.get(), normal.get()});

    auto lit = std::make_shared<Texture>(window_size, ImageFormat::RGBA16_FLOAT);
    Framebuffer main_framebuffer(depth.get(), std::array{lit.get()});

    auto ds_program = Program::from_files("lit.frag", "screen.vert");
    auto ds_material = std::make_shared<Material>();
    ds_material->set_program(ds_program);
    ds_material->set_texture(0u, albedo);
    ds_material->set_texture(1u, normal);
    ds_material->set_texture(2u, depth);
    ds_material->set_blend_mode(BlendMode::Alpha);
    ds_material->set_depth_test_mode(DepthTestMode::None);
    ds_material->set_depth_writing(false);

    auto lc_program = Program::from_files("lit.frag", "basic.vert", {"LIGHT_CULL"});
    auto lc_material = std::make_shared<Material>();
    lc_material->set_program(lc_program);
    lc_material->set_texture(0u, albedo);
    lc_material->set_texture(1u, normal);
    lc_material->set_texture(2u, depth);
    lc_material->set_blend_mode(BlendMode::Additive);
    lc_material->set_depth_test_mode(DepthTestMode::Reversed);
    lc_material->set_depth_writing(false);

    // Shadowmap setup
    auto shadowmap_size = glm::vec2(2000, 2000);
    auto shadowmap_texture = std::make_shared<Texture>(shadowmap_size, ImageFormat::Depth32_FLOAT);
    shadowmap_texture->set_parameter(GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    shadowmap_texture->set_parameter(GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    shadowmap_texture->set_parameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    shadowmap_texture->set_parameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    auto shadowmap_framebuffer = Framebuffer(shadowmap_texture.get());

    auto shadowmap_program = Program::from_files("shadowmap.frag", "shadowmap.vert");
    auto shadowmap_material = std::make_shared<Material>();
    shadowmap_material->set_program(shadowmap_program); 

    auto debug_programs = std::array{
        Program::from_files("lit.frag", "screen.vert", {"DEBUG_ALBEDO"}),
        Program::from_files("lit.frag", "screen.vert", {"DEBUG_NORMAL"}),
        Program::from_files("lit.frag", "screen.vert", {"DEBUG_DEPTH"}),
    };
    auto debug_defines = std::array{
        "DEBUG_ALBEDO",
        "DEBUG_NORMAL",
        "DEBUG_DEPTH",
    };

    auto debug_lc_program = Program::from_files("lit.frag", "basic.vert", std::vector<std::string>{"LIGHT_CULL", "DEBUG_LIGHT_CULL"});

    for(;;) {
        glfwPollEvents();
        if(glfwWindowShouldClose(window) || glfwGetKey(window, GLFW_KEY_ESCAPE)) {
            break;
        }

        update_delta_time();

        if(const auto& io = ImGui::GetIO(); !io.WantCaptureMouse && !io.WantCaptureKeyboard) {
            process_inputs(window, scene_view.camera());
        }

        // Render shadowmap
        shadowmap_framebuffer.bind();
        shadowmap_material->bind();
        scene_view.render_sun_shadowmap();

        // Render scene
        const auto framedata_buffer = scene->get_framedata_buffer(window_size, scene_view.camera());
        framedata_buffer->bind(BufferUsage::Uniform, 0);

        const auto lights = scene->get_in_frustum_lights(scene_view.camera());
        const auto lights_buffer = scene->get_lights_buffer(lights);
        lights_buffer->bind(BufferUsage::Storage, 1);
        rendered_point_lights = lights.size();

        if (!deferred_rendering) {

            main_framebuffer.bind();
            render_info = scene_view.render();

        } else {
            // Render the scene into the gbuffer
            gbuffer.bind();
            render_info = scene_view.render();

            // Ambiant + directional lighting
            ds_material->bind();
            main_framebuffer.bind();
            framedata_buffer->bind(BufferUsage::Uniform, 0);
            glDrawArrays(GL_TRIANGLES, 0, 3);

            if (!debug) {
                // Light culling
                lc_material->bind();
                main_framebuffer.bind(false);
                
                // Vertex shader
                const auto transform_buffer = std::make_shared<TypedBuffer<shader::Model>>(nullptr, std::max(lights.size(), size_t(1)));
                auto mapping = transform_buffer->map(AccessType::WriteOnly);
                for(size_t i = 0; i != lights.size(); ++i) {
                    const auto& light = lights[i];
                    mapping[i] = { 
                        glm::translate(glm::mat4(1.0f), light->position()) * glm::scale(glm::mat4(1.0f), glm::vec3(light->radius()))
                    };
                }
                transform_buffer->bind(BufferUsage::Storage, 2);

                // Fragment shader
                const auto light_buffer = scene_view.scene()->get_lights_buffer(lights);
                light_buffer->bind(BufferUsage::Storage, 1);

                sphere->draw_instanced(lights.size());
            }
        }

        // Apply a tonemap in compute shader
        if (tonemapping) {
            tonemap_program->bind();
            lit->bind(0);
            color->bind_as_image(1, AccessType::WriteOnly);
            glDispatchCompute(align_up_to(window_size.x, 8), align_up_to(window_size.y, 8), 1);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        tonemapping ? tonemap_framebuffer.blit() : main_framebuffer.blit();

        // GUI
        imgui.start();
        {
            for (const auto& path : scene_paths) {
                bool disabled = path == current_scene;
                if (disabled) {
                    ImGui::BeginDisabled();
                }
                if (ImGui::Button(path.filename().string().c_str())) {
                    auto result = Scene::from_gltf(path.string(), current_pipeline);
                    if(!result.is_ok) {
                        std::cerr << "Unable to load scene (" << path.string() << ")" << std::endl;
                    } else {
                        scene = std::move(result.value);
                        scene_view = SceneView(scene.get());
                        current_scene = path;
                    }
                    deferred_rendering = true;
                }
                if (disabled) {
                    ImGui::EndDisabled();
                }
                ImGui::SameLine();
            }
            ImGui::NewLine();
            ImGui::NewLine();

            if (ImGui::Checkbox("Deferred rendering", &deferred_rendering) || (!deferred_rendering && debug_updated)) {
                current_pipeline = deferred_rendering ? DEFERRED_PIPELINE : FORWARD_PIPELINE;
                auto result = Scene::from_gltf(current_scene->string(), current_pipeline,
                    debug ? Span<const std::string>{debug_defines[debug_shader]} : Span<const std::string>{});
                if(!result.is_ok) {
                    std::cerr << "Unable to reload scene (" << current_scene->string() << ")" << std::endl;
                } else {
                    scene = std::move(result.value);
                    scene_view.set_scene(scene.get());
                    std::cout << "Set rendering pipeline to: {\"" << current_pipeline.first << "\", \"" << current_pipeline.second << "\"}" << std::endl;
                }
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("Warning: reloads entire scene");
            }

            ImGui::Checkbox("Tonemapping", &tonemapping);
            ImGui::NewLine();

            debug_updated = ImGui::Checkbox("Debug shader", &debug);
            if (debug) {

                debug_updated |= ImGui::RadioButton("Albedo", &debug_shader, 0);
                debug_updated |= ImGui::RadioButton("Normal", &debug_shader, 1);
                if (deferred_rendering) {
                    debug_updated |= ImGui::RadioButton("Depth", &debug_shader, 2);
                }

                ds_material->set_program(debug_programs[debug_shader]);
            } else {
                ds_material->set_program(ds_program);
            }

            if (deferred_rendering && ImGui::Checkbox("Debug light culling", &debug_light_cull)) {
                lc_material->set_program(debug_light_cull ? debug_lc_program : lc_program);
                lc_material->set_depth_test_mode(debug_light_cull ? DepthTestMode::Standard : DepthTestMode::Reversed);
            }
            ImGui::NewLine();

            ImGui::Text("Render info:");
            ImGui::Text("  - scene objects: %zu", render_info.scene_objects);
            ImGui::Text("  - draw instanced calls: %zu", render_info.draw_instanced_calls);
            ImGui::Text("  - points lights: %zu", scene->get_point_light_count());
            ImGui::Text("  - rendered points lights: %zu", rendered_point_lights);
        }
        imgui.finish();

        glfwSwapBuffers(window);
    }

    scene = nullptr; // destroy scene and child OpenGL objects
}
