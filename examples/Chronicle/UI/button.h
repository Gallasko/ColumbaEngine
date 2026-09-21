#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ECS/entitysystem.h"
#include "Input/inputcomponent.h"   // HoverChangedEvent, OnMouseClick/Release
#include "Input/sdlevents.h"        // OnSDLScanCode
#include "UI/focusable.h"           // OnFocus

#include "Core/tokens.h"
#include "Core/textstyle.h"

#include "label.h"
#include "mark.h"
#include "focusorder.h"

namespace chronicle
{
    enum class ButtonVariant : uint8_t { Quiet, Study, Seal };

    struct ButtonSpec
    {
        ButtonVariant variant = ButtonVariant::Quiet;
        std::string label;                 // required, set in `control`
        std::string glyph;                 // mark S16, "" = none
        int months = -1;                   // < 0 = no cost; else "N mo" with the time mark
        bool disabled = false;
        std::string reason;                // shown under the face when disabled; caption, ink-faint
        std::string tag;                   // carried by ButtonActivatedEvent
        int z = 20;                        // root/face z
    };

    // Sent by ButtonSystem on activation (release inside a pressed face, or Enter/Space on focus).
    struct ButtonActivatedEvent { pg::_unique_id face; std::string tag; };

    // State on the FACE entity (the hit area). Plain pg::Component; never serialised.
    struct ButtonState : public pg::Component
    {
        ButtonState() = default;

        ButtonVariant variant = ButtonVariant::Quiet;
        bool disabled = false;
        bool hovered = false, pressed = false, focused = false, keyboardFocus = false;
        std::string tag;

        pg::_unique_id ground = 0, frame = 0, ring = 0, sheen = 0, reasonBox = 0;
        std::vector<pg::_unique_id> inked;      // glyph, label text, cost mark, cost text
        std::vector<pg::_unique_id> costParts;  // subset of inked painted status-time on Quiet
    };

    struct Button
    {
        pg::EntityRef root;                // anchor this. Height 36, or 36 + 4 + 15 with a reason
        pg::EntityRef face;                // the hit area (36 tall), carries ButtonState + FocusableComponent
        std::optional<Mark> glyph;
        Label label;
        std::optional<Mark> costMark;
        std::optional<Label> cost;
        std::optional<Label> reason;
        ButtonSpec spec;
        const TextStyles* styles = nullptr;   // for re-fitting the reason on setDisabled

        void setDisabled(pg::EntitySystem*, bool, const std::string& reason = "");
        void setLabel(pg::EntitySystem*, const TextStyles&, const std::string&);
        void setMonths(pg::EntitySystem*, const TextStyles&, int);
        float faceWidth(pg::EntitySystem*) const;
    };

    Button makeButton(pg::EntitySystem*, const Tokens&, const TextStyles&, const ButtonSpec&);

    struct ButtonSystem : public pg::System<pg::Own<ButtonState>,
        pg::Listener<pg::HoverChangedEvent>, pg::Listener<pg::OnMouseClick>, pg::Listener<pg::OnMouseRelease>,
        pg::Listener<pg::OnSDLScanCode>, pg::Listener<KeyboardFocusChangedEvent>, pg::Listener<ThemeChangedEvent>, pg::InitSys>
    {
        explicit ButtonSystem(const Tokens* tokens);

        std::string getSystemName() const override { return "Chronicle Button System"; }

        void init() override;

        void onEvent(const pg::HoverChangedEvent&) override;
        void onEvent(const pg::OnMouseClick&) override;
        void onEvent(const pg::OnMouseRelease&) override;
        void onEvent(const pg::OnSDLScanCode&) override;   // RETURN/SPACE only; Tab is FocusOrderSystem's
        void onEvent(const KeyboardFocusChangedEvent&) override;
        void onEvent(const ThemeChangedEvent&) override;

        void applyVisual(pg::EntityRef face);   // repaints from state; public for tests
        void activate(pg::EntityRef face);      // sends ButtonActivatedEvent unless disabled

    private:
        const Tokens* tokens;
    };
}
