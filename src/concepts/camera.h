#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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

    inline glm::mat4 get_view_mat() const {
        return glm::lookAt(pos, pos + front, up);
    }

    inline glm::mat4 get_proj_mat() const {
        return glm::perspective(glm::radians(fov), ratio, near, far);
    }

    void update_position(float duration);
};