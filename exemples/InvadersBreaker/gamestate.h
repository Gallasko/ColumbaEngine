#pragma once

#include "player.h"
#include "enemy.h"
#include "events.h"

#include "UI/ttftext.h"

using namespace pg;

enum class GamePhase
{
    MENU,
    PLAYING,
    PAUSED,
    GAME_OVER,
    VICTORY
};

class GameStateSystem : public System<InitSys, Listener<OnSDLScanCode>, Own<GameScore>,
    Ref<AlienBullet>, Ref<Alien>,
    Listener<GameEnd>>
{
private:
    bool initialized = false;

    GamePhase currentPhase = GamePhase::MENU;

    EntityRef menuText;
    EntityRef menuSubText;
    EntityRef gameOverText;
    EntityRef victoryText;
    EntityRef restartText;
    EntityRef finalScoreText;

public:
    std::string getSystemName() const override
    {
        return "Game Init System";
    }

    void init() override
    {
        menuText = makeTTFText(ecsRef, 300, 300, 6, "bold", "SPACE BREAKER", 1.0);
        menuSubText = makeTTFText(ecsRef, 250, 350, 4, "light", "Press SPACE to Start", 0.8);

        gameOverText = makeTTFText(ecsRef, 280, 280, 6, "bold", "GAME OVER", 1.0);
        gameOverText->get<TTFText>()->setColor({255, 0, 0, 255});
        gameOverText->get<PositionComponent>()->setVisibility(false);

        victoryText = makeTTFText(ecsRef, 320, 280, 6, "bold", "VICTORY!", 1.0);
        victoryText->get<TTFText>()->setColor({0, 255, 0, 255});
        victoryText->get<PositionComponent>()->setVisibility(false);

        restartText = makeTTFText(ecsRef, 250, 380, 3, "light", "Press SPACE to Try Again", 0.7);
        restartText->get<PositionComponent>()->setVisibility(false);

        finalScoreText = makeTTFText(ecsRef, 280, 330, 4, "light", "Final Score: 0", 0.8);
        finalScoreText->get<PositionComponent>()->setVisibility(false);

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
                case GamePhase::VICTORY:
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

    void onEvent(const GameEnd& event)
    {
        if (event.won)
            triggerVictory();
        else
            triggerGameOver();
    }

    void restartGame()
    {
        currentPhase = GamePhase::PLAYING;

        updateUIVisibility();
        clearEndScreen();

        // Nuclear option: destroy everything except paddle
        for (auto alien : view<Alien>())
        {
            ecsRef->removeEntity(alien->entityId);
        }

        for (auto bullet : view<AlienBullet>())
        {
            ecsRef->removeEntity(bullet->entityId);
        }

        if (auto ent = ecsRef->getEntity("FormationController"))
            ecsRef->removeEntity(ent);

        // Reset ball
        for (auto ball : viewGroup<Ball>())
        {
            ball->get<Ball>()->launched = false;
            // Reset position
        }


        ecsRef->sendEvent(GameStart{});

        // Respawn aliens
        spawnAlienFormation();  // Your existing spawn code

        resetScoreKeeper();
    }

private:
    void clearEndScreen()
    {
        gameOverText->get<PositionComponent>()->setVisibility(false);
        victoryText->get<PositionComponent>()->setVisibility(false);

        finalScoreText->get<PositionComponent>()->setVisibility(false);

        restartText->get<PositionComponent>()->setVisibility(false);
    }

    void triggerGameOver()
    {
        currentPhase = GamePhase::GAME_OVER;
        gameOverText->get<PositionComponent>()->setVisibility(true);

        // Show final score
        for (auto score : view<GameScore>())
        {
            finalScoreText->get<TTFText>()->setText("Final Score: " + std::to_string(score->score));
            finalScoreText->get<PositionComponent>()->setVisibility(true);
        }

        restartText->get<PositionComponent>()->setVisibility(true);
    }

    void triggerVictory() {
        currentPhase = GamePhase::VICTORY;
        victoryText->get<PositionComponent>()->setVisibility(true);

        for (auto score : view<GameScore>())
        {
            finalScoreText->get<TTFText>()->setText("Final Score: " + std::to_string(score->score));
            finalScoreText->get<PositionComponent>()->setVisibility(true);
        }

        restartText->get<PositionComponent>()->setVisibility(true);
    }

    void updateUIVisibility()
    {
        // Hide/show menu based on state
        if (menuText)
        {
            menuText->get<PositionComponent>()->setVisibility(currentPhase == GamePhase::MENU);
        }

        if (menuSubText)
        {
            menuSubText->get<PositionComponent>()->setVisibility(currentPhase == GamePhase::MENU);
        }
    }

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

    void resetScoreKeeper()
    {
        auto scoreKeeper = ecsRef->getEntity("ScoreKeeper");
        auto score = scoreKeeper->get<GameScore>();
        score->aliensRemaining = 28;  // 4x7 grid
        score->lives = 3;
        score->score = 0;

        printf("Game initialized: 3 lives, 28 aliens\n");
    }
};