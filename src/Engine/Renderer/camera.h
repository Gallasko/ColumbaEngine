#pragma once
// Todo

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

// #include <QVector3D>
// #include <QMatrix4x4>

// #include <QObject>
// #include <QOpenGLFunctions>

#include "pgconstant.h"
// #include "Input/input.h"

namespace pg
{
    struct AbstractCamera
    {
        virtual const glm::mat4& getProjectionMatrix() = 0;
        virtual const glm::mat4& getViewMatrix() = 0;

        virtual constant::Vector2D screenToWorld(float mouseX, float mouseY) const = 0;

        virtual ~AbstractCamera() {}

        glm::mat4 projectionMatrix = glm::mat4(1.0f);
        glm::mat4 viewMatrix = glm::mat4(1.0f);
    };

    struct BaseCamera2D : public AbstractCamera
    {
        virtual const glm::mat4& getProjectionMatrix() override;
        virtual const glm::mat4& getViewMatrix() override;

        virtual constant::Vector2D screenToWorld(float mouseX, float mouseY) const override;

        void setOffset(const constant::Vector2D& offset)
        {
            xOffset = offset.x;
            yOffset = offset.y;

            dirty = true;
        }

        void constructMatrices();

        virtual ~BaseCamera2D() {}

        float width = 0.0f;
        float height = 0.0f;
        float nearPlane = -1.0f;
        float farPlane = 1.0f;

        float x = 0.0f;
        float y = 0.0f;

        float xOffset = 0.0f;
        float yOffset = 0.0f;

        bool dirty = false;
    };

    struct BaseCamera3D : public AbstractCamera
    {
        // Camera Attributes
        glm::vec3 position;
        glm::vec3 front;
        glm::vec3 up;
        glm::vec3 right;
        glm::vec3 worldUp;

        // Todo change this to calculate rotation with quaternion
        // euler Angles
        float yaw;
        float pitch;

        // camera options
        float movementSpeed;
        float mouseSensitivity;
        float zoom;

        // projection parameters
        float width = 800.0f;
        float height = 600.0f;
        float nearPlane = 0.1f;
        float farPlane = 1000.0f;

        // constructor with vectors
        BaseCamera3D(glm::vec3 position = glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), float yaw = -90.0f, float pitch = 0.0f);

        // constructor with scalar values
        BaseCamera3D(float posX, float posY, float posZ, float upX, float upY, float upZ, float yaw, float pitch);

        //Init the camera positions and VAOs
        void init(glm::vec3 position, glm::vec3 up, float yaw, float pitch);

        void setSensitivity(float sensitivity);

        void setPos(float x, float y, float z);

        virtual const glm::mat4& getProjectionMatrix() override;
        virtual const glm::mat4& getViewMatrix() override;
        virtual constant::Vector2D screenToWorld(float mouseX, float mouseY) const override;

        // processes input received from any keyboard-like input system
        void processCameraMovement(const constant::Camera_Movement& direction, float deltaTime);

        virtual ~BaseCamera3D() {}

    private:
        // calculates the front vector from the Camera's (updated) Euler Angles
        void updateCameraVectors();
    };
}