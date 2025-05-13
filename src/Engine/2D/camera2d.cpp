#include "camera2d.h"

namespace pg
{
    namespace
    {
        static constexpr char const * DOM = "Camera2D";
    }

    glm::mat4 BaseCamera2D::getProjectionMatrix()
    {
        return glm::ortho(0.f, width, height, 0.f, near, far);
    }

    glm::mat4 BaseCamera2D::getViewMatrix()
    {
        // return glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.f));
        return glm::translate(glm::mat4(1.0f), glm::vec3(x / width, y / height, 0.f));
    }
}