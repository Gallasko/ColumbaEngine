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
#include <vector>

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
        OnMouseRelease(const Point2D& pos, const MouseButton& button, bool cancelled = false) : pos(pos), button(button), cancelled(cancelled) { }
        DEFAULT_COMPONENT_MEMBERS(OnMouseRelease)

        Point2D pos;
        MouseButton button;

        // The press became something else (a drag that scrolled a layout): the release ends it,
        // it is not a click. Release areas are not called; listeners should not act on it.
        bool cancelled = false;

        STANDARD_EVENT_CONVERTIBLE(OnMouseRelease)
    };

    // Sent while a button is held: the press in progress is not a click. Its release reaches the
    // listeners as OnMouseRelease{cancelled = true} and calls no release area.
    struct CancelMouseClickEvent
    {
        MouseButton button;
    };

    // Component that triggers a callback when the mouse enters the entity’s area.
    // A pass-through entity is hovered when the cursor is inside it, but never
    // occludes what is under it (it is skipped when finding the topmost entity).
    struct MouseEnterComponent : public Component
    {
        MouseEnterComponent(CallablePtr callback) : callback(callback) { }
        MouseEnterComponent(CallablePtr callback, bool passThrough) : callback(callback), passThrough(passThrough) { }
        DEFAULT_COMPONENT_MEMBERS(MouseEnterComponent)

        CallablePtr callback;
        bool passThrough = false;
    };

    // Component that triggers a callback when the mouse leaves the entity’s area.
    struct MouseLeaveComponent : public Component
    {
        MouseLeaveComponent(CallablePtr callback) : callback(callback) { }
        MouseLeaveComponent(CallablePtr callback, bool passThrough) : callback(callback), passThrough(passThrough) { }
        DEFAULT_COMPONENT_MEMBERS(MouseLeaveComponent)

        CallablePtr callback;
        bool passThrough = false;
    };

    // Emitted by MouseHoverSystem after each move, once the topmost-only hover
    // diff is computed. Consumers (e.g. TooltipSystem) react to this instead of
    // running their own hit-testing.
    struct HoverChangedEvent
    {
        std::vector<_unique_id> entered;
        std::vector<_unique_id> left;
        Point2D pos;
    };

    // Asks MouseHoverSystem to work the hover out again where the mouse last was. The hover is
    // only computed on a move: what is built or moved under a mouse that stays still is not
    // hovered until then. Send it once such entities are in place
    struct RefreshHoverEvent {};

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

    struct MouseClickSystem : public System<Own<MouseLeftClickComponent>, Own<MouseRightClickComponent>, Listener<CancelMouseClickEvent>, InitSys>
    {
        MouseClickSystem(Input* inputHandler) : inputHandler(inputHandler) { LOG_THIS_MEMBER("MouseClickSystem"); }

        virtual std::string getSystemName() const override { return "Mouse Click System"; }

        virtual void init() override;

        virtual void execute() override;

        virtual void onEvent(const CancelMouseClickEvent& event) override;

        void handleClick(const MouseButton& button, const std::set<MouseAreaZ, std::greater<>>& pressAreas, const std::set<MouseAreaZ, std::greater<>>& releaseAreas);

        void callCallback(const MouseButton& button, _unique_id id);

        Input *inputHandler;
        Point2D oldMousePos;
        std::set<MouseAreaZ, std::greater<>> mouseLeftAreaPressHolder;
        std::set<MouseAreaZ, std::greater<>> mouseLeftAreaReleaseHolder;
        std::set<MouseAreaZ, std::greater<>> mouseRightAreaPressHolder;
        std::set<MouseAreaZ, std::greater<>> mouseRightAreaReleaseHolder;
        std::unordered_map<MouseButton, bool> pressedList;
        std::unordered_map<MouseButton, bool> cancelledList;   // The press in progress is not a click
    };

    // Todo combine this in the MouseClickSystem
    struct MouseLeaveClickSystem : public System<Listener<OnMouseClick>, Own<MouseLeaveClickComponent>, InitSys, StoragePolicy>
    {
        MouseLeaveClickSystem(Input* inputHandler) : inputHandler(inputHandler) { LOG_THIS_MEMBER("MouseLeaveClickSystem"); }

        virtual std::string getSystemName() const override { return "Mouse Leave Click System"; }

        virtual void init() override;

        virtual void onEvent(const OnMouseClick&) override;

        Input *inputHandler;
        std::set<MouseAreaZ, std::less<>> mouseAreaHolder;
    };

    struct MouseWheelSystem : public System<Listener<OnSDLMouseWheel>, Own<MouseWheelComponent>, InitSys, StoragePolicy>
    {
        MouseWheelSystem(Input *inputHandler) : inputHandler(inputHandler) { LOG_THIS_MEMBER("MouseWheelSystem"); }

        virtual std::string getSystemName() const override { return "Mouse Wheel System"; }

        virtual void init() override;

        virtual void onEvent(const OnSDLMouseWheel& event) override;

        Input *inputHandler;
        std::set<MouseAreaZ, std::greater<>> mouseAreaHolder;
    };

    struct MouseHoverSystem : public System<Listener<OnMouseMove>, Listener<RefreshHoverEvent>, Own<MouseEnterComponent>, Own<MouseLeaveComponent>, InitSys, StoragePolicy>
    {
        virtual std::string getSystemName() const override { return "Mouse Hover System"; }

        // On initialization, register all entities that have PositionComponent and a hover component.
        virtual void init() override;

        // Listen for mouse move events. Fires enter/leave only for the topmost
        // entity (and its (viewport, z) peers), then emits a HoverChangedEvent.
        virtual void onEvent(const OnMouseMove& event) override;

        // The same where the mouse last was, for what changed under it. Nothing before a first
        // move, and no HoverChangedEvent when nothing was entered or left.
        virtual void onEvent(const RefreshHoverEvent& event) override;

        // Computes the hovered set at mousePos and fires the enter/leave callbacks. The
        // HoverChangedEvent is sent when something changed, or always when announce is true.
        void updateHover(const Point2D& mousePos, bool announce);

        // Map of entity id to whether the mouse is currently hovering.
        std::unordered_map<_unique_id, bool> hoverState;

        // Where the last OnMouseMove was
        Point2D lastMousePos;

        bool moved = false;
    };

    bool operator<(MouseAreaZ lhs, MouseAreaZ rhs);
    bool operator>(MouseAreaZ lhs, MouseAreaZ rhs);

    // Helper functions to convert SDL input codes to friendly strings
    std::string scancodeToString(SDL_Scancode key);
    std::string modifierToString(Uint16 mod);

}