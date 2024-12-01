#include "locationscene.h"

#include "UI/ttftext.h"

namespace pg
{
    namespace
    {
        struct SelectedCharacter
        {
            SelectedCharacter(PlayerCharacter *chara) : chara(chara) {}

            PlayerCharacter *chara;
        };
    }

    void LocationScene::init()
    {
        auto listView = makeListView(this, 0, 150, 300, 120);

        listView.get<ListView>()->setVisibility(false);

        characterList = listView.get<ListView>();

        auto listView2 = makeListView(this, 400, 150, 300, 400);

        locationList = listView2.get<ListView>();

        
    }

    void LocationScene::startUp()
    {
        LOG_INFO("Location", "Starting up");
        LOG_INFO("Location", "Loading players");

        for (auto player : ecsRef->view<PlayerCharacter>())
        {
            if (player)
            {
                LOG_INFO("Player", "Got player: " << player->character.name);

                addPlayerToListView(player);
            }
        }

        LOG_INFO("Location", "Loading locations");

        auto sys = ecsRef->getSystem<LocationSystem>();

        for (const auto& location : sys->locations)
        {
            addLocation(location);
        }
    }

    void LocationScene::addPlayerToListView(PlayerCharacter* player)
    {
        auto ttf = makeTTFText(this, 0, 0, "res/font/Inter/static/Inter_28pt-Light.ttf", player->character.name, 0.4);
        auto ttfUi = ttf.get<UiComponent>();
        ttfUi->setVisibility(false);

        attach<MouseLeftClickComponent>(ttf.entity, makeCallable<SelectedCharacter>(player));

        characterList->addEntity(ttfUi);
    }

    void LocationScene::addLocation(const Location& location)
    {
        auto ttf = makeTTFText(this, 0, 0, "res/font/Inter/static/Inter_28pt-Light.ttf", location.name, 0.4);
        auto ttfUi = ttf.get<UiComponent>();
        ttfUi->setVisibility(false);

        locationList->addEntity(ttfUi);
    }
}