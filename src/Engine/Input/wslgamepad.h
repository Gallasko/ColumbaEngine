#pragma once

#include <memory>
#include <vector>

namespace pg
{
    /**
     * Controllers for WSLg, where the Windows host keeps them: no /dev/input reaches the Linux side, so SDL finds
     * none. This runs a small helper on the Windows side (powershell.exe, through WSL interop, as WslCursor) that
     * reads XInput (Xbox controllers and the pads that act as one) and tells each change, as SDL's own controller
     * would: a button pressed or released, an axis moved. It also plays rumble back.
     *
     * The controllers are numbered from ID_BASE (the XInput slot added), so they never clash with SDL's instance ids.
     * The helper ends when the game closes its input (the game ended, or crashed).
     */
    class WslGamepad
    {
    public:
        static constexpr int ID_BASE = 1000;

        /** What the helper told since the last take: SDL's numbers (SDL_GameControllerButton, SDL_GameControllerAxis). */
        struct Event
        {
            enum class Kind { Pressed, Released, Axis } kind = Kind::Pressed;
            int id = 0;          // ID_BASE + the XInput slot
            int which = 0;       // the button or the axis
            int value = 0;       // an axis: -32768..32767 (a trigger 0..32767)
        };

        /** Starts the helper. Its startup takes about a second; null when there is no powershell.exe */
        static std::unique_ptr<WslGamepad> start();

        ~WslGamepad();

        std::vector<Event> take();

        /** Rumble on a controller of the helper's (low and high: the two motors, 0..1), for ms milliseconds */
        void rumble(int id, float low, float high, int ms);

        bool alive() const;

        struct Shared;

    private:
        WslGamepad() = default;

        std::shared_ptr<Shared> shared;
        int toHelper = -1;
        int pid = -1;
    };
}
