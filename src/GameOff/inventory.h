#pragma once

#include "ECS/system.h"

#include "Scene/scenemanager.h"

#include "item.h"

namespace pg
{
    struct GainItem
    {
        GainItem(const Item& item) : item(item) {}

        Item item;
    };

    struct LoseItem
    {
        LoseItem(const Item& item) : item(item) {}

        Item item;
    };

    struct UseItem
    {

    };

    struct InventorySystem : public System<Listener<GainItem>, Listener<LoseItem>, StoragePolicy>
    {
        void onEvent(const GainItem& event);
        void onEvent(const LoseItem& event);

        bool hasEnough(const Item& item) const;

        std::unordered_map<ItemType, std::vector<Item>> items;
    };

    struct InventoryScene : public Scene
    {
        virtual void init() override;

        void populateView(const ItemType& type);

        std::unordered_map<std::string, EntityRef> inventoryUi;
    };
}