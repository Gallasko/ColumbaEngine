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
        LOG_INFO(DOM, "x: " << x << ", y: " << y);
        LOG_INFO(DOM, "width: " << width << ", height: " << height);
        LOG_INFO(DOM, "near: " << -x / width << ", far: " << y / height);
        // return glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.f));
        return glm::translate(glm::mat4(1.0f), glm::vec3(-x / width, y / height, 0.f));
        // return glm::translate(glm::mat4(1.0f), glm::vec3(x / (width / 2.0), y / (height / 2.0), 0.f));
        // return glm::translate(glm::mat4(1.0f), glm::vec3(-0.1, -0.1, 0.f));
        // return glm::mat4(1.0f);
    }
}