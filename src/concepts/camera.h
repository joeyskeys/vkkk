#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

class Camera {
public:
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

    inline glm::mat4 get_trans_mat() const {
        return glm::translate(glm::mat4(), pos);
    }

    inline glm::mat4 get_view_mat() const {
        return glm::lookAt(pos, pos + front, up);
    }

    inline glm::mat4 get_proj_mat() const {
        return glm::perspective(glm::radians(fov), ratio, near, far);
    }

    void update_position(float duration);
    void update_orientation();
};