#pragma once

#include "ECS/system.h"

#include "uisystem.h"

namespace pg
{
    struct Scrollable
    {
        EntityRef horizontalSlider;
        EntityRef horizontalCursor;

        EntityRef verticalSlider;
        EntityRef verticalCursor;

        std::vector<EntityRef> entities;
    };

    struct ScrollableSystem : public System<Own<Scrollable>, StoragePolicy>
    {
        virtual std::string getSystemName() const override { return "Scrollable System"; }

        void addChild(EntityRef list, EntityRef child);
        
        void removeChild(EntityRef list, EntityRef child);
    };

}