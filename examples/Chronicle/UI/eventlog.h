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
        float age = 7.0f;              // Years; shown as year.month, months 1-12 (14.3 -> "14.4", in his 4th month); the rubric is the whole year
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
        bool years = true;             // A rubric at each new year. false: the lines alone, their age says the year
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

        // "IN HIS" and "YEAR" in the display face; the ordinal in the text face, whose figures
        // are lining (the display face's old-style figures sit below its capitals).
        struct Year
        {
            pg::EntityRef line;        // Line width x 22, with 12 above unless it is the first item
            std::optional<Label> rubric;   // The prefix, "IN HIS"; none when the prefix is empty
            Label ordinal;             // "14TH", log.year.ordinal (figure)
            Label suffix;              // "YEAR"
            std::string text;          // The whole rubric, "IN HIS 14TH YEAR"
            int year = 0;
        };

        pg::EntityRef root;            // PositionComponent + UiAnchor + Prefab: width x (height + 4 + footnote), or height
        pg::EntityRef gutter;          // Simple2DObject, log.gutter
        pg::EntityRef edge;            // Simple2DObject 2 px, log.edge
        pg::EntityRef list;            // The scrollable VerticalLayout (the clipper), inset 8 / 12 / 4, spacing 1
        pg::EntityRef scroll;          // The thumb in the list's right lane: shown when the rows overflow, draggable
        std::vector<std::variant<Year, Row>> items;   // In order
        std::optional<Label> footnote;
        EventLogSpec spec;             // width, height, yearPrefix, footnote and z; the entries live in `items`
        int lastYear = -1;

        void append(pg::EntitySystem*, const LogEntry&);   // A rubric first when floor(age) > lastYear
        void clear(pg::EntitySystem*);
        void setFootnote(pg::EntitySystem*, const std::string&);   // "" removes
        void setHeight(pg::EntitySystem*, float height);            // The well's; the list inside follows
        void scrollToEnd(pg::EntitySystem*);
        pg::EntityRef lightLast(pg::EntitySystem*);            // A ground under the last line, log.line.new: the caller's to dim again
        void dim(pg::EntitySystem*, pg::_unique_id light);    // The ground lightLast gave, gone; nothing if its line already is
        bool atEnd(pg::EntitySystem*) const;                // scrollOffset >= contentHeight - viewport - 1
        size_t size() const;                                 // Rows, not rubrics

        // Internal
        float lineWidth() const;       // The lines; the list is 8 px wider, the thumb's lane
    };

    EventLog makeEventLog(pg::EntitySystem*, const EventLogSpec&);

    // "1ST", "2ND", "3RD", "4TH", "11TH", "21ST": the kit's caps.
    std::string ordinal(int n);

    // An age as the log writes it: the year and the month in it, 1 to 12 ("14.4", "16.12").
    std::string logAge(float age);

    // The whole year an age falls in, counted in the same whole months as logAge.
    int logYear(float age);

    // The glyph a kind draws with when the entry names none.
    std::string defaultGlyph(LogKind kind);
}
