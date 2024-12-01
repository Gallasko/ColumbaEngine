#pragma once

#include "location.h"

#include "characustomizationscene.h"

#include "Scene/scenemanager.h"

#include "UI/listview.h"

namespace pg
{
    struct LocationSystem : public System<StoragePolicy>
    {
        std::vector<Location> locations;
    };

    struct LocationScene : public Scene
    {
        virtual void init() override;
        virtual void startUp() override;

        void addPlayerToListView(PlayerCharacter* player);

        CompRef<ListView> characterList;
    };
}