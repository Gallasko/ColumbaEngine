#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ECS/entitysystem.h"

#include "label.h"

namespace chronicle
{
    enum class GlossKind : uint8_t { Margin, Tooltip };

    // "Time", "6 mo" - value set in figure-sm, in the colour its tone names: "" ink, "gain", "loss",
    // "time", "muted". A heading opens a section: a hair rule across the gloss, then its label
    // alone, in caps and in the figures' weight; with no label it is the rule alone.
    struct GlossRow
    {
        std::string label;
        std::string value;
        std::string tone = "";
        bool heading = false;
    };

    struct GlossSpec
    {
        GlossKind kind = GlossKind::Margin;
        std::string title;                 // Tooltip only: gloss-title, ink
        std::string aside;                 // Tooltip only: figure-sm, ink-muted, at the right of the title (how often it was done)
        std::string text;                  // Margin: gloss italic, ink-muted, wraps. Tooltip: body-sm, ink-muted, wraps
        std::vector<GlossRow> rows;        // Tooltip only
        std::string footnote;              // Tooltip only: caption, ink-faint
        float width = 0.0f;                // 0 -> 240 (Margin) / 280 (Tooltip)
        int z = 20;                        // Margin: content band. Tooltip: ignored (placed at 200)
        bool inlineValues = false;         // Tooltip only: false, the values in a column at the right edge; true, each after its label
    };

    struct Gloss
    {
        pg::EntityRef root;
        pg::EntityRef edge;                // Margin only: the rule-hair left edge
        pg::EntityRef ground, frame;       // Tooltip only
        std::optional<Label> title, aside, text, footnote;
        std::vector<std::pair<Label, Label>> rows;
        std::vector<pg::EntityRef> rules;  // Tooltip only: the hair rule of each heading, in the rows' order
        GlossSpec spec;

        float height(pg::EntitySystem*) const;
        void setText(pg::EntitySystem*, const std::string&);   // Margin: re-wraps, updates height
    };

    Gloss makeGloss(pg::EntitySystem*, const GlossSpec&);

    // The tooltip side. A gloss to show on hover is registered once under a key; TooltipComponent{key, "gloss"}
    // on any hoverable entity then shows it through the engine's TooltipSystem.
    struct GlossRegistry : public pg::System<pg::InitSys>
    {
        std::string getSystemName() const override { return "Chronicle Gloss Registry"; }

        void init() override;                              // registers the "gloss" style on TooltipSystem
        void set(const std::string& key, GlossSpec spec);  // kind forced to Tooltip; replaces
        bool has(const std::string& key) const;
        void erase(const std::string& key);
        const GlossSpec* find(const std::string& key) const;

        Gloss lastBuilt;   // the most recently built gloss, for tests

    private:
        std::unordered_map<std::string, GlossSpec> specs;
    };

    // Convenience: attach a tooltip gloss to an entity.
    void attachGloss(pg::EntitySystem*, pg::EntityRef target, const std::string& key, uint32_t delayMs = 200);
}
