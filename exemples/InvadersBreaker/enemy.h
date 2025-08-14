#pragma once

#include <random>

#include "components.h"

#include "Systems/basicsystems.h"
#include "2D/simple2dobject.h"

using namespace pg;

class AlienFormationSystem : public System<InitSys, Own<AlienFormation>, Own<Alien>,
    Listener<TickEvent>, Listener<GamePaused>, Listener<GameResume>>
{
private:
    float deltaTime = 0.0f;
    const float SCREEN_WIDTH = 820.0f;
    const float FORMATION_STEP = 40.0f;
    const float DANGER_ZONE_Y = 420.0f;  // Aliens stop here - still plenty of room to play

    bool paused = false;

public:
    std::string getSystemName() const override
    {
        return "Alien Formation System";
    }

    void init() override
    {
        registerGroup<PositionComponent, Alien>();
    }

    void onEvent(const TickEvent& event) override
    {
        deltaTime += event.tick;
    }

    void onEvent(const GamePaused&) override
    {
        paused = true;
    }

    void onEvent(const GameResume&) override
    {
        paused = false;
    }

    void execute() override {
        if (deltaTime == 0.0f)
            return;

        auto dt = deltaTime;

        deltaTime = 0.0f;

        if (paused)
            return;

        for (auto formation : view<AlienFormation>())
        {
            formation->moveTimer += dt;

            if (formation->moveTimer >= formation->moveInterval) {
                formation->moveTimer = 0;
                moveFormation(formation);

                // Speed scaling - but less aggressive
                int aliensLeft = countAliveAliens();
                if (aliensLeft < formation->totalAliens) {
                    // Gentler curve - aliens don't become cocaine-fueled
                    float speedRatio = (float)aliensLeft / (float)formation->totalAliens;
                    // Inverse curve: fewer aliens = faster, but not crazy fast
                    formation->moveInterval = 800.0f + (speedRatio * 700.0f);  // Range: 800ms to 1500ms
                    formation->moveInterval = std::max(400.0f, formation->moveInterval); // Never faster than 400ms
                }
            }
        }


    }

private:
    void moveFormation(AlienFormation* formation) {
        bool hitEdge = false;
        bool atDangerZone = false;

        // Check if we're at the danger zone
        for (auto alienEntity : viewGroup<PositionComponent, Alien>()) {
            auto pos = alienEntity->get<PositionComponent>();
            if (pos->y >= DANGER_ZONE_Y) {
                atDangerZone = true;
                break;
            }
        }

        // If at danger zone, only move horizontally
        if (!atDangerZone) {
            // Normal movement checks
            for (auto alienEntity : viewGroup<PositionComponent, Alien>()) {
                auto pos = alienEntity->get<PositionComponent>();

                float nextX = pos->x + (formation->direction * FORMATION_STEP);
                if (nextX <= 20 || nextX + pos->width >= SCREEN_WIDTH - 20) {
                    hitEdge = true;
                    break;
                }
            }
        } else {
            // At danger zone - still check horizontal edges but no dropping
            for (auto alienEntity : viewGroup<PositionComponent, Alien>()) {
                auto pos = alienEntity->get<PositionComponent>();

                float nextX = pos->x + (formation->direction * FORMATION_STEP);
                if (nextX <= 20 || nextX + pos->width >= SCREEN_WIDTH - 20) {
                    hitEdge = true;
                    break;
                }
            }
        }

        // Move all aliens
        for (auto alienEntity : viewGroup<PositionComponent, Alien>()) {
            auto pos = alienEntity->get<PositionComponent>();

            if (hitEdge && !atDangerZone) {
                // Drop down - but slower than before
                float dropAmount = formation->dropDistance * 0.6f;  // 60% of original drop
                pos->setY(std::min(pos->y + dropAmount, DANGER_ZONE_Y + 30.0f)); // Cap at danger zone
            } else if (!hitEdge) {
                // Move horizontally
                pos->setX(pos->x + formation->direction * FORMATION_STEP);
            }
        }

        // Reverse direction after hitting edge
        if (hitEdge) {
            formation->direction *= -1;
        }
    }

    int countAliveAliens() {
        int count = 0;
        for (auto entity : viewGroup<PositionComponent, Alien>()) {
            count++;
        }
        return count;
    }
};

// More Aggressive Shooting When in Danger Zone
// ---------------------------------------------
class AlienShootingSystem : public System<InitSys, Own<AlienBullet>,
    Listener<TickEvent>, Listener<GamePaused>, Listener<GameResume>>
{
private:
    float shootTimer = 0.0f;
    float baseShootInterval = 2000.0f;
    float currentShootInterval = 2000.0f;
    std::mt19937 rng{std::random_device{}()};
    const float DANGER_ZONE_Y = 420.0f;

    bool paused = false;

public:
    std::string getSystemName() const override { return "Alien Shooting System"; }

    void init() override
    {
        registerGroup<PositionComponent, Alien>();
    }

    void onEvent(const TickEvent& event) override
    {
        shootTimer += event.tick;
    }

    void onEvent(const GamePaused&) override
    {
        paused = true;
    }

    void onEvent(const GameResume&) override
    {
        paused = false;
    }

    void execute() override {
        if (shootTimer < currentShootInterval) return;

        shootTimer = 0;

        if (paused)
            return;

        // Check if any aliens are in danger zone
        bool inDangerZone = false;
        for (auto alienEntity : viewGroup<PositionComponent, Alien>()) {
            auto pos = alienEntity->get<PositionComponent>();
            if (pos->y >= DANGER_ZONE_Y) {
                inDangerZone = true;
                break;
            }
        }

        // THE FIX: Store the actual entity wrapper, not raw pointers
        // Use the same type throughout - no mixing!
        std::map<int, std::pair<float, float>> frontRowPositions;  // col -> (x, y)

        for (auto alienEntity : viewGroup<PositionComponent, Alien>()) {
            auto alien = alienEntity->get<Alien>();
            auto pos = alienEntity->get<PositionComponent>();

            // Track front row by position, not entity reference
            if (frontRowPositions.find(alien->col) == frontRowPositions.end() ||
                frontRowPositions[alien->col].second < pos->y) {
                // Store position data, not entity references
                frontRowPositions[alien->col] = {
                    pos->x + pos->width/2,  // Center X for bullet spawn
                    pos->y + pos->height     // Bottom Y for bullet spawn
                };
            }
        }

        // Now work with positions, not entities
        if (!frontRowPositions.empty()) {
            std::vector<std::pair<float, float>> shootPositions;
            for (auto& [col, pos] : frontRowPositions) {
                shootPositions.push_back(pos);
            }

            // Shuffle the positions for random selection
            std::shuffle(shootPositions.begin(), shootPositions.end(), rng);

            // Fire bullets from selected positions
            int shooterCount = inDangerZone ?
                std::min(4, (int)shootPositions.size()) :
                std::min(2, (int)shootPositions.size());

            for (int i = 0; i < shooterCount; i++) {
                spawnBullet(shootPositions[i].first, shootPositions[i].second);
            }
        }

        // Adjust fire rate based on danger zone
        if (inDangerZone) {
            std::uniform_real_distribution<> intervalDis(800.0f, 1500.0f);
            currentShootInterval = intervalDis(rng);
        } else {
            std::uniform_real_distribution<> intervalDis(1500.0f, 3000.0f);
            currentShootInterval = intervalDis(rng);
        }
    }

private:
    void spawnBullet(float x, float y) {
        auto bullet = makeSimple2DShape(ecsRef, Shape2D::Square, 4, 12);
        auto pos = bullet.get<PositionComponent>();

        pos->setX(x - 2);  // Center the 4px wide bullet
        pos->setY(y);

        auto vel = bullet.attach<Velocity>();
        vel->dy = 250.0f;

        bullet.attach<AlienBullet>();

        if (auto shape = bullet.get<Simple2DObject>()) {
            shape->setColors({255, 255, 100, 255});
        }
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

                    ecsRef->sendEvent(AlienDestroyedEvent {
                        alienPos->x + alienPos->width / 2.0f,
                        alienPos->y + alienPos->height / 2.0f,
                        alien->points} );

                    // Bounce ball
                    ballVel->dy = -ballVel->dy;

                    // Update score
                    for (auto scoreEntity : viewGroup<GameScore>())
                    {
                        auto score = scoreEntity->get<GameScore>();
                        score->score += alien->points * score->scoreMultiplier;
                        score->aliensRemaining--;
                        printf("Score: %d | Aliens left: %d\n", score->score, score->aliensRemaining);

                        if (score->aliensRemaining <= 0)
                        {
                            ecsRef->sendEvent(GameEnd{true});
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

                    ecsRef->sendEvent(PlayerHitEvent{});

                    // Lose life
                    for (auto score : view<GameScore>())
                    {
                        score->lives--;
                        printf("Hit! Lives: %d\n", score->lives);

                        if (score->lives <= 0)
                        {
                            ecsRef->sendEvent(GameEnd{false});
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
class BulletPhysicsSystem : public System<InitSys, Listener<TickEvent>, Listener<GamePaused>, Listener<GameResume>>
{
private:
    float deltaTime = 0.0f;

    bool paused = false;

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

    void onEvent(const GamePaused&) override
    {
        paused = true;
    }

    void onEvent(const GameResume&) override
    {
        paused = false;
    }

    void execute() override
    {
        if (deltaTime == 0.0f)
            return;

        float dt = deltaTime / 1000.0f;

        deltaTime = 0.0f;

        if (paused)
            return;

        for (auto bulletEntity : viewGroup<PositionComponent, AlienBullet, Velocity>()) {
            auto pos = bulletEntity->get<PositionComponent>();
            auto vel = bulletEntity->get<Velocity>();

            pos->setY(pos->y + vel->dy * dt);
        }
    }
};

class DangerZoneVisualSystem : public System<InitSys>
{
private:
    const float DANGER_ZONE_Y = 420.0f;
    bool wasInDangerZone = false;

public:
    std::string getSystemName() const override { return "Danger Zone Visual System"; }

    void init() override {
        registerGroup<PositionComponent, Alien, Simple2DObject>();
    }

    void execute() override {
        bool inDangerZone = false;

        // Check if any alien is in danger zone
        for (auto alienEntity : viewGroup<PositionComponent, Alien, Simple2DObject>())
        {
            auto pos = alienEntity->get<PositionComponent>();

            if (pos->y >= DANGER_ZONE_Y)
            {
                inDangerZone = true;

                // Make aliens flash or pulse when in danger zone
                auto shape = alienEntity->get<Simple2DObject>();

                // Simple pulsing effect using time
                static float pulseTimer = 0;
                pulseTimer += 0.1f;

                // Preserve original color but add red tint
                float redBoost = std::sin(pulseTimer * 5.0f) * 30 + 30;
                auto alien = alienEntity->get<Alien>();

                // Recolor based on row but with danger tint
                switch(alien->row) {
                    case 0: shape->setColors({255.0f, 100.0f - redBoost, 100.0f - redBoost, 255.0f}); break;
                    case 1: shape->setColors({255.0f, 200.0f - redBoost, 100.0f - redBoost, 255.0f}); break;
                    case 2: shape->setColors({100.0f + redBoost, 255.0f, 100.0f, 255.0f}); break;
                    case 3: shape->setColors({100.0f + redBoost, 100.0f, 255.0f, 255.0f}); break;
                }
            }
        }

        // Log transition (once)
        if (inDangerZone && !wasInDangerZone) {
            printf("WARNING: Aliens entered danger zone! Increased aggression!\n");
        }
        wasInDangerZone = inDangerZone;
    }
};
