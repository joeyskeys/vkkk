#include "camera.h"

void Camera::update_position(float duration)
{
    auto right = glm::cross(front, up);
    pos += speed * (x * right + z * front + y * up);
}