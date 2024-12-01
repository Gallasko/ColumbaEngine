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

        characterList = listView.get<ListView>();
    }

    void LocationScene::startUp()
    {
        LOG_INFO("Player", "Starting up");

        for (auto player : ecsRef->view<PlayerCharacter>())
        {
            LOG_INFO("Player", "Loading players");

            if (player)
            {
                LOG_INFO("Player", "Got player: " << player->character.name);

                addPlayerToListView(player);
            }
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
}