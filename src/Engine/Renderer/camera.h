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

    protected:
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

        // Projection field setters
        void setWidth(float w)     { if (areNotAlmostEqual(width, w))     { width = w;     projDirty = true; dirty = true; } }
        void setHeight(float h)    { if (areNotAlmostEqual(height, h))    { height = h;    projDirty = true; dirty = true; } }
        void setNearPlane(float n) { if (areNotAlmostEqual(nearPlane, n)) { nearPlane = n; projDirty = true; } }
        void setFarPlane(float f)  { if (areNotAlmostEqual(farPlane, f))  { farPlane = f;  projDirty = true; } }

        // Projection field getters
        float getWidth() const     { return width; }
        float getHeight() const    { return height; }
        float getNearPlane() const { return nearPlane; }
        float getFarPlane() const  { return farPlane; }

        float x = 0.0f;
        float y = 0.0f;

        float xOffset = 0.0f;
        float yOffset = 0.0f;

        bool dirty = false;

    private:
        float width = 0.0f;
        float height = 0.0f;
        float nearPlane = -1.0f;
        float farPlane = 1.0f;

        bool projDirty = true;
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

        // Projection getters
        const glm::mat4& getProjectionMatrix();
        float getFovDegrees() const  { return fovDegrees; }
        float getAspectRatio() const { return aspectRatio; }
        float getNearPlane() const   { return nearPlane; }
        float getFarPlane() const    { return farPlane; }

        // Projection setters
        void setFovDegrees(float fov)    { if (areNotAlmostEqual(fovDegrees, fov))    { fovDegrees = fov;    projDirty = true; } }
        void setAspectRatio(float ratio) { if (areNotAlmostEqual(aspectRatio, ratio)) { aspectRatio = ratio;  projDirty = true; } }
        void setNearPlane(float n)       { if (areNotAlmostEqual(nearPlane, n))       { nearPlane = n;        projDirty = true; } }
        void setFarPlane(float f)        { if (areNotAlmostEqual(farPlane, f))        { farPlane = f;         projDirty = true; } }

        // constructor with vectors
        Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), float yaw = -90.0f, float pitch = 0.0f);

        // constructor with scalar values
        Camera(float posX, float posY, float posZ, float upX, float upY, float upZ, float yaw, float pitch);

        //Init the camera positions and VAOs
        void init(glm::vec3 position, glm::vec3 up, float yaw, float pitch);

        // Recompute front/right/up from the current yaw/pitch/worldUp.
        // Public so callers that edit yaw/pitch in place (e.g. an FPS
        // controller) can refresh derived vectors without going through
        // init(), which would also rewrite position/yaw/pitch from
        // captured parameters and race with concurrent position updates.
        void updateCameraVectors();

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
        glm::mat4 viewMatrix;
        glm::mat4 projectionMatrix = glm::mat4(1.0f);

        float fovDegrees   = 60.0f;
        float aspectRatio  = 16.0f / 9.0f;
        float nearPlane    = 0.1f;
        float farPlane     = 500.0f;

        bool projDirty = false;
    };
}
