#pragma once

#include <set>
#include <unordered_map>
#include <vector>

#include "ECS/entitysystem.h"

#include "Core/tokens.h"
#include "PaintComponent.generated.h"

namespace chronicle
{
    // Keeps every PaintComponent holder drawn in its colour token. Fully reactive:
    // attach a PaintComponent (or call setToken/setAlpha on one) and the repaint
    // happens on its own — no manual paint() call. One ECS group per paintable
    // drawable type feeds the jobs map; adding a new paintable type is one
    // registerPaintable<T>() line in registerAllPaintables().
    struct PaintSystem : public pg::System<pg::Own<PaintComponent>,
                                           pg::QueuedListener<ThemeChangedEvent>,
                                           pg::QueuedListener<PaintComponentChangedEvent>,
                                           pg::InitSys>
    {
        explicit PaintSystem(const Tokens* tokens);

        std::string getSystemName() const override { return "Chronicle Paint System"; }

        // Registers one group per paintable drawable type. init() runs synchronously at
        // createSystem time (before the ECS is running), and Chronicle creates PaintSystem
        // after the engine's render systems exist — so every drawable type is already
        // registered and no group falls back to a flag system.
        void init() override;

        // QueuedListener: both repaints run when the event queue drains in the system's
        // execute() slot, a frame after the event was sent. That guarantees any entity or
        // component created on the same frame as the event has been committed before we
        // read it back.
        void onProcessEvent(const ThemeChangedEvent&);
        void onProcessEvent(const PaintComponentChangedEvent& event);

        void repaintAll();                                 // public so a test can call it without an event

    private:
        using ApplyFn = void (*)(pg::EntityRef, const pg::constant::Vector4D&);

        void registerAllPaintables();

        template <typename Comp>
        void registerPaintable();

        template <typename Comp>
        static void applyTo(pg::EntityRef ent, const pg::constant::Vector4D& colour);

        void applyNow(pg::_unique_id id);

        const Tokens* tokens;

        // entity id -> one apply job per paintable drawable type present on it
        std::unordered_map<pg::_unique_id, std::vector<ApplyFn>> jobs;

        std::set<pg::_unique_id> loggedUnpaintable;
    };
}
