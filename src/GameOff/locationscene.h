#pragma once

#include "location.h"
#include "customlocation.h"

#include "characustomizationscene.h"

#include "Scene/scenemanager.h"

#include "UI/listview.h"

namespace pg
{
    struct LocationSystem : public System<StoragePolicy>
    {
        std::vector<Location> locations = { SlimeForest{} };
    };

    struct LocationScene : public Scene
    {
        virtual void init() override;
        virtual void startUp() override;

        void addPlayerToListView(PlayerCharacter* player);
        void addLocation(const Location& location);

        CompRef<ListView> characterList;
        CompRef<ListView> locationList;

        std::unordered_map<std::string, EntityRef> selectedCharaUi;
    };
}