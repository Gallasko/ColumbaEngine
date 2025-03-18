#include "nexusscene.h"

#include "managenerator.h"

#include "gamefacts.h"

#include "2D/simple2dobject.h"

namespace pg
{
    void NexusScene::init()
    {
        auto basicGen = createEntity();
        attach<ManaGenerator>(basicGen);

        auto basicHarvest = makeUiSimple2DShape(this, Shape2D::Square, 60, 60, {0.0f, 196.0f, 0.0f, 255.0f});
        basicHarvest.get<PositionComponent>()->setX(30);
        basicHarvest.get<PositionComponent>()->setY(80);

        ecsRef->attach<MouseLeftClickComponent>(basicHarvest.entity, makeCallable<OnManaGeneratorHarvest>(basicGen.id));

        listenToEvent<WorldFactsUpdate>([this](const WorldFactsUpdate& event) {
            auto it = std::find(event.changedFacts.begin(), event.changedFacts.end(), "mana");

            if (it != event.changedFacts.end())
            {
                LOG_INFO("onManaGeneratorHarvest", "Current Mana: " << event.factMap->at("mana").get<float>());
            }
        });
    }

    void NexusScene::startUp()
    {
    }

    void NexusScene::execute()
    {
    }
}