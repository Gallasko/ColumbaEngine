#pragma once

#include "ECS/system.h"

namespace pg
{
    struct HorizontalLayout: public Ctor
    {
        void onCreation(EntityRef entity) override
        {
            id = entity.id;
            ecsRef = entity.ecsRef;
        }

        std::vector<EntityRef> entities;

        _unique_id id;

        EntitySystem *ecsRef;
    };
}