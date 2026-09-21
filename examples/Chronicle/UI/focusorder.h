#pragma once

#include <unordered_set>
#include <vector>

#include "ECS/entitysystem.h"
#include "Input/inputcomponent.h"   // OnMouseClick
#include "Input/sdlevents.h"        // OnSDLScanCode

namespace chronicle
{
    // Sent after every focus change so each component's system can show/hide its ring.
    struct KeyboardFocusChangedEvent { pg::_unique_id face; bool keyboard; };

    // One Tab order for every keyboard-focusable face in the kit (buttons, tabs, later rows).
    // Owns nothing visual: it only decides WHO gets FocusableComponent::focus(); each component's
    // own system reacts to KeyboardFocusChangedEvent and draws its ring.
    struct FocusOrderSystem : public pg::System<pg::Listener<pg::OnSDLScanCode>, pg::Listener<pg::OnMouseClick>, pg::InitSys>
    {
        std::string getSystemName() const override { return "Chronicle Focus Order"; }

        void init() override {}

        void add(pg::_unique_id face);              // in creation order; idempotent
        void remove(pg::_unique_id face);
        void setEnabled(pg::_unique_id face, bool); // disabled faces are skipped, not removed

        void focusNext();
        void focusPrev();
        void focus(pg::_unique_id face);            // focus a specific face (keyboard), keeping current in sync

        pg::_unique_id current() const { return currentFace; }   // 0 = none
        bool keyboardFocus() const { return kbFocus; }           // true after Tab, false after any mouse click

        void onEvent(const pg::OnSDLScanCode&) override;
        void onEvent(const pg::OnMouseClick&) override;

    private:
        void step(int dir);

        std::vector<pg::_unique_id> order;
        std::unordered_set<pg::_unique_id> disabledFaces;
        pg::_unique_id currentFace = 0;
        bool kbFocus = false;
    };
}
