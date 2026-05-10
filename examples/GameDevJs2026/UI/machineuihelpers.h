#pragma once

#include "ECS/entitysystem.h"
#include "2D/position.h"
#include "UI/ttftext.h"

#include "inventoryui.h"

// Free helpers shared by every machine UI implementation. Anything that
// only depends on the ECS + standard prefab/anchor APIs lives here so the
// per-machine classes don't duplicate it.

namespace pg_machineui
{
    inline void setEntityVisibility(pg::EntitySystem* ecs, uint64_t id, bool vis)
    {
        if (id == 0)
            return;
        auto ent = ecs->getEntity(id);
        if (ent)
            ent->get<pg::PositionComponent>()->setVisibility(vis);
    }

    inline void setEntityText(pg::EntitySystem* ecs, uint64_t id, const std::string& text)
    {
        if (id == 0)
            return;
        auto ent = ecs->getEntity(id);
        if (ent and ent->has<pg::TTFText>())
            ent->get<pg::TTFText>()->setText(text);
    }

    inline bool hitButtonEntity(pg::EntitySystem* ecs, uint64_t id, float mx, float my)
    {
        if (id == 0)
            return false;
        auto ent = ecs->getEntity(id);
        if (not ent)
            return false;
        auto pos = ent->get<pg::PositionComponent>();
        return mx >= pos->getX() and mx <= pos->getX() + pos->getWidth()
            and my >= pos->getY() and my <= pos->getY() + pos->getHeight();
    }

    // Anchor target for "panel sits to the left of the inventory, vertically
    // centred against it". Falls back to the main window if the inventory
    // hasn't created its panel yet. Returns 0 only if neither is available.
    inline uint64_t resolveLeftPanelAnchor(pg::EntitySystem* ecs, InventoryUISystem* inventoryUI)
    {
        uint64_t target = 0;
        if (inventoryUI)
            target = inventoryUI->getBackdropEntityId();
        if (target == 0)
        {
            auto windowEnt = ecs->getEntity("__MainWindow");
            if (windowEnt) target = windowEnt->id;
        }
        return target;
    }
}
