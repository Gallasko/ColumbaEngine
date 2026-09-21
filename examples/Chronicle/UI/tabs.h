#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ECS/entitysystem.h"
#include "Input/inputcomponent.h"
#include "Input/sdlevents.h"
#include "UI/focusable.h"

#include "Core/tokens.h"
#include "Core/textstyle.h"

#include "label.h"
#include "mark.h"
#include "focusorder.h"

namespace chronicle
{
    struct TabItem { std::string label; std::string glyph; int badge = 0; };   // badge 0 = none

    struct TabsSpec
    {
        std::vector<TabItem> items;        // 2-6; seven logs a warning
        int active = 0;
        float width = 0.0f;                // 0 -> row grows to content; > 0 -> hairline spans this, tabs at the left
        std::string tag;                   // carried by TabSelectedEvent
        int z = 20;
    };

    struct TabSelectedEvent { pg::_unique_id tabs; std::string tag; int index; };

    struct TabState : public pg::Component   // on each tab FACE (the hit area)
    {
        TabState() = default;

        pg::_unique_id tabs = 0;
        int index = 0;
        std::string tag;                     // the row's tag, carried by TabSelectedEvent
        bool active = false, hovered = false, pressed = false, keyboardFocus = false;
        pg::_unique_id underline = 0, ring = 0;
        std::vector<pg::_unique_id> inked;   // glyph, label text
    };

    struct Tabs
    {
        pg::EntityRef root;                // PositionComponent + UiAnchor + Prefab; height 44
        pg::EntityRef rule;                // the 1 px rule-ruled hairline along the bottom

        struct Tab
        {
            pg::EntityRef face;
            std::optional<Mark> glyph;
            Label label;
            std::optional<Label> badge;
            pg::EntityRef badgeFrame;
            pg::EntityRef underline;
            pg::EntityRef ring;
        };

        std::vector<Tab> tabs;
        TabsSpec spec;

        int active() const;
        void setActive(pg::EntitySystem*, int);                   // programmatic: repaints, sends no event
        void setBadge(pg::EntitySystem*, const TextStyles&, int index, int count);   // 0 removes; re-measures the face
    };

    Tabs makeTabs(pg::EntitySystem*, const Tokens&, const TextStyles&, const TabsSpec&);

    struct TabsSystem : public pg::System<pg::Own<TabState>,
        pg::Listener<pg::HoverChangedEvent>, pg::Listener<pg::OnMouseClick>, pg::Listener<pg::OnMouseRelease>,
        pg::Listener<pg::OnSDLScanCode>, pg::Listener<KeyboardFocusChangedEvent>, pg::Listener<ThemeChangedEvent>, pg::InitSys>
    {
        explicit TabsSystem(const Tokens* tokens);

        std::string getSystemName() const override { return "Chronicle Tabs System"; }

        void init() override;

        void onEvent(const pg::HoverChangedEvent&) override;
        void onEvent(const pg::OnMouseClick&) override;
        void onEvent(const pg::OnMouseRelease&) override;
        void onEvent(const pg::OnSDLScanCode&) override;
        void onEvent(const KeyboardFocusChangedEvent&) override;
        void onEvent(const ThemeChangedEvent&) override;

        void select(pg::EntityRef face);   // sets active on its row; sends TabSelectedEvent unless already active
        void applyVisual(pg::EntityRef face);

    private:
        const Tokens* tokens;
    };
}
