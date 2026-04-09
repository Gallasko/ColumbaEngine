#include "stdafx.h"

#include "camera.h"

#include "logger.h"

namespace pg
{
    static constexpr const char * const DOM = "Camera";

    const glm::mat4& BaseCamera2D::getProjectionMatrix()
    {
        return projectionMatrix;
    }

    const glm::mat4& BaseCamera2D::getViewMatrix()
    {
        return viewMatrix;
    }

    constant::Vector2D BaseCamera2D::screenToWorld(float mouseX, float mouseY) const
    {
        // Ensure the viewport dimensions are valid
        if (width <= 0.0f || height <= 0.0f)
        {
            LOG_ERROR("FollowCamera2D", "Invalid viewport dimensions: width = " << width << ", height = " << height);
            return {0.0f, 0.0f};
        }

        // Normalize screen coordinates to range [0, 1]
        float normalizedX = mouseX / width;
        float normalizedY = mouseY / height;

        // Convert normalized coordinates to world space
        float worldX = x + xOffset + normalizedX * width;
        float worldY = y + yOffset + normalizedY * height;

        return {worldX, worldY};
    }

    void BaseCamera2D::constructMatrices()
    {
        viewMatrix = glm::mat4(1.0f);

        auto realX = x + xOffset;
        auto realY = y + yOffset;

        viewMatrix[3][0] = -realX * 2.0f / width;
        viewMatrix[3][1] =  realY * 2.0f / height;
        viewMatrix[3][2] = -2;

        projectionMatrix = getProjectionMatrix();

        dirty = false;
    }

    BaseCamera3D::BaseCamera3D(glm::vec3 position, glm::vec3 up, float yaw, float pitch) : front(glm::vec3(0.0f, 0.0f, -1.0f)),
        movementSpeed(0.5f), mouseSensitivity(0.005f), zoom(0.5f) // Todo make this configurable
    {
        LOG_THIS_MEMBER(DOM);

        init(position, up, yaw, pitch);
    }

    BaseCamera3D::BaseCamera3D(float posX, float posY, float posZ, float upX, float upY, float upZ, float yaw, float pitch) : front(glm::vec3(0.0f, 0.0f, -1.0f)),
        movementSpeed(0.5f), mouseSensitivity(0.005f), zoom(0.5f)
    {
        LOG_THIS_MEMBER(DOM);

        glm::vec3 position = glm::vec3(posX, posY, posZ);
        glm::vec3 up = glm::vec3(upX, upY, upZ);

        init(position, up, yaw, pitch);
    }

    void BaseCamera3D::init(glm::vec3 position, glm::vec3 up, float yaw, float pitch)
    {
        LOG_THIS_MEMBER(DOM);

        this->position = position;
        this->worldUp = up;
        this->yaw = yaw;
        this->pitch = pitch;
        this->zoom = 30.0f;

        updateCameraVectors();
    }

    void BaseCamera3D::setSensitivity(float sensitivity)
    {
        LOG_THIS_MEMBER(DOM);

        mouseSensitivity = sensitivity;
    }

    void BaseCamera3D::setPos(float x, float y, float z)
    {
        LOG_THIS_MEMBER(DOM);

        position.x = x;
        position.y = y;
        position.z = z;
    }

    const glm::mat4& BaseCamera3D::getProjectionMatrix()
    {
        projectionMatrix = glm::perspective(glm::radians(zoom), width / height, nearPlane, farPlane);

        return projectionMatrix;
    }

    const glm::mat4& BaseCamera3D::getViewMatrix()
    {
        viewMatrix = glm::lookAt(position, position + front, up);

        return viewMatrix;
    }

    constant::Vector2D BaseCamera3D::screenToWorld(float mouseX, float mouseY) const
    {
        glm::vec3 win(mouseX, height - mouseY, 0.0f);
        glm::vec4 viewport(0.0f, 0.0f, width, height);
        glm::vec3 world = glm::unProject(win, viewMatrix, projectionMatrix, viewport);

        return {world.x, world.y};
    }

    void BaseCamera3D::processCameraMovement(const constant::Camera_Movement& direction, float deltaTime)
    {
        float velocity = movementSpeed * deltaTime;

        if (direction == constant::Camera_Movement::FORWARD)
            // position += up * velocity;
            position.z += 0.1;
        if (direction == constant::Camera_Movement::BACKWARD)
            // position -= up * velocity;
            position.z -= 0.1;
        if (direction == constant::Camera_Movement::LEFT)
            position -= right * velocity;
        if (direction == constant::Camera_Movement::RIGHT)
            position += right * velocity;

        LOG_INFO(DOM, "New position: x = " << position.x << ", y = " << position.y << ", z = " << position.z);
    }

    void BaseCamera3D::updateCameraVectors()
    {
        // Calculate the new Front vector
        glm::vec3 front;
        front.x = (cos(glm::radians(yaw)) * cos(glm::radians(pitch)));
        front.y = (sin(glm::radians(pitch)));
        front.z = (sin(glm::radians(yaw)) * cos(glm::radians(pitch)));
        front = glm::normalize(front);
        // Also re-calculate the Right and Up vector
        // Normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.
        right = glm::normalize(glm::cross(front, worldUp));
        up    = glm::normalize(glm::cross(right, front));
    }
}