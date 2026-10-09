#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ECS/entitysystem.h"

#include "label.h"
#include "mark.h"

namespace chronicle
{
    // The tone of a row's mark; Relic also gilds the figure.
    enum class LedgerTone : uint8_t
    {
        None  = 0,
        Coin  = 1,
        Guild = 2,
        Relic = 3
    };

    struct LedgerRowSpec
    {
        std::string id;                // "coin", "guild.bellmoor", "relic.sunstone": the key the setters use
        std::string glyph = "gold";
        std::string name;              // "Coin"
        std::string value = "0";       // Preformatted by the scene ("412", "−3", "2 of 3"); the ledger draws strings
        std::string rate;              // "" or "+2 / mo"; a leading minus (U+2212 or '-') paints it as a loss
        LedgerTone tone = LedgerTone::None;
        bool muted = false;            // A row the player cannot use yet: ink-faint throughout
        std::string glossKey;          // "" = none; else attachGloss(line, key)
    };

    struct LedgerGroupSpec
    {
        std::string id;                // "purse"
        std::string label;             // "PURSE": passed in caps by the scene
        std::vector<LedgerRowSpec> rows;
    };

    struct ResourceLedgerSpec
    {
        float width = 288.0f;
        std::vector<LedgerGroupSpec> groups;
        int z = 20;                    // Root, headings and lines; marks and leaders z+1, texts z+2
    };

    // Everything the character holds, as a column of account-book rows grouped by kind. A row is
    // mark, name, a dotted leader, the figure and, after it, a rate. The leader is sized by the
    // engine between the name's right and the figure's left; the figures share one right edge.
    // Tones colour the mark (coin and guild ochre, relic gold-edge with a gold-edge figure);
    // muted greys the row. State beats tone: a muted relic is muted, not gold.
    //
    // Strings in, no arithmetic: every value and rate is the scene's, already formatted. A row
    // never earned this life is never added. Fed: setValue, setRate, setMuted, addRow, removeRow.
    struct ResourceLedger
    {
        struct Row
        {
            pg::EntityRef line;        // PositionComponent + UiAnchor + Prefab, width x 26
            Mark mark;
            Label name;
            pg::EntityRef leader;      // DottedLine2DObject, anchored name.right + 8 -> figure.left - 8
            Label figure;
            std::optional<Label> rate;
            LedgerRowSpec spec;
            bool shortLeader = false;  // Warned once while the leader is under 24 px
        };

        struct Group
        {
            pg::EntityRef heading;     // PositionComponent + UiAnchor + Prefab: 12 above (none for the first) + 16 + 4 below
            Label label;
            std::vector<Row> rows;
            LedgerGroupSpec spec;      // id and label; the rows live in `rows`
        };

        pg::EntityRef root;            // PositionComponent + UiAnchor + Prefab: anchor this. Height follows the body
        pg::EntityRef body;            // The VerticalLayout the headings and lines stack in, spacing 0
        std::vector<Group> groups;
        ResourceLedgerSpec spec;       // width and z; the groups live in `groups`

        void setValue(pg::EntitySystem*, const std::string& id, const std::string& value);   // The figure's right edge stays
        void setRate(pg::EntitySystem*, const std::string& id, const std::string& rate);     // "" removes
        void setMuted(pg::EntitySystem*, const std::string& id, bool muted);
        void addGroup(pg::EntitySystem*, const std::string& groupId, const std::string& label);   // Appended; a known id is left as it is
        void addRow(pg::EntitySystem*, const std::string& groupId, const LedgerRowSpec& row, const std::string& groupLabel = "");   // Appended to the group; an unknown group is created at the end with groupLabel
        void removeRow(pg::EntitySystem*, const std::string& id);
        void clear(pg::EntitySystem*);                                     // Every group and row
        Row* row(const std::string& id);
        pg::EntityRef rowEntity(const std::string& id) const;              // The row's line, empty when unknown
        Group* group(const std::string& id);
        float height(pg::EntitySystem*) const;

        // Internal
        int layoutIndexAfter(size_t groupIndex) const;   // Where the next line of that group goes in the body
    };

    ResourceLedger makeResourceLedger(pg::EntitySystem*, const ResourceLedgerSpec&);
}
