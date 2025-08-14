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
    float scoreMultiplier = 1.0f;
};

// === POWER UPS COMPONENTS ===

enum class PowerUpType
{
    HEALTH,      // Restore 1 life
    MULTIBALL,   // Spawn 2 extra balls
    FASTBALL,    // 1.5x ball speed (risk/reward)
    BARRIER,     // Bottom barrier for 10 seconds
    WIDE_PADDLE, // 1.5x paddle width for 15 seconds
    TINY_PADDLE  // 0.7x paddle width but 2x points for 15 seconds
};

struct PowerUp : public Component
{
    DEFAULT_COMPONENT_MEMBERS(PowerUp)

    PowerUpType type;
    float fallSpeed = 100.0f;
};

struct PowerUpEffect : public Component
{
    DEFAULT_COMPONENT_MEMBERS(PowerUpEffect)

    PowerUpType type;

    float duration;      // milliseconds (-1 for permanent)
    float elapsed = 0;
    float value;         // Generic value (multiplier, etc)
};

struct PulseEffect : public Component
{
    DEFAULT_COMPONENT_MEMBERS(PulseEffect)

    float time = 0;
    float speed = 3.0f;
};

struct Barrier : public Component
{
    DEFAULT_COMPONENT_MEMBERS(Barrier)

    float health = 3;  // Can take 3 hits
};

// === JUICER COMPONENTS ===

struct ScreenShake : public Component
{
    DEFAULT_COMPONENT_MEMBERS(ScreenShake)

    float trauma = 0.0f;
    float maxOffset = 10.0f;
};

struct Particle : public Component
{
    DEFAULT_COMPONENT_MEMBERS(Particle)

    float lifetime = 500.0f;  // milliseconds
    float elapsed = 0.0f;
};

struct FlashEffect : public Component
{
    DEFAULT_COMPONENT_MEMBERS(FlashEffect)

    float duration = 200.0f;  // milliseconds
    float elapsed = 0.0f;
    constant::Vector4D originalColor;
};

struct Trail : public Component
{
    DEFAULT_COMPONENT_MEMBERS(Trail)

    std::deque<std::pair<float, float>> positions;
    int maxLength = 10;
};