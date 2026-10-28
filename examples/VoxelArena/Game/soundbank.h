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

        /** distance from the listener: full up close, quieter further, silent past FAR. */
        void play(pg::EntitySystem* ecs, const std::string& name, float distance);

        static constexpr float NEAR = 10.0f;
        static constexpr float FAR = 32.0f;

    private:
        struct Variants
        {
            std::vector<std::string> near, far;
        };

        std::map<std::string, Variants> sounds;
        std::mt19937 rng{1234};
    };
}
