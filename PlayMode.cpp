#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Load.hpp"
#include "Mesh.hpp"
#include "data_path.hpp"
#include "gl_errors.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>

#include <algorithm>
#include <random>

GLuint program = 0;
Load<MeshBuffer> load_meshes(LoadTagDefault, []() -> MeshBuffer const* {
    MeshBuffer const* ret = new MeshBuffer(data_path("world.pnct"));
    program = ret->make_vao_for_program(lit_color_texture_program->program);
    return ret;
});

// define static variable
std::unordered_map<std::string, const Mesh*> Scene::all_meshes = {};

Load<Scene> load_scene(LoadTagDefault, []() -> Scene const* {
    return new Scene(data_path("world.scene"), [&](Scene& scene, Scene::Transform* transform, std::string const& mesh_name) {
        Mesh const& mesh = load_meshes->lookup(mesh_name);

        // assign this mesh to the corresponding scene transform
        Scene::all_meshes[transform->name] = &mesh;

        scene.drawables.emplace_back(transform);
        Scene::Drawable& drawable = scene.drawables.back();

        drawable.pipeline = lit_color_texture_program_pipeline;

        drawable.pipeline.vao = program;
        drawable.pipeline.type = mesh.type;
        drawable.pipeline.start = mesh.start;
        drawable.pipeline.count = mesh.count;
    });
});

Load<Sound::Sample> honk_sample(LoadTagDefault, []() -> Sound::Sample const* {
    return new Sound::Sample(data_path("honk.opus"));
});

Load<Sound::Sample> alien_sample(LoadTagDefault, []() -> Sound::Sample const* {
    return new Sound::Sample(data_path("alien.opus"));
});

PlayMode::PlayMode()
    : scene(*load_scene)
{

    std::vector<std::string> vehicle_names = {
        "ambulance",
        "delivery",
        "deliveryFlat",
        "firetruck",
        "garbageTruck",
        "hatchbackSports",
        "police",
        "race",
        "sedan",
        "sedanSports",
        "suv",
        "suvLuxury",
        "taxi",
        "tractor",
        "tractorPolice",
        "tractorShovel",
        "truck",
        "truckFlat",
        "van"
    };
    std::random_device rd;
    std::mt19937 rng(rd());
    std::shuffle(std::begin(vehicle_names), std::end(vehicle_names), rng);

    // make is so the target is always the previous guy
    for (const std::string& name : vehicle_names) {
        FourWheeledVehicle* FWV = new FourWheeledVehicle(name);
        FWV->initialize_from_scene(scene);
        vehicle_map.push_back(FWV);
        // the first vehicle will be the target
    }

    target = vehicle_map[0];
    // std::cout << "Determined target to be \"" << target->name << "\"" << std::endl;

    // get pointer to camera for convenience:
    if (scene.cameras.size() != 1)
        throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
    camera = &scene.cameras.front();
}

PlayMode::~PlayMode()
{
}

bool PlayMode::handle_event(SDL_Event const& evt, glm::uvec2 const& window_size)
{

    if (evt.type == SDL_KEYDOWN) {
        if (evt.key.keysym.sym == SDLK_ESCAPE) {
            SDL_SetRelativeMouseMode(SDL_FALSE);
            return true;
        } else if (evt.key.keysym.sym == SDLK_a) {
            left.downs += 1;
            left.pressed = true;
            return true;
        } else if (evt.key.keysym.sym == SDLK_d) {
            right.downs += 1;
            right.pressed = true;
            return true;
        } else if (evt.key.keysym.sym == SDLK_w) {
            up.downs += 1;
            up.pressed = true;
            return true;
        } else if (evt.key.keysym.sym == SDLK_s) {
            down.downs += 1;
            down.pressed = true;
            return true;
        } else if (evt.key.keysym.sym == SDLK_SPACE) {
            jump.downs += 1;
            jump.pressed = true;
            return true;
        }
    } else if (evt.type == SDL_KEYUP) {
        if (evt.key.keysym.sym == SDLK_a) {
            left.pressed = false;
            return true;
        } else if (evt.key.keysym.sym == SDLK_d) {
            right.pressed = false;
            return true;
        } else if (evt.key.keysym.sym == SDLK_w) {
            up.pressed = false;
            return true;
        } else if (evt.key.keysym.sym == SDLK_s) {
            down.pressed = false;
            return true;
        } else if (evt.key.keysym.sym == SDLK_SPACE) {
            jump.pressed = false;
            return true;
        }
    } else if (evt.type == SDL_MOUSEBUTTONDOWN) {
        check_if_clicked(glm::vec2(evt.motion.x / float(window_size.x), evt.motion.y / float(window_size.y)));
        return true;
    } else if (evt.type == SDL_MOUSEMOTION) {
        if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
            move = glm::vec2(
                evt.motion.xrel / float(window_size.y),
                -evt.motion.yrel / float(window_size.y));
            return true;
        }
    }

    return false;
}

void PlayMode::check_if_clicked(const glm::vec2& mouse_rel)
{
    glm::vec2 m = mouse_rel - 0.5f; // origin is (0.5, 0.5)
    m = glm::vec2(m.x, -m.y); // center is (0, 0), top right is (0.5, 0.5), bottom left is (-0.5, -0.5)
    // std::cout << "(" << m.x << " x " << m.y << ")" << std::endl;
    // create vector from camera to point
    glm::vec3 ray = glm::vec3(0, 0, 0);
    {
        // update listener to camera position:
        glm::mat4x3 frame = camera->transform->make_local_to_parent();
        glm::vec3 right = frame[0];
        glm::vec3 up = frame[1];
        glm::vec3 forward = -frame[2];
        const float fovY = camera->fovy;
        const float fovX = camera->fovy * camera->aspect;
        const float dX = fovX * m.x;
        const float dY = fovY * m.y;
        ray = forward + right * dX + up * dY;
    }

    // process ray-box intersection

    for (FourWheeledVehicle* FWV : vehicle_map) {
        FWV->bounds.collided = FWV->bounds.intersects(camera->transform->position, ray);
        if (FWV->bounds.collided) {
            FWV->die();
            if (FWV == target) {
                game_over = true;
                win = true;
            } else {
                game_over = true;
                win = false;
            }
        }
    }
}

void PlayMode::update(float elapsed)
{

    // if (vehicle_map.size() == 1 && vehicle_map[0]->bIsPlayer) {
    //     // last one standing
    //     game_over = true;
    //     win = true;
    // }

    // update all the vehicles
    for (FourWheeledVehicle* FWV : vehicle_map) {
        if (!FWV->bIsPlayer) {
            FWV->think(elapsed, vehicle_map); // determine target & controls
        }
        FWV->update(elapsed);
        // check collisions
        // FWV->bounds.collided = false; // no need bc ray-box intersection
        // glm::vec3 heading = FWV->get_heading();
        for (FourWheeledVehicle* otherFWV : vehicle_map) {
            bool was_collision = (FWV->bounds.collides_with(otherFWV->bounds) || otherFWV->bounds.collides_with(FWV->bounds));
            if (otherFWV != FWV && was_collision) {
                // FWV->bounds.collided = true;

                glm::vec3 dir = FWV->pos - otherFWV->pos;
                FWV->collision_force = 0.5f * dir / elapsed;
                const float volume = 1.f;
                const float radius = 0.1f;
                if (FWV == target) {
                    sound = Sound::play_3D(*honk_sample, volume, target->pos, radius);
                } else {
                    sound = Sound::play_3D(*alien_sample, volume, target->pos, radius);
                }
                sound->set_position(FWV->pos, 1.0f / 60.0f);
                break;
            }
        }
    }

    {
        // delete all disabled vehicles
        std::vector<FourWheeledVehicle*> alive_vehicles = {};
        for (FourWheeledVehicle* FWV : vehicle_map) {
            if (FWV->enabled) {
                alive_vehicles.push_back(FWV);
            } else {
                /// TODO: figure out a better/proper way to destroy
                // move it to under the screen so it is invis
                FWV->all->position = glm::vec3(0, 0, -100);
            }
        }

        vehicle_map = std::move(alive_vehicles);
    }

    {
        // std::cout << Player->steer << std::endl;

        if (up.pressed) {
            camera_offset = glm::vec3(0, 1, 0);
        } else if (down.pressed) {
            camera_offset = glm::vec3(0, -1, 0);
        } else if (right.pressed) {
            camera_offset = glm::vec3(1, 0, 0);
        } else if (left.pressed) {
            camera_offset = glm::vec3(-1, 0, 0);
        } else {
            camera_offset = glm::vec3(0, 0, 0);
        }
        /// TODO: rotate camera?
        camera->transform->position += 0.1f * camera_offset;
    }

    { // update listener to camera position:
        glm::mat4x3 frame = camera->transform->make_local_to_parent();
        glm::vec3 right = frame[0];
        glm::vec3 at = frame[3];
        Sound::listener.set_position_right(at, right, 1.0f / 60.0f);
    }

    // reset button press counters:
    left.downs = 0;
    right.downs = 0;
    up.downs = 0;
    down.downs = 0;
}

void PlayMode::draw(glm::uvec2 const& drawable_size)
{
    // update camera aspect ratio for drawable:
    camera->aspect = float(drawable_size.x) / float(drawable_size.y);

    // set up light type and position for lit_color_texture_program:
    //  TODO: consider using the Light(s) in the scene to do this
    glUseProgram(lit_color_texture_program->program);
    glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
    glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, -1.0f)));
    glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
    glUseProgram(0);

    glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
    glClearDepth(1.0f); // 1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS); // this is the default depth comparison function, but FYI you can change it.

    GL_ERRORS(); // print any errors produced by this setup code

    scene.draw(*camera);

    {
        // use DrawLines to overlay some text:
        glDisable(GL_DEPTH_TEST);
        float aspect = float(drawable_size.x) / float(drawable_size.y);
        auto projection = glm::mat4(
            1.0f / aspect, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f);

        if (game_over) {
            DrawLines lines(projection, true);
            float win_message_width = win ? 0.5f : 0.7f;
            float win_message_height = 0.3f;
            auto win_message = win ? "VICTORY ACHIEVED!" : "GAME OVER";
            glm::u8vec4 win_colour = win ? glm::u8vec4(0xff, 0xff, 0, 0xff) : glm::u8vec4(0xff, 0, 0, 0xff);
            float pos_x = win ? -1.5f : -1.3f;
            lines.draw_text(win_message,
                glm::vec3(pos_x, 0, 0),
                glm::vec3(win_message_width, 0.0f, 0.0f),
                glm::vec3(0.0f, win_message_height, 0.0f),
                win_colour);

        } else {
            DrawLines lines(projection, false);
            constexpr float H = 0.09f;
            lines.draw_text("# vehicles left: " + std::to_string(vehicle_map.size()),
                glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
                glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
                glm::u8vec4(0x00, 0x00, 0x00, 0xF0));
            float ofs = 2.0f / drawable_size.y;
            lines.draw_text("# vehicles left: " + std::to_string(vehicle_map.size()),
                glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + +0.1f * H + ofs, 0.0),
                glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
                glm::u8vec4(0xff, 0xff, 0xff, 0x00));
        }
    }

    // draw lines in 3D space
    if (true) {
        glDisable(GL_DEPTH_TEST);
        glm::mat4 world_to_clip = camera->make_projection() * glm::mat4(camera->transform->make_world_to_local());

        DrawLines lines(world_to_clip);
        for (FourWheeledVehicle* FWV : vehicle_map) {
            // draw bounding box
            auto collision_colour = FWV->bounds.collided ? glm::u8vec4(0xff, 0x0, 0x0, 0xff) : glm::u8vec4(0xff);
            lines.draw_box(FWV->bounds.get_mat(), collision_colour);
        }
    }
}
