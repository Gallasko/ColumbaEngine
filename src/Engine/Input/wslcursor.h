#pragma once

#include <memory>

namespace pg
{
    /**
     * Mouse capture for WSLg, where the Windows host owns the real cursor: nothing on the Linux side
     * can warp, grab or pointer-lock it, so a look camera turns until the cursor reaches the edge of
     * the screen. This runs a small helper on the Windows side (powershell.exe, through WSL interop)
     * that, while held, reads how far the cursor moved and puts it back where it was.
     *
     * The game sends a heartbeat each frame it wants the mouse; the helper lets go on its own when the
     * heartbeat stops (game frozen in a debugger, crashed, closed) or another Windows window comes to
     * the front, so the mouse can never stay trapped.
     */
    class WslCursor
    {
    public:
        /** Starts the helper. Its startup takes about a second, during which held() stays false */
        static std::unique_ptr<WslCursor> start();

        ~WslCursor();

        /** Call once per frame: true while the game wants the mouse and its window has focus */
        void hold(bool want);

        /** True while the helper is holding the Windows cursor in place */
        bool held() const;

        /** False once the helper has died (no powershell, interop disabled ...) */
        bool alive() const;

        /** The cursor movement since the last call, in pixels */
        void takeMotion(int& dx, int& dy);

        struct Shared;

    private:
        WslCursor() = default;

        std::shared_ptr<Shared> shared;
        int toHelper = -1;
        int pid = -1;
        bool sentHold = false;
    };

    /** True when running under WSL (detected once from the environment) */
    bool runningOnWsl();
}
