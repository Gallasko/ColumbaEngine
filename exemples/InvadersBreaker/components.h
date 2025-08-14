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

struct Alien : public Component
{
    DEFAULT_COMPONENT_MEMBERS(Alien)

    int row;     // Which row in formation (0-3)
    int col;     // Which column (0-6)
    int points = 100;  // Score value
};

struct AlienFormation : public Component
{
    DEFAULT_COMPONENT_MEMBERS(AlienFormation)

    float moveTimer = 0.0f;
    float moveInterval = 1500.0f;  // milliseconds between moves
    int direction = 1;              // 1 = right, -1 = left
    float dropDistance = 25.0f;
    int totalAliens = 0;
};

struct AlienBullet : public Component
{
    DEFAULT_COMPONENT_MEMBERS(AlienBullet)

    float speed = 200.0f;
};

struct GameScore : public Component
{
    DEFAULT_COMPONENT_MEMBERS(GameScore)

    int score = 0;
    int lives = 3;
    int aliensRemaining = 0;
};

struct ScreenShake : public Component
{
    DEFAULT_COMPONENT_MEMBERS(ScreenShake)
    float trauma = 0.0f;
    float maxOffset = 10.0f;
};
