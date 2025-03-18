#pragma once

#include "ECS/system.h"

#include "Scene/scenemanager.h"
#include "UI/listview.h"

#include "item.h"

namespace pg
{
    struct PlayerCharacter;
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

    struct InventorySystem : public System<Listener<GainItem>, Listener<LoseItem>, StoragePolicy, SaveSys>
    {
        virtual std::string getSystemName() const override { return "InventorySystem"; }

        void onEvent(const GainItem& event) override;
        void onEvent(const LoseItem& event) override;

        bool hasEnough(const Item& item) const;

        virtual void save(Archive& archive) override
        {
            for (const auto& elem : items)
            {
                serialize(archive, itemTypeToString.at(elem.first), elem.second);
            }
        }

        virtual void load(const UnserializedObject& ss) override
        {
            for (const auto& elem : itemTypeToString)
            {
                std::vector<Item> temp;

                defaultDeserialize(ss, elem.second, temp);

                items[elem.first] = temp;
            }
        }

        std::unordered_map<ItemType, std::vector<Item>> items;
    };

    struct InventoryScene : public Scene
    {
        virtual void init() override;
        virtual void startUp() override;

        void addPlayerToListView(PlayerCharacter* player);

        void populateView(const ItemType& type);

        std::unordered_map<std::string, EntityRef> inventoryUi;
        size_t nbItemShown = 0;

        PlayerCharacter *selectedCharacter;
        Item* selectedItem;

        CompRef<ListView> characterList;

        CompRef<PositionComponent> learnSkillTreeUi;
    };
}