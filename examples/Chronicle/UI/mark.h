#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ECS/entitysystem.h"

#include "label.h"

namespace chronicle
{
    // The five pixel sizes a mark is ever drawn at. Registered exactly, so the atlas
    // entry matches the quad and nothing is resampled.
    enum class MarkSize : uint8_t
    {
        S14 = 14,
        S16 = 16,
        S18 = 18,
        S24 = 24,
        S48 = 48
    };

    inline float px(MarkSize s) { return static_cast<float>(s); }

    // Which size pairs with which text style - the only place this mapping lives.
    //   caption, tick, body-sm, gloss, label -> S14
    //   body                                 -> S16
    //   figure, heading                      -> S18
    //   title, figure-xl, chapter            -> S24
    //   versal                               -> S48 (plates use S48 explicitly)
    MarkSize markSizeFor(const std::string& style);

    // The 28 names, in the design-system order (Parts, Resources, Activities, Places,
    // Meta, Status). First "strength", last "cross". "manicule" is the guide's pointing hand.
    const std::vector<std::string>& markNames();
    bool isMarkName(const std::string&);

    // Calls IconSystem::registerIconSet("chronicle", <iconRoot>/<name>.svg x 28, {14,16,18,24,48}).
    // Returns false (and logs) if the IconSystem is missing. Idempotent: a second call is a no-op.
    bool registerMarks(pg::EntitySystem*, const std::string& iconRoot = "res/icons/chronicle");

    struct MarkSpec
    {
        std::string name;             // one of markNames(); unknown -> "seal" drawn + one logged error
        MarkSize size = MarkSize::S16;
        std::string color = "ink";    // token; normally supplied by the paired label
        int z = 0;
    };

    // The theme element of a mark: "mark.<color>".
    std::string markElement(const std::string& color);

    struct Mark
    {
        pg::EntityRef entity;         // PositionComponent + UiAnchor + ViewportComponent + IconComponent + ThemeComponent
        MarkSpec spec;

        void setName(pg::EntitySystem*, const std::string&);           // re-validates (unknown -> seal)
        void setColor(pg::EntitySystem*, const std::string& token);    // ThemeComponent::setElement
        void setSize(pg::EntitySystem*, MarkSize);                     // width and height together
    };

    Mark makeMark(pg::EntitySystem*, const MarkSpec&);

    struct MarkedLabelSpec
    {
        std::string mark;              // "" = no mark
        bool reserveMark = false;      // keep the mark column even when `mark` is empty
        LabelSpec label;               // color here is THE colour: the mark takes it
        float gap = -1.0f;             // < 0 -> theme space(2) (8 px)
        int z = 0;                     // root z; label glyphs z+1, mark z+1
    };

    struct MarkedLabel
    {
        pg::EntityRef root;            // PositionComponent + UiAnchor + Prefab - anchor this
        std::optional<Mark> mark;
        Label label;

        void setColor(pg::EntitySystem*, const std::string& token);                       // both
        void setText(pg::EntitySystem*, const std::string&);                              // re-measures root for Grow
        void setMark(pg::EntitySystem*, const std::string& name);                         // "" removes
    };

    MarkedLabel makeMarkedLabel(pg::EntitySystem*, const MarkedLabelSpec&);
}
