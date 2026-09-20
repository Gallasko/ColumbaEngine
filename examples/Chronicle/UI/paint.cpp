#include "paint.h"

#include "logger.h"

#include "2D/simple2dobject.h"
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
                applyColour(entity, paint->token);
        }
    }

    void PaintSystem::paint(EntityRef ent, const std::string& token)
    {
        if (ent->has<PaintComponent>())
            ent->get<PaintComponent>()->token = token;
        else
            ecsRef->attach<PaintComponent>(ent, token);

        applyColour(ent, token);
    }

    void PaintSystem::applyColour(EntityRef ent, const std::string& token)
    {
        const constant::Vector4D colour = tokens->colour(token);

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

        if (not painted and loggedUnpaintable.insert(ent->id).second)
            LOG_WARNING(DOM, "Entity " << ent->id << " has a PaintComponent but nothing paintable");
    }
}
