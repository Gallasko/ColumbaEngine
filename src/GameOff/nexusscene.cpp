#include "nexusscene.h"

#include "managenerator.h"
#include "gamefacts.h"

#include "2D/simple2dobject.h"
#include "UI/ttftext.h"
#include "UI/prefab.h"
#include "UI/sizer.h"

namespace pg
{
    void NexusScene::init()
    {
        // Create the basic mana generator entity.
        auto basicGen = createEntity();
        attach<ManaGenerator>(basicGen);

        maskedButtons.push_back(DynamicNexusButton{
            "TouchAltar",
            "Touch Altar",
            {   FactChecker("altar_touched", false, FactCheckEquality::Equal),
                FactChecker("startTuto", true, FactCheckEquality::Equal) },
            {   AchievementReward(AddFact{"altar_touched", ElementType{true}}) },
            "main",
            {},
            1
        });

        maskedButtons.push_back(DynamicNexusButton{
            "BasicHarvest",
            "Harvest",
            { FactChecker("altar_touched", true, FactCheckEquality::Equal) },
            { AchievementReward(StandardEvent("mana_harvest", "id", basicGen.id)) },
            "main",
            {},
            0
        });

        maskedButtons.push_back(DynamicNexusButton{
            "UpgradeProd1",
            "UpgradeProd",
            { FactChecker("altar_touched", true, FactCheckEquality::Equal) },
            { AchievementReward(StandardEvent("mana_gen_upgrade", "id", basicGen.id, "upgradeAmount", 0.5f)) },
            "main",
            {},
            0
        });

        maskedButtons.push_back(DynamicNexusButton{
            "SpellMage",
            "Spell Mage",
            {   FactChecker("altar_touched", true, FactCheckEquality::Equal),
                FactChecker("mage_tier", 0, FactCheckEquality::Equal) },
            {   AchievementReward(AddFact{"mage_tier", ElementType{1}}) },
            "main",
            {},
            1
        });

        maskedButtons.push_back(DynamicNexusButton{
            "RunicMage",
            "Runic Mage",
            {   FactChecker("altar_touched", true, FactCheckEquality::Equal),
                FactChecker("mage_tier", 0, FactCheckEquality::Equal) },
            {   AchievementReward(AddFact{"mage_tier", ElementType{1}}) },
            "main",
            {},
            1
        });

        auto layout = makeHorizontalLayout(ecsRef, 30, 150, 500, 400);

        layout.get<HorizontalLayout>()->spacing = 30;

        layout.get<HorizontalLayout>()->spacedInWidth = false;
        layout.get<HorizontalLayout>()->fitToWidth = true;

        nexusLayout = layout.entity;

        // Listen for world fact updates to log mana or upgrades.
        listenToEvent<WorldFactsUpdate>([this](const WorldFactsUpdate& event) {
            const auto& it = std::find(event.changedFacts.begin(), event.changedFacts.end(), "mana");

            if (it != event.changedFacts.end())
            {
                LOG_INFO("onManaGeneratorHarvest", "Current Mana: " << event.factMap->at("mana").get<float>());
            }

            updateDynamicButtons(*event.factMap);
        });

        listenToStandardEvent("nexus_button_clicked", [this](const StandardEvent& event) {
            auto buttonId = event.values.at("id").get<std::string>();

            auto it = std::find_if(visibleButtons.begin(), visibleButtons.end(), [buttonId](const DynamicNexusButton& button) {
                return button.id == buttonId;
            });

            if (it == visibleButtons.end())
            {
                LOG_ERROR("NexusScene", "Button not found: " << buttonId);
                return;
            }
            else
            {
                // Todo check if all the conditions are met
                for (auto it2 : it->outcome)
                {
                    it2.call(ecsRef);
                }

                it->nbClick++;

                if (it->nbClickBeforeArchive != 0 and it->nbClick >= it->nbClickBeforeArchive)
                {
                    it->archived = true;

                    auto id = it->entityId;

                    // Archive the button and remove it from the visible buttons.
                    archivedButtons.push_back(*it);
                    visibleButtons.erase(it);

                    auto layout = nexusLayout.get<HorizontalLayout>();
                    layout->removeEntity(id);
                }

                // Todo if nbclick >= nbClickBeforeArchive and nbClickBeforeArchive != 0 -> the button should be archived
            }
        });
    }

    void NexusScene::startUp()
    {
        // Additional startup logic can go here.

        auto sys = ecsRef->getSystem<WorldFacts>();

        updateDynamicButtons(sys->factMap);
    }

    void NexusScene::execute()
    {
        // This scene could be extended to update UI, handle animations, etc.
    }

    EntityRef createButtonPrefab(NexusScene *scene, const std::string& text, const std::string& id)
    {
        auto prefabEnt = makeAnchoredPrefab(scene);
        auto prefab = prefabEnt.get<Prefab>();
        auto prefabAnchor = prefabEnt.get<UiAnchor>();

        auto background = makeUiSimple2DShape(scene->ecsRef, Shape2D::Square, 130, 60, {0, 196, 0, 255});
        auto backgroundAnchor = background.get<UiAnchor>();

        scene->ecsRef->attach<MouseLeftClickComponent>(background.entity, makeCallable<StandardEvent>("nexus_button_clicked", "id", id));

        prefabAnchor->setWidthConstrain(PosConstrain{background.entity.id, AnchorType::Width});
        prefabAnchor->setHeightConstrain(PosConstrain{background.entity.id, AnchorType::Height});
        backgroundAnchor->setTopAnchor(prefabAnchor->top);
        backgroundAnchor->setLeftAnchor(prefabAnchor->left);

        auto ttfText = makeTTFText(scene->ecsRef, 0, 0, 1, "res/font/Inter/static/Inter_28pt-Light.ttf", text, 0.4f);
        auto ttfTextAnchor = ttfText.get<UiAnchor>();

        ttfTextAnchor->centeredIn(backgroundAnchor);

        prefab->addToPrefab(background.entity);
        prefab->addToPrefab(ttfText.entity);

        return prefabEnt.entity;
    }

    void NexusScene::updateButtonsVisibility(const std::unordered_map<std::string, ElementType>& factMap, std::vector<DynamicNexusButton>& in, std::vector<DynamicNexusButton>& out, bool visiblility)
    {
        auto layout = nexusLayout.get<HorizontalLayout>();

        for (auto it = in.begin(); it != in.end();)
        {
            bool reveal = true;

            if (it->neededConditionsForVisibility.empty())
            {
                for (const auto& it2 : it->conditions)
                {
                    if (not it2.check(factMap))
                    {
                        reveal = false;
                        break;
                    }
                }
            }
            else
            {
                // Check if needed conditions for visibility are met
                for (size_t idx : it->neededConditionsForVisibility)
                {
                    if (idx >= it->conditions.size() or not it->conditions[idx].check(factMap))
                    {
                        reveal = false;
                        break;
                    }
                }
            }

            if (reveal == visiblility)
            {
                if (visiblility)
                {
                    if (it->entityId != 0)
                    {
                        LOG_ERROR("Nexuse scene", "Button id: " << it->id << " should not have an entity already has we are creating right now!");
                    }
                    else
                    {
                        // Create a new entity for the button
                        auto buttonEntity = createButtonPrefab(this, it->label, it->id);
                        it->entityId = buttonEntity.id;

                        layout->addEntity(buttonEntity);
                    }
                }
                else
                {
                    if (it->entityId == 0)
                    {
                        LOG_ERROR("Nexus scene", "Button id: " << it->id << " should have an entity but it does not!");
                    }
                    else
                    {
                        LOG_INFO("Nexus scene", "Button id: " << it->entityId);
                        layout->removeEntity(it->entityId);
                    }
                }

                out.push_back(*it);  // Move button to visible
                it = in.erase(it);   // Remove from masked list
            }
            else
            {
                ++it;
            }
        }
    }

    void NexusScene::updateDynamicButtons(const std::unordered_map<std::string, ElementType>& factMap)
    {
        updateButtonsVisibility(factMap, maskedButtons, visibleButtons, true);
        updateButtonsVisibility(factMap, visibleButtons, maskedButtons, false);
    }

}
