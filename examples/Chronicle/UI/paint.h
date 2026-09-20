#pragma once

#include <set>
#include <string>

#include "ECS/entitysystem.h"

#include "Core/tokens.h"

namespace chronicle
{
    // Which colour token an entity draws in, and an alpha the token's own alpha is scaled by.
    // Chronicle-only, never serialised.
    struct PaintComponent : public pg::Component
    {
        PaintComponent() = default;
        explicit PaintComponent(std::string token, float alpha = 1.0f) : token(std::move(token)), alpha(alpha) {}

        std::string token;      // "ink", "vermilion", "status-gain", ...
        float alpha = 1.0f;     // multiplies the token's w; how a disabled control dims without a 2nd token
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
        void paint(pg::EntityRef ent, const std::string& token, float alpha = 1.0f);   // attach-or-update + apply once
        void setAlpha(pg::EntityRef ent, float alpha);     // keep the token, re-apply at a new alpha

    private:
        void applyColour(pg::EntityRef ent, const std::string& token, float alpha);

        const Tokens* tokens;

        std::set<pg::_unique_id> loggedUnpaintable;
    };
}
