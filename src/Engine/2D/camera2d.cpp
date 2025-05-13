#include "camera2d.h"

namespace pg
{
    namespace
    {
        static constexpr char const * DOM = "Camera2D";
    }

    glm::mat4 BaseCamera2D::getProjectionMatrix()
    {
        return glm::ortho(0.f, width, height, 0.f, nearPlane, farPlane);
    }

    glm::mat4 BaseCamera2D::getViewMatrix()
    {
        //LOG_INFO(DOM, "x: " << x << ", y: " << y);
        //LOG_INFO(DOM, "width: " << width << ", height: " << height);
        //LOG_INFO(DOM, "near: " << -x / width << ", far: " << y / height);
        // return glm::translate(glm::mat4(1.0f), glm::vec3(x, y, 0.f));
        auto mat4 = glm::mat4(1.0f);

        mat4[3][0] = -x * 2.0f / width;
        mat4[3][1] =  y * 2.0f / height;
        mat4[3][2] = -2;

        return mat4;

        // return glm::translate(glm::mat4(1.0f), glm::vec3(-x * 2.0f / width, y * 2.0f / height, -10));
        // return glm::translate(glm::mat4(1.0f), glm::vec3(x / (width / 2.0), y / (height / 2.0), 0.f));
        // return glm::translate(glm::mat4(1.0f), glm::vec3(-0.1, -0.1, 0.f));
        // return glm::mat4(1.0f);
    }
}