#pragma once

#include "gamestate.h"

#include "UI/ttftext.h"

using namespace pg;

class HUDSystem : public System<InitSys, Listener<TickEvent>>
{
    EntityRef scoreDisplay;
    EntityRef livesDisplay;

    void init() override
    {
        // Create text entities at top of screen
        // Use your Simple2DObject system or whatever text rendering you have
        scoreDisplay = makeTTFText(ecsRef, 10, 10, 4, "light", "Score: 0", 0.5);
        livesDisplay = makeTTFText(ecsRef, 700, 10, 4, "light", "Lives: 3", 0.5);
    }

    void execute() override {
        for (auto gameScore : viewGroup<GameScore>())
        {
            auto score = gameScore->get<GameScore>();

            scoreDisplay->get<TTFText>()->setText("Score: " + std::to_string(score->score));
            livesDisplay->get<TTFText>()->setText("Lives: " + std::to_string(score->lives));

            // Flash lives text when hit
            if (score->lives == 1)
            {
                livesDisplay->get<TTFText>()->setColor({255.0f, 0.0f, 0.0f, 255.0f});
            }
        }
    }
};