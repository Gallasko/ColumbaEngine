#pragma once

#include "gamestate.h"

#include "UI/ttftext.h"

using namespace pg;

class HUDSystem : public System<InitSys>
{
    EntityRef scoreDisplay;
    EntityRef scoreMultiplierDisplay;
    EntityRef livesDisplay;
    
    EntityRef highscore;
    size_t hscore = 0;

    void init() override
    {
        // Create text entities at top of screen
        // Use your Simple2DObject system or whatever text rendering you have
        scoreDisplay = makeTTFText(ecsRef, 10, 10, 4, "light", "Score: 0", 0.5);
        scoreMultiplierDisplay = makeTTFText(ecsRef, 10, 30, 4, "light", "Multiplier: x1.0", 0.4);
        livesDisplay = makeTTFText(ecsRef, 700, 10, 4, "light", "Lives: 3", 0.5);

        hscore = ecsRef->getSavedData("highscore").get<size_t>();

        highscore = makeTTFText(ecsRef, 285, 10, 4, "light", "Highscore: " + std::to_string(hscore), 0.5);
    }

    void execute() override {
        for (auto gameScore : viewGroup<GameScore>())
        {
            auto score = gameScore->get<GameScore>();

            scoreDisplay->get<TTFText>()->setText("Score: " + std::to_string(score->score));
            scoreMultiplierDisplay->get<TTFText>()->setText("Multiplier: x" + std::to_string(score->scoreMultiplier) + ".0");            
            livesDisplay->get<TTFText>()->setText("Lives: " + std::to_string(score->lives));

            if (score->score > hscore)
            {
                hscore = score->score;
                highscore->get<TTFText>()->setText("Highscore: " + std::to_string(score->score));

                ecsRef->sendEvent(SaveElementEvent{"highscore", hscore});
            }

            // Flash lives text when hit
            if (score->lives == 1)
            {
                livesDisplay->get<TTFText>()->setColor({255.0f, 0.0f, 0.0f, 255.0f});
            }
            else
            {
                livesDisplay->get<TTFText>()->setColor({255.0f, 255.0f, 255.0f, 255.0f});
            }
        }
    }
};