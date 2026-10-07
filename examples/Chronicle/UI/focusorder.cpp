#include "focusorder.h"

#ifdef __linux__
#include <SDL2/SDL.h>
#elif _WIN32
#include <SDL.h>
#endif

#include <algorithm>

#include "UI/focusable.h"

using namespace pg;

namespace chronicle
{
    void FocusOrderSystem::add(_unique_id face)
    {
        if (std::find(order.begin(), order.end(), face) == order.end())
            order.push_back(face);
    }

    void FocusOrderSystem::remove(_unique_id face)
    {
        order.erase(std::remove(order.begin(), order.end(), face), order.end());
        disabledFaces.erase(face);
        if (currentFace == face)
            currentFace = 0;
    }

    void FocusOrderSystem::setEnabled(_unique_id face, bool enabled)
    {
        if (enabled)
            disabledFaces.erase(face);
        else
            disabledFaces.insert(face);
    }

    void FocusOrderSystem::step(int dir)
    {
        std::vector<_unique_id> enabled;
        for (auto f : order)
            if (disabledFaces.find(f) == disabledFaces.end())
                enabled.push_back(f);
        if (enabled.empty())
            return;

        int cur = -1;
        for (size_t i = 0; i < enabled.size(); ++i)
            if (enabled[i] == currentFace)
                cur = static_cast<int>(i);

        int next;
        if (cur < 0)
            next = dir > 0 ? 0 : static_cast<int>(enabled.size()) - 1;
        else
            next = ((cur + dir) % static_cast<int>(enabled.size()) + static_cast<int>(enabled.size())) % static_cast<int>(enabled.size());

        focus(enabled[next]);
    }

    void FocusOrderSystem::focus(_unique_id face)
    {
        currentFace = face;
        kbFocus = true;
        if (auto e = ecsRef->getEntity(face); e and e->has<FocusableComponent>())
            e->get<FocusableComponent>()->focus();   // OnFocus -> the engine's FocusableSystem
        ecsRef->sendEvent(KeyboardFocusChangedEvent{face, true});
    }

    void FocusOrderSystem::focusNext() { step(+1); }
    void FocusOrderSystem::focusPrev() { step(-1); }

    void FocusOrderSystem::onEvent(const OnSDLScanCode& event)
    {
        if (event.key != SDL_SCANCODE_TAB)
            return;
        if (event.mod & KMOD_SHIFT)
            focusPrev();
        else
            focusNext();
    }

    void FocusOrderSystem::onEvent(const OnMouseClick&)
    {
        // A mouse click hides every ring but keeps `current` where it was.
        if (kbFocus)
        {
            kbFocus = false;
            ecsRef->sendEvent(KeyboardFocusChangedEvent{currentFace, false});
        }
    }
}
