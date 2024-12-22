#include "camera.h"

#include "logger.h"

namespace pg
{
    static constexpr const char * const DOM = "Camera";    

    Camera::Camera(glm::vec3 position, glm::vec3 up, float yaw, float pitch) : front(glm::vec3(0.0f, 0.0f, -1.0f)),
        movementSpeed(0.5f), mouseSensitivity(0.005f), zoom(0.5f) // Todo make this configurable
    {
        LOG_THIS_MEMBER(DOM);

        init(position, up, yaw, pitch); 
    }

    Camera::Camera(float posX, float posY, float posZ, float upX, float upY, float upZ, float yaw, float pitch) : front(glm::vec3(0.0f, 0.0f, -1.0f)),
        movementSpeed(0.5f), mouseSensitivity(0.005f), zoom(0.5f)
    {
        LOG_THIS_MEMBER(DOM);

        glm::vec3 position = glm::vec3(posX, posY, posZ);
        glm::vec3 up = glm::vec3(upX, upY, upZ);

        init(position, up, yaw, pitch); 
    }

    void Camera::init(glm::vec3 position, glm::vec3 up, float yaw, float pitch)
    {
        LOG_THIS_MEMBER(DOM);

        this->position = position;
        this->worldUp = up;
        this->yaw = yaw;
        this->pitch = pitch;
        this->zoom = 30.0f;

        updateCameraVectors();
    }

    void Camera::setSensitivity(float sensitivity)
    {
        LOG_THIS_MEMBER(DOM);

        mouseSensitivity = sensitivity;
    }

    void Camera::setPos(float x, float y, float z)
    {
        LOG_THIS_MEMBER(DOM);

        position.x = x;
        position.y = y;
        position.z = z;
    }

    glm::mat4 Camera::getViewMatrix()
    {
        viewMatrix = glm::lookAt(position, position + front, up);

        return viewMatrix;
    }

    void Camera::processCameraMovement(const constant::Camera_Movement& direction, float deltaTime)
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

    // void Camera::ProcessMouseMovement(float xoffset, float yoffset, Input *inputHandler, GLboolean)
    // {
    //     xoffset *= MouseSensitivity;
    //     yoffset *= MouseSensitivity;

    //     if(inputHandler->isButtonPressed(Qt::RightButton))
    //     {
    //         //TODO check this value and correct them 
    //         Position += Up * yoffset / 2.0f;
    //         Position += Right * xoffset / 2.0f;
    //     }

    //     // update Front, Right and Up Vectors using the updated Euler angles
    //     updateCameraVectors();
    // }

    // void Camera::ProcessMouseScroll(float yoffset)
    // {
    //     Zoom -= yoffset;

    //     if (Zoom < 10.0f)
    //         Zoom = 10.0f;
    //     if (Zoom > 45.0f)
    //         Zoom = 45.0f; 
    // }

    void Camera::updateCameraVectors()
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