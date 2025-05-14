#pragma once
// Todo

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

// #include <QVector3D>
// #include <QMatrix4x4>

// #include <QObject>
// #include <QOpenGLFunctions>

#include "constant.h"
// #include "Input/input.h"

namespace pg
{
    struct AbstractCamera
    {
        virtual const glm::mat4& getProjectionMatrix() = 0;
        virtual const glm::mat4& getViewMatrix() = 0;

        virtual ~AbstractCamera() {}

        glm::mat4 projectionMatrix = glm::mat4(1.0f);
        glm::mat4 viewMatrix = glm::mat4(1.0f);
    };

    struct BaseCamera2D : public AbstractCamera
    {
        virtual const glm::mat4& getProjectionMatrix() override;
        virtual const glm::mat4& getViewMatrix() override;

        void constructMatrices();

        virtual ~BaseCamera2D() {}

        float width = 0.0f;
        float height = 0.0f;
        float nearPlane = -1.0f;
        float farPlane = 1.0f;

        float x = 0.0f;
        float y = 0.0f;
    };

    class Camera
    {
    public:
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

        // constructor with vectors
        Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), float yaw = -90.0f, float pitch = 0.0f);

        // constructor with scalar values
        Camera(float posX, float posY, float posZ, float upX, float upY, float upZ, float yaw, float pitch);

        //Init the camera positions and VAOs
        void init(glm::vec3 position, glm::vec3 up, float yaw, float pitch);

        void setSensitivity(float sensitivity);

        void setPos(float x, float y, float z);

        // returns the view matrix calculated using Euler Angles and the LookAt Matrix
        glm::mat4 getViewMatrix();

        // // processes input received from any keyboard-like input system. Accepts input parameter in the form of camera defined ENUM (to abstract it from windowing systems)
        void processCameraMovement(const constant::Camera_Movement& direction, float deltaTime);

        // // processes input received from a mouse input system. Expects the offset value in both the x and y direction.
        // void ProcessMouseMovement(float xoffset, float yoffset, Input *inputHandler, GLboolean constrainPitch = true);

        // // processes input received from a mouse scroll-wheel event. Only requires input on the vertical wheel-axis
        // void ProcessMouseScroll(float yoffset);

    // public slots:
    //     void updateKeyboard(Input *inputHandler, double deltaTime...);
    //     void updateMouse(Input *inputHandler, double deltaTime...);

    private:
        // calculates the front vector from the Camera's (updated) Euler Angles
        void updateCameraVectors();

        glm::mat4 viewMatrix;
    };
}