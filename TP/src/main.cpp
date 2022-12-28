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

auto current_scene = std::optional<std::filesystem::path>();
auto scene_paths = std::vector<std::filesystem::path>();


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

    {
        glm::vec3 movement = {};
        if(glfwGetKey(window, 'W') == GLFW_PRESS) {
            movement += camera.forward();
        }
        if(glfwGetKey(window, 'S') == GLFW_PRESS) {
            movement -= camera.forward();
        }
        if(glfwGetKey(window, 'D') == GLFW_PRESS) {
            movement += camera.right();
        }
        if(glfwGetKey(window, 'A') == GLFW_PRESS) {
            movement -= camera.right();
        }
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            movement += camera.up();
        }
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
            movement -= camera.up();
        }

        float speed = 10.0f;
        if(glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
            speed *= 10.0f;
        }

        if(movement.length() > 0.0f) {
            const glm::vec3 new_pos = camera.position() + movement * delta_time * speed;
            camera.set_view(glm::lookAt(new_pos, new_pos + camera.forward(), camera.up()));
        }
    }

    if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        const glm::vec2 delta = glm::vec2(mouse_pos - new_mouse_pos) * 0.01f;
        if(delta.length() > 0.0f) {
            glm::mat4 rot = glm::rotate(glm::mat4(1.0f), delta.x, glm::vec3(0.0f, 1.0f, 0.0f));
            rot = glm::rotate(rot, delta.y, camera.right());
            camera.set_view(glm::lookAt(camera.position(), camera.position() + (glm::mat3(rot) * camera.forward()), (glm::mat3(rot) * camera.up())));
        }

    }

    mouse_pos = new_mouse_pos;
}


std::unique_ptr<Scene> create_default_scene() {
    auto scene = std::make_unique<Scene>();

    // Load default cube model
    auto result = Scene::from_gltf(std::string(data_path) + "cube.glb", "gbuffer.frag", "basic.vert");
    if (result.is_ok) {
        current_scene.emplace(std::string(data_path) + "cube.glb");
    }

    ALWAYS_ASSERT(result.is_ok, "Unable to load default scene");
    scene = std::move(result.value);

    // Add lights
    {
        PointLight light;
        light.set_position(glm::vec3(1.0f, 2.0f, 4.0f));
        light.set_color(glm::vec3(0.0f, 10.0f, 0.0f));
        light.set_radius(100.0f);
        scene->add_object(std::move(light));
    }
    {
        PointLight light;
        light.set_position(glm::vec3(1.0f, 2.0f, -4.0f));
        light.set_color(glm::vec3(10.0f, 0.0f, 0.0f));
        light.set_radius(50.0f);
        scene->add_object(std::move(light));
    }

    return scene;
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
    for (const auto& entry : std::filesystem::directory_iterator(data_path)) {
        if (entry.path().extension() == ".glb") {
            std::cout << "  - " << entry.path().filename().string() << std::endl;
            scene_paths.push_back(entry.path());
        }
    }

    ImGuiRenderer imgui(window);
    bool debug = false;
    int debug_shader = 0;
    bool tonemapping = true;
    bool basic_rendering = false;

    std::unique_ptr<Scene> scene = create_default_scene();
    SceneView scene_view(scene.get());

    auto tonemap_program = Program::from_file("tonemap.comp");
    auto color = std::make_shared<Texture>(window_size, ImageFormat::RGBA8_UNORM);
    Framebuffer tonemap_framebuffer(nullptr, std::array{color.get()});

    auto albedo = std::make_shared<Texture>(window_size, ImageFormat::RGBA8_sRGB);
    auto normal = std::make_shared<Texture>(window_size, ImageFormat::RGBA8_UNORM);
    auto depth = std::make_shared<Texture>(window_size, ImageFormat::Depth32_FLOAT);
    Framebuffer gbuffer(depth.get(), std::array{albedo.get(), normal.get()});

    auto lit = std::make_shared<Texture>(window_size, ImageFormat::RGBA16_FLOAT);
    auto trash_depth = std::make_shared<Texture>(window_size, ImageFormat::Depth32_FLOAT);
    Framebuffer main_framebuffer(trash_depth.get(), std::array{lit.get()});

    auto ds_program = Program::from_files("lit.frag", "screen.vert");
    auto ds_material = std::make_shared<Material>();
    ds_material->set_program(ds_program);
    ds_material->set_texture(0u, albedo);
    ds_material->set_texture(1u, normal);
    ds_material->set_texture(2u, depth);

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

    for(;;) {
        glfwPollEvents();
        if(glfwWindowShouldClose(window) || glfwGetKey(window, GLFW_KEY_ESCAPE)) {
            break;
        }

        update_delta_time();

        if(const auto& io = ImGui::GetIO(); !io.WantCaptureMouse && !io.WantCaptureKeyboard) {
            process_inputs(window, scene_view.camera());
        }

        if (basic_rendering) {

            main_framebuffer.bind();
            scene_view.render();

        } else {

            // Render the scene into the gbuffer
            gbuffer.bind();
            scene_view.render();

            if (debug) {
                ds_material->set_program(debug_programs[debug_shader]);
            } else {
                ds_material->set_program(ds_program);
            }

            // Compute lighting from the gbuffer
            const auto framedata_buffer = scene_view.scene()->get_framedata_buffer(scene_view.camera());
            framedata_buffer->bind(BufferUsage::Uniform, 0);

            const auto lights_buffer = scene_view.scene()->get_lights_buffer();
            lights_buffer->bind(BufferUsage::Storage, 1);

            ds_material->bind();
            main_framebuffer.bind();
            glDrawArrays(GL_TRIANGLES, 0, 3);
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
                    auto result = Scene::from_gltf(path.string(), "gbuffer.frag", "basic.vert");
                    if(!result.is_ok) {
                        std::cerr << "Unable to load scene (" << path.string() << ")" << std::endl;
                    } else {
                        scene = std::move(result.value);
                        scene_view = SceneView(scene.get());
                        current_scene = path;
                    }
                    basic_rendering = false;
                }
                if (disabled) {
                    ImGui::EndDisabled();
                }
                ImGui::SameLine();
            }
            ImGui::NewLine();
            ImGui::NewLine();

            bool debug_updated = false;
            debug_updated |= ImGui::Checkbox("Debug shader", &debug);
            if (debug)
            {
                debug_updated |= ImGui::RadioButton("Albedo", &debug_shader, 0);
                debug_updated |= ImGui::RadioButton("Normal", &debug_shader, 1);
                debug_updated |= ImGui::RadioButton("Depth", &debug_shader, 2);
            }
            ImGui::NewLine();

            ImGui::Checkbox("Tonemapping", &tonemapping);

            if (ImGui::Checkbox("Basic rendering", &basic_rendering) || (basic_rendering && debug_updated)) {
                std::pair<std::string, std::string> pipeline = basic_rendering
                    ? std::pair{"basic_lit.frag", "basic.vert"}
                    : std::pair{"gbuffer.frag", "basic.vert"};
                auto result = Scene::from_gltf(current_scene->string(), pipeline.first, pipeline.second,
                    debug ? Span<const std::string>{debug_defines[debug_shader]} : Span<const std::string>{});
                if(!result.is_ok) {
                    std::cerr << "Unable to reload scene (" << current_scene->string() << ")" << std::endl;
                } else {
                    scene = std::move(result.value);
                    scene_view.set_scene(scene.get());
                    std::cout << "Set rendering pipeline to: {\"" << pipeline.first << "\", \"" << pipeline.second << "\"}" << std::endl;
                }
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("Warning: reloads entire scene");
            }
        }
        imgui.finish();

        glfwSwapBuffers(window);
    }

    scene = nullptr; // destroy scene and child OpenGL objects
}
