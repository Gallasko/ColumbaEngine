#pragma once

#include <string>

#include <SDL_scancode.h>
#include <SDL_stdinc.h>

#include "ECS/standardevent.h"

#include "Maths/geometry.h"

namespace pg
{
    class Input;

    struct OnMouseMove
    {
        Point2D pos;
        Input *inputHandler;

        STANDARD_EVENT_CONVERTIBLE(OnMouseMove)
    };

    /**
     * Raw SDL mouse motion event, forwarded verbatim from the SDL event loop.
     * `xrel`/`yrel` are SDL's authoritative relative motion and are reliable
     * in both normal and relative-mouse-mode (SDL_SetRelativeMouseMode), which
     * makes this event the right signal for FPS-style look controllers.
     * Unlike `OnMouseMove` (position-based), this is not derived from UI
     * hover tracking and does not race with the main-thread input reset.
     */
    struct OnSDLMouseMotion
    {
        Sint32 x;
        Sint32 y;
        Sint32 xrel;
        Sint32 yrel;
    };

    struct OnSDLTextInput
    {
        std::string text;

        STANDARD_EVENT_CONVERTIBLE(OnSDLTextInput)
    };

    struct OnSDLScanCode
    {
        SDL_Scancode key;
        Uint16 mod;

        STANDARD_EVENT_CONVERTIBLE(OnSDLScanCode)
    };

    struct OnSDLScanCodeReleased
    {
        SDL_Scancode key;
        Uint16 mod;

        STANDARD_EVENT_CONVERTIBLE(OnSDLScanCodeReleased)
    };

    struct OnSDLMouseWheel
    {
        Sint32 x;
        Sint32 y;

        STANDARD_EVENT_CONVERTIBLE(OnSDLMouseWheel)
    };

    struct OnSDLGamepadPressed
    {
        int id;

        unsigned int button;
    };

    struct OnSDLGamepadReleased
    {
        int id;

        unsigned int button;
    };

    struct OnSDLGamepadAxisChanged
    {
        int id;

        unsigned int axis;

        int value;
    };
}
