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
    struct PaintSystem : public pg::System<pg::Own<PaintComponent>, pg::QueuedListener<ThemeChangedEvent>>
    {
        explicit PaintSystem(const Tokens* tokens);

        std::string getSystemName() const override { return "Chronicle Paint System"; }

        // QueuedListener: the repaint runs when the event queue drains in execute(), a frame
        // after the event was sent. That guarantees any entity/component created on the same
        // frame as the theme change has been committed before we read it back.
        void onProcessEvent(const ThemeChangedEvent&);

        void repaintAll();                                 // public so a test can call it without an event
        void paint(pg::EntityRef ent, const std::string& token);   // attach-or-update + apply once

    private:
        void applyColour(pg::EntityRef ent, const std::string& token);

        const Tokens* tokens;

        std::set<pg::_unique_id> loggedUnpaintable;
    };
}
