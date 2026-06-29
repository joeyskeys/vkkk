#pragma once

#ifndef GL_core_profile
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cstring>
#include <cstdint>

#include "built_in_shader/common.h"

namespace vkkk
{
#endif

struct Camera {
    // User-friendly camera control state.
    glm::vec3 pos;
    glm::vec3 front;
    glm::vec3 up;
    float fov;
    float ratio;
    float near;
    float far;
    float speed = 1.f;
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
    bool rotating = false;
    double prev_x = 0.f;
    double prev_y = 0.f;
    glm::quat rotation;

    // Final data mirrored to GPU uniform buffers.
    CameraUBO ubo_data;

#ifndef GL_core_profile
    inline glm::mat4 get_trans_mat() const {
        auto flipped = pos;
        flipped[1] *= -1;
        return glm::translate(glm::mat4(), flipped);
    }

    inline glm::mat4 get_view_mat() const {
        return glm::lookAt(pos, pos + front, up);
    }

    inline glm::mat4 get_proj_mat() const {
        // How to properly solve the upside down problem?
        auto persp_mat = glm::perspective(glm::radians(fov), ratio, near, far);
        persp_mat[1][1] *= -1;
        return persp_mat;
        //return glm::perspective(glm::radians(fov), ratio, near, far);
    }

    inline void look_at(const glm::vec3& pos, const glm::vec3& fr, const glm::vec3& up) {
        ubo_data.view = glm::lookAt(pos, pos + fr, up);
    }
    void perspective(const float fov, const float ratio, const float near, const float far) {
        ubo_data.proj = glm::perspective(glm::radians(fov), ratio, near, far);
        ubo_data.proj[1][1] *= -1;
    }

    void update_position(float duration);
    void update_orientation();
    void update_ubo_data();
#endif
};

#ifndef GL_core_profile
}
#endif