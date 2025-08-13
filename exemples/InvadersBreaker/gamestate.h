#pragma once

#include "player.h"
#include "enemy.h"

using namespace pg;

class GameStateSystem : public System<InitSys>
{
private:
    bool initialized = false;

public:
    std::string getSystemName() const override
    {
        return "Game Init System";
    }

    void init() override
    {
        spawnPaddle();
        spawnBall();
        spawnAlienFormation();
        spawnScoreKeeper();
        // Grab ECS reference if available in init
        // ecs = getECS(); // or however you access it
    }

    void execute() override
    {
    }

private:
    void spawnPaddle()
    {
        // Position at bottom center of screen
        float startX = 410.0f - 75.0f; // Center of 820px screen
        float startY = 580.0f;         // Near bottom of 640px screen

        auto paddle = makeSimple2DShape(ecsRef, Shape2D::Square, 150, 40);

        auto pos = paddle.get<PositionComponent>();

        pos->setX(startX);
        pos->setY(startY);

        paddle.attach<Velocity>();
        paddle.attach<Paddle>();
    }

    void spawnBall()
    {
        // Create ball using your shape system
        auto ball = makeSimple2DShape(ecsRef, Shape2D::Square, 16, 16, {255, 100, 100, 255});  // 16x16 for circle
        auto pos = ball.get<PositionComponent>();

        // Start position doesn't matter - will follow paddle
        pos->setX(400.0f);
        pos->setY(550.0f);

        ball.attach<Velocity>();
        ball.attach<Ball>();

        printf("Ball spawned\n");
    }

    void spawnAlienFormation()
    {
        const int ROWS = 4;
        const int COLS = 7;
        const float START_X = 110.0f;
        const float START_Y = 60.0f;
        const float SPACING_X = 80.0f;
        const float SPACING_Y = 50.0f;

        int alienCount = 0;

        // Spawn the grid
        for (int row = 0; row < ROWS; row++)
        {
            for (int col = 0; col < COLS; col++)
            {
                auto alien = makeSimple2DShape(ecsRef, Shape2D::Square, 40, 30);
                auto pos = alien.get<PositionComponent>();

                pos->setX(START_X + col * SPACING_X);
                pos->setY(START_Y + row * SPACING_Y);

                auto alienComp = alien.attach<Alien>();
                alienComp->row = row;
                alienComp->col = col;
                alienComp->points = (ROWS - row) * 100;  // Top rows worth more

                // Color by row for visual variety
                if (auto shape = alien.get<Simple2DObject>())
                {
                    switch(row)
                    {
                        case 0: shape->setColors({255, 100, 100, 255}); break;  // Red
                        case 1: shape->setColors({255, 200, 100, 255}); break;  // Orange
                        case 2: shape->setColors({100, 255, 100, 255}); break;  // Green
                        case 3: shape->setColors({100, 100, 255, 255}); break;  // Blue
                    }
                }

                alienCount++;
            }
        }

        // Create formation controller
        auto formationController = ecsRef->createEntity("FormationController");
        auto formation = formationController.attach<AlienFormation>();
        formation->totalAliens = alienCount;

        printf("Spawned %d aliens in %dx%d formation\n", alienCount, COLS, ROWS);
    }

    void spawnScoreKeeper()
    {
        auto scoreKeeper = ecsRef->createEntity("ScoreKeeper");
        auto score = scoreKeeper.attach<GameScore>();
        score->aliensRemaining = 28;  // 4x7 grid
        score->lives = 3;
        score->score = 0;

        printf("Game initialized: 3 lives, 28 aliens\n");
    }
};