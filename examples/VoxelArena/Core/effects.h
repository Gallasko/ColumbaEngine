#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "content.h"

namespace arena
{
    /**
     * A sound made from numbers (sounds.json), or a .wav a mod ships ("file").
     *
     * The synthesised one is a tone and white noise mixed, gliding from frequency to frequencyEnd,
     * shaped by a quick attack and an exponential decay, then darkened by a low-pass filter.
     * Enough for hits, whooshes, thuds, twangs and rumbles, with no audio file in the repository.
     */
    struct SoundRecipe
    {
        std::string name;
        std::string file;            // a .wav to play instead (absolute path)
        std::string wave = "sine";   // sine, square, saw, triangle: the tone
        float duration = 0.2f;       // s
        float frequency = 440.0f;    // Hz at the start ...
        float frequencyEnd = 440.0f; // ... and at the end
        float tone = 1.0f;           // 1 all tone, 0 all noise
        float attack = 0.005f;       // s to full volume
        float decay = 0.1f;          // s for the volume to fall to a third (exponential)
        float lowpass = 0.0f;        // 0 bright to 1 dark
        float volume = 0.5f;         // 0..1
        int variants = 3;            // copies with a slightly different pitch, picked at random
        float pitchJitter = 0.08f;   // how different (fraction)
    };

    /** 16-bit mono samples at SAMPLE_RATE. seed varies the noise and the pitch of a variant. */
    constexpr int SAMPLE_RATE = 44100;
    std::vector<int16_t> synthesise(const SoundRecipe& recipe, uint32_t seed, float loudness = 1.0f);

    bool writeWav(const std::string& path, const std::vector<int16_t>& samples);

    /** A burst of little cubes. */
    struct ParticlePreset
    {
        int count = 0;
        float speed = 4.0f;          // m/s
        float up = 0.5f;             // 0 bursts every way, 1 mostly upwards
        float size = 0.08f;          // m
        float life = 0.4f;           // s
        float gravity = 12.0f;       // m/s^2
        glm::vec3 color = glm::vec3(1.0f);
        bool eventColor = false;     // "color": "event" takes the colour of what broke
    };

    /** What a game event looks and sounds like (effects.json). */
    struct EffectDef
    {
        std::string sound;           // a name from sounds.json, "" = silent
        ParticlePreset particles;
        float shake = 0.0f;          // camera trauma when it happens near the player
        bool along = false;          // particles spread along a line event, not burst from its point

        // A status's look (the entry named after its id): a glow over the body, and particles every so often
        bool hasTint = false;
        glm::vec3 tint = glm::vec3(1.0f);
        float every = 0.0f;          // s between particle bursts while it lasts (0: none)
    };

    std::map<std::string, SoundRecipe> readSounds(const Json& tree, ContentReport& report);
    std::map<std::string, EffectDef> readEffects(const Json& tree, const std::map<std::string, SoundRecipe>& sounds, ContentReport& report);
}
