#include "inventory.h"

#include "UI/listview.h"

#include "UI/ttftext.h" 

namespace pg
{
    void InventorySystem::onEvent(const GainItem& event)
    {
        auto item = event.item;

        if (item.stacksize == 1)
        {
            items[item.type].push_back(item);
            return;
        }

        auto it = std::find(items[item.type].rbegin(), items[item.type].rend(), item);

        if (it != items[item.type].rend())
        {
            // A stack size of -1 means it is infinite
            if (it->stacksize == -1)
            {
                it->nbItems += item.nbItems;
                return;
            }

            int remainingSize = it->stacksize - it->nbItems;

            LOG_INFO("Inventory", remainingSize);

            if (remainingSize > 0)
            {
                if (static_cast<size_t>(remainingSize + 1) > item.nbItems)
                {
                    it->nbItems += item.nbItems;
                    return; 
                }
                else
                {
                    it->nbItems += remainingSize;
                    item.nbItems -= remainingSize;
                }
            }
        }

        if (item.stacksize == -1)
        {
            items[item.type].push_back(item);
            return;
        }

        bool haveRemainingItem = false;

        do
        {
            haveRemainingItem = false;
            auto itemToPush = item;

            if (item.nbItems >= static_cast<size_t>(item.stacksize))
            {
                itemToPush.nbItems = item.stacksize;
                item.nbItems -= item.stacksize;
                haveRemainingItem = true;
            }

            items[item.type].push_back(itemToPush);
        }
        while (haveRemainingItem);
    }

    void InventorySystem::onEvent(const LoseItem& event)
    {
        auto item = event.item;

        auto it = std::find(items[item.type].rbegin(), items[item.type].rend(), item);

        if (it == items[item.type].rend())
        {
            LOG_ERROR("Inventory", "Requested removing of an item that is not in the inventory: " << item.name);
            return;
        }

        it->nbItems -= 1;

        if (it->nbItems <= 0)
        {
            items[item.type].erase(std::next(it).base());
        }
    }

    bool InventorySystem::hasEnough(const Item& item) const
    {
        try
        {
            auto it = items.at(item.type).begin();

            do
            {
                it = std::find(it, items.at(item.type).end(), item);
            
                if (it == items.at(item.type).end())
                    return false;

                size_t nbStoredItem = it->nbItems;

                if (nbStoredItem >= item.nbItems)
                    return true;

                if (it->stacksize == -1)
                    return false;

                std::next(it);

            } while (it != items.at(item.type).end());
        }
        catch (const std::exception& e)
        {
            LOG_MILE("Inventory", "Inventory tab not available, error happend: " << e.what());
        }

        return false;
    }

    struct TitleClicked
    {
        TitleClicked(const std::string& title) : title(title) {}

        std::string title;
    };

    void InventoryScene::init()
    {
        std::vector<std::string> tabNames = {"Weapon", "Armor", "Conso", "SkillBook", "Material", "SStones", "KeyItems"};

        auto xBase = 100;

        for (auto tabName : tabNames)
        {
            auto titleTTF = makeTTFText(this, xBase, 20, "res/font/Inter/static/Inter_28pt-Light.ttf", tabName, 0.4);

            attach<MouseLeftClickComponent>(titleTTF.entity, makeCallable<TitleClicked>(tabName));

            xBase += 90;
        }

        listenToEvent<TitleClicked>([this](const TitleClicked& event) {
            std::unordered_map<std::string, ItemType> itemTypeMap = {
                {"Weapon", ItemType::Weapon},
                {"Armor", ItemType::Armor},
                {"Conso", ItemType::Consomable},
                {"SkillBook", ItemType::SkillBook},
                {"Material", ItemType::Material},
                {"SStones", ItemType::SpellStone},
                {"KeyItems", ItemType::Key}};

            try
            {
                populateView(itemTypeMap.at(event.title));
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Inventory", "Error trying to populate view: " << e.what());
            }
        });

        auto listView = makeListView(this, 50, 125, 600, 500);

        inventoryUi["InventoryView"] = listView.entity;

        auto addSword = makeTTFText(this, 0, 550, "res/font/Inter/static/Inter_28pt-Light.ttf", "Add sword", 0.5);

        attach<MouseLeftClickComponent>(addSword.entity, makeCallable<GainItem>(Item{"Copper Sword", ItemType::Weapon, 4, 3}));

        auto removeSword = makeTTFText(this, 100, 550, "res/font/Inter/static/Inter_28pt-Light.ttf", "Remove sword", 0.5);

        attach<MouseLeftClickComponent>(removeSword.entity, makeCallable<LoseItem>(Item{"Copper Sword", ItemType::Weapon, 4, 1}));
    }

    void InventoryScene::populateView(const ItemType& type)
    {
        auto view = inventoryUi["InventoryView"].get<ListView>();

        view->clear();

        auto sys = ecsRef->getSystem<InventorySystem>();

        for (auto& item : sys->items[type])
        {
            auto itemUi = makeTTFText(this, 0, 0, "res/font/Inter/static/Inter_28pt-Light.ttf", item.name + " x" + std::to_string(item.nbItems), 0.4);

            itemUi.get<UiComponent>()->setVisibility(false);

            view->addEntity(itemUi.get<UiComponent>());
        }
    }
}