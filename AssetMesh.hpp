#include "Mode.hpp"

#include "BBox.hpp"
#include "Scene.hpp"
#include "Utils.hpp"

#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>

#include <deque>
#include <iostream>
#include <vector>

struct AssetMesh {
    AssetMesh(const std::string& nameIn)
    {
        name = nameIn;
    }

    std::unordered_map<std::string, Scene::Transform**> components;

    BBox bounds;

    // metadata
    std::string name = "";

    bool enabled = true;

    void die()
    {
        enabled = false;
    }
};

struct PhysicalAssetMesh : AssetMesh {

    glm::vec3 pos, vel, accel;
    glm::vec3 rot, rotvel, rotaccel;

    glm::vec3 collision_force;
    constexpr static glm::vec3 gravity = glm::vec3(0, 0, -9.8);

    PhysicalAssetMesh(const std::string& nameIn)
        : AssetMesh(nameIn)
    {
        pos = glm::vec3(0, 0, 0);
        vel = glm::vec3(0, 0, 0);
        accel = gravity;
        rot = glm::vec3(0, 0, 0);
        rotvel = glm::vec3(0, 0, 0);
        rotaccel = glm::vec3(0, 0, 0);
        collision_force = glm::vec3(0, 0, 0);
    }

    void update(const float dt)
    {
        // update positional kinematics
        vel += dt * accel;
        vel += dt * collision_force;

        // reset collision force until next collision
        collision_force = glm::vec3(0, 0, 0);

        if (pos.z <= 0) {
            // downward velocity is 0 when on the ground
            vel.z = std::max(0.f, vel.z);
        }
        // std::cout << glm::to_string(vel) << std::endl;
        pos += dt * vel;
        pos.z = std::max(0.f, pos.z);

        // update rotational/angular kinematics
        rotvel += dt * rotaccel;
        rot += dt * rotvel;
        normalize(rot);

        // update bounds based off position and rotation
        bounds.update(pos, rot.z); // only rotate with yaw
    }
};

struct FourWheeledVehicle : PhysicalAssetMesh {

    FourWheeledVehicle(const std::string& nameIn)
        : PhysicalAssetMesh(nameIn)
    {
    }

    bool bIsPlayer = false;
    Scene::Transform *all, *chassis, *wheel_FL, *wheel_FR, *wheel_BL, *wheel_BR;

    void initialize_components()
    {
        components[name] = &all;
        components["body"] = &chassis;
        components["wheel_frontLeft"] = &wheel_FL;
        components["wheel_frontRight"] = &wheel_FR;
        components["wheel_backLeft"] = &wheel_BL;
        components["wheel_backRight"] = &wheel_BR;
    }

    void initialize_from_scene(Scene& scene)
    {
        assert(scene.transforms.size() > 0);

        // add components to the global dictionary
        initialize_components();

        // find the first (and should be only) instance of $name in scene
        std::string suffix = find_suffix_in_scene(name, "body", scene);

        // get pointers to scene components for convenience:
        /// TODO: should we flip the order of these loops?
        for (auto& s : components) {
            const std::string key = s.first;
            // only add suffix if not searching for the name of the object itself
            const std::string search = key + ((key == name) ? "" : suffix);
            for (auto& transform : scene.transforms) {
                if (transform.name == search) { // contains key
                    // std::cout << "found match for " << search << " to be " << transform.name << std::endl;
                    (*s.second) = &transform;
                    break;
                }
            }
            /// TODO: break early once all the components are found
            if (s.second == nullptr) {
                throw std::runtime_error("this should not be null in \"" + name + "\"");
            }
            if ((*s.second) == nullptr) {
                throw std::runtime_error("Unable to find " + name + "'s \"" + s.first + "\" in scene");
            }
        }

        auto* mesh = Scene::all_meshes["body" + suffix];
        if (mesh == nullptr) {
            throw std::runtime_error("null mesh in chassis (\"" + chassis->name + "\") \"" + name + "\"!");
        }

        bounds = BBox(mesh->min, mesh->max);

        pos = all->position;
        rot = glm::eulerAngles(all->rotation);
    }

    glm::vec3 get_heading(bool raw = false) const
    {
        const float yaw = rot.z + (raw ? 0 : (M_PI / 2));
        return glm::vec3(glm::cos(yaw), glm::sin(yaw), 0);
    }

    FourWheeledVehicle* target = nullptr;
    void think(const float dt, const std::vector<FourWheeledVehicle*>& others)
    {
        if (others.size() <= 1) {
            return;
        }

        // randomly choose another target
        while ((target == nullptr || target == this || !target->enabled)) {
            target = others[std::rand() % others.size()];
        }

        // get direction to target
        glm::vec2 dir2D = glm::vec2(target->pos - this->pos);

        // turn to face the target
        // glm::vec2 heading = glm::vec2(get_heading());
        glm::vec2 heading_swap = glm::vec2(get_heading(true));

        // positive when front, negative when behind
        // float dot = glm::dot(glm::normalize(dir2D), glm::normalize(heading));
        // positive when right, negative when left
        float dot2 = glm::dot(glm::normalize(dir2D), glm::normalize(heading_swap));
        float angle = std::acos(dot2) - M_PI / 2.f;
        // whether the target is within the bounds of steering
        int forward = (std::fabs(angle) < wheel_bounds.y) && (std::fabs(angle) > wheel_bounds.x);
        angle = std::min(wheel_bounds.y, std::max(wheel_bounds.x, angle));
        if (!forward) {
            angle = -glm::sign(dot2) * float(M_PI / 4);
        }
        this->throttle = glm::min(1.f, 1.f / glm::length(dir2D));
        this->steer = angle;
    }

    void update(const float dt)
    {
        woggle += 2 * dt;
        woggle -= std::floor(woggle);

        if (throttle > 0) {
            chassis->rotation = glm::angleAxis(glm::radians(std::sin(woggle * 2 * float(M_PI))), glm::vec3(0.0f, 1.0f, 0.0f));
        }
        // create 3D acceleration vector
        auto heading = get_heading();
        accel = heading * (throttle_force * throttle - brake_force * brake) + glm::vec3(0, 0, accel.z);

        // compute forward speed
        glm::vec3 vel_2D = glm::vec3(vel.x, vel.y, 0);
        int velocity_sign = glm::sign(glm::dot(vel_2D, heading));
        float signed_speed = velocity_sign * glm::length(vel_2D);

        wheel_rot -= dt * signed_speed;
        wheel_FL->rotation = glm::angleAxis(steer, glm::vec3(0, 0, 1)) * glm::angleAxis(wheel_rot, glm::vec3(1, 0, 0));
        wheel_FR->rotation = glm::angleAxis(steer, glm::vec3(0, 0, 1)) * glm::angleAxis(wheel_rot, glm::vec3(1, 0, 0));
        // these (rear) wheels are not on a z-axis rotation
        wheel_BL->rotation = glm::angleAxis(wheel_rot, glm::vec3(1, 0, 0));
        wheel_BR->rotation = glm::angleAxis(wheel_rot, glm::vec3(1, 0, 0));

        if (pos.z <= 0) { // ground update
            // inspiration for this physics update was taken from this code:
            // https://github.com/winstxnhdw/KinematicBicycleModel

            // compute friction
            float friction = signed_speed * (c_r + c_a * signed_speed);
            accel -= vel_2D * friction; // scale forward velocity by friction
            const float MAX_ACCEL = 100;
            accel.x = std::min(std::max(accel.x, -MAX_ACCEL), MAX_ACCEL);
            accel.y = std::min(std::max(accel.y, -MAX_ACCEL), MAX_ACCEL);

            // ensure velocity in x/y is linked to heading
            vel.x = signed_speed * heading.x;
            vel.y = signed_speed * heading.y;

            // compute angular velocity (only along yaw)
            rotvel = (signed_speed * glm::tan(steer_force * steer) / wheel_diameter_m) * glm::vec3(0, 0, 1);
        } else { // in the air
            accel = gravity;
        }

        // finally perform the physics update
        PhysicalAssetMesh::update(dt);
        all->position = pos;
        all->rotation = glm::quat(rot); // euler to Quat!
    }

    void turn_wheel(const float delta)
    {
        // clamp steer between -pi/4 to pi/4
        this->steer = std::min(wheel_bounds.y, std::max(wheel_bounds.x, steer + delta));
    }

    // how strong these effects get scaled
    float throttle_force = 10.f;
    float brake_force = 5.f; // brake or reverse?
    float steer_force = 1.f;
    glm::vec2 wheel_bounds = glm::vec2(-M_PI / 4.f, M_PI / 4.f); // [LB, UB]

    // constants
    float wheel_diameter_m = 1.0f;
    float c_r = 0.02f; // coefficient of resistance
    float c_a = 0.025f; // drag coefficient
    float woggle = 0;
    float wheel_rot = 0;

    float timeLastHit = -1e5; // when was the player last hit? (init to negative inf)
    float health = 2; // maximum number of bumps

    // control scheme inputs
    // throttle and brake are between 0..1, steer is between -PI..PI
    float throttle = 0.f, brake = 0.f, steer = 0.f;
};
