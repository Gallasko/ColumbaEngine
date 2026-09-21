#pragma once

namespace chronicle
{
    // The one global the kit reads for animation. Set from --reduced-motion (main.cpp) and,
    // later, from settings.
    struct Motion
    {
        static bool reduced();               // true -> every animated setter jumps to its final value
        static void setReduced(bool);
        static constexpr float kMsPerPercent = 6.0f;   // a full 0->100 fill takes 600 ms (linear, months are uniform)
    };
}
