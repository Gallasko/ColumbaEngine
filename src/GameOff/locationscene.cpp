#include "locationscene.h"

#include "UI/ttftext.h"

#include "fightscene.h"

namespace pg
{
    namespace
    {
        struct SelectedCharacter
        {
            SelectedCharacter(PlayerCharacter *chara) : chara(chara) {}

            PlayerCharacter *chara;
        };

        struct SelectedLocation
        {
            SelectedLocation(const Location& location) : location(location) {}

            Location location;
        };

        struct ChooseCharacter
        {
            ChooseCharacter(size_t index) : index(index) {}

            size_t index;
        };

        struct StartBattleClicked {};
    }

    void LocationScene::init()
    {
        auto listView = makeListView(this, 0, 260, 300, 250);

        listView.get<ListView>()->setVisibility(false);

        characterList = listView.get<ListView>();

        auto listView2 = makeListView(this, 400, 150, 300, 400);

        locationList = listView2.get<ListView>();

        auto p1 = makeTTFText(this, 0, 100, "res/font/Inter/static/Inter_28pt-Light.ttf", "Empty", 0.4);
        attach<MouseLeftClickComponent>(p1.entity, makeCallable<ChooseCharacter>(0));
        p1.get<UiComponent>()->setVisibility(false);

        auto p2 = makeTTFText(this, 150, 150, "res/font/Inter/static/Inter_28pt-Light.ttf", "Empty", 0.4);
        attach<MouseLeftClickComponent>(p2.entity, makeCallable<ChooseCharacter>(1));
        p2.get<UiComponent>()->setVisibility(false);
        
        auto p3 = makeTTFText(this, 300, 100, "res/font/Inter/static/Inter_28pt-Light.ttf", "Empty", 0.4);
        attach<MouseLeftClickComponent>(p3.entity, makeCallable<ChooseCharacter>(2));
        p3.get<UiComponent>()->setVisibility(false);

        auto startBattle = makeTTFText(this, 155, 215, "res/font/Inter/static/Inter_28pt-Light.ttf", "Start", 0.4, {255.0f, 0.0f, 0.0f, 255.0f});
        attach<MouseLeftClickComponent>(startBattle.entity, makeCallable<StartBattleClicked>());
        startBattle.get<UiComponent>()->setVisibility(false);

        selectedCharaUi["p1"] = p1.entity;
        selectedCharaUi["p2"] = p2.entity;
        selectedCharaUi["p3"] = p3.entity;
        selectedCharaUi["StartBattle"] = startBattle.entity;

        listenToEvent<ChooseCharacter>([this](const ChooseCharacter& event) {
            currentPlayerToChange = event.index;

            characterList->setVisibility(true);
        });

        listenToEvent<SelectedLocation>([this](const SelectedLocation& event) {
            const auto& location = event.location;

            selectedLocation = location;

            selectedCharaUi["p1"].get<UiComponent>()->setVisibility(true);
            selectedCharaUi["p2"].get<UiComponent>()->setVisibility(true);
            selectedCharaUi["p3"].get<UiComponent>()->setVisibility(true);
            selectedCharaUi["StartBattle"].get<UiComponent>()->setVisibility(true);
        });

        listenToEvent<SelectedCharacter>([this](const SelectedCharacter& event) {
            auto player = event.chara;

            for (size_t i = 0; i < TEAMSIZE; ++i)
            {
                if (currentTeam[i] == player)
                {
                    LOG_INFO("Location", "Player already on the team");
                    auto temp = currentTeam[currentPlayerToChange];
                    currentTeam[i] = temp;

                    std::string tempName = "Empty";

                    if (temp)
                    {
                        tempName = temp->character.name;
                    }

                    selectedCharaUi["p" + std::to_string(i + 1)].get<TTFText>()->setText(tempName);
                }
            }

            currentTeam[currentPlayerToChange] = player;
            selectedCharaUi["p" + std::to_string(currentPlayerToChange + 1)].get<TTFText>()->setText(player->character.name);
            characterList->setVisibility(false);

            selectedCharaUi["StartBattle"].get<TTFText>()->setColor({255.0f, 255.0f, 255.0f, 255.0f});
        });

        listenToEvent<StartBattleClicked>([this](const StartBattleClicked&) {
            bool canStartBattle = false;

            for (size_t i = 0; i < TEAMSIZE; ++i)
            {
                if (currentTeam[i])
                    canStartBattle = true;
            }

            if (canStartBattle)
            {
                std::vector<Character> players;

                for (size_t i = 0; i < TEAMSIZE; ++i)
                {
                    if (currentTeam[i])
                    {
                        players.push_back(currentTeam[i]->character);
                    }
                }

                ecsRef->sendEvent(StartFightAtLocation{selectedLocation, players});
            }
        });
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

        attach<MouseLeftClickComponent>(ttf.entity, makeCallable<SelectedLocation>(location));

        locationList->addEntity(ttfUi);
    }
}