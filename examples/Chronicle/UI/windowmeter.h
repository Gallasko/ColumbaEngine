#pragma once

#include <cstdint>
#include <string>

#include "ECS/entitysystem.h"

#include "label.h"
#include "mark.h"
#include "progressrule.h"

namespace chronicle
{
    enum class WindowState : uint8_t
    {
        Upcoming = 0,
        Open     = 1,
        Closed   = 2
    };

    struct WindowMeterSpec
    {
        std::string name = "Squire";
        float from = 16.0f;            // Ages; to > from
        float to = 22.0f;
        float age = 7.0f;              // Fill = clamp((age - from) / (to - from), 0, 1)
        WindowState state = WindowState::Open;
        std::string note;              // As the scene words it ("Open 22 more months. One attempt fits; two do not."); not forced to caps
        std::string glossKey;          // "" = none; else attachGloss(root, key)
        float width = 288.0f;
        int z = 20;                    // Root; mark z+1, texts z+2; rule root z+1
    };

    // A door that closes: an age-limited chance, how much of it is spent, and what still fits
    // inside it. The name with the gate mark at the left, the age range at the right, a small
    // ProgressRule (6 px, no nib) filled by how far the age is through the range, and the note
    // that does the work, said in attempts, never percentages. Three states, each an element
    // suffix on the name, the mark and the note: upcoming (ink name, verdigris note naming the
    // entry requirement), open (ochre name, status-time note) and closed (all state-locked, the
    // mark a cross; it stays on the screen for the rest of the life).
    //
    // Fed, never deciding: setAge, setState, setNote, setRange. The state is windows.pg's to say.
    struct WindowMeter
    {
        pg::EntityRef root;            // PositionComponent + UiAnchor + Prefab: width = spec.width; height 24 + 4 + 6 + 4 + note (54 with one line)
        Mark mark;                     // gate (cross when closed) S16, window.mark[.state]
        Label name;                    // tab, window.name[.state], elided to the room the range leaves
        Label range;                   // control, window.range, "16–22", right edge on the root's
        ProgressRule rule;             // small, nib off, width = spec.width
        Label note;                    // tick, wrapped at spec.width, window.note[.state]
        WindowMeterSpec spec;

        void setAge(pg::EntitySystem*, float age, bool animate = true);
        void setState(pg::EntitySystem*, WindowState);
        void setNote(pg::EntitySystem*, const std::string&);                 // A second line grows the root
        void setRange(pg::EntitySystem*, float from, float to);             // "16–22" (U+2013) and re-fill
        float height(pg::EntitySystem*) const;

        // Internal
        float percent() const;         // The fill of spec.age over the range, 0-100
        void fitName(pg::EntitySystem*);
        void paint();
        void resize(pg::EntitySystem*);
    };

    WindowMeter makeWindowMeter(pg::EntitySystem*, const WindowMeterSpec&);

    // "16–22": whole ages bare, others to one decimal, an en dash between.
    std::string windowRangeText(float from, float to);
}
