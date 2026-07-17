#pragma once

#include <cstdint>
#include <memory>

#include "../serialization.h"

namespace pg
{
    /// Lightweight seedable xorshift32 RNG for local, deterministic/reproducible
    /// use (e.g. procedural generation), as opposed to the global RandomNumberGenerator singleton.
    struct LocalRng
    {
        uint32_t state;

        explicit LocalRng(uint32_t seed) : state(seed ? seed : 0x1u) {}

        uint32_t next()
        {
            uint32_t x = state;
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            state = x;
            return x;
        }

        // Inclusive range.
        int rangeInt(int lo, int hi)
        {
            if (hi <= lo)
                return lo;
            return lo + static_cast<int>(next() % static_cast<uint32_t>(hi - lo + 1));
        }

        // [0, 1)
        float unitFloat()
        {
            return (next() & 0xFFFFFFu) / static_cast<float>(0x1000000);
        }
    };

    class RandomNumberGenerator
    {
    public:
        static std::unique_ptr<RandomNumberGenerator>& generator() { static auto generator = std::unique_ptr<RandomNumberGenerator>(new RandomNumberGenerator(true)); return generator; }

        int generateNumber();

        void setSeed(unsigned int seed);
        inline unsigned int getSeed() const { return seed; }

    private:
        friend RandomNumberGenerator deserialize<>(const UnserializedObject& serializedString);

        RandomNumberGenerator(bool fromSerialization = false);
        unsigned int seed = 0;
    };

}