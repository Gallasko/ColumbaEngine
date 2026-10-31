#pragma once

#include <map>
#include <random>
#include <string>
#include <vector>

#include "Core/effects.h"

namespace pg
{
    class EntitySystem;
}

namespace arena
{
    /**
     * The game's sounds, ready to play: every recipe of sounds.json is synthesised once into .wav
     * files (a few pitch variants, each at full and at far loudness) in a cache folder, because the
     * engine's audio plays files. A sound a mod gives as a file is played as it is.
     */
    class SoundBank
    {
    public:
        void load(const std::map<std::string, SoundRecipe>& recipes, const std::string& cacheDir, ContentReport& report);

        /**
         * distance from the listener: full up close, quieter further, silent past FAR_RANGE. pan: -1 all
         * left to 1 all right, behind: 0 (in front) to 1 (right behind, a little quieter).
         */
        void play(pg::EntitySystem* ecs, const std::string& name, float distance, float pan = 0.0f, float behind = 0.0f);

        /** How much the far ear loses at a sound fully to one side (0.65: it keeps 35%). */
        static constexpr float PAN_DEPTH = 0.65f;
        static constexpr float BEHIND_DROP = 0.2f;

        static constexpr float NEAR_RANGE = 10.0f;   // not NEAR/FAR (nor near/far below): windows.h defines those as macros
        static constexpr float FAR_RANGE = 32.0f;

    private:
        struct Variants
        {
            std::vector<std::string> nearFiles, farFiles;
        };

        std::map<std::string, Variants> sounds;
        std::mt19937 rng{1234};
    };
}
