#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ECS/entitysystem.h"

#include "Core/tokens.h"
#include "Core/textstyle.h"

#include "label.h"

namespace chronicle
{
    enum class GlossKind : uint8_t { Margin, Tooltip };

    struct GlossRow { std::string label; std::string value; };   // "Time", "6 mo" - value set in figure-sm

    struct GlossSpec
    {
        GlossKind kind = GlossKind::Margin;
        std::string title;                 // Tooltip only: gloss-title, ink
        std::string text;                  // Margin: gloss italic, ink-muted, wraps. Tooltip: body-sm, ink-muted, wraps
        std::vector<GlossRow> rows;        // Tooltip only
        std::string footnote;              // Tooltip only: caption, ink-faint
        float width = 0.0f;                // 0 -> 240 (Margin) / 280 (Tooltip)
        int z = 20;                        // Margin: content band. Tooltip: ignored (placed at 200)
    };

    struct Gloss
    {
        pg::EntityRef root;
        pg::EntityRef edge;                // Margin only: the rule-hair left edge
        pg::EntityRef ground, frame;       // Tooltip only
        std::optional<Label> title, text, footnote;
        std::vector<std::pair<Label, Label>> rows;
        GlossSpec spec;

        float height(pg::EntitySystem*) const;
        void setText(pg::EntitySystem*, const TextStyles&, const std::string&);   // Margin: re-wraps, updates height
    };

    Gloss makeGloss(pg::EntitySystem*, const Tokens&, const TextStyles&, const GlossSpec&);

    // The tooltip side. A gloss to show on hover is registered once under a key; TooltipComponent{key, "gloss"}
    // on any hoverable entity then shows it through the engine's TooltipSystem.
    struct GlossRegistry : public pg::System<pg::InitSys>
    {
        GlossRegistry(const Tokens* tokens, const TextStyles* styles);

        std::string getSystemName() const override { return "Chronicle Gloss Registry"; }

        void init() override;                              // registers the "gloss" style on TooltipSystem
        void set(const std::string& key, GlossSpec spec);  // kind forced to Tooltip; replaces
        bool has(const std::string& key) const;
        void erase(const std::string& key);
        const GlossSpec* find(const std::string& key) const;

        Gloss lastBuilt;   // the most recently built gloss, for tests

    private:
        const Tokens* tokens;
        const TextStyles* styles;
        std::unordered_map<std::string, GlossSpec> specs;
    };

    // Convenience: attach a tooltip gloss to an entity.
    void attachGloss(pg::EntitySystem*, pg::EntityRef target, const std::string& key, uint32_t delayMs = 200);
}
