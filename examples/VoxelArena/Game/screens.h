#pragma once

#include <string>

#include "ui.h"

namespace arena
{
    /** The title screen (UI_SPEC.md section 8): the game's name, what to play, the best score. */
    struct TitleView
    {
        int waves = 20;
        int best = 0;
        bool waitingForHost = false;    // a client: only the host starts the run
        int players = 1, maxPlayers = 8;
        float time = 0.0f;
    };

    /** The end of a run (section 9). */
    struct EndView
    {
        bool won = false;
        int wave = 1, waves = 20;
        int score = 0, best = 0, kills = 0, level = 1;
        float time = 0.0f;              // s the run lasted
        bool solo = true;               // the training ground is offered alone only
        std::string boss = "the Stone Warden";
    };

    namespace screens
    {
        // Click actions
        constexpr int TITLE_ARENA = 1, TITLE_TRAINING = 2, TITLE_SPELLBOOK = 3, TITLE_OPTIONS = 4;
        constexpr int END_AGAIN = 11, END_TRAINING = 12;

        void drawTitle(Ui& ui, const TitleView& v);
        void drawEnd(Ui& ui, const EndView& v);

        /** 7073 as "7 073". */
        std::string grouped(int value);
    }
}
