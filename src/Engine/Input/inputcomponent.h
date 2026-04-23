#pragma once

#include "input.h"
#include "Input/sdlevents.h"

#include "pgconstant.h"

#include "ECS/system.h"
#include "ECS/callable.h"
#include "2D/position.h"
#include "Components/ViewportComponent.generated.h"

#include <functional>
#include <memory>

namespace pg
{
    enum class MouseStateTrigger : uint8_t
    {
        OnPress = 0,
        OnRelease,
        Both
    };

    struct MouseLeftClickComponent : public Component
    {
        MouseLeftClickComponent(CallablePtr callback, const MouseStateTrigger& trigger = MouseStateTrigger::OnRelease) : callback(callback), trigger(trigger) { LOG_THIS_MEMBER("MouseLeftClickComponent"); }
        DEFAULT_COMPONENT_MEMBERS(MouseLeftClickComponent)

        CallablePtr callback;
        MouseStateTrigger trigger;
    };

    template<>
    void serialize(Archive& archive, const MouseLeftClickComponent& component);

    struct MouseRightClickComponent : public Component
    {
        MouseRightClickComponent(CallablePtr callback, const MouseStateTrigger& trigger = MouseStateTrigger::OnRelease) : callback(callback), trigger(trigger) { LOG_THIS_MEMBER("MouseRightClickComponent"); }
        DEFAULT_COMPONENT_MEMBERS(MouseRightClickComponent)

        CallablePtr callback;
        MouseStateTrigger trigger;
    };

    struct MouseLeaveClickComponent : public Component
    {
        MouseLeaveClickComponent(CallablePtr callback) : callback(callback) { LOG_THIS_MEMBER("MouseLeaveClickComponent"); }
        DEFAULT_COMPONENT_MEMBERS(MouseLeaveClickComponent)

        CallablePtr callback;
    };

    struct MouseWheelComponent : public Component
    {
        MouseWheelComponent(const StandardEvent& event) : event(event) { LOG_THIS_MEMBER("MouseWheelComponent"); }
        DEFAULT_COMPONENT_MEMBERS(MouseWheelComponent)

        StandardEvent event;
    };

    struct OnMouseClick : public Component
    {
        OnMouseClick(const Point2D& pos, const MouseButton& button) : pos(pos), button(button) { }

        DEFAULT_COMPONENT_MEMBERS(OnMouseClick)

        Point2D pos;
        MouseButton button;

        STANDARD_EVENT_CONVERTIBLE(OnMouseClick)
    };

    struct OnMouseRelease : public Component
    {
        OnMouseRelease(const Point2D& pos, const MouseButton& button) : pos(pos), button(button) { }
        DEFAULT_COMPONENT_MEMBERS(OnMouseRelease)

        Point2D pos;
        MouseButton button;

        STANDARD_EVENT_CONVERTIBLE(OnMouseRelease)
    };

    // Component that triggers a callback when the mouse enters the entity’s area.
    struct MouseEnterComponent : public Component
    {
        MouseEnterComponent(CallablePtr callback) : callback(callback) { }
        DEFAULT_COMPONENT_MEMBERS(MouseEnterComponent)

        CallablePtr callback;
    };

    // Component that triggers a callback when the mouse leaves the entity’s area.
    struct MouseLeaveComponent : public Component
    {
        MouseLeaveComponent(CallablePtr callback) : callback(callback) { }
        DEFAULT_COMPONENT_MEMBERS(MouseLeaveComponent)

        CallablePtr callback;
    };

    // SDL event structs are defined in the lightweight sdlevents.h header
    // to avoid pulling heavy system templates into files that only need events.

    struct MouseAreaZ
    {
        MouseAreaZ(_unique_id id, EntityRef ui, CompRef<PositionComponent> pos, CompRef<ViewportComponent> vp = {}) : id(id), ui(ui), pos(pos), vp(vp) { LOG_THIS_MEMBER("MouseArea"); }

        _unique_id id;
        EntityRef ui;
        CompRef<PositionComponent> pos;
        CompRef<ViewportComponent> vp;
    };

    struct MouseClickSystem : public System<Own<MouseLeftClickComponent>, Own<MouseRightClickComponent>, InitSys>
    {
        MouseClickSystem(Input* inputHandler) : inputHandler(inputHandler) { LOG_THIS_MEMBER("MouseClickSystem"); }

        virtual std::string getSystemName() const override { return "Mouse Click System"; }

        virtual void init() override;

        virtual void execute() override;

        void handleClick(const MouseButton& button, const std::set<MouseAreaZ, std::greater<>>& pressAreas, const std::set<MouseAreaZ, std::greater<>>& releaseAreas);

        void callCallback(const MouseButton& button, _unique_id id);

        Input *inputHandler;
        Point2D oldMousePos;
        std::set<MouseAreaZ, std::greater<>> mouseLeftAreaPressHolder;
        std::set<MouseAreaZ, std::greater<>> mouseLeftAreaReleaseHolder;
        std::set<MouseAreaZ, std::greater<>> mouseRightAreaPressHolder;
        std::set<MouseAreaZ, std::greater<>> mouseRightAreaReleaseHolder;
        std::unordered_map<MouseButton, bool> pressedList;
    };

    // Todo combine this in the MouseClickSystem
    struct MouseLeaveClickSystem : public System<Listener<OnMouseClick>, Own<MouseLeaveClickComponent>, InitSys, StoragePolicy>
    {
        MouseLeaveClickSystem(Input* inputHandler) : inputHandler(inputHandler) { LOG_THIS_MEMBER("MouseLeaveClickSystem"); }

        virtual std::string getSystemName() const override { return "Mouse Leave Click System"; }

        virtual void init() override
        {
            LOG_THIS_MEMBER("MouseLeaveClickSystem");

            auto group = registerGroup<PositionComponent, MouseLeaveClickComponent>();

            group->addOnGroup([this](EntityRef entity) {
                LOG_MILE("MouseLeaveClickSystem", "Add entity " << entity->id << " to ui - mouse leave click group !");

                CompRef<ViewportComponent> vp;
                if (entity->has<ViewportComponent>())
                    vp = entity->get<ViewportComponent>();

                mouseAreaHolder.emplace(entity->id, entity, entity->get<PositionComponent>(), vp);
            });

            group->removeOfGroup([this](EntitySystem*, _unique_id id) {
                LOG_MILE("MouseLeaveClickSystem", "Remove entity " << id << " of ui - mouse leave click group !");

                const auto& it = std::find_if(mouseAreaHolder.begin(), mouseAreaHolder.end(), [id](const MouseAreaZ& area) { return area.id == id; });

                if (it != mouseAreaHolder.end())
                {
                    mouseAreaHolder.erase(it);
                }
            });
        }

        virtual void onEvent(const OnMouseClick&) override
        {
            LOG_THIS_MEMBER("MouseLeaveClickSystem");

            auto mousePos = inputHandler->getMousePos();

            for (auto mouseArea : mouseAreaHolder)
            {
                if (not inClipBound(mouseArea.ui, mousePos.x, mousePos.y))
                {
                    auto comp = getComponent(mouseArea.id);

                    comp->callback->call(world());
                }
            }
        }

        Input *inputHandler;
        std::set<MouseAreaZ, std::less<>> mouseAreaHolder;
    };

    struct MouseWheelSystem : public System<Listener<OnSDLMouseWheel>, Own<MouseWheelComponent>, InitSys, StoragePolicy>
    {
        MouseWheelSystem(Input *inputHandler) : inputHandler(inputHandler) { LOG_THIS_MEMBER("MouseWheelSystem"); }

        virtual std::string getSystemName() const override { return "Mouse Wheel System"; }

        virtual void init() override
        {
            LOG_THIS_MEMBER("MouseWheelSystem");

            auto group = registerGroup<PositionComponent, MouseWheelComponent>();

            group->addOnGroup([this](EntityRef entity) {
                LOG_MILE("MouseWheelSystem", "Add entity " << entity->id << " to ui - mouse wheel group !");

                CompRef<ViewportComponent> vp;
                if (entity->has<ViewportComponent>())
                    vp = entity->get<ViewportComponent>();

                mouseAreaHolder.emplace(entity->id, entity, entity->get<PositionComponent>(), vp);
            });

            group->removeOfGroup([this](EntitySystem*, _unique_id id) {
                LOG_MILE("MouseWheelSystem", "Remove entity " << id << " of ui - mouse wheel group !");

                const auto& it = std::find_if(mouseAreaHolder.begin(), mouseAreaHolder.end(), [id](const MouseAreaZ& area) { return area.id == id; });

                if (it != mouseAreaHolder.end())
                {
                    mouseAreaHolder.erase(it);
                }
            });
        }

        virtual void onEvent(const OnSDLMouseWheel& event) override;

        Input *inputHandler;
        std::set<MouseAreaZ, std::greater<>> mouseAreaHolder;
    };

    struct MouseHoverSystem : public System<Listener<OnMouseMove>, Own<MouseEnterComponent>, Own<MouseLeaveComponent>, InitSys, StoragePolicy>
    {
        virtual std::string getSystemName() const override { return "Mouse Hover System"; }

        // On initialization, register all entities that have PositionComponent and a hover component.
        virtual void init() override
        {
            // Register group for entities with PositionComponent and MouseEnterComponent.
            auto groupEnter = registerGroup<PositionComponent, MouseEnterComponent>();

            groupEnter->addOnGroup([this](EntityRef entity) {
                // Insert the entity into our hover state map.
                hoverState[entity->id] = false;
            });

            groupEnter->removeOfGroup([this](EntitySystem*, _unique_id id) {
                auto entity = ecsRef->getEntity(id);
                // Only remove from hoverState if the entity no longer has either hover component.
                if (not entity or (not entity->has<MouseEnterComponent>() and not entity->has<MouseLeaveComponent>()))
                {
                    hoverState.erase(id);
                }
            });

            // Register group for entities with PositionComponent and MouseLeaveComponent.
            auto groupLeave = registerGroup<PositionComponent, MouseLeaveComponent>();

            groupLeave->addOnGroup([this](EntityRef entity) {
                // Insert the entity into our hover state map.
                hoverState[entity->id] = false;
            });

            groupLeave->removeOfGroup([this](EntitySystem*, _unique_id id) {
                auto entity = ecsRef->getEntity(id);
                if (not entity or (not entity->has<MouseEnterComponent>() and not entity->has<MouseLeaveComponent>()))
                {
                    hoverState.erase(id);
                }
            });
        }

        // Listen for mouse move events.
        virtual void onEvent(const OnMouseMove& event) override
        {
            Point2D mousePos = event.pos;
            // Iterate over all entities in our hover state.
            for (auto& pair : hoverState)
            {
                _unique_id entityId = pair.first;
                bool currentlyHovering = pair.second;
                auto entity = ecsRef->getEntity(entityId);

                if (not entity or (not entity->has<PositionComponent>()))
                    continue;

                bool inside = inClipBound(entity, mousePos.x, mousePos.y);

                // If the mouse has entered and wasn't previously inside...
                if (inside and not currentlyHovering)
                {
                    if (entity->has<MouseEnterComponent>())
                    {
                        auto comp = entity->get<MouseEnterComponent>();
                        comp->callback->call(world());
                    }

                    pair.second = true;
                }
                // If the mouse was inside and now has left...
                else if (not inside and currentlyHovering)
                {
                    if (entity->has<MouseLeaveComponent>())
                    {
                        auto comp = entity->get<MouseLeaveComponent>();
                        comp->callback->call(world());
                    }

                    pair.second = false;
                }
            }
        }

        // Map of entity id to whether the mouse is currently hovering.
        std::unordered_map<_unique_id, bool> hoverState;
    };

    bool operator<(MouseAreaZ lhs, MouseAreaZ rhs);
    bool operator>(MouseAreaZ lhs, MouseAreaZ rhs);

    // Helper functions to convert SDL input codes to friendly strings
    std::string scancodeToString(SDL_Scancode key);
    std::string modifierToString(Uint16 mod);

}