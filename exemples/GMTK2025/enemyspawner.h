
#include <random>
#include <cmath>
#include <vector>

#include "Systems/basicsystems.h"

#include "UI/ttftext.h"

#include "Systems/tween.h"

#include "2D/simple2dobject.h"

#include "ECS/entitysystem.h"

#include "constant.h"

namespace pg
{
    using Vector2D = constant::Vector2D;

    struct EnemyFlag : public Component {};

    // Components that integrate with your existing system
    struct VelocityComponent : public Component {
        Vector2D velocity{0.0f, 0.0f};
        
        VelocityComponent() = default;
        VelocityComponent(float vx, float vy) : velocity(vx, vy) {}
        VelocityComponent(const Vector2D& vel) : velocity(vel) {}
    };

    struct EnemyMeta : public Component {
        Vector2D targetPoint{0.0f, 0.0f};
        float crossTime{4.0f};
        float timeAlive{0.0f};
        
        EnemyMeta() = default;
        EnemyMeta(const Vector2D& target, float time) 
            : targetPoint(target), crossTime(time) {}
    };

    // Helper struct for spawn data
    struct EnemySpawn {
        Vector2D start;
        Vector2D target;
        Vector2D direction;
        float speed;
    };

    // Random number generator - static to maintain state
    static std::random_device rd;
    static std::mt19937 gen(rd());

    EnemySpawn makeRandomEdgeToCenterSpawn(
        float screenW, float screenH,
        float edgePad,         // how far off-screen
        float centerWFrac,     // fraction (0–1) of screen width for center rect
        float centerHFrac,     // fraction (0–1) of screen height
        float crossTime        // seconds to reach target
    ) {
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
        spawn.target = Vector2D(targetXDist(gen), targetYDist(gen));
        
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
        
        // Calculate direction vector (normalized)
        Vector2D displacement = spawn.target - spawn.start;
        float distance = displacement.length();
        spawn.direction = (distance > 0.0f) ? displacement.normalized() : Vector2D(0.0f, 0.0f);
        
        // Calculate speed needed to reach target in crossTime seconds
        spawn.speed = (crossTime > 0.0f && distance > 0.0f) ? distance / crossTime : 0.0f;
        
        return spawn;
    }

    // Event for changing spawn parameters during gameplay
    struct UpdateSpawnParamsEvent {
        float newSpawnInterval = -1.0f;     // -1 means don't change
        int newEnemiesPerSpawn = -1;        // -1 means don't change
        float newCrossTime = -1.0f;         // -1 means don't change
        float newCenterWFrac = -1.0f;       // -1 means don't change
        float newCenterHFrac = -1.0f;       // -1 means don't change
        
        UpdateSpawnParamsEvent() = default;
        
        // Convenience constructors
        static UpdateSpawnParamsEvent setSpawnRate(float interval, int enemiesPerSpawn = -1) {
            UpdateSpawnParamsEvent event;
            event.newSpawnInterval = interval;
            event.newEnemiesPerSpawn = enemiesPerSpawn;
            return event;
        }
        
        static UpdateSpawnParamsEvent setDifficulty(float crossTime, int enemiesPerSpawn) {
            UpdateSpawnParamsEvent event;
            event.newCrossTime = crossTime;
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

    // Enhanced enemy spawning system with dynamic parameters
    struct EnemySpawnerSystem : public System<InitSys, Listener<TickEvent>, Listener<UpdateSpawnParamsEvent>>
    {
        // Screen dimensions
        float screenWidth = 820.0f;
        float screenHeight = 640.0f;
        
        // Spawn parameters - can be changed during gameplay
        float spawnInterval = 3.0f;          // seconds between spawn waves
        int enemiesPerSpawn = 1;             // number of enemies to spawn per wave
        float crossTime = 4.0f;              // time for enemies to reach target
        float centerWFrac = 0.5f;            // target area width fraction
        float centerHFrac = 0.5f;            // target area height fraction
        float edgePadding = 100.0f;          // distance off-screen to spawn
        
        // Runtime state
        float timeSinceLastSpawn = 0.0f;
        int playerHealth = 100;
        
        // Wave/difficulty progression
        int currentWave = 1;
        float waveTimer = 0.0f;
        float waveInterval = 30.0f;          // 30 seconds per wave
        
        virtual void init() override
        {
            LOG_THIS_MEMBER("EnemySpawnerSystem");
            
            // Register groups for entities with velocity and enemy components
            registerGroup<PositionComponent, VelocityComponent, EnemyMeta, EnemyFlag>();
            
            LOG_INFO("EnemySpawnerSystem", "Initial spawn parameters - Interval: " << spawnInterval 
                    << "s, Enemies per spawn: " << enemiesPerSpawn << ", Cross time: " << crossTime << "s");
        }
        
        virtual void onEvent(const TickEvent& event) override
        {
            float deltaTime = event.tick / 1000.0f; // Convert to seconds if needed
            
            // Update wave progression
            updateWaves(deltaTime);
            
            // Handle spawning
            timeSinceLastSpawn += deltaTime;
            if (timeSinceLastSpawn >= spawnInterval) {
                spawnEnemyWave();
                timeSinceLastSpawn = 0.0f;
            }
            
            // Handle enemy movement and collision detection
            updateEnemyMovement(deltaTime);
        }
        
        virtual void onEvent(const UpdateSpawnParamsEvent& event) override
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
            
            if (event.newCrossTime > 0.0f) {
                crossTime = event.newCrossTime;
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
                        << "s, Enemies per spawn: " << enemiesPerSpawn << ", Cross time: " << crossTime << "s");
            }
        }
        
        void updateWaves(float deltaTime) {
            waveTimer += deltaTime;
            
            if (waveTimer >= waveInterval) {
                currentWave++;
                waveTimer = 0.0f;
                
                // Increase difficulty each wave
                increaseDifficulty();
                
                LOG_INFO("EnemySpawnerSystem", "Wave " << currentWave << " started!");
            }
        }
        
        void increaseDifficulty() {
            // Each wave: slightly more enemies, slightly faster crossing
            if (currentWave % 2 == 0) {
                enemiesPerSpawn = std::min(enemiesPerSpawn + 1, 8); // Cap at 8 enemies per spawn
            }
            
            if (currentWave % 3 == 0) {
                crossTime = std::max(crossTime - 0.2f, 1.5f); // Minimum 1.5 seconds to cross
            }
            
            if (currentWave % 4 == 0) {
                spawnInterval = std::max(spawnInterval - 0.3f, 0.8f); // Minimum 0.8 seconds between spawns
            }
            
            LOG_INFO("EnemySpawnerSystem", "Difficulty increased - Wave: " << currentWave 
                    << ", Enemies: " << enemiesPerSpawn << ", Cross time: " << crossTime 
                    << ", Spawn interval: " << spawnInterval);
        }
        
        void spawnEnemyWave() {
            for (int i = 0; i < enemiesPerSpawn; ++i) {
                spawnSingleEnemy();
            }
            
            LOG_INFO("EnemySpawnerSystem", "Spawned wave of " << enemiesPerSpawn << " enemies");
        }
        
        void spawnSingleEnemy() {
            // Generate spawn data
            EnemySpawn spawnData = makeRandomEdgeToCenterSpawn(
                screenWidth, screenHeight,
                edgePadding,
                centerWFrac,
                centerHFrac,
                crossTime
            );
            
            // Create enemy entity using your existing shape creation
            auto shape = makeSimple2DShape(ecsRef, Shape2D::Square, 50.0f, 50.0f, {255.0f, 0.0f, 0.0f, 255.0f});
            
            // Set position to spawn point
            shape.get<PositionComponent>()->setX(spawnData.start.x - 25.0f); // Center the 50x50 square
            shape.get<PositionComponent>()->setY(spawnData.start.y - 25.0f);
            
            // Attach velocity component
            Vector2D velocity = spawnData.direction * spawnData.speed;
            shape.attach<VelocityComponent>(velocity);
            
            // Attach enemy metadata
            shape.attach<EnemyMeta>(spawnData.target, crossTime);
            
            // Attach enemy flag
            shape.attach<EnemyFlag>();
        }
        
        void updateEnemyMovement(float deltaTime) {
            std::vector<_unique_id> enemiesToDestroy;
            
            // Process all enemies with required components
            for (const auto& ent : viewGroup<PositionComponent, VelocityComponent, EnemyMeta, EnemyFlag>()) {
                auto pos = ent->get<PositionComponent>();
                auto vel = ent->get<VelocityComponent>();
                auto meta = ent->get<EnemyMeta>();
                
                if (!pos || !vel || !meta) continue;
                
                // Update position by velocity * deltaTime
                pos->setX(pos->x + vel->velocity.x * deltaTime);
                pos->setY(pos->y + vel->velocity.y * deltaTime);
                
                // Update time alive
                meta->timeAlive += deltaTime;
                
                // Get current position (center of the enemy)
                Vector2D currentPos(pos->x + 25.0f, pos->y + 25.0f);
                
                // Check if enemy has reached or passed its target
                Vector2D toTarget = meta->targetPoint - currentPos;
                float distanceToTarget = toTarget.length();
                
                // If very close to target OR if it has been alive longer than expected cross time
                if (distanceToTarget < 25.0f || meta->timeAlive >= meta->crossTime) {
                    // Deduct player health
                    if (playerHealth > 0) {
                        playerHealth--;
                        LOG_INFO("EnemySpawnerSystem", "Enemy reached target! Player health: " << playerHealth);
                        
                        // Game over check
                        if (playerHealth <= 0) {
                            LOG_INFO("EnemySpawnerSystem", "GAME OVER! Player health reached 0");
                            // You could trigger a game over event here
                        }
                    }
                    
                    // Mark for destruction
                    enemiesToDestroy.push_back(ent->entityId);
                }
            }
            
            // Destroy enemies that reached their targets
            for (auto entityId : enemiesToDestroy) {
                ecsRef->removeEntity(entityId);
            }
        }
        
        // Public methods to control the spawner
        void setSpawnInterval(float interval) {
            spawnInterval = std::max(interval, 0.1f); // Minimum 0.1 seconds
        }
        
        void setEnemiesPerSpawn(int count) {
            enemiesPerSpawn = std::max(count, 1); // Minimum 1 enemy
        }
        
        void setCrossTime(float time) {
            crossTime = std::max(time, 0.5f); // Minimum 0.5 seconds
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
        int getPlayerHealth() const { return playerHealth; }
        void setPlayerHealth(int health) { playerHealth = health; }
        int getCurrentWave() const { return currentWave; }
        float getSpawnInterval() const { return spawnInterval; }
        int getEnemiesPerSpawn() const { return enemiesPerSpawn; }
        float getCrossTime() const { return crossTime; }
        
        // Reset game state
        void resetGame() {
            currentWave = 1;
            waveTimer = 0.0f;
            timeSinceLastSpawn = 0.0f;
            playerHealth = 100;
            spawnInterval = 3.0f;
            enemiesPerSpawn = 1;
            crossTime = 4.0f;
            centerWFrac = 0.5f;
            centerHFrac = 0.5f;
            
            LOG_INFO("EnemySpawnerSystem", "Game reset to initial parameters");
        }
    };

}