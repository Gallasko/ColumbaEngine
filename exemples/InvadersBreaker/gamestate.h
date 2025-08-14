#pragma once

#include "player.h"
#include "enemy.h"
#include "events.h"

using namespace pg;

enum class GamePhase
{
    MENU,
    PLAYING,
    PAUSED,
    GAME_OVER,
    VICTORY
};

class GameStateSystem : public System<InitSys, Listener<OnSDLScanCode>>
{
private:
    bool initialized = false;

    GamePhase currentPhase = GamePhase::MENU;

public:
    std::string getSystemName() const override
    {
        return "Game Init System";
    }

    void init() override
    {
        spawnPaddle();
        spawnBall();
        spawnScoreKeeper();
        // Grab ECS reference if available in init
        // ecs = getECS(); // or however you access it
    }

    void onEvent(const OnSDLScanCode& event) override
    {
        if (event.key == SDL_SCANCODE_SPACE)
        {
            switch(currentPhase)
            {
                case GamePhase::MENU:
                case GamePhase::GAME_OVER:
                    restartGame();
                    break;
            }
        }

        if (event.key == SDL_SCANCODE_ESCAPE)
        {
            if (currentPhase == GamePhase::PLAYING)
            {
                currentPhase = GamePhase::PAUSED;

                ecsRef->sendEvent(GamePaused{});
            }
            else if (currentPhase == GamePhase::PAUSED)
            {
                currentPhase = GamePhase::PLAYING;

                ecsRef->sendEvent(GameResume{});
            }

        }
    }

    void restartGame()
    {
        // Nuclear option: destroy everything except paddle
        for (auto alien : viewGroup<Alien>())
        {
            ecsRef->removeEntity(alien->entity);
        }

        for (auto bullet : viewGroup<AlienBullet>())
        {
            ecsRef->removeEntity(bullet->entity);
        }

        // Reset ball
        for (auto ball : viewGroup<Ball>())
        {
            ball->get<Ball>()->launched = false;
            // Reset position
        }

        ecsRef->sendEvent(GameStart{});

        // Respawn aliens
        spawnAlienFormation();  // Your existing spawn code

        // Reset score
        for (auto score : viewGroup<GameScore>())
        {
            score->get<GameScore>()->lives = 3;
            score->get<GameScore>()->score = 0;
        }

        currentPhase = GamePhase::PLAYING;
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