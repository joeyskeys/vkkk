#include "camera.h"

namespace vkkk
{

void CameraDeprecated::update_position(float duration) {
    auto right = glm::cross(front, up);
    pos += speed * (x * right + z * front + y * up);
}

void CameraDeprecated::update_orientation() {
    front = rotation * front;
    rotation = glm::quat();
}

}