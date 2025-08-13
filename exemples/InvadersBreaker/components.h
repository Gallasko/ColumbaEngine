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