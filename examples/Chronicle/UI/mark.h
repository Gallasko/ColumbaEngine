#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ECS/entitysystem.h"

#include "Core/tokens.h"

namespace chronicle
{
    // The five pixel sizes a mark is ever drawn at. Registered exactly, so the atlas
    // entry matches the quad and nothing is resampled.
    enum class MarkSize : uint8_t { S14 = 14, S16 = 16, S18 = 18, S24 = 24, S48 = 48 };

    inline float px(MarkSize s) { return static_cast<float>(s); }

    // Which size pairs with which text style - the only place this mapping lives.
    //   caption, tick, body-sm, gloss, label -> S14
    //   body                                 -> S16
    //   figure, heading                      -> S18
    //   title, figure-xl, chapter            -> S24
    //   versal                               -> S48 (plates use S48 explicitly)
    MarkSize markSizeFor(const std::string& style);

    // The 27 names, in the design-system order (Parts, Resources, Activities, Places,
    // Meta, Status). First "strength", last "cross".
    const std::vector<std::string>& markNames();
    bool isMarkName(const std::string&);

    // Calls IconSystem::registerIconSet("chronicle", <iconRoot>/<name>.svg x 27, {14,16,18,24,48}).
    // Returns false (and logs) if the IconSystem is missing. Idempotent: a second call is a no-op.
    bool registerMarks(pg::EntitySystem*, const std::string& iconRoot = "res/icons/chronicle");

    struct MarkSpec
    {
        std::string name;             // one of markNames(); unknown -> "seal" drawn + one logged error
        MarkSize size = MarkSize::S16;
        std::string colour = "ink";   // token; normally supplied by the paired label
        int z = 0;
    };

    struct Mark
    {
        pg::EntityRef entity;         // PositionComponent + UiAnchor + ViewportComponent + IconComponent
        MarkSpec spec;

        void setName(pg::EntitySystem*, const std::string&);           // re-validates (unknown -> seal)
        void setColour(pg::EntitySystem*, const std::string& token);   // PaintSystem::paint
        void setSize(pg::EntitySystem*, MarkSize);                     // width and height together
    };

    Mark makeMark(pg::EntitySystem*, const Tokens&, const MarkSpec&);
}
