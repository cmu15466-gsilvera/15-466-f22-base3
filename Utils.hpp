#pragma once
#include "Scene.hpp"

#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>

inline std::string find_suffix_in_scene(const std::string& name, const std::string& prefix, Scene& scene)
{
    std::string suffix = "";
    bool found_name = false;
    for (auto& transform : scene.transforms) {
        if (!found_name) {
            if (transform.name == name) {
                found_name = true;
            }
        } else {
            if (transform.name.find(prefix) != std::string::npos) {
                auto const pos = transform.name.find_last_of('.');
                suffix = transform.name.substr(pos + 1);
                if (suffix == prefix) {
                    suffix = ""; // "." not found
                } else {
                    // include the . before the suffix (eg. .001, .007)
                    suffix = "." + suffix;
                }
                break;
            }
        }
    }
    // std::cout << "found suffix to be \"" << suffix << "\"" << std::endl;
    return suffix;
}

inline glm::vec3 rotate_yaw(const float yaw, const glm::vec3& vec)
{
    glm::mat4 rotMat(1);
    rotMat = glm::rotate(rotMat, yaw, glm::vec3(0, 0, 1.f));
    return glm::vec3(rotMat * glm::vec4(vec, 1.0));
}

inline float repeat(float x, float min, float max)
{
    // assumes the x is bounded
    float normalized = x;
    if (normalized > max) {
        normalized -= 2 * max;
    }
    if (normalized < min) {
        normalized -= 2 * min;
    }
    return normalized;
}

inline float repeat_loop(float x, float min, float max)
{
    float normalized = x;
    while (normalized > max) {
        normalized -= 2 * max;
    }
    while (normalized < min) {
        normalized -= 2 * min;
    }
    return normalized;
}

inline void normalize(glm::vec3& v, float min, float max)
{
    v.x = repeat(v.x, min, max);
    v.y = repeat(v.y, min, max);
    v.z = repeat(v.z, min, max);
}

inline void normalize(glm::vec3& v)
{
    // default bounds within -pi : pi
    normalize(v, -M_PI, M_PI);
}
