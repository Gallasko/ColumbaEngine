#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ECS/entitysystem.h"
#include "Input/inputcomponent.h"   // HoverChangedEvent, OnMouseClick/Release
#include "Input/sdlevents.h"        // OnSDLScanCode
#include "Systems/coresystems.h"    // TickEvent

#include "label.h"
#include "mark.h"
#include "progressrule.h"
#include "requirementlist.h"
#include "focusorder.h"

namespace chronicle
{
    // Selected is the list's business: a row only carries it as a flag.
    enum class ActivityState : uint8_t
    {
        Idle    = 0,
        Running = 1,
        Locked  = 2
    };

    struct Gain
    {
        std::string stat;              // "STR"
        int amount = 0;                // +1 -> "STR +1"
    };

    struct ActivityRowSpec
    {
        std::string id;                // The activity's key ("train.yard"), carried by the events; required
        std::string name;              // "Train at the yard"
        std::string glyph = "training";
        std::string rank;              // "" or "RANK 2" (caps)
        std::string count;             // "" or "DONE 2 · 1 LEFT" (caps): written after the rank, on its label
        int months = 1;                // 0 = done at once: the cost reads "NOW"
        std::string each;              // "" or "AT THE YARD"
        std::vector<Gain> gains;       // Shown when Idle
        std::vector<Requirement> requirements;   // Shown when Locked, as a dense list
        std::string until;             // "" or "CLOSES IN 14 MO" (caps): a line of its own under the middle, in every state
        bool urgent = false;           // The closing is near: the line in the loss's colour
        bool compact = false;          // The name, the time and the closing only: no rank, no `each`, no middle block (its gloss says the rest), on a ground that says its kind
        bool tile = false;             // A compact row stacked for a grid: the name over the time (two lines of name at most), no mark. A list with a tileWidth makes its rows tiles
        float minHeight = 0.0f;        // The row is at least that tall: a list keeps the tiles of one line level
        float percent = 0.0f;          // Running
        float glideTo = 0.0f;          // Running: where the fill is moving to, from percent
        float glideMs = 0.0f;          // Running: in so many ms; 0 = not moving
        std::string caption;           // Running: "MONTH 3 OF 6"
        ActivityState state = ActivityState::Idle;
        bool stripe = false;           // Even-row ground; the list sets it
        std::string glossKey;          // "" = none
        float width = 620.0f;
        int z = 20;                    // Root and ground z; rule z+1; edge and ring z+2; marks z+3, texts z+4; the embedded rule or list root z+3
    };

    // Sent by the ActivitySystem when a list's selection changes ("" = cleared).
    struct ActivitySelectedEvent
    {
        std::string list;
        std::string id;
    };

    // Confirm to run: a second release on the selected row within 400 ms, or Enter / Space on it.
    struct ActivityActivatedEvent
    {
        std::string list;
        std::string id;
    };

    // State on the row ROOT, the hit area. Never serialised.
    struct ActivityRowState : public pg::Component
    {
        ActivityRowState() = default;

        std::string id;
        ActivityState state = ActivityState::Idle;

        bool selected = false;
        bool stripe = false;
        bool hovered = false;
        bool pressed = false;
        bool keyboardFocus = false;
        bool last = false;             // The last row of its list hides its rule
        bool compact = false;          // Its ground says its kind
        bool instant = false;          // Done at once (months 0): a transaction, not work
        bool tile = false;             // One of several on a line: no rule under it

        pg::_unique_id list = 0;       // The list root, once a list adopts the row

        pg::_unique_id ground = 0;
        pg::_unique_id rule = 0;
        pg::_unique_id edge = 0;
        pg::_unique_id ring = 0;
        pg::_unique_id mark = 0;
        pg::_unique_id name = 0;
        pg::_unique_id cost = 0;
        pg::_unique_id costMark = 0;
    };

    // One thing the character could spend months doing, in four states: idle (the gains it
    // brings), selected (a focus-ink left edge), running (a verdigris edge and mark, a progress
    // rule naming what it reaches at term) and locked (fully legible in state-locked, with what
    // it still needs as a dense requirement list). The month cost always shows.
    //
    // The row lives on its root as a component too: a list reaches it with
    // root->get<ActivityRow>(), and every setter keeps that copy and the caller's in step.
    struct ActivityRow
    {
        pg::EntityRef root;            // PositionComponent + UiAnchor + Prefab + ActivityRowState + FocusableComponent
        pg::EntityRef ground;
        pg::EntityRef rule;
        pg::EntityRef edge;
        pg::EntityRef ring;
        Mark mark;
        Label name;
        std::optional<Label> rank;
        Mark costMark;
        Label cost;
        std::optional<Label> each;
        std::optional<Label> gains;                  // Idle
        std::optional<ProgressRule> progress;        // Running
        std::optional<RequirementList> reqs;         // Locked
        std::optional<Label> until;                  // When it closes, while it has something to say
        ActivityRowSpec spec;
        float natural = 0.0f;                        // The height its content asks, before spec.minHeight

        void setState(pg::EntitySystem*, ActivityState);                 // Swaps the middle block, repaints
        void setPercent(pg::EntitySystem*, float percent, bool animate = true);   // Running only; ends a glide
        void setGlide(pg::EntitySystem*, float percent, float ms);   // Running only: the fill moves to percent in ms; 0 holds it
        void setCaption(pg::EntitySystem*, const std::string&);
        void setGains(pg::EntitySystem*, const std::vector<Gain>&);
        void setRequirements(pg::EntitySystem*, const std::vector<Requirement>&);
        void setRequirement(pg::EntitySystem*, size_t index, int current, int needed);
        void setName(pg::EntitySystem*, const std::string&);            // Elided when it no longer fits
        void setGlyph(pg::EntitySystem*, const std::string& name);
        void setMonths(pg::EntitySystem*, int);
        void setEach(pg::EntitySystem*, const std::string&);            // Only on a row built with an `each` line
        void setCount(pg::EntitySystem*, const std::string&);           // Only on a row built with a rank or a count
        void setUntil(pg::EntitySystem*, const std::string&, bool urgent = false);   // "" removes the line; the row follows its height
        void setWidth(pg::EntitySystem*, float width);                  // The name re-fits, the rule follows
        void setMinHeight(pg::EntitySystem*, float height);             // At least that tall, whatever it holds
        float height(pg::EntitySystem*) const;       // Idle 68 (16 more a wrapped line of gains), Running 81 (15 more a wrapped line of caption), Locked 24 + 4 + list + 24

        // Internal
        float middleWidth() const;
        void buildMiddle(pg::EntitySystem*);
        void clearMiddle(pg::EntitySystem*);
        void buildUntil(pg::EntitySystem*);          // The closing line as the spec says: made, rewritten or removed
        void fitName(pg::EntitySystem*);             // Grows to its text, or elides in the room left
        void resize(pg::EntitySystem*);
    };

    ActivityRow makeActivityRow(pg::EntitySystem*, const ActivityRowSpec&);

    // "STR +1   VIT -2": three spaces between pairs, U+2212 for a loss, no-break spaces inside a pair.
    std::string gainsText(const std::vector<Gain>& gains);

    struct ActivityGroup
    {
        std::string label;             // "Training"
        std::vector<ActivityRowSpec> rows;
    };

    struct ActivityListSpec
    {
        std::string id = "activities"; // Carried by the events
        float width = 620.0f;
        float height = 0.0f;           // 0 = as tall as its rows; > 0 = that tall, and the rows scroll
        bool stripes = true;           // Alternate grounds like an account book; false: every row on the same ground
        bool compact = false;          // Every row compact (ActivityRowSpec::compact), whatever its own spec says
        float tileWidth = 0.0f;        // > 0: the rows are tiles, as many to a line as fit at that width or more, 8 apart; the tiles of a line are level
        std::vector<ActivityGroup> groups;
        int z = 20;                    // Root, body and rows; the scroll thumb z+6
    };

    // State on the list root. The rows are found by walking the body, so a list filled by the
    // prefab builder (headings and rows as children in its slot) works like one built here.
    struct ActivityListState : public pg::Component
    {
        ActivityListState() = default;

        std::string id;
        std::string selected;
        bool stripes = true;
        std::vector<pg::_unique_id> rows;        // Row roots, in list order
        std::vector<pg::_unique_id> headings;    // Heading blocks, in list order
    };

    // A line of tiles in a list's body: the tiles stand side by side on it, and it is as tall as
    // the tallest. The list builds it; a row is found through it.
    struct ActivityLineState : public pg::Component
    {
        ActivityLineState() = default;

        std::vector<pg::EntityRef> tiles;        // Row roots, left to right
    };

    // A heading block: the group's name in the display face, 24 tall with space-3 above (none
    // for the first group) and space-1 below.
    struct ActivityHeadingState : public pg::Component
    {
        ActivityHeadingState() = default;

        pg::_unique_id label = 0;
    };

    // The rows of the Life screen, grouped and headed, alternating their grounds across the
    // whole list like an account book. Selection is the list's: one row at most, never a
    // locked or a running one. Given a height, the list keeps it and its rows scroll.
    struct ActivityList
    {
        pg::EntityRef root;            // PositionComponent + UiAnchor + Prefab: anchor this. Width spec.width, height spec.height or the rows'
        pg::EntityRef body;            // The VerticalLayout the rows and headings stack in, spacing 0
        pg::EntityRef scroll;          // The scroll thumb; empty when the list does not scroll
        ActivityListSpec spec;

        void select(pg::EntitySystem*, const std::string& id);          // "" clears; refuses Locked and Running (logs)
        const std::string& selected() const;
        ActivityRow* row(pg::EntitySystem*, const std::string& id);     // nullptr when unknown (logged)
        ActivityRow* find(pg::EntitySystem*, const std::string& id);    // nullptr when unknown or not laid out yet (quiet)
        void setRowState(pg::EntitySystem*, const std::string& id, ActivityState);   // Clears the selection if it was that row
        void setRows(pg::EntitySystem*, const std::vector<ActivityGroup>& groups);   // Rebuilds
        void setSize(pg::EntitySystem*, float width, float height);      // Rows and headings follow the width; height only on a list that scrolls. Tiles are laid again when the width changes how many fit
        int columns() const;           // The tiles a line holds at this width: 0 when the rows are not tiles
        float tileSize() const;        // And how wide each is: the width shared out, 8 between two
    };

    ActivityList makeActivityList(pg::EntitySystem*, const ActivityListSpec&);

    // The heading block of a group, for the list and for the ActivityGroup prefab kind.
    pg::EntityRef makeActivityHeading(pg::EntitySystem*, const std::string& label, float width, int z);

    struct ActivitySystem : public pg::System<pg::Own<ActivityRowState>, pg::Own<ActivityListState>, pg::Own<ActivityHeadingState>, pg::Own<ActivityLineState>,
        pg::Listener<pg::HoverChangedEvent>, pg::Listener<pg::OnMouseClick>, pg::Listener<pg::OnMouseRelease>,
        pg::Listener<pg::OnSDLScanCode>, pg::Listener<KeyboardFocusChangedEvent>, pg::Listener<pg::TickEvent>, pg::InitSys>
    {
        virtual std::string getSystemName() const override { return "Chronicle Activity System"; }

        virtual void init() override;

        virtual void onEvent(const pg::HoverChangedEvent& event) override;

        virtual void onEvent(const pg::OnMouseClick& event) override;

        virtual void onEvent(const pg::OnMouseRelease& event) override;

        virtual void onEvent(const pg::OnSDLScanCode& event) override;   // RETURN / SPACE only; Tab is FocusOrderSystem's

        virtual void onEvent(const KeyboardFocusChangedEvent& event) override;

        virtual void onEvent(const pg::TickEvent& event) override;

        virtual void execute() override;                                 // Adopts the rows of every list

        void applyVisual(pg::EntityRef root);                           // State and flags -> elements; public for tests

        bool select(pg::EntityRef listRoot, const std::string& id);     // False when refused
        void activate(pg::EntityRef row);

        void adopt(pg::EntityRef listRoot);                             // Walks the body: owner, stripes, last rule, first heading
        void level(pg::EntityRef listRoot);                             // The tiles of a line as tall as the tallest, and the line with them

        float now = 0.0f;              // Milliseconds of TickEvent received
        float lastReleaseAt = -1000.0f;
        pg::_unique_id lastReleased = 0;
    };

    // Guarded registration of the ActivityRow component (the factories and the list both need it).
    void registerActivityComponents(pg::EntitySystem*);
}

namespace pg
{
    // Empty archive forms (the pieces are rebuilt from their spec, never saved); in activityrow.cpp.
    template <>
    void serialize(Archive& archive, const chronicle::ActivityRow& value);

    template <>
    chronicle::ActivityRow deserialize(const UnserializedObject& serializedString);

    template <>
    void serialize(Archive& archive, const chronicle::ActivityList& value);

    template <>
    chronicle::ActivityList deserialize(const UnserializedObject& serializedString);
}
