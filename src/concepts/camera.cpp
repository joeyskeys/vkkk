#include "camera.h"

namespace vkkk
{

void Camera::update_position(float duration) {
    auto right = glm::cross(front, up);
    pos += speed * (x * right + z * front + y * up);
}

void Camera::update_orientation() {
    front = rotation * front;
    rotation = glm::quat();
}

void Camera::update_ubo_data() {
    ubo_data.view = get_view_mat();
    ubo_data.proj = get_proj_mat();
}

}