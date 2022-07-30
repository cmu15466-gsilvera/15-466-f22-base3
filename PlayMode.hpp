#include "Mode.hpp"

#include "AssetMesh.hpp"
#include "BBox.hpp"
#include "Scene.hpp"
#include "Sound.hpp"

#include <glm/glm.hpp>

#include <deque>
#include <vector>

struct PlayMode : Mode {
    PlayMode();
    virtual ~PlayMode();

    // functions called by main loop:
    virtual bool handle_event(SDL_Event const&, glm::uvec2 const& window_size) override;
    virtual void update(float elapsed) override;
    virtual void draw(glm::uvec2 const& drawable_size) override;

    //----- game state -----

    // input tracking:
    struct Button {
        uint8_t downs = 0;
        uint8_t pressed = 0;
    } left, right, down, up, jump;

    void check_if_clicked(const glm::vec2& mouse);

    // music coming from the sound cue
    std::shared_ptr<Sound::PlayingSample> honk_sound;

    // local copy of the game scene (so code can change it during gameplay):
    Scene scene;
    bool game_over = false;
    bool win = true;

    // all the vehicles in the scene
    std::vector<FourWheeledVehicle*> vehicle_map;
    FourWheeledVehicle* target = nullptr;

    float time = 0.f;
    float next_sound_play = 0.f;

    // camera:
    glm::vec2 move = glm::vec2(0, 0);
    float camera_arm_length = 25.f; // "distance" from camera to player
    glm::vec3 camera_offset = glm::vec3(0, 0, 0);
    float mouse_drag_speed_x = -10;
    float mouse_drag_speed_y = -10;
    float mouse_scroll_speed = 5;
    Scene::Camera* camera = nullptr;
};
