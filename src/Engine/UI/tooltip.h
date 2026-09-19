#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include "ECS/entitysystem.h"
#include "2D/position.h"
#include "Input/inputcomponent.h"
#include "Systems/coresystems.h"

namespace pg
{
    /// Attach to any entity that also has MouseEnter/MouseLeave (or let TooltipSystem attach them).
    struct TooltipComponent : public Component
    {
        TooltipComponent(const std::string& text, const std::string& style = "default") : text(text), style(style) {}
        DEFAULT_COMPONENT_MEMBERS(TooltipComponent)

        std::string text;     // what to show; a style decides how
        std::string style;    // key into TooltipSystem's builders
        uint32_t delayMs = 200;
    };

    /// Builds the visible tooltip for one style. Returns the root entity; it must carry a PositionComponent whose
    /// width/height are final after the next executeOnce (measure text with TTFTextSystem::measureText inside).
    using TooltipBuilder = std::function<EntityRef(EntitySystem& ecs, const TooltipComponent& tip)>;

    struct TooltipSystem : public System<Own<TooltipComponent>, Listener<HoverChangedEvent>, Listener<OnMouseClick>,
        Listener<ResizeEvent>, QueuedListener<TickEvent>, InitSys>
    {
        virtual std::string getSystemName() const override { return "Tooltip System"; }

        virtual void init() override;

        void registerStyle(const std::string& name, TooltipBuilder builder);
        /// The built-in "default" style: a rounded box with one TTFText line. Call once with a registered font alias.
        void setDefaultFont(const std::string& fontAlias, float scale = 1.0f);

        virtual void onEvent(const HoverChangedEvent& event) override;
        virtual void onEvent(const OnMouseClick& event) override;   // any click hides
        virtual void onEvent(const ResizeEvent& event) override;
        virtual void onProcessEvent(const TickEvent& event) override;

        // Read-only state for tests and debugging.
        bool isShowing() const { return shownFor != 0; }
        _unique_id shownFor = 0;

    private:
        void show(_unique_id target, Point2D at);
        void hide();
        void place(EntityRef root, Point2D at);

        std::unordered_map<std::string, TooltipBuilder> builders;
        _unique_id pendingTarget = 0;
        uint32_t pendingMs = 0;
        Point2D lastPos;
        EntityRef current;
        float screenWidth = 0.0f;
        float screenHeight = 0.0f;
        std::string defaultFont;
        float defaultScale = 1.0f;
    };
}
