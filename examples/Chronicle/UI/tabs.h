#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ECS/entitysystem.h"
#include "Input/inputcomponent.h"
#include "Input/sdlevents.h"
#include "UI/focusable.h"
#include "UI/themesystem.h"

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
        bool disabled = false;               // A page that is not open yet: faint, and neither clicked nor focused
        bool hidden = false;                 // A page there is no word of yet: nothing of its tab is drawn
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
            pg::EntityRef noticeGround;        // The round of a notice, made with the first one: at the tab's top right corner
            std::optional<Label> noticeText;   // Its count
            int notice = 0;                    // What is new on its page since it was last open, 0 for nothing
            bool shown = true;                 // Drawn at all: setShown
        };

        std::vector<Tab> tabs;
        TabsSpec spec;

        int active() const;
        void setActive(pg::EntitySystem*, int);                   // programmatic: repaints, sends no event
        void setEnabled(pg::EntitySystem*, int index, bool enabled);   // A tab that is not enabled is faint, takes no click and no focus
        bool enabled(int index) const;
        void setShown(pg::EntitySystem*, int index, bool shown);        // A tab not shown is not drawn and keeps its room: for the last of a row. Shown, it is enabled as well
        void setNotice(pg::EntitySystem*, int index, int count);        // A small round with a count at the tab's corner: something new on a page that is not the one in view. 0 takes it away. The faces keep their size
        void setBadge(pg::EntitySystem*, int index, int count);   // 0 removes; re-measures the face
        void setWidth(pg::EntitySystem*, float width);            // The row and its hairline; the faces keep their size
    };

    Tabs makeTabs(pg::EntitySystem*, const TabsSpec&);

    struct TabsSystem : public pg::System<pg::Own<TabState>,
        pg::Listener<pg::HoverChangedEvent>, pg::Listener<pg::OnMouseClick>, pg::Listener<pg::OnMouseRelease>,
        pg::Listener<pg::OnSDLScanCode>, pg::Listener<KeyboardFocusChangedEvent>, pg::Listener<pg::ThemeChangedEvent>, pg::InitSys>
    {
        std::string getSystemName() const override { return "Chronicle Tabs System"; }

        void init() override;

        void onEvent(const pg::HoverChangedEvent&) override;
        void onEvent(const pg::OnMouseClick&) override;
        void onEvent(const pg::OnMouseRelease&) override;
        void onEvent(const pg::OnSDLScanCode&) override;
        void onEvent(const KeyboardFocusChangedEvent&) override;
        void onEvent(const pg::ThemeChangedEvent&) override;

        void select(pg::EntityRef face);   // sets active on its row; sends TabSelectedEvent unless already active
        void applyVisual(pg::EntityRef face);
    };
}
