#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Load.hpp"
#include "Mesh.hpp"
#include "data_path.hpp"
#include "gl_errors.hpp"

#include <glm/gtc/type_ptr.hpp>
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

Load<Sound::Sample> dusty_floor_sample(LoadTagDefault, []() -> Sound::Sample const* {
    return new Sound::Sample(data_path("dusty-floor.opus"));
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

    // auto rng = std::default_random_engine {};
    // std::shuffle(std::begin(vehicle_names), std::end(vehicle_names), rng);

    for (const std::string& name : vehicle_names) {
        FourWheeledVehicle* FWV = new FourWheeledVehicle(name);
        FWV->initialize_from_scene(scene);
        vehicle_map.push_back(FWV);
        // the first vehicle will be the player
    }

    Player = vehicle_map[0];
    std::cout << "Determined player to be \"" << Player->name << "\"" << std::endl;
    Player->bIsPlayer = true;
    Player->health = 10;

    // get pointer to camera for convenience:
    if (scene.cameras.size() != 1)
        throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
    camera = &scene.cameras.front();

    // start music loop playing:
    //  (note: position will be over-ridden in update())
    leg_tip_loop = Sound::loop_3D(*dusty_floor_sample, 1.0f, Player->pos, 10.0f);
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
        if (SDL_GetRelativeMouseMode() == SDL_FALSE) {
            SDL_SetRelativeMouseMode(SDL_TRUE);
            return true;
        }
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

void PlayMode::update(float elapsed)
{

    if (vehicle_map.size() == 1 && vehicle_map[0]->bIsPlayer) {
        // last one standing
        game_over = true;
        win = true;
    }

    // move sound to follow leg tip position:
    leg_tip_loop->set_position(Player->pos, 1.0f / 60.0f);


    // update all the vehicles
    for (FourWheeledVehicle* FWV : vehicle_map) {
        if (!FWV->bIsPlayer) {
            FWV->target = Player;
            FWV->think(elapsed, vehicle_map); // determine target & controls
        }
        FWV->update(elapsed);
        // check collisions
        FWV->bounds.collided = false;
        glm::vec3 heading = FWV->get_heading();
        for (FourWheeledVehicle* otherFWV : vehicle_map) {
            bool was_collision = (FWV->bounds.collides_with(otherFWV->bounds) || otherFWV->bounds.collides_with(FWV->bounds));
            if (otherFWV != FWV && was_collision) {
                FWV->bounds.collided = true;

                glm::vec3 dir = FWV->pos - otherFWV->pos;
                FWV->collision_force = 0.5f * dir / elapsed;
                // check if got bumped
                if (glm::dot(dir, heading) > 0) {
                    FWV->health--;
                    if (FWV->health == 0)
                        FWV->die();
                }
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
                if (FWV->bIsPlayer) {
                    game_over = true;
                    win = false;
                    return;
                }
                /// TODO: figure out a better/proper way to destroy
                // move it to under the screen so it is invis
                FWV->all->position = glm::vec3(0, 0, -100);
            }
        }

        vehicle_map = std::move(alive_vehicles);
    }

    {
        // combine inputs into a move:
        if (left.pressed || right.pressed) {
            const float wheel_turn_rate = Player->pos.z > 0 ? 2.f : 0.5f; // how many radians per second are turned
            float delta = elapsed * wheel_turn_rate;
            if (left.pressed && !right.pressed)
                Player->turn_wheel(delta);
            if (!left.pressed && right.pressed)
                Player->turn_wheel(-delta);
        } else {
            // force feedback return steering wheel to 0
            Player->steer += elapsed * 2.f * (0 - Player->steer);
        }
        if (jump.pressed) {
            if (!justJumped && Player->pos.z == 0) {
                // give some initial velocity
                Player->vel += glm::vec3(0, 0, 10);
                justJumped = true;
            }
        } else {
            justJumped = false;
        }

        // std::cout << Player->steer << std::endl;

        if (up.pressed || down.pressed) {
            if (down.pressed && !up.pressed) {
                Player->throttle = 0;
                Player->brake = 1;
            }
            if (!down.pressed && up.pressed) {
                Player->throttle = 1;
                Player->brake = 0;
            }
        } else {
            Player->throttle = 0;
            Player->brake = 0;
        }
        /// TODO: rotate camera?
        camera->transform->position = Player->pos + camera_offset;
    }

    // move camera:
    {
        // camera->transform->position += glm::vec3(motion.x, motion.y, 0);

        glm::mat4x3 frame = camera->transform->make_local_to_parent();
        glm::vec3 right = frame[0];
        glm::vec3 up = frame[1];
        // glm::vec3 forward = -frame[2];

        camera_offset += mouse_drag_speed_x * move.x * right + mouse_drag_speed_y * move.y * up; // + mouse_scroll_speed * move.z * forward;

        // reset the delta's so the camera stops when mouse up
        move = glm::vec2(0, 0);

        // camera_offset.y = std::min(-1.f, std::max(camera_offset.y, -camera_arm_length)); // forward (negative bc looking behind vehicle)
        camera_offset.z = std::min(camera_arm_length, std::max(camera_offset.z, 1.f)); // vertical
        // normalize camera so its always camera_arm_length away
        camera_offset /= glm::length(camera_offset);
        camera_offset *= camera_arm_length;

        glm::vec3 dir = glm::normalize(Player->all->position - camera->transform->position);
        /// TODO: fix the spinning when go directly over and up is parallel to dir
        camera->transform->rotation = glm::quatLookAt(dir, glm::vec3(0, 0, 1));
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
            float win_message_width = win ? 0.5f : 0.9f;
            float win_message_height = 0.3f;
            auto win_message = win ? "VICTORY ACHIEVED!" : "YOU DIED";
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
            lines.draw_text("Health: " + std::to_string(Player->health),
                glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
                glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
                glm::u8vec4(0x00, 0x00, 0x00, 0xF0));
            float ofs = 2.0f / drawable_size.y;
            lines.draw_text("Health: " + std::to_string(Player->health),
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
