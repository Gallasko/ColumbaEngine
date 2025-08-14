#pragma once

#include "components.h"
#include "events.h"

#include "2D/simple2dobject.h"
#include "Systems/basicsystems.h"

using namespace pg;

class PowerUpSystem : public System<InitSys, Listener<TickEvent>, Listener<AlienDestroyedEvent>>
{
private:
    // Drop rates for each power-up type (must sum to less than 1.0)
    std::map<PowerUpType, float> dropRates = {
        {PowerUpType::HEALTH,      0.05f},  // 5% - rare, valuable
        {PowerUpType::MULTIBALL,   0.08f},  // 8% - chaos maker
        {PowerUpType::FASTBALL,    0.06f},  // 6% - risk/reward
        {PowerUpType::BARRIER,     0.04f},  // 4% - very rare, powerful
        {PowerUpType::WIDE_PADDLE, 0.10f},  // 10% - common, helpful
        {PowerUpType::TINY_PADDLE, 0.03f}   // 3% - rare, expert mode
    };
    
    // Visual properties for each power-up
    std::map<PowerUpType, constant::Vector4D> powerUpColors = {
        {PowerUpType::HEALTH,      {255, 100, 100, 255}},  // Red
        {PowerUpType::MULTIBALL,   {255, 100, 255, 255}},  // Purple
        {PowerUpType::FASTBALL,    {255, 200, 100, 255}},  // Orange
        {PowerUpType::BARRIER,     {100, 255, 255, 255}},  // Cyan
        {PowerUpType::WIDE_PADDLE, {100, 100, 255, 255}},  // Blue
        {PowerUpType::TINY_PADDLE, {255, 255, 100, 255}}   // Yellow
    };
        
public:
    std::string getSystemName() const override { return "Power-Up System"; }
    
    void init() override
    {
        registerGroup<PowerUp, PositionComponent, Velocity>();
        registerGroup<PowerUpEffect>();
        registerGroup<Paddle, PositionComponent>();
    }
    
    void onEvent(const AlienDestroyedEvent& event) override {
        trySpawnPowerUp(event.x, event.y);
    }
    
    void onEvent(const TickEvent& event) override {
        updatePowerUpPhysics(event.tick);
        checkPowerUpCollection();
        updateActiveEffects(event.tick);
    }
    
private:
    void trySpawnPowerUp(float x, float y) {
        float roll = (rand() % 100) / 100.0f;
        float cumulative = 0;
        
        for (auto& [type, rate] : dropRates) {
            cumulative += rate;
            if (roll < cumulative) {
                spawnPowerUp(type, x, y);
                return;
            }
        }
    }
    
    void spawnPowerUp(PowerUpType type, float x, float y) {
        auto powerup = makeSimple2DShape(ecsRef, Shape2D::Square, 24, 24);
        auto pos = powerup.get<PositionComponent>();
        pos->setX(x - 12);
        pos->setY(y);
        
        auto vel = powerup.attach<Velocity>();
        vel->dy = 100;  // Gentle fall
        
        auto pu = powerup.attach<PowerUp>();
        pu->type = type;
        
        // Set color based on type
        powerup.get<Simple2DObject>()->setColors(powerUpColors[type]);
        
        // Add pulsing effect for visibility
        powerup.attach<PulseEffect>();
        
        printf("Spawned power-up: %s\n", getPowerUpName(type).c_str());
    }
    
    void updatePowerUpPhysics(float dt) {
        std::vector<EntityRef> toDestroy;
        
        for (auto entity : viewGroup<PowerUp, PositionComponent, Velocity>()) {
            auto pos = entity->get<PositionComponent>();
            auto vel = entity->get<Velocity>();
            
            // Simple falling
            pos->setY(pos->y + vel->dy * dt / 1000.0f);
            
            // Destroy if fell off screen
            if (pos->y > 640) {
                toDestroy.push_back(entity->entity);
            }
        }
        
        for (auto& e : toDestroy) {
            ecsRef->removeEntity(e.entity);
        }
    }
    
    void checkPowerUpCollection() {
        for (auto powerupEntity : viewGroup<PowerUp, PositionComponent>()) {
            auto puPos = powerupEntity->get<PositionComponent>();
            auto powerup = powerupEntity->get<PowerUp>();
            
            for (auto paddleEntity : viewGroup<Paddle, PositionComponent>()) {
                auto paddlePos = paddleEntity->get<PositionComponent>();
                
                // Simple AABB collision
                if (checkCollision(puPos, paddlePos)) {
                    applyPowerUp(powerup->type, paddleEntity->entity);
                    ecsRef->removeEntity(powerupEntity->entity);
                    
                    // Visual/audio feedback
                    ecsRef->sendEvent(PowerUpCollectedEvent{powerup->type});
                    break;
                }
            }
        }
    }
    
    void applyPowerUp(PowerUpType type, EntityRef paddle) {
        switch(type) {
            case PowerUpType::HEALTH:
                applyHealth();
                break;
            case PowerUpType::MULTIBALL:
                applyMultiBall();
                break;
            case PowerUpType::FASTBALL:
                applyFastBall();
                break;
            case PowerUpType::BARRIER:
                applyBarrier();
                break;
            case PowerUpType::WIDE_PADDLE:
                applyWidePaddle(paddle);
                break;
            case PowerUpType::TINY_PADDLE:
                applyTinyPaddle(paddle);
                break;
        }
        
        printf("Applied power-up: %s\n", getPowerUpName(type).c_str());
    }
    
    // Individual Power-Up Implementations
    // ------------------------------------
    
    void applyHealth() {
        for (auto score : viewGroup<GameScore>()) {
            auto gameScore = score->get<GameScore>();
            if (gameScore->lives < 5) {  // Cap at 5 lives
                gameScore->lives++;
                
                // Flash screen green briefly
                ecsRef->sendEvent(ScreenFlashEvent{{100, 255, 100, 255}, 200});
            }
        }
    }
    
    void applyMultiBall() {
        // Find active balls
        std::vector<std::pair<float, float>> ballVelocities;
        std::vector<std::pair<float, float>> ballPositions;
        
        for (auto entity : viewGroup<Ball, PositionComponent, Velocity>()) {
            auto ball = entity->get<Ball>();
            if (ball->launched) {
                auto pos = entity->get<PositionComponent>();
                auto vel = entity->get<Velocity>();
                ballPositions.push_back({pos->x, pos->y});
                ballVelocities.push_back({vel->dx, vel->dy});
            }
        }
        
        // Spawn 2 extra balls for each existing ball (cap at 8 total balls)
        int currentBalls = ballPositions.size();
        int ballsToSpawn = std::min(2 * currentBalls, 8 - currentBalls);
        
        for (int i = 0; i < ballsToSpawn && i < ballPositions.size(); i++) {
            for (int j = 0; j < 2; j++) {
                auto ball = makeSimple2DShape(ecsRef, Shape2D::Square, 16, 16, {255, 150, 255, 255});
                auto pos = ball.get<PositionComponent>();
                pos->setX(ballPositions[i].first);
                pos->setY(ballPositions[i].second);
                
                auto vel = ball.attach<Velocity>();
                
                // Spread them out in a fan pattern
                float angle = (j == 0) ? -25 : 25;  // Degrees
                float rad = angle * 3.14159f / 180.0f;
                
                vel->dx = ballVelocities[i].first * std::cos(rad) - ballVelocities[i].second * std::sin(rad);
                vel->dy = ballVelocities[i].first * std::sin(rad) + ballVelocities[i].second * std::cos(rad);
                
                auto ballComp = ball.attach<Ball>();
                ballComp->launched = true;
                ballComp->isExtra = true; 
            }
        }
        
        // Epic screen shake for the chaos
        ecsRef->sendEvent(ScreenShakeEvent{0.5f});
    }
    
    void applyFastBall() {
        // Speed up all balls by 50% - risk/reward!
        for (auto entity : viewGroup<Ball, Velocity>()) {
            auto vel = entity->get<Velocity>();
            vel->dx *= 1.5f;
            vel->dy *= 1.5f;
            
            // Add red trail effect
            if (not entity->entity.has<Trail>())
            {
                entity->entity.attach<Trail>()->maxLength = 15;  // Longer trail for speed
            }
        }
        
        // Also boost score multiplier for duration
        auto effect = ecsRef->createEntity("FastBallEffect");
        auto eff = effect.attach<PowerUpEffect>();
        eff->type = PowerUpType::FASTBALL;
        eff->duration = 10000;  // 10 seconds
        
        for (auto score : viewGroup<GameScore>())
        {
            score->get<GameScore>()->scoreMultiplier *= 2.0f;
        }
    }
    
    void applyBarrier() {
        // Remove old barrier if exists
        auto oldBarrier = ecsRef->getEntity("Barrier");

        if (oldBarrier)
        {
            oldBarrier->get<PowerUpEffect>()->duration = 10000;
            return;
        }
        
        // Create barrier entity at bottom of screen
        auto barrierEntity = makeSimple2DShape(ecsRef, Shape2D::Square, 820, 10, {100, 255, 255, 200});
        auto pos = barrierEntity.get<PositionComponent>();
        pos->setX(0);
        pos->setY(620);  // Just below paddle level
        
        barrierEntity.attach<EntityName>("Barrier");
        barrierEntity.attach<Barrier>();
        
        // Add timer to remove it
        auto effect = barrierEntity.attach<PowerUpEffect>();
        effect->type = PowerUpType::BARRIER;
        effect->duration = 10000;  // 10 seconds
    }
    
    void applyWidePaddle(EntityRef paddle) {
        auto pos = paddle->get<PositionComponent>();
        auto paddleComp = paddle->get<Paddle>();
        
        // Store original width if not already modified
        if (!paddle->has<PowerUpEffect>()) {
            auto effect = paddle.attach<PowerUpEffect>();
            effect->type = PowerUpType::WIDE_PADDLE;
            effect->duration = 15000;  // 15 seconds
            effect->value = pos->width;  // Store original
            
            // Make paddle wider
            pos->width *= 1.5f;
            // paddleComp->maxX = 820 - pos->width - 10;  // Update boundaries
            
            // Visual feedback - make it blue
            paddle->get<Simple2DObject>()->setColors({100, 100, 255, 255});

            // Add score multiplier
            for (auto score : viewGroup<GameScore>()) {
                score->get<GameScore>()->scoreMultiplier *= 2.0f;
            }

        }
    }
    
    void applyTinyPaddle(EntityRef paddle) {
        auto pos = paddle->get<PositionComponent>();
        auto paddleComp = paddle->get<Paddle>();
        
        if (!paddle->has<PowerUpEffect>()) {
            auto effect = paddle.attach<PowerUpEffect>();
            effect->type = PowerUpType::TINY_PADDLE;
            effect->duration = 15000;
            effect->value = pos->width;
            
            // Make paddle smaller but give 2x points
            pos->width *= 0.6f;
            // paddleComp->maxX = 820 - pos->width - 10;
            
            // Visual feedback - golden paddle for high risk/reward
            paddle->get<Simple2DObject>()->setColors({255, 215, 0, 255});

            // Add score multiplier
            for (auto score : viewGroup<GameScore>()) {
                score->get<GameScore>()->scoreMultiplier *= 2.0f;
            }
        }
    }
    
    void updateActiveEffects(float dt) {
        std::vector<EntityRef> toRemove;
        
        for (auto entity : viewGroup<PowerUpEffect>()) {
            auto effect = entity->get<PowerUpEffect>();
            
            if (effect->duration > 0) {
                effect->elapsed += dt;
                
                if (effect->elapsed >= effect->duration) {
                    // Effect expired - revert changes
                    revertPowerUp(entity->entity, effect);
                    toRemove.push_back(entity->entity);
                }
            }
        }
        
        for (auto& e : toRemove) {
            ecsRef->detach<PowerUpEffect>(e);
        }
    }
    
    void revertPowerUp(EntityRef entity, PowerUpEffect* effect) {
        switch(effect->type) {
            case PowerUpType::WIDE_PADDLE:
            case PowerUpType::TINY_PADDLE:
                // Restore original paddle width
                if (entity->has<PositionComponent>() && entity->has<Paddle>()) {
                    entity->get<PositionComponent>()->width = effect->value;
                    // entity->get<Paddle>()->maxX = 820 - effect->value - 10;
                    entity->get<Simple2DObject>()->setColors({255, 255, 255, 255});
                    
                    for (auto score : viewGroup<GameScore>())
                    {
                        score->get<GameScore>()->scoreMultiplier /= 2.0f;
                    }
                }
                break;
                
            case PowerUpType::BARRIER:
                // Barrier entity destroys itself
                ecsRef->sendEvent(RemoveEntityEvent{entity.id});
                break;
                
            case PowerUpType::FASTBALL:
                // Reset ball speeds
                for (auto ball : viewGroup<Ball, Velocity>()) {
                    auto vel = ball->get<Velocity>();
                    vel->dx /= 1.5f;
                    vel->dy /= 1.5f;

                    if (ball->entity->has<Trail>())
                        ecsRef->detach<Trail>(ball->entity);
                }
                
                // Reset score multiplier
                for (auto score : viewGroup<GameScore>())
                {
                    score->get<GameScore>()->scoreMultiplier /= 2.0f;
                }

                break;
        }
    }
    
    bool checkCollision(PositionComponent* a, PositionComponent* b) {
        return a->x < b->x + b->width &&
               a->x + a->width > b->x &&
               a->y < b->y + b->height &&
               a->y + a->height > b->y;
    }
    
    std::string getPowerUpName(PowerUpType type) {
        switch(type) {
            case PowerUpType::HEALTH: return "Extra Life";
            case PowerUpType::MULTIBALL: return "Multi-Ball";
            case PowerUpType::FASTBALL: return "Speed Ball";
            case PowerUpType::BARRIER: return "Safety Barrier";
            case PowerUpType::WIDE_PADDLE: return "Wide Paddle";
            case PowerUpType::TINY_PADDLE: return "Tiny Paddle";
        }
        return "Unknown";
    }
};

// Barrier Collision System
// ------------------------

class BarrierSystem : public System<InitSys>
{
public:
    void init() override {
        registerGroup<Barrier, PositionComponent>();
        registerGroup<Ball, PositionComponent, Velocity>();
    }
    
    void execute() override {
        for (auto barrierEntity : viewGroup<Barrier, PositionComponent>()) {
            auto barrier = barrierEntity->get<Barrier>();
            auto barrierPos = barrierEntity->get<PositionComponent>();
            
            for (auto ballEntity : viewGroup<Ball, PositionComponent, Velocity>()) {
                auto ballPos = ballEntity->get<PositionComponent>();
                auto ballVel = ballEntity->get<Velocity>();
                
                // Check if ball hit barrier
                if (ballPos->y + ballPos->height >= barrierPos->y &&
                    ballPos->y < barrierPos->y + barrierPos->height &&
                    ballVel->dy > 0) {  // Moving downward
                    
                    // Bounce ball
                    ballVel->dy = -abs(ballVel->dy);
                    ballPos->setY(barrierPos->y - ballPos->height - 1);
                    
                    // Damage barrier
                    barrier->health--;
                    
                    // Visual feedback - flash and fade
                    auto colors = barrierEntity->get<Simple2DObject>()->colors;
                    colors.w = (uint8_t)(255 * (barrier->health / 3.0f));
                    barrierEntity->get<Simple2DObject>()->setColors(colors);
                    
                    if (barrier->health <= 0) {
                        ecsRef->removeEntity(barrierEntity->entity);
                        break;
                    }
                }
            }
        }
    }
};