#include "paint.h"

#include <algorithm>

#include "logger.h"

#include "2D/simple2dobject.h"
#include "2D/decoratedshapes.h"
#include "UI/ttftext.h"
#include "UI/iconsystem.h"

using namespace pg;

namespace chronicle
{
    namespace
    {
        constexpr const char * const DOM = "Chronicle.Paint";
    }

    PaintSystem::PaintSystem(const Tokens* tokens) : tokens(tokens) {}

    void PaintSystem::init()
    {
        registerAllPaintables();
    }

    void PaintSystem::onProcessEvent(const ThemeChangedEvent&)
    {
        repaintAll();
    }

    void PaintSystem::onProcessEvent(const PaintComponentChangedEvent& event)
    {
        applyNow(event.id);
    }

    template <typename Comp>
    void PaintSystem::applyTo(EntityRef ent, const constant::Vector4D& colour)
    {
        ent->get<Comp>()->setColors(colour);
    }

    template <typename Comp>
    void PaintSystem::registerPaintable()
    {
        // Skip drawables whose owning system isn't registered. Otherwise
        // registerGroup -> Group::process -> checkOneGroupType would call
        // registerFlagComponent<Comp>() -> createSystem, which corrupts the ECS if it
        // happens while running. A type with no owner can't be on any entity anyway, so
        // there is nothing to paint.
        if (not registry->hasTypeId<Comp>())
            return;

        auto group = registerGroup<PaintComponent, Comp>();

        group->addOnGroup([this](EntityRef entity) {
            auto& fns = jobs[entity->id];
            if (std::find(fns.begin(), fns.end(), &PaintSystem::applyTo<Comp>) == fns.end())
                fns.push_back(&PaintSystem::applyTo<Comp>);

            loggedUnpaintable.erase(entity->id);

            // Paint on arrival, whichever of the two components landed last
            applyNow(entity->id);
        });

        group->removeOfGroup([this](EntitySystem*, _unique_id id) {
            auto it = jobs.find(id);
            if (it == jobs.end())
                return;

            auto& fns = it->second;
            fns.erase(std::remove(fns.begin(), fns.end(), &PaintSystem::applyTo<Comp>), fns.end());

            if (fns.empty())
            {
                jobs.erase(it);
                loggedUnpaintable.erase(id);
            }
        });
    }

    void PaintSystem::registerAllPaintables()
    {
        registerPaintable<TTFText>();
        registerPaintable<Simple2DObject>();
        registerPaintable<RoundedRect2DObject>();
        registerPaintable<IconComponent>();
        registerPaintable<HatchRect2DObject>();
        registerPaintable<DottedLine2DObject>();
        registerPaintable<StrokeRect2DObject>();
    }

    void PaintSystem::applyNow(_unique_id id)
    {
        auto it = jobs.find(id);
        if (it == jobs.end())
            return;    // No paintable drawable yet: the group callback paints on arrival

        auto entity = ecsRef->getEntity(id);
        if (not entity or not entity->has<PaintComponent>())
            return;

        auto paint = entity->get<PaintComponent>();

        constant::Vector4D colour = tokens->colour(paint->token);
        colour.w *= paint->alpha;

        for (auto fn : it->second)
            fn(entity, colour);
    }

    void PaintSystem::repaintAll()
    {
        // We own PaintComponent, so a view over it is the whole working set.
        for (auto* paint : view<PaintComponent>())
        {
            if (jobs.count(paint->entityId))
                applyNow(paint->entityId);
            else if (loggedUnpaintable.insert(paint->entityId).second)
                LOG_WARNING(DOM, "Entity " << paint->entityId << " has a PaintComponent but nothing paintable");
        }
    }
}
