#pragma once

#ifndef GL_core_profile
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstring>
#include <cstdint>

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
    glm::mat4 view;
    glm::mat4 proj;

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
        view = glm::lookAt(pos, pos + fr, up);
    }
    void perspective(const float fov, const float ratio, const float near, const float far) {
        proj = glm::perspective(glm::radians(fov), ratio, near, far);
        proj[1][1] *= -1;
    }

    inline void update_uniform(void* data, uint32_t n) {
        memcpy(data, this, n);
    }

    void update_position(float duration);
    void update_orientation();
#endif
};

using CameraDeprecated = Camera;

#ifndef GL_core_profile
}
#endif