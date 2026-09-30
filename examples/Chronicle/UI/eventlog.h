#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "ECS/entitysystem.h"

#include "label.h"
#include "mark.h"

namespace chronicle
{
    enum class LogKind : uint8_t
    {
        Note      = 0,   // Things that happened to him: italic, muted
        Gain      = 1,
        Loss      = 2,
        Coin      = 3,
        Milestone = 4    // Full ink and weight, a gold seal
    };

    struct LogEntry
    {
        float age = 7.0f;              // 14.3 -> "14.3"; the year rubric is floor(age)
        std::string text;              // "Gored in the North Forest"
        LogKind kind = LogKind::Note;
        std::string figure;            // "" or "−9 vit": preformatted
        std::string glyph;             // "" -> by kind: seal / cross / check / gold / quill
    };

    struct EventLogSpec
    {
        float width = 300.0f;
        float height = 560.0f;         // The gutter's fixed height; the list scrolls inside it
        std::string yearPrefix = "IN HIS ";   // Rubric = prefix + ordinal(year) + " YEAR" ("IN HIS 14TH YEAR")
        std::vector<LogEntry> entries;
        std::string footnote;
        int z = 20;                    // Root and gutter z; edge z+1; list and lines z+2 (a layout holds its children at its z); marks and ages z+4; texts, figures and rubrics z+5; footnote z+1
    };

    // The running account of everything that has happened in this life, entered under the year it
    // happened in: a vellum-worn gutter with a 2 px rule-hair left edge, vermilion year rubrics, and
    // rows of age, mark, text and figure. Every row carries a mark as well as a colour.
    //
    // The whole life is kept, never truncated: rows scrolled out of the gutter are clipped, not
    // removed. New entries append at the bottom, and the view follows them only when it was
    // already at the end: a player reading further up is left where they are. Wheel scrolling is
    // the engine's (the list is a scrollable VerticalLayout). Year rubrics are inserted by the log
    // when the whole year changes - grouping what it is given, not game logic.
    struct EventLog
    {
        struct Row
        {
            pg::EntityRef line;        // PositionComponent + UiAnchor + Prefab, list width x 20
            Label age;
            Mark mark;
            Label text;
            std::optional<Label> figure;
            LogEntry entry;
        };

        struct Year
        {
            pg::EntityRef line;        // List width x 22, with 12 above unless it is the first item
            Label rubric;
            int year = 0;
        };

        pg::EntityRef root;            // PositionComponent + UiAnchor + Prefab: width x (height + 4 + footnote), or height
        pg::EntityRef gutter;          // Simple2DObject, log.gutter
        pg::EntityRef edge;            // Simple2DObject 2 px, log.edge
        pg::EntityRef list;            // The scrollable VerticalLayout (the clipper), inset 8 / 12, spacing 1
        std::vector<std::variant<Year, Row>> items;   // In order
        std::optional<Label> footnote;
        EventLogSpec spec;             // width, height, yearPrefix, footnote and z; the entries live in `items`
        int lastYear = -1;

        void append(pg::EntitySystem*, const LogEntry&);   // A rubric first when floor(age) > lastYear
        void clear(pg::EntitySystem*);
        void setFootnote(pg::EntitySystem*, const std::string&);   // "" removes
        void scrollToEnd(pg::EntitySystem*);
        bool atEnd(pg::EntitySystem*) const;                // scrollOffset >= contentHeight - viewport - 1
        size_t size() const;                                 // Rows, not rubrics

        // Internal
        float listWidth() const;
    };

    EventLog makeEventLog(pg::EntitySystem*, const EventLogSpec&);

    // "1ST", "2ND", "3RD", "4TH", "11TH", "21ST": the kit's caps.
    std::string ordinal(int n);

    // The glyph a kind draws with when the entry names none.
    std::string defaultGlyph(LogKind kind);
}
