// #pragma once

// #include <vector>
// #include <functional>

// #include "../pgconstant.h"
// #include "renderer.h"

// //TODO create a 3D particles system -> this one can only draw 2D particles
// namespace pg
// {
//     //#pragma pack(1)
//     struct Particle
//     {
//         float lifetime;
//         constant::Vector3D pos;
//         float texOffset;
//         float scale;
//     };
//     //#pragma pop

//     struct ParticleSubData {};

//     struct ParticleComponent : protected QOpenGLFunctions
//     {
//         ParticleComponent() { initializeOpenGLFunctions(); }

//         unsigned int count;
//         unsigned int texture;
//         std::function<void()> onTick;

//         Particle *particleList = nullptr;
//         ParticleSubData **particleSubDataList = nullptr;

//         OpenGLObject openglObject;
//         QOpenGLBuffer *instanceVBO = nullptr;
//     };

//     struct ParticleMoveSubData : public ParticleSubData
//     {
//         ParticleMoveSubData(const constant::Vector3D& velocity, const std::vector<float>& textureSeq, const unsigned int& textureChangeRate) : timeAlive(0), velocity(velocity), textureSeq(textureSeq), textureChangeRate(textureChangeRate) {}

//         int timeAlive;
//         constant::Vector3D velocity;
//         std::vector<float> textureSeq;
//         unsigned int textureChangeRate;
//     };

//     //TODO create a particle system that create and manage all the particle component

// }