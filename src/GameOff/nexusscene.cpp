#include "nexusscene.h"

#include "managenerator.h"
#include "gamefacts.h"

#include "2D/simple2dobject.h"
#include "UI/ttftext.h"

namespace pg
{
    void NexusScene::init()
    {
        // Create the basic mana generator entity.
        auto basicGen = createEntity();
        attach<ManaGenerator>(basicGen);

        // Create the Harvest UI element.
        auto basicHarvest = makeUiSimple2DShape(this, Shape2D::Square, 60, 60, {0, 196, 0, 255});
        auto basicHarvestAnchor = basicHarvest.get<UiAnchor>();
        basicHarvest.get<PositionComponent>()->setX(30);
        basicHarvest.get<PositionComponent>()->setY(80);
        ecsRef->attach<MouseLeftClickComponent>(basicHarvest.entity, makeCallable<OnManaGeneratorHarvest>(basicGen.id));

        auto harvestText = makeTTFText(this, 0, 0, 1, "res/font/Inter/static/Inter_28pt-Light.ttf", "Harvest", 0.4f);
        auto harvestTextAnchor = harvestText.get<UiAnchor>();
        
        harvestTextAnchor->centeredIn(basicHarvestAnchor);

        // Create an Upgrade UI element.
        auto upgradeButton = makeUiSimple2DShape(this, Shape2D::Square, 60, 60, {196, 196, 0, 255});
        auto upgradeButtonAnchor = upgradeButton.get<UiAnchor>();
        upgradeButton.get<PositionComponent>()->setX(110);
        upgradeButton.get<PositionComponent>()->setY(80);
        // In this example, clicking the upgrade button increases production rate by 0.5.
        ecsRef->attach<MouseLeftClickComponent>(upgradeButton.entity, makeCallable<UpgradeManaGeneratorProductionEvent>(basicGen.id, 0.5f));

        auto upgradeProdText = makeTTFText(this, 0, 0, 1, "res/font/Inter/static/Inter_28pt-Light.ttf", "UpgradeProd", 0.4f);
        auto upgradeProdAnchor = upgradeProdText.get<UiAnchor>();
        
        upgradeProdAnchor->centeredIn(upgradeButtonAnchor);
        
        // Listen for world fact updates to log mana or upgrades.
        listenToEvent<WorldFactsUpdate>([this](const WorldFactsUpdate& event) {
            const auto& it = std::find(event.changedFacts.begin(), event.changedFacts.end(), "mana");

            if (it != event.changedFacts.end())
            {
                LOG_INFO("onManaGeneratorHarvest", "Current Mana: " << event.factMap->at("mana").get<float>());
            }
        });
    }

    void NexusScene::startUp()
    {
        // Additional startup logic can go here.
    }

    void NexusScene::execute()
    {
        // This scene could be extended to update UI, handle animations, etc.
    }
}
