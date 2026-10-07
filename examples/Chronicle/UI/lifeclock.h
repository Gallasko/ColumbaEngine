#pragma once

#include <string>
#include <vector>

#include "ECS/entitysystem.h"

#include "label.h"
#include "mark.h"

namespace chronicle
{
    struct ClockMilestone
    {
        float age = 0.0f;
        std::string label;             // Tooltip text for the scene; not drawn
    };

    struct ClockWindow
    {
        float from = 0.0f;
        float to = 0.0f;
        std::string label;             // Kept for the gloss the scene attaches to the band; not drawn
        bool closed = false;           // Closed: rule-ruled edges, the wash stays
    };

    struct LifeClockSpec
    {
        float width = 640.0f;          // The middle column
        float startAge = 7.0f;
        float endAge = 43.0f;          // > startAge
        float age = 7.0f;              // Clamped to [startAge, endAge]
        float runningMonths = 0.0f;    // 0 = nothing running; drawn from age forward, clipped to endAge
        std::vector<ClockMilestone> milestones;   // Outside the span: dropped with a log
        std::vector<ClockWindow> windows;         // Wholly outside the span: dropped with a log
        std::string unit = "YEARS";    // What follows the age's figure: "YEARS", or "YEARS 10 MONTHS OLD" (caps)
        std::string nextLabel;         // "Choose a path"; "" = no right-hand line
        int nextIn = -1;               // Months; < 0 = no figure
        int z = 20;                    // Root; head marks z+1, texts z+2; track z+1, lived z+2, wash z+3, running z+4, running edge and window edges z+5, frame z+6; tick marks z+1, tick labels z+2
    };

    // One whole life on a single track. The head line carries the age in figure-xl with
    // YEARS beside it and, at the right, the next milestone with the time mark and the
    // months to it. Under it the track: months lived solid, the running activity hatched
    // ahead of them with a hairline at its start, age-limited windows as a wash between
    // two ruled edges, and ticks at the milestone ages only.
    //
    // The promise: the clock advances by exactly the months the activity promised. A
    // running segment of n months, once settled by setRunning(0) and setAge(age + n / 12),
    // ends where the lived fill then ends.
    struct LifeClock
    {
        struct Band
        {
            pg::EntityRef wash;
            pg::EntityRef left;
            pg::EntityRef right;
            ClockWindow spec;
        };

        struct Tick
        {
            pg::EntityRef mark;
            Label label;
            float age = 0.0f;
        };

        pg::EntityRef root;            // Width = spec.width; height = 34 + 8 + 14 + 20 = 76
        Label age;                     // clock.age, the whole years
        Label unit;                    // clock.unit, "YEARS" or what the scene says after the figure
        Mark nextMark;                 // time S14, clock.next.mark; hidden with the right-hand line
        Label next;                    // clock.next
        Label nextIn;                  // clock.next.in, "14 mo"
        pg::EntityRef track;
        pg::EntityRef frame;
        pg::EntityRef lived;           // Carries the age tween
        pg::EntityRef running;
        pg::EntityRef runningEdge;
        std::vector<Band> windows;
        std::vector<Tick> ticks;
        LifeClockSpec spec;
        float shownAge = 0.0f;         // The age drawn while the lived fill animates

        void setAge(pg::EntitySystem*, float age, bool animate = true);          // Lived fill, tick states; the running segment re-anchors
        void setUnit(pg::EntitySystem*, const std::string& text);                // What follows the figure, on its baseline
        void setRunning(pg::EntitySystem*, float months);                        // 0 clears; never animated
        void setNext(pg::EntitySystem*, const std::string& label, int months);   // "" hides the right-hand line
        void setWindows(pg::EntitySystem*, const std::vector<ClockWindow>& windows);
        void setWindowClosed(pg::EntitySystem*, size_t index, bool closed);      // Edges only
        void setMilestones(pg::EntitySystem*, const std::vector<ClockMilestone>& milestones);

        // Root-relative x of an age on the track's inner span.
        float xFor(float age) const;

        // Internal
        void layoutAt(float shown);    // Lived, running, running edge and tick states at a drawn age
        void placeNext();              // Visibility and right anchors of the right-hand line
        void buildWindows(pg::EntitySystem*, const std::vector<ClockWindow>& windows);
        void buildTicks(pg::EntitySystem*, const std::vector<ClockMilestone>& milestones);
    };

    LifeClock makeLifeClock(pg::EntitySystem*, const LifeClockSpec&);
}
