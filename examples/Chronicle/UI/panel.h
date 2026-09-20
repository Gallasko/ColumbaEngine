#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ECS/entitysystem.h"

#include "Core/tokens.h"
#include "Core/textstyle.h"

#include "label.h"
#include "mark.h"
#include "ornament.h"

namespace chronicle
{
    enum class PanelFrame : uint8_t { Hair, Ruled, Plain, Illuminated };

    struct PanelSpec
    {
        PanelFrame frame = PanelFrame::Ruled;
        float width = 320.0f;          // required; the height derives from the content
        std::string heading;           // "" -> no head row and no rule
        std::string glyph;             // mark name; "" -> none. Drawn in ink-muted
        std::string aside;             // set in `label` style, ink-muted; pass it already in caps
        int z = 10;                    // root: Panels band 10-19
        int contentZ = 20;             // what the caller gives its children; must be > z + 4
    };

    struct Panel
    {
        pg::EntityRef root;            // PositionComponent + UiAnchor + Prefab: anchor this
        pg::EntityRef ground;          // Simple2DObject painted folio (absent for Plain)
        pg::EntityRef frame;           // StrokeRect2DObject (absent for Plain)
        std::vector<Ornament> corners; // Illuminated only, four
        std::optional<Mark> glyph;
        std::optional<Label> title;
        std::optional<Label> aside;
        std::optional<Ornament> rule;  // the knotless hair divider under the head
        pg::EntityRef body;            // the VerticalLayout entity
        PanelSpec spec;
        float padding = 16.0f;         // resolved from the frame
        float headBlock = 0.0f;        // 0, or 26 + 12 + 1 + 12 = 51 when there is a heading
        const TextStyles* styles = nullptr;   // for re-fitting the title on setWidth/setAside

        void addChild(pg::EntitySystem*, pg::EntityRef);
        void removeChild(pg::EntitySystem*, pg::EntityRef);
        void setHeading(pg::EntitySystem*, const TextStyles&, const std::string&);
        void setAside(pg::EntitySystem*, const TextStyles&, const std::string&);
        void setWidth(pg::EntitySystem*, float);
        float width(pg::EntitySystem*) const;
        float height(pg::EntitySystem*) const;
        float innerWidth() const;      // spec.width - 2 * padding
    };

    Panel makePanel(pg::EntitySystem*, const Tokens&, const TextStyles&, const PanelSpec&);
}
