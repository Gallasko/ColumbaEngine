#pragma once

#include <random>

#include "components.h"

#include "Systems/basicsystems.h"
#include "2D/simple2dobject.h"

using namespace pg;

class AlienFormationSystem : public System<InitSys, Listener<TickEvent>> {
private:
    float deltaTime = 0.0f;
    const float SCREEN_WIDTH = 820.0f;
    const float FORMATION_STEP = 40.0f;  // Pixels to move each step

public:
    std::string getSystemName() const override {
        return "Alien Formation System";
    }

    void init() override {
        registerGroup<AlienFormation>();
        registerGroup<PositionComponent, Alien>();
    }

    void onEvent(const TickEvent& event) override {
        deltaTime += event.tick;
    }

    void execute() override {
        if (deltaTime == 0.0f) return;

        // Find the formation controller (singleton entity)
        for (auto formationEntity : viewGroup<AlienFormation>()) {
            auto formation = formationEntity->get<AlienFormation>();

            formation->moveTimer += deltaTime;

            if (formation->moveTimer >= formation->moveInterval) {
                formation->moveTimer = 0;
                moveFormation(formation);

                // Speed up as aliens die (classic Space Invaders)
                int aliensLeft = countAliveAliens();
                if (aliensLeft < formation->totalAliens) {
                    // Exponential speedup as aliens die
                    float speedMultiplier = (float)formation->totalAliens / (float)std::max(1, aliensLeft);
                    formation->moveInterval = 1000.0f / speedMultiplier;
                    formation->moveInterval = std::max(200.0f, formation->moveInterval); // Cap at 200ms
                }
            }
        }

        deltaTime = 0.0f;
    }

private:
    void moveFormation(AlienFormation* formation) {
        bool hitEdge = false;

        // First pass: check if any alien would hit the edge
        for (auto alienEntity : viewGroup<PositionComponent, Alien>()) {
            auto pos = alienEntity->get<PositionComponent>();

            float nextX = pos->x + (formation->direction * FORMATION_STEP);
            if (nextX <= 20 || nextX + pos->width >= SCREEN_WIDTH - 20) {
                hitEdge = true;
                break;
            }
        }

        // Second pass: actually move them
        for (auto alienEntity : viewGroup<PositionComponent, Alien>()) {
            auto pos = alienEntity->get<PositionComponent>();

            if (hitEdge) {
                // Drop down
                pos->setY(pos->y + formation->dropDistance);

                // Check for invasion (game over condition)
                if (pos->y > 500) {  // Too close to paddle
                    printf("INVASION! Aliens reached the bottom!\n");
                    // TODO: Trigger game over
                }
            } else {
                // Move horizontally
                pos->setX(pos->x + formation->direction * FORMATION_STEP);
            }
        }

        // Reverse direction after hitting edge
        if (hitEdge) {
            formation->direction *= -1;
        }

        // TODO: Play step sound here
    }

    int countAliveAliens() {
        int count = 0;
        for (auto entity : viewGroup<PositionComponent, Alien>()) {
            count++;
        }
        return count;
    }
};

// Alien Shooting - Keep it simple, front row only
// ------------------------------------------------

class AlienShootingSystem : public System<InitSys, Listener<TickEvent>> {
private:
    float shootTimer = 0.0f;
    float shootInterval = 2000.0f;  // Base interval between shots
    std::mt19937 rng{std::random_device{}()};

public:
    std::string getSystemName() const override {
        return "Alien Shooting System";
    }

    void init() override {
        registerGroup<PositionComponent, Alien>();
    }

    void onEvent(const TickEvent& event) override {
        shootTimer += event.tick;
    }

    void execute() override {
        if (shootTimer < shootInterval) return;

        shootTimer = 0;

        // Find front-row aliens (highest Y per column)
        std::map<int, EntityRef> frontRowAliens;  // col -> entity

        for (auto alienEntity : viewGroup<PositionComponent, Alien>()) {
            auto alien = alienEntity->get<Alien>();
            auto pos = alienEntity->get<PositionComponent>();

            // Is this alien in front for its column?
            if (frontRowAliens.find(alien->col) == frontRowAliens.end() ||
                frontRowAliens[alien->col]->get<PositionComponent>()->y < pos->y) {
                frontRowAliens[alien->col] = alienEntity->entity;
            }
        }

        // Pick 1-2 random shooters from front row
        if (!frontRowAliens.empty()) {
            std::vector<Entity*> shooters;
            for (auto& [col, entity] : frontRowAliens) {
                shooters.push_back(entity);
            }

            std::uniform_int_distribution<> dis(0, shooters.size() - 1);
            int shooterCount = std::min(2, (int)shooters.size());

            for (int i = 0; i < shooterCount; i++) {
                auto shooter = shooters[dis(rng)];
                auto pos = shooter->get<PositionComponent>();
                spawnBullet(pos->x + pos->width/2, pos->y + pos->height);
            }
        }

        // Randomize next shot interval
        std::uniform_real_distribution<> intervalDis(1500.0f, 3000.0f);
        shootInterval = intervalDis(rng);
    }

private:
    void spawnBullet(float x, float y) {
        // Create bullet using shape system
        auto bullet = makeSimple2DShape(ecsRef, Shape2D::Square, 4, 12);
        auto pos = bullet.get<PositionComponent>();

        pos->setX(x - 2);  // Center the 4px wide bullet
        pos->setY(y);

        auto vel = bullet.attach<Velocity>();
        vel->dy = 200.0f;  // Downward speed

        bullet.attach<AlienBullet>();

        // Make it visually distinct
        if (auto shape = bullet.get<Simple2DObject>()) {
            shape->setColors({255, 255, 100, 255});  // Yellow bullets
        }

        printf("Alien fired from (%.1f, %.1f)\n", x, y);
    }
};

// The Meat: Ball vs Alien Collision
// ----------------------------------

class AlienCollisionSystem : public System<InitSys, Listener<TickEvent>, Ref<GameScore>>
{
private:
    float deltaTime = 0.0f;

public:
    std::string getSystemName() const override { return "Alien Collision System"; }

    void init() override
    {
        registerGroup<PositionComponent, Velocity, Ball>();
        registerGroup<PositionComponent, Alien>();
        registerGroup<PositionComponent, AlienBullet, Velocity>();
        registerGroup<PositionComponent, Paddle>();
    }

    void onEvent(const TickEvent& event) override
    {
        deltaTime += event.tick;
    }

    void execute() override {
        if (deltaTime == 0.0f)
            return;

        checkBallVsAliens();
        checkBulletsVsPaddle();
        cleanupBullets();

        deltaTime = 0.0f;
    }

private:
    void checkBallVsAliens()
    {
        for (auto ballEntity : viewGroup<PositionComponent, Velocity, Ball>())
        {
            auto ballPos = ballEntity->get<PositionComponent>();
            auto ballVel = ballEntity->get<Velocity>();
            auto ball = ballEntity->get<Ball>();

            if (!ball->launched)
                continue;

            for (auto alienEntity : viewGroup<PositionComponent, Alien>())
            {
                auto alienPos = alienEntity->get<PositionComponent>();
                auto alien = alienEntity->get<Alien>();

                if (checkAABB(ballPos, alienPos))
                {
                    // Destroy alien
                    ecsRef->removeEntity(alienEntity->entity);

                    // Bounce ball
                    ballVel->dy = -ballVel->dy;

                    // Update score
                    for (auto scoreEntity : viewGroup<GameScore>())
                    {
                        auto score = scoreEntity->get<GameScore>();
                        score->score += alien->points;
                        score->aliensRemaining--;
                        printf("Score: %d | Aliens left: %d\n", score->score, score->aliensRemaining);

                        if (score->aliensRemaining <= 0)
                        {
                            printf("VICTORY! All aliens destroyed!\n");
                            // TODO: Trigger win state
                        }
                    }

                    // TODO: Spawn explosion particles

                    break;  // Ball can only hit one alien per frame
                }
            }
        }
    }

    void checkBulletsVsPaddle()
    {
        for (auto bulletEntity : viewGroup<PositionComponent, AlienBullet, Velocity>())
        {
            auto bulletPos = bulletEntity->get<PositionComponent>();

            for (auto paddleEntity : viewGroup<PositionComponent, Paddle>())
            {
                auto paddlePos = paddleEntity->get<PositionComponent>();

                if (checkAABB(bulletPos, paddlePos))
                {
                    // Destroy bullet
                    ecsRef->removeEntity(bulletEntity->entity);

                    // Lose life
                    for (auto scoreEntity : view<GameScore>())
                    {
                        auto score = scoreEntity->get<GameScore>();
                        score->lives--;
                        printf("Hit! Lives: %d\n", score->lives);

                        if (score->lives <= 0)
                        {
                            printf("GAME OVER!\n");
                            // TODO: Trigger game over
                        }
                    }

                    // TODO: Flash paddle red
                    break;
                }
            }
        }
    }

    void cleanupBullets()
    {
        // Remove bullets that went off screen
        for (auto bulletEntity : viewGroup<PositionComponent, AlienBullet, Velocity>())
        {
            auto pos = bulletEntity->get<PositionComponent>();

            // Off bottom
            if (pos->y > 640)
            {
                ecsRef->removeEntity(bulletEntity->entity);
            }
        }
    }

    bool checkAABB(PositionComponent* a, PositionComponent* b)
    {
        return a->x < b->x + b->width   and
                a->x + a->width > b->x  and
                a->y < b->y + b->height and
                a->y + a->height > b->y;
    }
};

// Update bullet physics (they just fall)
class BulletPhysicsSystem : public System<InitSys, Listener<TickEvent>> {
private:
    float deltaTime = 0.0f;

public:
    std::string getSystemName() const override {
        return "Bullet Physics System";
    }

    void init() override {
        registerGroup<PositionComponent, AlienBullet, Velocity>();
    }

    void onEvent(const TickEvent& event) override {
        deltaTime += event.tick;
    }

    void execute() override {
        if (deltaTime == 0.0f) return;

        float dt = deltaTime / 1000.0f;

        for (auto bulletEntity : viewGroup<PositionComponent, AlienBullet, Velocity>()) {
            auto pos = bulletEntity->get<PositionComponent>();
            auto vel = bulletEntity->get<Velocity>();

            pos->setY(pos->y + vel->dy * dt);
        }

        deltaTime = 0.0f;
    }
};
