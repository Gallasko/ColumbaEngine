#pragma once

#include "Engine/ECS/componentregistry.h"

using namespace pg;

// === CORE GAME COMPONENTS ===
struct Position : public Component {
    float x, y;
    DEFAULT_COMPONENT_MEMBERS(Position)
};

struct Velocity : public Component {
    float dx, dy;
    DEFAULT_COMPONENT_MEMBERS(Velocity)
};

struct BoundingBox : public Component {
    float width, height;
    DEFAULT_COMPONENT_MEMBERS(BoundingBox)
};

struct Sprite : public Component {
    std::string texturePath;
    float width, height;
    DEFAULT_COMPONENT_MEMBERS(Sprite)
};

// === GAME-SPECIFIC COMPONENTS ===
struct Paddle : public Component {
    float speed;        // Movement speed
    float minX, maxX;   // Boundary constraints
    DEFAULT_COMPONENT_MEMBERS(Paddle)
};

struct Ball : public Component {
    float baseSpeed;    // Base movement speed
    bool launched;      // Has ball been launched?
    DEFAULT_COMPONENT_MEMBERS(Ball)
};

struct Alien : public Component {
    int row, col;       // Position in formation
    float shootCooldown; // Time until can shoot again
    DEFAULT_COMPONENT_MEMBERS(Alien)
};

struct Bullet : public Component {
    bool isPlayerBullet; // false = alien bullet
    DEFAULT_COMPONENT_MEMBERS(Bullet)
};

struct AlienFormation : public Component {
    float moveTimer;        // Time until formation moves
    float moveInterval;     // How often formation moves
    float dropDistance;     // How far to drop when hitting edge
    int direction;          // 1 = right, -1 = left
    DEFAULT_COMPONENT_MEMBERS(AlienFormation)
};

struct GameState : public Component {
    enum State { READY, PLAYING, GAME_OVER, WIN } currentState;
    int lives;
    int score;
    int aliensRemaining;
    DEFAULT_COMPONENT_MEMBERS(GameState)
};