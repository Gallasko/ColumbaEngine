#pragma once

#include <set>
#include <string>

#include "ECS/entitysystem.h"

#include "Core/tokens.h"

namespace chronicle
{
    // Which colour token an entity draws in. Chronicle-only, never serialised.
    struct PaintComponent : public pg::Component
    {
        PaintComponent() = default;
        explicit PaintComponent(std::string token) : token(std::move(token)) {}

        std::string token;   // "ink", "vermilion", "status-gain", ...
    };

    // Repaints every PaintComponent holder on ThemeChangedEvent. Knows the paintable engine
    // components: TTFText, Simple2DObject, IconComponent.
    struct PaintSystem : public pg::System<pg::Own<PaintComponent>, pg::Listener<ThemeChangedEvent>, pg::InitSys>
    {
        explicit PaintSystem(const Tokens* tokens);

        std::string getSystemName() const override { return "Chronicle Paint System"; }

        void init() override;
        void onEvent(const ThemeChangedEvent&) override;   // sets dirty = true
        void execute() override;                           // if dirty: repaintAll()

        void repaintAll();                                 // public so a test can call it without an event
        void paint(pg::EntityRef ent, const std::string& token);   // attach-or-update + apply once

    private:
        void applyColour(pg::EntityRef ent, const std::string& token);

        const Tokens* tokens;
        bool dirty = false;

        std::set<pg::_unique_id> paintedIds;
        std::set<pg::_unique_id> loggedUnpaintable;
    };
}
