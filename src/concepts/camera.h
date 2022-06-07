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

    inline glm::mat4 get_view_mat() const {
        return glm::lookAt(pos, pos + front, up);
    }

    inline glm::mat4 get_proj_mat() const {
        return glm::perspective(fov, ratio, near, far);
    }
};