
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
        Vector2D exitPoint{0.0f, 0.0f};     // Point where enemy exits screen
        float crossTime{4.0f};               // Time to cross entire screen
        float timeAlive{0.0f};
        Vector2D spawnPoint{0.0f, 0.0f};     // Where enemy started
        
        EnemyMeta() = default;
        EnemyMeta(const Vector2D& spawn, const Vector2D& exit, float time) 
            : spawnPoint(spawn), exitPoint(exit), crossTime(time) {}
    };

    // Helper struct for spawn data
    struct EnemySpawn {
        Vector2D start;
        Vector2D exit;
        Vector2D direction;
        float speed;
        float totalDistance;
    };

    // Random number generator - static to maintain state
    static std::random_device rd;
    static std::mt19937 gen(rd());

    EnemySpawn makeRandomCrossScreenSpawn(
        float screenW, float screenH,
        float edgePad,         // how far off-screen to spawn/exit
        float crossTime        // seconds to cross entire screen
    ) {
        EnemySpawn spawn;
        
        // Choose random edge to spawn from (0=left, 1=right, 2=top, 3=bottom)
        std::uniform_int_distribution<int> edgeDist(0, 3);
        int spawnEdge = edgeDist(gen);
        
        // Calculate spawn position and corresponding exit point
        switch (spawnEdge) {
            case 0: // Spawn from left edge
                spawn.start.x = -edgePad;
                spawn.start.y = std::uniform_real_distribution<float>(0.0f, screenH)(gen);
                // Exit on right edge at roughly the same height (with some variation)
                spawn.exit.x = screenW + edgePad;
                spawn.exit.y = spawn.start.y + std::uniform_real_distribution<float>(-screenH * 0.3f, screenH * 0.3f)(gen);
                spawn.exit.y = std::clamp(spawn.exit.y, 0.0f, screenH);
                break;
                
            case 1: // Spawn from right edge
                spawn.start.x = screenW + edgePad;
                spawn.start.y = std::uniform_real_distribution<float>(0.0f, screenH)(gen);
                // Exit on left edge
                spawn.exit.x = -edgePad;
                spawn.exit.y = spawn.start.y + std::uniform_real_distribution<float>(-screenH * 0.3f, screenH * 0.3f)(gen);
                spawn.exit.y = std::clamp(spawn.exit.y, 0.0f, screenH);
                break;
                
            case 2: // Spawn from top edge
                spawn.start.x = std::uniform_real_distribution<float>(0.0f, screenW)(gen);
                spawn.start.y = -edgePad;
                // Exit on bottom edge
                spawn.exit.x = spawn.start.x + std::uniform_real_distribution<float>(-screenW * 0.3f, screenW * 0.3f)(gen);
                spawn.exit.x = std::clamp(spawn.exit.x, 0.0f, screenW);
                spawn.exit.y = screenH + edgePad;
                break;
                
            case 3: // Spawn from bottom edge
                spawn.start.x = std::uniform_real_distribution<float>(0.0f, screenW)(gen);
                spawn.start.y = screenH + edgePad;
                // Exit on top edge
                spawn.exit.x = spawn.start.x + std::uniform_real_distribution<float>(-screenW * 0.3f, screenW * 0.3f)(gen);
                spawn.exit.x = std::clamp(spawn.exit.x, 0.0f, screenW);
                spawn.exit.y = -edgePad;
                break;
        }
        
        // Calculate direction vector (normalized)
        Vector2D displacement = spawn.exit - spawn.start;
        spawn.totalDistance = displacement.length();
        spawn.direction = (spawn.totalDistance > 0.0f) ? displacement.normalized() : Vector2D(0.0f, 0.0f);
        
        // Calculate speed needed to traverse the full distance in crossTime seconds
        spawn.speed = (crossTime > 0.0f && spawn.totalDistance > 0.0f) ? spawn.totalDistance / crossTime : 0.0f;
        
        return spawn;
    }

    // Event for changing spawn parameters during gameplay
    struct UpdateSpawnParamsEvent {
        float newSpawnInterval = -1.0f;     // -1 means don't change
        int newEnemiesPerSpawn = -1;        // -1 means don't change
        float newCrossTime = -1.0f;         // -1 means don't change
        
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
    };

    // Enhanced enemy spawning system that spawns enemies crossing the full screen
    struct EnemySpawnerSystem : public System<InitSys, Listener<TickEvent>, Listener<UpdateSpawnParamsEvent>>
    {
        // Screen dimensions
        float screenWidth = 820.0f;
        float screenHeight = 640.0f;
        
        // Spawn parameters - can be changed during gameplay
        float spawnInterval = 3.0f;          // seconds between spawn waves
        int enemiesPerSpawn = 6;             // number of enemies to spawn per wave
        float crossTime = 12.0f;              // time for enemies to cross entire screen
        float edgePadding = 100.0f;          // distance off-screen to spawn/exit
        
        // Runtime state
        float timeSinceLastSpawn = 0.0f;
        
        // Wave/difficulty progression
        int currentWave = 1;
        float waveTimer = 0.0f;
        float waveInterval = 30.0f;          // 30 seconds per wave
        
        virtual void init() override
        {
            LOG_THIS_MEMBER("EnemySpawnerSystem");
            
            // Register groups for entities with velocity and enemy components
            registerGroup<PositionComponent, VelocityComponent, EnemyMeta, EnemyFlag>();
            
            LOG_INFO("EnemySpawnerSystem", "Cross-screen spawn system initialized - Interval: " << spawnInterval 
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
            
            // Handle enemy movement and cleanup
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
                crossTime = std::max(crossTime - 0.3f, 2.0f); // Minimum 2.0 seconds to cross
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
            // Generate spawn data for cross-screen movement
            EnemySpawn spawnData = makeRandomCrossScreenSpawn(
                screenWidth, screenHeight,
                edgePadding,
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
            
            // Attach enemy metadata with exit point instead of target
            shape.attach<EnemyMeta>(spawnData.start, spawnData.exit, crossTime);
            
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
                
                // Check if enemy has moved off screen (reached exit area)
                bool offScreen = false;
                
                // Check if enemy is well beyond screen boundaries
                if (currentPos.x < -edgePadding * 1.5f || currentPos.x > screenWidth + edgePadding * 1.5f ||
                    currentPos.y < -edgePadding * 1.5f || currentPos.y > screenHeight + edgePadding * 1.5f) {
                    offScreen = true;
                }
                
                // Also check if it's been alive longer than expected (safety cleanup)
                if (meta->timeAlive >= meta->crossTime * 1.5f) {
                    offScreen = true;
                }
                
                if (offScreen) {
                    // Mark for destruction - enemy successfully crossed screen
                    enemiesToDestroy.push_back(ent->entityId);
                    LOG_INFO("EnemySpawnerSystem", "Enemy exited screen after " << meta->timeAlive << "s");
                }
            }
            
            // Destroy enemies that have left the screen
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
            crossTime = std::max(time, 1.0f); // Minimum 1.0 seconds
        }
        
        void setScreenSize(float width, float height) {
            screenWidth = width;
            screenHeight = height;
        }
        
        // Getters for UI/debugging
        int getCurrentWave() const { return currentWave; }
        float getSpawnInterval() const { return spawnInterval; }
        int getEnemiesPerSpawn() const { return enemiesPerSpawn; }
        float getCrossTime() const { return crossTime; }
        
        // Reset game state
        void resetGame() {
            currentWave = 1;
            waveTimer = 0.0f;
            timeSinceLastSpawn = 0.0f;
            spawnInterval = 3.0f;
            enemiesPerSpawn = 1;
            crossTime = 4.0f;
            
            LOG_INFO("EnemySpawnerSystem", "Game reset to initial parameters");
        }
        
        // Get count of active enemies (for debugging/UI)
        int getActiveEnemyCount() const {
            int count = 0;
            for (const auto& ent : viewGroup<EnemyFlag>()) {
                count++;
            }
            return count;
        }
    };


}