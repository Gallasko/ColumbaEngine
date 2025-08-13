#pragma once

#include "ECS/componentregistry.h"

using namespace pg;

// === CORE GAME COMPONENTS ===
struct Velocity : public Component
{
    DEFAULT_COMPONENT_MEMBERS(Velocity)

    float dx = 0, dy = 0;
};

// === GAME SPECIFIC COMPONENTS ===

struct Paddle : public Component
{
    DEFAULT_COMPONENT_MEMBERS(Paddle)

    float speed = 400.0f;
};

struct Ball : public Component
{
    DEFAULT_COMPONENT_MEMBERS(Ball)

    float radius = 8.0f;      // Visual size
    float speed = 350.0f;     // Base speed when launched
    bool launched = false;    // Still attached to paddle?
};