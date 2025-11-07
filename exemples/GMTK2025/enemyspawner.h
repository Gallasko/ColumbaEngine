#include <random>
#include <cmath>
#include <vector>
#include <iomanip>

#include "Systems/basicsystems.h"

#include "UI/ttftext.h"

#include "Systems/tween.h"

#include "2D/simple2dobject.h"

#include "ECS/entitysystem.h"

#include "pgconstant.h"

#include "enemy.h"
#include "pointaggregator.h"

namespace pg
{
    using Vector2D = constant::Vector2D;

    // Components that integrate with your existing system
    struct VelocityComponent : public Component {
        Vector2D velocity{0.0f, 0.0f};

        VelocityComponent() = default;
        VelocityComponent(float vx, float vy) : velocity(vx, vy) {}
        VelocityComponent(const Vector2D& vel) : velocity(vel) {}
    };

    struct EnemyMeta : public Component {
        Vector2D targetPoint{0.0f, 0.0f};   // Center target point
        float timeAlive{0.0f};

        EnemyMeta() = default;
        EnemyMeta(const Vector2D& target) : targetPoint(target) {}
    };

    // Helper struct for spawn data
    struct EnemySpawn {
        Vector2D start;
        Vector2D target;
        Vector2D direction;
        float speed;
        float totalDistance;
    };

    // Generalize this strat of creating a Dtor or a Ctor to be able to send event on entity deletion/creation
    struct CenterEnemyDeath : public Dtor
    {
        virtual void onDeletion(EntityRef entity)
        {
            entity->world()->sendEvent(StartGame{});
        }
    };

    // Random number generator - static to maintain state
    static std::random_device rd;
    static std::mt19937 gen(rd());

    EnemySpawn makeRandomEdgeToCenterSpawn(
        float screenW, float screenH,
        float edgePad,         // how far off-screen
        float centerWFrac,     // fraction (0–1) of screen width for center rect
        float centerHFrac,     // fraction (0–1) of screen height
        float speed            // constant speed for enemies
    )
    {
        EnemySpawn spawn;

        // Define the center rectangle bounds
        float centerW = screenW * centerWFrac;
        float centerH = screenH * centerHFrac;
        float centerLeft = (screenW - centerW) * 0.5f;
        float centerRight = centerLeft + centerW;
        float centerTop = (screenH - centerH) * 0.5f;
        float centerBottom = centerTop + centerH;

        // Choose random target point within center rectangle
        std::uniform_real_distribution<float> targetXDist(centerLeft, centerRight);
        std::uniform_real_distribution<float> targetYDist(centerTop, centerBottom);
        Vector2D target = Vector2D(targetXDist(gen), targetYDist(gen));

        // Choose random edge (0=left, 1=right, 2=top, 3=bottom)
        std::uniform_int_distribution<int> edgeDist(0, 3);
        int edge = edgeDist(gen);

        // Calculate spawn position based on chosen edge
        switch (edge) {
            case 0: // Left edge
                spawn.start.x = -edgePad;
                spawn.start.y = std::uniform_real_distribution<float>(0.0f, screenH)(gen);
                break;
            case 1: // Right edge
                spawn.start.x = screenW + edgePad;
                spawn.start.y = std::uniform_real_distribution<float>(0.0f, screenH)(gen);
                break;
            case 2: // Top edge
                spawn.start.x = std::uniform_real_distribution<float>(0.0f, screenW)(gen);
                spawn.start.y = -edgePad;
                break;
            case 3: // Bottom edge
                spawn.start.x = std::uniform_real_distribution<float>(0.0f, screenW)(gen);
                spawn.start.y = screenH + edgePad;
                break;
        }

        // Calculate direction vector toward center target (normalized)
        Vector2D displacement = target - spawn.start;
        float distance = displacement.length();
        spawn.direction = (distance > 0.0f) ? displacement.normalized() : Vector2D(0.0f, 0.0f);

        // Use constant speed
        spawn.speed = speed;
        spawn.totalDistance = distance; // Distance to center target

        return spawn;
    }

    // Event for changing spawn parameters during gameplay
    struct UpdateSpawnParamsEvent
    {
        float newSpawnInterval = -1.0f;     // -1 means don't change
        int newEnemiesPerSpawn = -1;        // -1 means don't change
        float newEnemySpeed = -1.0f;        // -1 means don't change
        float newCenterWFrac = -1.0f;       // -1 means don't change
        float newCenterHFrac = -1.0f;       // -1 means don't change
        int maxHp = -1;                     // -1 means don't change

        UpdateSpawnParamsEvent() = default;

        // Convenience constructors
        static UpdateSpawnParamsEvent setSpawnRate(float interval, int enemiesPerSpawn = -1) {
            UpdateSpawnParamsEvent event;
            event.newSpawnInterval = interval;
            event.newEnemiesPerSpawn = enemiesPerSpawn;
            return event;
        }

        static UpdateSpawnParamsEvent setDifficulty(float speed, int enemiesPerSpawn) {
            UpdateSpawnParamsEvent event;
            event.newEnemySpeed = speed;
            event.newEnemiesPerSpawn = enemiesPerSpawn;
            return event;
        }

        static UpdateSpawnParamsEvent setTargetArea(float centerWFrac, float centerHFrac) {
            UpdateSpawnParamsEvent event;
            event.newCenterWFrac = centerWFrac;
            event.newCenterHFrac = centerHFrac;
            return event;
        }
    };

    // Enhanced enemy spawning system that spawns enemies crossing the full screen
    struct EnemySpawnerSystem : public System<InitSys, QueuedListener<StartGame>, QueuedListener<TickEvent>, QueuedListener<UpdateSpawnParamsEvent>,
        QueuedListener<PauseGame>, QueuedListener<ResumeGame>, QueuedListener<RestartGame>>
    {
        std::string getSystemName() const override { return "Enemy Spawner System"; }

        // Screen dimensions
        float screenWidth = 820.0f;
        float screenHeight = 640.0f;

        // BALANCED SPAWN PARAMETERS - Toned down for gentler start
        float spawnInterval = 4.0f;          // seconds between spawn waves (increased from 3.0f)
        int enemiesPerSpawn = 3;             // number of enemies to spawn per wave (reduced from 6)
        float enemySpeed = 120.0f;           // pixels per second (reduced from 150.0f)
        float centerWFrac = 0.2f;            // target area width fraction (VERY SMALL - enemies go to center!)
        float centerHFrac = 0.2f;            // target area height fraction (VERY SMALL - enemies go to center!)
        float edgePadding = 100.0f;          // distance off-screen to spawn
        int maxHp = 1;                       // default HP for enemies

        // Runtime state
        float timeSinceLastSpawn = 0.0f;

        // Wave/difficulty progression
        int currentWave = 1;
        float waveTimer = 0.0f;
        float waveInterval = 3.0f;           // 3 seconds per wave

        // HP introduction system
        bool justIncreasedHP = false;        // flag to track when HP was just increased
        int hpIntroWaves = 0;                // counter for waves with only new HP enemies

        bool started = false;
        bool paused = false;                 // Pause state

        virtual void onProcessEvent(const PauseGame&) override
        {
            LOG_INFO("EnemySpawnerSystem", "Game Paused");
            paused = true;
        }

        virtual void onProcessEvent(const ResumeGame&) override
        {
            LOG_INFO("EnemySpawnerSystem", "Game Resumed");
            paused = false;
        }

        std::vector<constant::Vector4D> enemyColors = {
            {255.0f, 0.0f, 0.0f, 255.0f}, // 1hp = Red
            {0.0f, 255.0f, 0.0f, 255.0f}, // 2hp = Green
            {0.0f, 0.0f, 255.0f, 255.0f}, // 3hp = Blue
        };

        virtual void init() override
        {
            LOG_THIS_MEMBER("EnemySpawnerSystem");

            // Register groups for entities with velocity and enemy components
            registerGroup<PositionComponent, VelocityComponent, EnemyMeta, EnemyFlag>();

            LOG_INFO("EnemySpawnerSystem", "Balanced spawn system initialized - Interval: " << spawnInterval
                    << "s, Enemies per spawn: " << enemiesPerSpawn << ", Speed: " << enemySpeed << " px/s");

            spawnCenterEnemy();
        }

        virtual void onProcessEvent(const StartGame&) override
        {
            start();
        }

        void start()
        {
            started = true;
            timeSinceLastSpawn = spawnInterval;
        }

        virtual void onProcessEvent(const TickEvent& event) override
        {
            if (paused or not started)
                return; // Skip updates if paused

            float deltaTime = event.tick / 1000.0f; // Convert to seconds if needed

            // Update wave progression
            updateWaves(deltaTime);

            // Handle spawning - only if wave is ready to start
            timeSinceLastSpawn += deltaTime;
            if (timeSinceLastSpawn >= spawnInterval) {
                spawnEnemyWave();
                timeSinceLastSpawn = 0.0f;
            }

            // Handle enemy movement and cleanup
            updateEnemyMovement(deltaTime);
        }

        virtual void onProcessEvent(const RestartGame&) override
        {
            LOG_INFO("EnemySpawnerSystem", "Restarting enemy spawner");

            // Reset state
            resetGame();

            std::vector<_unique_id> enemyIds;
            // Clear existing enemies
            for (auto ent : viewGroup<PositionComponent, VelocityComponent, EnemyMeta, EnemyFlag>())
            {
                enemyIds.push_back(ent->entityId);
            }

            for (const auto& id : enemyIds)
            {
                ecsRef->removeEntity(id);
            }
        }

        virtual void onProcessEvent(const UpdateSpawnParamsEvent& event) override
        {
            bool changed = false;

            if (event.newSpawnInterval > 0.0f) {
                spawnInterval = event.newSpawnInterval;
                changed = true;
            }

            if (event.newEnemiesPerSpawn > 0) {
                enemiesPerSpawn = event.newEnemiesPerSpawn;
                changed = true;
            }

            if (event.newEnemySpeed > 0.0f) {
                enemySpeed = event.newEnemySpeed;
                changed = true;
            }

            if (event.newCenterWFrac > 0.0f && event.newCenterWFrac <= 1.0f) {
                centerWFrac = event.newCenterWFrac;
                changed = true;
            }

            if (event.newCenterHFrac > 0.0f && event.newCenterHFrac <= 1.0f) {
                centerHFrac = event.newCenterHFrac;
                changed = true;
            }

            if (changed) {
                LOG_INFO("EnemySpawnerSystem", "Updated spawn parameters - Interval: " << spawnInterval
                        << "s, Enemies per spawn: " << enemiesPerSpawn << ", Speed: " << enemySpeed << " px/s");
            }
        }

        void updateWaves(float deltaTime) {
            waveTimer += deltaTime;

            if (waveTimer >= waveInterval) {
                currentWave++;
                waveTimer = 0.0f;

                // Only spawn center enemy on the very first wave
                if (currentWave == 1)
                {
                    spawnCenterEnemy();
                    LOG_INFO("EnemySpawnerSystem", "Wave " << currentWave << " - Center enemy spawned! Kill it to start the wave!");
                }
                else
                {
                    // For all other waves, start immediately
                    LOG_INFO("EnemySpawnerSystem", "Wave " << currentWave << " started!");
                }

                // Increase difficulty each wave
                increaseDifficulty();
            }
        }

        // Improved curved difficulty with HP introduction system and expanding target area
        void increaseDifficulty() {
            // Use logarithmic scaling to slow down difficulty increases over time
            float difficultyFactor = 1.0f + std::log(currentWave) * 0.08f;

            // Check if we need to increase HP (every 12 waves instead of 8)
            int targetHp = 1 + ((currentWave - 1) / 12);
            targetHp = std::min(targetHp, 3);

            // HP introduction system
            if (targetHp > maxHp) {
                maxHp = targetHp;
                justIncreasedHP = true;
                hpIntroWaves = 0;
                LOG_INFO("EnemySpawnerSystem", "NEW ENEMY TYPE! HP increased to " << maxHp
                        << " - Next 2 waves will only spawn 1-2 " << maxHp << "-HP enemies");
            }

            // Handle HP introduction waves (2 waves with only the new HP enemies)
            if (justIncreasedHP && hpIntroWaves < 2) {
                hpIntroWaves++;
                if (hpIntroWaves >= 2) {
                    justIncreasedHP = false;
                    LOG_INFO("EnemySpawnerSystem", "HP introduction complete - normal mixed spawning resumed");
                }
            }

            // Enemy count follows a gentler stepped curve (starts at 3, caps at 8)
            int targetEnemies = std::min(3 + (currentWave / 5), 8);
            if (enemiesPerSpawn < targetEnemies) {
                enemiesPerSpawn = targetEnemies;
            }

            // Speed follows a more gradual logarithmic curve
            float targetSpeed = 120.0f + std::log(currentWave + 1) * 20.0f;
            enemySpeed = std::min(targetSpeed, 250.0f);

            // Spawn interval follows a gentler inverse logarithmic curve
            float targetInterval = 4.0f - (std::log(currentWave + 1) * 0.25f);
            spawnInterval = std::max(targetInterval, 1.5f);

            // GRADUALLY EXPAND TARGET AREA - Start focused, become more spread out
            float targetCenterWFrac = std::min(0.2f + (currentWave - 1) * 0.025f, 0.7f);
            float targetCenterHFrac = std::min(0.2f + (currentWave - 1) * 0.025f, 0.7f);

            centerWFrac = targetCenterWFrac;
            centerHFrac = targetCenterHFrac;

            LOG_INFO("EnemySpawnerSystem", "Curved difficulty - Wave: " << currentWave
                    << ", Factor: " << difficultyFactor
                    << ", Enemies: " << enemiesPerSpawn
                    << ", Speed: " << enemySpeed << " px/s"
                    << ", Interval: " << spawnInterval << "s"
                    << ", HP: " << maxHp
                    << ", Target: " << centerWFrac << "x" << centerHFrac
                    << (justIncreasedHP ? " (INTRO MODE)" : ""));
        }

        void spawnCenterEnemy() {
            // Spawn an enemy at the center of the screen
            Vector2D centerPos(screenWidth * 0.5f, screenHeight * 0.5f);

            // Determine HP for center enemy
            int hp = maxHp; // Center enemy always has max HP for current wave

            // Create center enemy entity
            auto shape = makeSimple2DShape(ecsRef, Shape2D::Square, 50.0f, 50.0f, enemyColors[hp - 1]);

            // Set position to screen center (accounting for 50x50 size)
            shape.get<PositionComponent>()->setX(centerPos.x - 25.0f);
            shape.get<PositionComponent>()->setY(centerPos.y - 25.0f);

            // Center enemy has no velocity (stationary)
            shape.attach<VelocityComponent>(Vector2D(0.0f, 0.0f));

            // Attach enemy metadata with same center target point
            shape.attach<EnemyMeta>(centerPos);

            // Attach enemy flag
            shape.attach<EnemyFlag>(hp);

            // Enable the game start once the center enemy dies
            shape.attachGeneric<CenterEnemyDeath>();

            LOG_INFO("EnemySpawnerSystem", "Center enemy spawned with " << hp << " HP at screen center");
        }

        void spawnEnemyWave() {
            // Special handling for HP introduction waves - spawn only 1-2 enemies
            int enemiesToSpawn = enemiesPerSpawn;

            if (justIncreasedHP && hpIntroWaves <= 2) {
                enemiesToSpawn = (hpIntroWaves == 1) ? 1 : 2;  // Wave 1: 1 enemy, Wave 2: 2 enemies
                LOG_INFO("EnemySpawnerSystem", "HP Introduction wave - spawning only " << enemiesToSpawn << " enemies");
            }

            for (int i = 0; i < enemiesToSpawn; ++i) {
                spawnSingleEnemy();
            }

            LOG_INFO("EnemySpawnerSystem", "Spawned wave of " << enemiesToSpawn << " enemies");
        }

        void spawnSingleEnemy() {
            // Generate spawn data for edge-to-center movement
            EnemySpawn spawnData = makeRandomEdgeToCenterSpawn(
                screenWidth, screenHeight,
                edgePadding,
                centerWFrac,
                centerHFrac,
                enemySpeed
            );

            int hp;

            // HP introduction system: spawn only new HP enemies for 2 waves after HP increase
            if (justIncreasedHP && hpIntroWaves < 2) {
                hp = maxHp;  // Only spawn enemies with the new HP level
                LOG_INFO("EnemySpawnerSystem", "Spawning intro enemy with " << hp << " HP (intro wave " << hpIntroWaves + 1 << "/2)");
            }
            else {
                // Normal spawning: random HP between 1 and maxHp
                std::uniform_int_distribution<int> hpGen(1, maxHp);
                hp = hpGen(gen);
            }

            // Create enemy entity using your existing shape creation
            auto shape = makeSimple2DShape(ecsRef, Shape2D::Square, 50.0f, 50.0f, enemyColors[hp - 1]);

            // Set position to spawn point
            shape.get<PositionComponent>()->setX(spawnData.start.x - 25.0f); // Center the 50x50 square
            shape.get<PositionComponent>()->setY(spawnData.start.y - 25.0f);

            // Attach velocity component
            Vector2D velocity = spawnData.direction * spawnData.speed;
            shape.attach<VelocityComponent>(velocity);

            // Attach enemy metadata with target point
            shape.attach<EnemyMeta>(spawnData.target);

            // Attach enemy flag
            shape.attach<EnemyFlag>(hp);
        }

        void updateEnemyMovement(float deltaTime)
        {
            std::vector<_unique_id> enemiesToDestroy;

            auto loop = ecsRef->getSystem<PointAggregator>()->mousePosList;

            bool loopHit = false;

            // Process all enemies with required components
            for (const auto& ent : viewGroup<PositionComponent, VelocityComponent, EnemyMeta, EnemyFlag>()) {
                auto pos = ent->get<PositionComponent>();
                auto vel = ent->get<VelocityComponent>();
                auto meta = ent->get<EnemyMeta>();

                if (!pos || !vel || !meta) continue;

                Point2D beginPos(pos->x + 25.0f, pos->y + 25.0f);
                Point2D endPos(pos->x + 25.0f + vel->velocity.x * deltaTime, pos->y + 25.0f + vel->velocity.y * deltaTime);

                if (checkSegmentIntersectWithLoop({beginPos, endPos}, loop))
                {
                    loopHit = true;
                    enemiesToDestroy.push_back(ent->entityId);
                    LOG_INFO("EnemySpawnerSystem", "Enemy hit the loop after " << meta->timeAlive << "s");
                    continue; // Skip further processing if hit
                }

                // Update position by velocity * deltaTime (center enemies have 0 velocity)
                pos->setX(pos->x + vel->velocity.x * deltaTime);
                pos->setY(pos->y + vel->velocity.y * deltaTime);

                // Update time alive
                meta->timeAlive += deltaTime;

                // Get current position (center of the enemy)
                Vector2D currentPos(pos->x + 25.0f, pos->y + 25.0f);

                // Check if enemy has moved off screen (out of bounds)
                bool offScreen = false;

                // Check if enemy is beyond screen boundaries (including padding)
                if (currentPos.x < -edgePadding || currentPos.x > screenWidth + edgePadding ||
                    currentPos.y < -edgePadding || currentPos.y > screenHeight + edgePadding) {
                    offScreen = true;
                }

                if (offScreen) {
                    // Mark for destruction - enemy went off screen
                    enemiesToDestroy.push_back(ent->entityId);
                    LOG_INFO("EnemySpawnerSystem", "Enemy went off-screen after " << meta->timeAlive << "s");
                }
            }

            // Destroy enemies that have left the screen or been hit
            for (auto entityId : enemiesToDestroy) {
                ecsRef->removeEntity(entityId);
            }

            if (loopHit)
            {
                ecsRef->sendEvent(EnemyLoopHitEvent{});
                ecsRef->sendEvent(ShakeMainCamera{});
            }
        }

        // Public methods to control the spawner
        void setSpawnInterval(float interval) {
            spawnInterval = std::max(interval, 0.1f); // Minimum 0.1 seconds
        }

        void setEnemiesPerSpawn(int count) {
            enemiesPerSpawn = std::max(count, 1); // Minimum 1 enemy
        }

        void setEnemySpeed(float speed) {
            enemySpeed = std::max(speed, 10.0f); // Minimum 10 px/s
        }

        void setTargetArea(float wFrac, float hFrac) {
            centerWFrac = std::clamp(wFrac, 0.1f, 1.0f);
            centerHFrac = std::clamp(hFrac, 0.1f, 1.0f);
        }

        void setScreenSize(float width, float height) {
            screenWidth = width;
            screenHeight = height;
        }

        // Getters for UI/debugging
        int getCurrentWave() const { return currentWave; }
        float getSpawnInterval() const { return spawnInterval; }
        int getEnemiesPerSpawn() const { return enemiesPerSpawn; }
        float getEnemySpeed() const { return enemySpeed; }
        float getCenterWFrac() const { return centerWFrac; }
        float getCenterHFrac() const { return centerHFrac; }
        int getMaxHp() const { return maxHp; }
        bool isInIntroMode() const { return justIncreasedHP; }
        int getIntroWavesRemaining() const { return justIncreasedHP ? (2 - hpIntroWaves) : 0; }

        // Reset game state
        void resetGame() {
            currentWave = 1;
            waveTimer = 0.0f;
            timeSinceLastSpawn = 0.0f;

            // Reset to balanced initial parameters
            spawnInterval = 4.0f;
            enemiesPerSpawn = 3;
            enemySpeed = 120.0f;
            centerWFrac = 0.2f;            // Start very focused!
            centerHFrac = 0.2f;            // Start very focused!
            maxHp = 1;

            // Reset HP introduction system
            justIncreasedHP = false;
            hpIntroWaves = 0;

            started = false;
            spawnCenterEnemy();

            LOG_INFO("EnemySpawnerSystem", "Game reset to balanced initial parameters");
        }

        // Get count of active enemies (for debugging/UI)
        int getActiveEnemyCount() const {
            int count = 0;
            for (const auto& _ : viewGroup<EnemyFlag>())
            {
                count++;
            }

            return count;
        }
    };

}