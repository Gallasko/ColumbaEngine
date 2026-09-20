#include "paint.h"

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

    void PaintSystem::onProcessEvent(const ThemeChangedEvent&)
    {
        repaintAll();
    }

    void PaintSystem::repaintAll()
    {
        // We own PaintComponent, so a view over it is the whole working set.
        for (auto* paint : view<PaintComponent>())
        {
            auto entity = ecsRef->getEntity(paint->entityId);
            if (entity)
                applyColour(entity, paint->token, paint->alpha);
        }
    }

    void PaintSystem::paint(EntityRef ent, const std::string& token, float alpha)
    {
        if (ent->has<PaintComponent>())
        {
            auto pc = ent->get<PaintComponent>();
            pc->token = token;
            pc->alpha = alpha;
        }
        else
        {
            ecsRef->attach<PaintComponent>(ent, token, alpha);
        }

        applyColour(ent, token, alpha);
    }

    void PaintSystem::setAlpha(EntityRef ent, float alpha)
    {
        if (not ent->has<PaintComponent>())
            return;

        auto pc = ent->get<PaintComponent>();
        pc->alpha = alpha;
        applyColour(ent, pc->token, alpha);
    }

    void PaintSystem::applyColour(EntityRef ent, const std::string& token, float alpha)
    {
        constant::Vector4D colour = tokens->colour(token);
        colour.w *= alpha;

        bool painted = false;

        if (ent->has<TTFText>())
        {
            ent->get<TTFText>()->setColors(colour);
            painted = true;
        }
        if (ent->has<Simple2DObject>())
        {
            ent->get<Simple2DObject>()->setColors(colour);
            painted = true;
        }
        if (ent->has<IconComponent>())
        {
            ent->get<IconComponent>()->setColors(colour);
            painted = true;
        }
        if (ent->has<HatchRect2DObject>())
        {
            ent->get<HatchRect2DObject>()->setColors(colour);
            painted = true;
        }
        if (ent->has<DottedLine2DObject>())
        {
            ent->get<DottedLine2DObject>()->setColors(colour);
            painted = true;
        }
        if (ent->has<StrokeRect2DObject>())
        {
            ent->get<StrokeRect2DObject>()->setColors(colour);
            painted = true;
        }

        if (not painted and loggedUnpaintable.insert(ent->id).second)
            LOG_WARNING(DOM, "Entity " << ent->id << " has a PaintComponent but nothing paintable");
    }
}
