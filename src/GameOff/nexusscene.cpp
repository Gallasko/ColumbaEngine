#include "nexusscene.h"

#include "managenerator.h"
#include "gamefacts.h"

#include "2D/simple2dobject.h"
#include "UI/ttftext.h"
#include "UI/prefab.h"
#include "UI/sizer.h"

#include "Interpreter/pginterpreter.h"
#include "Systems/logmodule.h"
#include "gamemodule.h"

namespace pg
{
    namespace
    {
        struct OnBackgroundButtonHover
        {
            std::string buttonId;

            _unique_id id;

            bool state;
        };
    }

    template <>
    void serialize(Archive& archive, const DynamicNexusButton& value)
    {
        archive.startSerialization("DynamicNexusButton");

        serialize(archive, "id", value.id);
        serialize(archive, "label", value.label);
        serialize(archive, "conditions", value.conditions);
        serialize(archive, "outcome", value.outcome);
        serialize(archive, "category", value.category);
        serialize(archive, "neededConditionsForVisibility", value.neededConditionsForVisibility);
        serialize(archive, "nbClickBeforeArchive", value.nbClickBeforeArchive);
        serialize(archive, "description", value.description);
        serialize(archive, "nbClick", value.nbClick);
        serialize(archive, "archived", value.archived);

        archive.endSerialization();
    }

    template <>
    DynamicNexusButton deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS("DynamicNexusButton");

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR("DynamicNexusButtonr", "Element is null");
        }
        else
        {
            LOG_MILE("DynamicNexusButtonr", "Deserializing DynamicNexusButton");

            DynamicNexusButton data;

            defaultDeserialize(serializedString, "id", data.id);
            defaultDeserialize(serializedString, "label", data.label);
            defaultDeserialize(serializedString, "conditions", data.conditions);
            defaultDeserialize(serializedString, "outcome", data.outcome);
            defaultDeserialize(serializedString, "category", data.category);
            defaultDeserialize(serializedString, "neededConditionsForVisibility", data.neededConditionsForVisibility);
            defaultDeserialize(serializedString, "nbClickBeforeArchive", data.nbClickBeforeArchive);
            defaultDeserialize(serializedString, "description", data.description);
            defaultDeserialize(serializedString, "nbClick", data.nbClick);
            defaultDeserialize(serializedString, "archived", data.archived);

            return data;
        }

        return DynamicNexusButton{};
    }

    class CreateNexusButton : public Function
    {
        using Function::Function;
    public:
        void setUp()
        {
            setArity(0, 0);
        }

        virtual ValuablePtr call(ValuableQueue&) override
        {
            DynamicNexusButton button;

            auto list = serializeToInterpreter(this, button);

            return list;
        }
    };

    class RegisterNexusButton : public Function
    {
        using Function::Function;
    public:
        void setUp(NexusScene *scene)
        {
            this->scene = scene;

            setArity(1, 1);
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            auto nexusButton = args.front();
            args.pop();

            auto button = deserializeTo<DynamicNexusButton>(nexusButton);

            scene->maskedButtons.push_back(button);

            return nullptr;
        }

        NexusScene *scene;
    };

    class TrackNewResource : public Function
    {
        using Function::Function;
    public:
        void setUp(NexusScene *scene)
        {
            this->scene = scene;

            setArity(1, 1);
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            auto resName = args.front()->getElement().toString();
            args.pop();

            scene->addResourceDisplay(resName);

            return nullptr;
        }

        NexusScene *scene;
    };

    class CreateGenerator : public Function
    {
        using Function::Function;
    public:
        void setUp(NexusScene *scene)
        {
            this->scene = scene;

            setArity(1, 4);
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            // Todo check type of elements gotten here

            RessourceGenerator gen;

            gen.ressource = args.front()->getElement().toString();
            args.pop();

            if (not args.empty())
            {
                gen.currentMana = args.front()->getElement().get<float>();
                args.pop();
            }

            if (not args.empty())
            {
                gen.productionRate = args.front()->getElement().get<float>();
                args.pop();
            }

            if (not args.empty())
            {
                gen.capacity = args.front()->getElement().get<float>();
                args.pop();
            }

            auto ent = scene->createEntity();
            scene->attach<RessourceGenerator>(ent, gen);

            return makeVar(ent.id);
        }

        NexusScene *scene;
    };

    struct NexusModule : public SysModule
    {
        NexusModule(NexusScene *scene)
        {
            addSystemFunction<CreateNexusButton>("NexusButton");
            addSystemFunction<RegisterNexusButton>("registerNexusButton", scene);
            addSystemFunction<TrackNewResource>("addResourceDisplay", scene);
            addSystemFunction<CreateGenerator>("createGenerator", scene);

            //Todo add basic generator / converter ids as system vars
            //addSystemVar("SCANCODE_A", SDL_SCANCODE_A);
        }
    };

    void NexusScene::init()
    {
        // Create the basic mana generator entity.
        auto basicGen = createEntity();
        attach<RessourceGenerator>(basicGen);

        // Mana -> Scrap converter
        auto converterEntity = createEntity();
        auto convComp = attach<ConverterComponent>(converterEntity);
        convComp->input = {"mana"};
        convComp->output = {"scrap"};
        convComp->cost = {5.0f};
        convComp->yield = {1.0f};

        PgInterpreter interpreter;

        interpreter.addSystemModule("nexus", NexusModule{this});
        interpreter.addSystemModule("log", LogModule{nullptr});
        interpreter.addSystemModule("achievement", AchievementModule{});

        interpreter.interpretFromFile("nexus.pg");

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
            { AchievementReward(StandardEvent("res_harvest", "id", basicGen.id)) },
            "main",
            {},
            0
        });

        maskedButtons.push_back(DynamicNexusButton{
            "ScrapConverter",
            "Convert [Scrap]",
            { FactChecker("altar_touched", true, FactCheckEquality::Equal) },
            { AchievementReward(StandardEvent(ConverterTriggeredEventName, "id", converterEntity.id)) },
            "main",
            {},
            0
        });

        maskedButtons.push_back(DynamicNexusButton{
            "Test_012",
            "Test show",
            { FactChecker("total_mana", 1, FactCheckEquality::GreaterEqual),
              FactChecker("mana", 25, FactCheckEquality::GreaterEqual) },
            { AchievementReward(StandardEvent("res_gen_upgrade", "id", basicGen.id, "upgradeAmount", 0.5f)) },
            "main",
            {0},
            5
        });

        maskedButtons.push_back(DynamicNexusButton{
            "UpgradeProd1",
            "UpgradeProd",
            { FactChecker("altar_touched", true, FactCheckEquality::Equal) },
            { AchievementReward(StandardEvent("res_gen_upgrade", "id", basicGen.id, "upgradeAmount", 0.5f)) },
            "main",
            {},
            5
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
            {   AchievementReward(AddFact{"mage_tier", ElementType{1}}),
                AchievementReward(AddFact{"runic_mage_1", ElementType{true}}),
                AchievementReward(StandardEvent("gamelog", "message", "You feel the power of the rune, coursing through your blood")) },
            "main",
            {},
            1
        });

        maskedButtons.push_back(DynamicNexusButton{
            "RunicMage2",
            "Runic Mage 2",
            {   FactChecker("runic_mage_1", true, FactCheckEquality::Equal),
                FactChecker("mage_tier", 1, FactCheckEquality::Equal) },
            {   AchievementReward(AddFact{"mage_tier", ElementType{2}}) },
            "main",
            {},
            1
        });

        auto windowEnt = ecsRef->getEntity("__MainWindow");
        auto windowAnchor = windowEnt->get<UiAnchor>();

        auto listView = makeListView(this, 1, 1, 150, 1);

        auto listViewComp = listView.get<ListView>();
        listViewComp->spacing = 8;

        auto listViewUi = listView.get<UiAnchor>();

        listViewUi->setTopAnchor(windowAnchor->top);
        listViewUi->setTopMargin(120);
        listViewUi->setBottomAnchor(windowAnchor->bottom);
        listViewUi->setLeftAnchor(windowAnchor->left);
        listViewUi->setLeftMargin(10);

        resLayout = listView.entity;

        addResourceDisplay("mana");
        addResourceDisplay("scrap");

        auto layout = makeHorizontalLayout(ecsRef, 30, 150, 500, 400);
        auto layoutAnchor = layout.get<UiAnchor>();

        layoutAnchor->setLeftAnchor(listViewUi->right);
        layoutAnchor->setLeftMargin(10);

        layout.get<HorizontalLayout>()->spacing = 30;

        layout.get<HorizontalLayout>()->spacedInWidth = false;
        layout.get<HorizontalLayout>()->fitToWidth = true;

        nexusLayout = layout.entity;

        // [Begin] Tooltip Ui definition

        auto tooltipBg = makeUiSimple2DShape(this, Shape2D::Square, 100, 70, {18, 18, 18, 255});
        tooltipBg.get<PositionComponent>()->setVisibility(false);
        tooltipBg.get<PositionComponent>()->setZ(5);
        auto tooltipBgAnchor = tooltipBg.get<UiAnchor>();

        auto tooltipBgHighLight = makeUiSimple2DShape(this, Shape2D::Square, 102, 72, {255, 255, 255, 255});
        tooltipBgHighLight.get<PositionComponent>()->setVisibility(false);
        tooltipBgHighLight.get<PositionComponent>()->setZ(4);
        auto tooltipBgHighLightAnchor = tooltipBgHighLight.get<UiAnchor>();

        tooltipBgHighLightAnchor->setTopAnchor(tooltipBgAnchor->top);
        tooltipBgHighLightAnchor->setTopMargin(-1);
        tooltipBgHighLightAnchor->setLeftAnchor(tooltipBgAnchor->left);
        tooltipBgHighLightAnchor->setLeftMargin(-1);

        auto descTextEnt = makeTTFText(this, 0, 0, 6, "res/font/Inter/static/Inter_28pt-Light.ttf", "", 0.4f);
        auto descTextPos = descTextEnt.get<PositionComponent>();
        auto descText = descTextEnt.get<TTFText>();
        auto descTextAnchor = descTextEnt.get<UiAnchor>();

        descTextPos->setVisibility(false);
        
        descText->setWrap(true);

        descTextAnchor->setTopAnchor(tooltipBgAnchor->top);
        descTextAnchor->setTopMargin(5);
        descTextAnchor->setLeftAnchor(tooltipBgAnchor->left);
        descTextAnchor->setLeftMargin(5);
        descTextAnchor->setRightAnchor(tooltipBgAnchor->right);
        descTextAnchor->setRightMargin(5);

        tooltipsEntities["background"] = tooltipBg.entity;
        tooltipsEntities["backHighlight"] = tooltipBgHighLight.entity;
        tooltipsEntities["desc"] = descTextEnt.entity;

        // [End] Tooltip Ui definition

        // Listen for world fact updates to log mana or upgrades.
        listenToEvent<WorldFactsUpdate>([this](const WorldFactsUpdate& event) {
            const auto& it = std::find(event.changedFacts.begin(), event.changedFacts.end(), "mana");

            if (it != event.changedFacts.end())
            {
                LOG_INFO("onRessourceGeneratorHarvest", "Current Mana: " << event.factMap->at("mana").get<float>());
            }

            const auto& scrapIt = std::find(event.changedFacts.begin(), event.changedFacts.end(), "scrap");

            if (scrapIt != event.changedFacts.end())
            {
                LOG_INFO("onRessourceGeneratorHarvest", "Current Scrap: " << event.factMap->at("scrap").get<float>());
            }

            updateDynamicButtons(*event.factMap);
            updateRessourceView();
        });

        listenToEvent<OnBackgroundButtonHover>([this](const OnBackgroundButtonHover& event) {
            auto bgEnt = ecsRef->getEntity(event.id);

            auto buttonId = event.buttonId;

            auto it = std::find_if(visibleButtons.begin(), visibleButtons.end(), [buttonId](const DynamicNexusButton& button) {
                return button.id == buttonId;
            });

            if (it == visibleButtons.end())
            {
                LOG_ERROR("NexusScene", "Button not found: " << buttonId);
                return;
            }

            if (bgEnt)
            {
                if (event.state)
                {
                    tooltipsEntities["background"]->get<PositionComponent>()->setVisibility(true);
                    tooltipsEntities["backHighlight"]->get<PositionComponent>()->setVisibility(true);

                    if (bgEnt->has<UiAnchor>())
                    {
                        tooltipsEntities["background"]->get<UiAnchor>()->setTopAnchor(bgEnt->get<UiAnchor>()->bottom);
                        tooltipsEntities["background"]->get<UiAnchor>()->setTopMargin(-2);
                        tooltipsEntities["background"]->get<UiAnchor>()->setHorizontalCenter(bgEnt->get<UiAnchor>()->horizontalCenter);
                    }

                    if (it->description != "")
                    {
                        tooltipsEntities["desc"]->get<PositionComponent>()->setVisibility(true);
                        tooltipsEntities["desc"]->get<TTFText>()->setText(it->description);
                    }
                }
                else
                {
                    tooltipsEntities["background"]->get<PositionComponent>()->setVisibility(false);
                    tooltipsEntities["backHighlight"]->get<PositionComponent>()->setVisibility(false);

                    tooltipsEntities["desc"]->get<PositionComponent>()->setVisibility(false);
                }

                if (not it->clickable)
                    return;

                if (bgEnt->has<Simple2DObject>())
                {
                    if (event.state)
                    {
                        bgEnt->get<Simple2DObject>()->setColors({0, 255, 0, 255});
                    }
                    else
                    {
                        bgEnt->get<Simple2DObject>()->setColors({0, 196, 0, 255});
                    }
                }
            }
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
                if (not it->clickable)
                {
                    LOG_WARNING("NexusScene", "Button is not clickable: " << buttonId);
                    return;
                }

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

        updateRessourceView();
    }

    void NexusScene::execute()
    {
        if (newRes)
        {
            updateRessourceView();
            newRes = false;
        }

        while (not resourceToBeDisplayed.empty())
        {
            const auto& res = resourceToBeDisplayed.front();

            _addResourceDisplay(res);

            resourceToBeDisplayed.pop();

            newRes = true;
        }

        // This scene could be extended to update UI, handle animations, etc.
    }

    EntityRef createButtonPrefab(NexusScene *scene, const std::string& text, const std::string& id, DynamicNexusButton* button)
    {
        auto prefabEnt = makeAnchoredPrefab(scene);
        auto prefab = prefabEnt.get<Prefab>();
        auto prefabAnchor = prefabEnt.get<UiAnchor>();

        constant::Vector4D colors = {0, 196, 0, 255};

        if (not button->clickable)
        {
            colors = {196, 0, 0, 255};
        }

        auto background = makeUiSimple2DShape(scene->ecsRef, Shape2D::Square, 130, 60, colors);
        auto backgroundAnchor = background.get<UiAnchor>();

        scene->ecsRef->attach<MouseLeftClickComponent>(background.entity, makeCallable<StandardEvent>("nexus_button_clicked", "id", id));

        scene->ecsRef->attach<MouseEnterComponent>(background.entity, makeCallable<OnBackgroundButtonHover>(OnBackgroundButtonHover{button->id, background.entity.id, true}));
        scene->ecsRef->attach<MouseLeaveComponent>(background.entity, makeCallable<OnBackgroundButtonHover>(OnBackgroundButtonHover{button->id, background.entity.id, false}));

        prefabAnchor->setWidthConstrain(PosConstrain{background.entity.id, AnchorType::Width});
        prefabAnchor->setHeightConstrain(PosConstrain{background.entity.id, AnchorType::Height});
        backgroundAnchor->setTopAnchor(prefabAnchor->top);
        backgroundAnchor->setLeftAnchor(prefabAnchor->left);

        auto ttfText = makeTTFText(scene->ecsRef, 0, 0, 1, "res/font/Inter/static/Inter_28pt-Light.ttf", text, 0.4f);
        auto ttfTextAnchor = ttfText.get<UiAnchor>();

        ttfTextAnchor->centeredIn(backgroundAnchor);

        prefab->addToPrefab(background.entity);
        prefab->addToPrefab(ttfText.entity);

        button->entityId = prefabEnt.entity.id;
        button->backgroundId = background.entity.id;

        return prefabEnt.entity;
    }

    void NexusScene::updateButtonsClickability(const std::unordered_map<std::string, ElementType>& factMap, std::vector<DynamicNexusButton>& in)
    {
        for (auto& button : in)
        {
            if (button.neededConditionsForVisibility.empty())
            {
                continue;
            }

            bool clickable = true;

            for (const auto& it : button.conditions)
            {
                if (not it.check(factMap))
                {
                    clickable = false;
                    break;
                }
            }

            if (button.clickable != clickable)
            {
                button.clickable = clickable;

                auto background = ecsRef->getEntity(button.backgroundId);
                if (not background or not background->has<PositionComponent>())
                {
                    LOG_ERROR("Nexus scene", "Background: " << button.backgroundId << " is not in a valid state!");
                    continue;
                }

                if (button.clickable)
                {
                    if (background->has<Simple2DObject>())
                        background->get<Simple2DObject>()->setColors({0, 196, 0, 255});
                }
                else
                {
                    if (background->has<Simple2DObject>())
                        background->get<Simple2DObject>()->setColors({196, 0, 0, 255});
                }
            }
        }
    }

    void NexusScene::updateButtonsVisibility(const std::unordered_map<std::string, ElementType>& factMap, std::vector<DynamicNexusButton>& in, std::vector<DynamicNexusButton>& out, bool visiblility)
    {
        auto layout = nexusLayout.get<HorizontalLayout>();

        for (auto it = in.begin(); it != in.end();)
        {
            bool reveal = true;
            bool clickable = true;

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

                for (const auto& it2 : it->conditions)
                {
                    if (not it2.check(factMap))
                    {
                        clickable = false;
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
                        it->clickable = clickable;
                        // Create a new entity for the button
                        auto buttonEntity = createButtonPrefab(this, it->label, it->id, it.base());

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

                        it->entityId = 0;
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
        updateButtonsClickability(factMap, visibleButtons);
        updateButtonsVisibility(factMap, maskedButtons, visibleButtons, true);
        updateButtonsVisibility(factMap, visibleButtons, maskedButtons, false);
    }

    // Helper method to add a new resource display entry.
    void NexusScene::_addResourceDisplay(const std::string& resourceName)
    {
        // Create a new UI text entity using your existing TTFText helper.
        // We start with an empty text; it'll be updated in execute().
        auto textEntity = makeTTFText(this, 0, 0, 1, "res/font/Inter/static/Inter_28pt-Light.ttf", "", 0.4f);
        textEntity.get<PositionComponent>()->setVisibility(false);

        resLayout->get<ListView>()->addEntity(textEntity.entity);

        // Create a ResourceDisplayEntry and add it to the list.
        ResourceDisplayEntry entry;
        entry.resourceName = resourceName;
        entry.uiEntity = textEntity.entity;
        resourceList.push_back(entry);
    }

    void NexusScene::updateRessourceView()
    {
        LOG_INFO("NexusScene", "updateRessourceView");
        // Update the resource list view.
        WorldFacts* wf = ecsRef->getSystem<WorldFacts>();
        if (not wf) return;

        for (auto& entry : resourceList)
        {
            float value = 0.0f;
            float maxValue = 0.0f;
            bool hasMax = false;

            auto it = wf->factMap.find(entry.resourceName);
            if (it != wf->factMap.end())
            {
                value = it->second.get<float>();
            }

            auto itMax = wf->factMap.find(entry.resourceName + "_max_value");
            if (itMax != wf->factMap.end())
            {
                maxValue = itMax->second.get<float>();
                hasMax = true;
            }

            std::ostringstream oss;
            oss << entry.resourceName << ": " << value;

            if (hasMax)
                oss << "/" << maxValue;

            // Update the text of the corresponding TTFText component.
            if (entry.uiEntity and entry.uiEntity->has<TTFText>())
            {
                auto textComp = entry.uiEntity->get<TTFText>();
                textComp->setText(oss.str());
            }
        }
    }
}
