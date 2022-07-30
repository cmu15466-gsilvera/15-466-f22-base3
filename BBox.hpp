#pragma once

#include "Utils.hpp"

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>

#include <iostream>
#include <vector>

class BBox {
public:
    BBox() = default;
    BBox(const glm::vec3& minIn, const glm::vec3& maxIn)
    {
        // initial bounds (relative to origin and no rotation)
        min0 = minIn;
        max0 = maxIn;
        midpt = get_midpoint0();
        extent = max0 - min0;
    }

    bool intersects(const glm::vec3& origin, const glm::vec3& ray)
    {
        /// HACK: only check with the bottom plane/face of the bbox
        // extend the ray: origin + t * ray = pt such that pt.z == 0
        // ==> origin.z + t * ray.z == 0 ==> t = -origin.z / ray.z;
        float t = -origin.z / ray.z;
        glm::vec3 pt_ground = (origin + t * ray);

        /// ALSO: can check with the top plane/face of the bbox
        // extend the day origin + t * ray = pt such that pt.z == midpt.z + extent.z / 2.f
        const float eps = 1e-5;
        float t2 = ((midpt.z + extent.z / 2.f - eps) - origin.z) / ray.z;
        glm::vec3 pt_roof = (origin + t2 * ray);

        return contains_pt(pt_ground) || contains_pt(pt_roof);
    }

    bool contains_pt(const glm::vec3& pt) const
    {
        // first rotate pt by the origin of this bbox by -yaw
        glm::vec3 pt_midpt = pt - midpt; // vector from origin of this box to the pt

        // rotate pt_midpt
        float yaw = rot.z; // since this bbox can be alined along yaw only
        // rotate point by -yaw to reach axis aligned
        glm::vec3 AA_pt = rotate_yaw(-yaw, pt_midpt);

        // now that the pt is axis-aligned to the original bounds, the check is trivial
        bool within_x = (AA_pt.x >= min0.x && AA_pt.x <= max0.x);
        bool within_y = (AA_pt.y >= min0.y && AA_pt.y <= max0.y);

        // z is weird bc only the AA x and y components need to be rotated, z just needs
        // to be within the same plane so it can use the absolute coordinates (not corrected for rotation)
        bool within_z = (pt.z >= midpt.z - extent.z / 2.f && pt.z <= midpt.z + extent.z / 2.f);
        return within_x && within_y && within_z;
    }

    bool collides_with(const BBox& other) const
    {
        /// NOTE: this impl is kinda buggy in that it fails if the two boxes are
        // aligned such that no 8 points are inside the other (like a cross) and midpt
        // is just a hack that works bc none of the bboxes are super long

        // check if this bbox contains any of the 9 points (vertices + midpt) of other
        const glm::vec3 size = other.extent / 2.f;
        const float other_yaw = other.rot.z;
        std::vector<glm::vec3> check_points = {
            other.midpt, // midpt
            other.midpt + rotate_yaw(other_yaw, (size * glm::vec3(1, 1, -1))), // front right bottom
            other.midpt + rotate_yaw(other_yaw, (size * glm::vec3(1, 1, 1))), // front right top
            other.midpt + rotate_yaw(other_yaw, (size * glm::vec3(1, -1, -1))), // front left bottom
            other.midpt + rotate_yaw(other_yaw, (size * glm::vec3(1, -1, 1))), // front left top
            other.midpt + rotate_yaw(other_yaw, (size * glm::vec3(-1, 1, -1))), // rear right bottom
            other.midpt + rotate_yaw(other_yaw, (size * glm::vec3(-1, 1, 1))), // rear right top
            other.midpt + rotate_yaw(other_yaw, (size * glm::vec3(-1, -1, -1))), // rear left bottom
            other.midpt + rotate_yaw(other_yaw, (size * glm::vec3(-1, -1, 1))), // rear left top
        };
        // now with all the pointe check if even one is contained
        for (const glm::vec3& pt : check_points) {
            if (this->contains_pt(pt)) {
                return true;
            }
        }
        return false;
    }

    void update(const glm::vec3& pos, const float yaw)
    {
        /// NOTE: for now these bboxes only support rotation along yaw
        rot = glm::vec3(0, 0, yaw);

        // translate to match pos
        midpt = pos + get_midpoint0();
    }

    glm::vec3 get_midpoint0() const
    {
        return (max0 + min0) / 2.f;
    }
    glm::mat3 get_rotation_mat() const
    {
        float yaw = rot.z;
        return glm::mat3(
            glm::vec3(glm::cos(yaw), -glm::sin(yaw), 0),
            glm::vec3(glm::sin(yaw), glm::cos(yaw), 0),
            glm::vec3(0, 0, 1));
    }
    glm::mat4x3 get_mat() const
    {
        glm::mat4 rot4 = glm::toMat4(glm::quat(rot));
        glm::mat4 trans = glm::translate(glm::mat4(1.0f), midpt);
        glm::mat4 scale = glm::mat4(1.0);
        scale[0][0] = extent.x / 2.f;
        scale[1][1] = extent.y / 2.f;
        scale[2][2] = extent.z / 2.f;
        // first scale, then rotate, then transform
        return trans * rot4 * scale;
    }

    bool collided = false;

    glm::vec3 min0, max0;

    glm::vec3 midpt;
    glm::vec3 extent;
    glm::vec3 rot;
};