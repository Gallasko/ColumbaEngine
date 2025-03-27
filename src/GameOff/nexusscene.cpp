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

#include <iomanip>
#include <sstream>

namespace pg
{
    namespace
    {
        std::string floatToString(float value, int decimalPlaces)
        {
            std::ostringstream out;
            out << std::fixed << std::setprecision(decimalPlaces) << value;
            return out.str();
        }

        struct OnBackgroundButtonHover
        {
            std::string buttonId;

            _unique_id id;

            bool state;
        };

        struct HideButton
        {
            std::string buttonId;
        };

        constant::Vector4D getButtonColors(ThemeInfo& info, bool clickable, bool highlight = false)
        {
            if (clickable)
            {
                if (highlight)
                {
                    return {
                        info.values["hoverClickableNexusButton.r"].get<float>(),
                        info.values["hoverClickableNexusButton.g"].get<float>(),
                        info.values["hoverClickableNexusButton.b"].get<float>(),
                        info.values["hoverClickableNexusButton.a"].get<float>()};
                }
                else
                {
                    return {
                        info.values["clickableNexusButton.r"].get<float>(),
                        info.values["clickableNexusButton.g"].get<float>(),
                        info.values["clickableNexusButton.b"].get<float>(),
                        info.values["clickableNexusButton.a"].get<float>()};
                }
            }
            else
            {
                return {
                    info.values["unclickableNexusButton.r"].get<float>(),
                    info.values["unclickableNexusButton.g"].get<float>(),
                    info.values["unclickableNexusButton.b"].get<float>(),
                    info.values["unclickableNexusButton.a"].get<float>()};
            }
        }

        bool isButtonClickable(const std::unordered_map<std::string, ElementType>& factMap, const DynamicNexusButton& button)
        {
            for (const auto& it : button.conditions)
            {
                if (not it.check(factMap))
                {
                    return false;
                }
            }

            for (const auto& it : button.costs)
            {
                auto fc = FactChecker();
                fc.name = it.resourceId;
                fc.equality = FactCheckEquality::GreaterEqual;

                if (it.valueId != "" and factMap.find(it.valueId) != factMap.end())
                {
                    fc.value = factMap.at(it.valueId);
                }
                else
                {
                    fc.value = it.value;
                }

                if (not fc.check(factMap))
                {
                    return false;
                }
            }

            return true;
        }
    }

    template <>
    void serialize(Archive& archive, const NexusButtonCost& value)
    {
        archive.startSerialization("NexusButtonCost");

        serialize(archive, "resourceId", value.resourceId);
        serialize(archive, "value", value.value);
        serialize(archive, "valueId", value.valueId);
        serialize(archive, "consumed", value.consumed);

        archive.endSerialization();
    }

    template <>
    NexusButtonCost deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS("NexusButtonCost");

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR("NexusButtonCost", "Element is null");
        }
        else
        {
            LOG_MILE("NexusButtonCost", "Deserializing NexusButtonCost");

            NexusButtonCost data;

            defaultDeserialize(serializedString, "resourceId", data.resourceId);
            defaultDeserialize(serializedString, "value", data.value);
            defaultDeserialize(serializedString, "valueId", data.valueId);
            defaultDeserialize(serializedString, "consumed", data.consumed);

            return data;
        }

        return NexusButtonCost{};
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
        serialize(archive, "description", value.description);
        serialize(archive, "nbClickBeforeArchive", value.nbClickBeforeArchive);
        serialize(archive, "costs", value.costs);
        serialize(archive, "costIncrease", value.costIncrease);
        serialize(archive, "neededConditionsForVisibility", value.neededConditionsForVisibility);
        serialize(archive, "nbClick", value.nbClick);
        serialize(archive, "archived", value.archived);
        serialize(archive, "activable", value.activable);
        serialize(archive, "active", value.active);
        serialize(archive, "activeTime", value.activeTime);
        serialize(archive, "activationTime", value.activationTime);

        archive.endSerialization();
    }

    template <>
    DynamicNexusButton deserialize(const UnserializedObject& serializedString)
    {
        LOG_THIS("DynamicNexusButton");

        std::string type = "";

        if (serializedString.isNull())
        {
            LOG_ERROR("DynamicNexusButton", "Element is null");
        }
        else
        {
            LOG_MILE("DynamicNexusButton", "Deserializing DynamicNexusButton");

            DynamicNexusButton data;

            defaultDeserialize(serializedString, "id", data.id);
            defaultDeserialize(serializedString, "label", data.label);
            defaultDeserialize(serializedString, "conditions", data.conditions);
            defaultDeserialize(serializedString, "outcome", data.outcome);
            defaultDeserialize(serializedString, "category", data.category);
            defaultDeserialize(serializedString, "description", data.description);
            defaultDeserialize(serializedString, "nbClickBeforeArchive", data.nbClickBeforeArchive);
            defaultDeserialize(serializedString, "costs", data.costs);
            defaultDeserialize(serializedString, "costIncrease", data.costIncrease);
            defaultDeserialize(serializedString, "neededConditionsForVisibility", data.neededConditionsForVisibility);
            defaultDeserialize(serializedString, "nbClick", data.nbClick);
            defaultDeserialize(serializedString, "archived", data.archived);
            defaultDeserialize(serializedString, "activable", data.activable);
            defaultDeserialize(serializedString, "active", data.active);
            defaultDeserialize(serializedString, "activeTime", data.activeTime);
            defaultDeserialize(serializedString, "activationTime", data.activationTime);

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
        void setUp(NexusSystem *sys)
        {
            this->sys = sys;

            setArity(1, 1);
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            auto nexusButton = args.front();
            args.pop();

            auto button = deserializeTo<DynamicNexusButton>(nexusButton);

            sys->savedButtons.push_back(button);

            return nullptr;
        }

        NexusSystem *sys;
    };

    class TrackNewResource : public Function
    {
        using Function::Function;
    public:
        void setUp(NexusSystem *sys)
        {
            this->sys = sys;

            setArity(1, 1);
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            auto resName = args.front()->getElement().toString();
            args.pop();

            sys->addResourceDisplay(resName);

            return nullptr;
        }

        NexusSystem *sys;
    };

    class CreateGenerator : public Function
    {
        using Function::Function;
    public:
        void setUp(NexusSystem *sys)
        {
            this->sys = sys;

            setArity(2, 6);
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            // Todo check type of elements gotten here

            RessourceGenerator gen;

            gen.id = args.front()->getElement().toString();
            args.pop();

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

            if (not args.empty())
            {
                gen.active = args.front()->getElement().get<bool>();
                args.pop();
            }

            sys->ecsRef->sendEvent(RessourceGenerator{gen});

            return makeVar(gen.id);
        }

        NexusSystem *sys;
    };

    class CreateButtonCost : public Function
    {
        using Function::Function;
    public:
        void setUp() { setArity(1, 4); }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            NexusButtonCost cost;

            // Todo check type of elements gotten here
            // Assume arguments: eventName (string), key (string), message (string)
            cost.resourceId = args.front()->getElement().toString();
            args.pop();

            if (not args.empty())
            {
                cost.value = args.front()->getElement().get<float>();
                args.pop();
            }

            if (not args.empty())
            {
                cost.valueId = args.front()->getElement().get<std::string>();
                args.pop();
            }

            if (not args.empty())
            {
                cost.consumed = args.front()->getElement().get<bool>();
                args.pop();
            }

            return serializeToInterpreter(this, cost);
        }
    };

    class CreateConverter : public Function
    {
        using Function::Function;
    public:
        void setUp() { setArity(1, 1); }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            ConverterComponent converter;

            // Todo check type of elements gotten here
            // Assume arguments: eventName (string), key (string), message (string)
            converter.id = args.front()->getElement().toString();
            args.pop();

            return serializeToInterpreter(this, converter);
        }
    };

    class RegisterConverter : public Function
    {
        using Function::Function;
    public:
        void setUp(NexusSystem *sys)
        {
            this->sys = sys;

            setArity(1, 1);
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            // Todo check type of elements gotten here
            // Assume arguments: eventName (string), key (string), message (string)
            auto arg = args.front();
            args.pop();

            auto converter = deserializeTo<ConverterComponent>(arg);

            auto converterEntity = sys->ecsRef->createEntity();
            sys->ecsRef->attach<ConverterComponent>(converterEntity, converter);

            return makeVar(converterEntity.id);
        }

        NexusSystem *sys;
    };

    struct NexusModule : public SysModule
    {
        NexusModule(NexusSystem *sys)
        {
            addSystemFunction<CreateNexusButton>("NexusButton");
            addSystemFunction<RegisterNexusButton>("registerNexusButton", sys);
            addSystemFunction<TrackNewResource>("addResourceDisplay", sys);
            addSystemFunction<CreateGenerator>("createGenerator", sys);
            addSystemFunction<CreateButtonCost>("ButtonCost");
            addSystemFunction<CreateConverter>("Converter");
            addSystemFunction<RegisterConverter>("registerConverter", sys);

            //Todo add basic generator / converter ids as system vars
            //addSystemVar("SCANCODE_A", SDL_SCANCODE_A);
        }
    };

    void NexusSystem::init()
    {
        PgInterpreter interpreter;

        interpreter.addSystemModule("nexus", NexusModule{this});
        interpreter.addSystemModule("log", LogModule{nullptr});
        interpreter.addSystemModule("achievement", AchievementModule{});

        interpreter.interpretFromFile("nexus.pg");


        // savedButtons.push_back(DynamicNexusButton{
        //     "ScrapConverter",
        //     "Convert [Scrap]",
        //     { FactChecker("scrapper_on", true, FactCheckEquality::Equal) },
        //     { AchievementReward(StandardEvent(ConverterTriggeredEventName, "id", converterEntity.id)) },
        //     "main",
        //     "Convert Mana to Scrap",
        //     0,
        //     { NexusButtonCost{"mana", 0, "scrap_converter_mana_cost", false} }
        // });

        // savedButtons.push_back(DynamicNexusButton{
        //     "Test_012",
        //     "Test show",
        //     { FactChecker("total_mana", 1, FactCheckEquality::GreaterEqual) },
        //     { AchievementReward(StandardEvent("res_gen_upgrade", "id", basicGen.id, "upgradeAmount", 0.5f)) },
        //     "main",
        //     "Upgrade Mana generation (+0,5 mana/sec)",
        //     5,
        //     { NexusButtonCost{"mana", 25} }
        // });

        // maskedButtons.push_back(DynamicNexusButton{
        //     "UpgradeProd1",
        //     "UpgradeProd",
        //     { FactChecker("altar_touched", true, FactCheckEquality::Equal) },
        //     { AchievementReward(StandardEvent("res_gen_upgrade", "id", basicGen.id, "upgradeAmount", 0.5f)) },
        //     "main",
        //     {},
        //     5
        // });

        savedButtons.push_back(DynamicNexusButton{
            "SpellMage",
            "Spell Mage",
            {   FactChecker("specialization_0_available", true, FactCheckEquality::Equal),
                FactChecker("mage_tier", 0, FactCheckEquality::Equal) },
            {   AchievementReward(AddFact{"mage_tier", ElementType{1}}) },
            "main",
        });

        savedButtons.push_back(DynamicNexusButton{
            "RunicMage",
            "Runic Mage",
            {   FactChecker("specialization_0_available", true, FactCheckEquality::Equal),
                FactChecker("mage_tier", 0, FactCheckEquality::Equal) },
            {   AchievementReward(AddFact{"mage_tier", ElementType{1}}),
                AchievementReward(AddFact{"runic_mage_1", ElementType{true}}),
                AchievementReward(StandardEvent("gamelog", "message", "You feel the power of the rune, coursing through your blood")) },
            "main",
            "Become a runic Mage",
            1,
            { NexusButtonCost{"mana", 300} }
        });

        savedButtons.push_back(DynamicNexusButton{
            "RunicMage2",
            "Runic Mage 2",
            {   FactChecker("runic_mage_1", true, FactCheckEquality::Equal),
                FactChecker("mage_tier", 1, FactCheckEquality::Equal) },
            {   AchievementReward(AddFact{"mage_tier", ElementType{2}}) },
            "main",
        });
    }

    void NexusSystem::execute()
    {
        if (activeButton and deltaTime > 0)
        {
            currentActiveButton->activeTime += deltaTime;

            if (currentActiveButton->activeTime >= currentActiveButton->activationTime)
            {
                WorldFacts* wf = ecsRef->getSystem<WorldFacts>();

                if (not isButtonClickable(wf->factMap, *currentActiveButton))
                {
                    currentActiveButton->active = false;
                    activeButton = false;
                    return;
                }

                // Todo check if all the conditions are met
                for (auto it2 : currentActiveButton->outcome)
                {
                    it2.call(ecsRef);
                }

                if (not currentActiveButton->costs.empty())
                {
                    for (auto it3 : currentActiveButton->costs)
                    {
                        if (it3.consumed)
                        {
                            auto cost = IncreaseFact(it3.resourceId, -it3.value);

                            if (it3.valueId != "" and wf->factMap.find(it3.valueId) != wf->factMap.end())
                            {
                                cost.value = -wf->factMap.at(it3.valueId);
                            }

                            ecsRef->sendEvent(cost);
                        }
                    }
                }

                currentActiveButton->nbClick++;
                currentActiveButton->activeTime -= currentActiveButton->activationTime;

                if (currentActiveButton->nbClickBeforeArchive != 0 and currentActiveButton->nbClick >= currentActiveButton->nbClickBeforeArchive)
                {
                    currentActiveButton->archived = true;
                    currentActiveButton->active = false;
                    activeButton = false;

                    ecsRef->sendEvent(HideButton{currentActiveButton->id});
                }
            }

            deltaTime = 0;
        }

    }

    void NexusScene::init()
    {
        auto themeSys = ecsRef->getSystem<ThemeSystem>();

        if (themeSys)
        {
            theme = themeSys->getCurrentTheme();
        }
        else
        {
            LOG_ERROR("Nexus", "Couldn't load theme !");
        }

        auto nexusSys = ecsRef->getSystem<NexusSystem>();

        maskedButtons = nexusSys->savedButtons;

        for (const auto& button : maskedButtons)
        {
            LOG_INFO("Nexus Scene", "Button loaded: " << button.id << " " << button.archived << " " << button.nbClick << " " << button.nbClickBeforeArchive);
        }

        for (const auto& res : nexusSys->resourceToBeDisplayed)
        {
            resourceToBeDisplayed.push(res);
        }

        // Todo add this in the converter sys as: <id>_<res>_<cost>
        ecsRef->sendEvent( AddFact{ "scrap_converter_mana_cost", ElementType{5.0f} } );

        auto windowEnt = ecsRef->getEntity("__MainWindow");
        auto windowAnchor = windowEnt->get<UiAnchor>();

        auto listView = makeListView(this, 1, 1, 350, 1);

        auto listViewComp = listView.get<ListView>();
        listViewComp->spacing = 8;

        auto listViewUi = listView.get<UiAnchor>();

        listViewUi->setTopAnchor(windowAnchor->top);
        listViewUi->setTopMargin(120);
        listViewUi->setBottomAnchor(windowAnchor->bottom);
        listViewUi->setLeftAnchor(windowAnchor->left);
        listViewUi->setLeftMargin(10);

        resLayout = listView.entity;

        auto layout = makeHorizontalLayout(ecsRef, 30, 150, 500, 400);
        auto layoutAnchor = layout.get<UiAnchor>();

        layoutAnchor->setLeftAnchor(listViewUi->right);
        layoutAnchor->setLeftMargin(10);

        layout.get<HorizontalLayout>()->spacing = 30;

        layout.get<HorizontalLayout>()->spacedInWidth = false;
        layout.get<HorizontalLayout>()->fitToWidth = true;

        nexusLayout = layout.entity;

        // [Begin] Tooltip Ui definition

        auto tooltipBg = makeUiSimple2DShape(this, Shape2D::Square, theme.values["tooltip.width"].get<float>(), theme.values["tooltip.height"].get<float>(), {theme.values["tooltipBg.r"].get<float>(), theme.values["tooltipBg.g"].get<float>(), theme.values["tooltipBg.b"].get<float>(), theme.values["tooltipBg.a"].get<float>()});
        tooltipBg.get<PositionComponent>()->setVisibility(false);
        tooltipBg.get<PositionComponent>()->setZ(5);
        auto tooltipBgAnchor = tooltipBg.get<UiAnchor>();

        tooltipBgAnchor->setBottomMargin(-theme.values["tooltip.bottomMargin"].get<float>());

        auto tooltipBgHighLight = makeUiSimple2DShape(this, Shape2D::Square, 0, 0, {theme.values["tooltipBgHighlight.r"].get<float>(), theme.values["tooltipBgHighlight.g"].get<float>(), theme.values["tooltipBgHighlight.b"].get<float>(), theme.values["tooltipBgHighlight.a"].get<float>()});
        tooltipBgHighLight.get<PositionComponent>()->setVisibility(false);
        tooltipBgHighLight.get<PositionComponent>()->setZ(4);
        auto tooltipBgHighLightAnchor = tooltipBgHighLight.get<UiAnchor>();

        tooltipBgHighLightAnchor->setWidthConstrain(PosConstrain{tooltipBg.entity.id, AnchorType::Width, PosOpType::Add, 2});
        tooltipBgHighLightAnchor->setHeightConstrain(PosConstrain{tooltipBg.entity.id, AnchorType::Height, PosOpType::Add, 2});

        tooltipBgHighLightAnchor->setTopAnchor(tooltipBgAnchor->top);
        tooltipBgHighLightAnchor->setTopMargin(-1);
        tooltipBgHighLightAnchor->setLeftAnchor(tooltipBgAnchor->left);
        tooltipBgHighLightAnchor->setLeftMargin(-1);

        // Description UI

        auto descTextEnt = makeTTFText(this, 0, 0, 6, theme.values["tooltipTitle.font"].get<std::string>(), "", theme.values["tooltipTitle.scale"].get<float>());
        auto descTextPos = descTextEnt.get<PositionComponent>();
        auto descText = descTextEnt.get<TTFText>();
        auto descTextAnchor = descTextEnt.get<UiAnchor>();

        descTextPos->setVisibility(false);

        descText->setWrap(true);

        descTextAnchor->setTopAnchor(tooltipBgAnchor->top);
        descTextAnchor->setTopMargin(theme.values["tooltip.topMargin"].get<float>());
        descTextAnchor->setLeftAnchor(tooltipBgAnchor->left);
        descTextAnchor->setLeftMargin(theme.values["tooltip.leftMargin"].get<float>());
        descTextAnchor->setRightAnchor(tooltipBgAnchor->right);
        descTextAnchor->setRightMargin(theme.values["tooltip.rightMargin"].get<float>());

        // Cost Spacer UI

        auto tooltipCostSpacer = makeUiSimple2DShape(this, Shape2D::Square, 0, 1, {theme.values["tooltipSpacer.r"].get<float>(), theme.values["tooltipSpacer.g"].get<float>(), theme.values["tooltipSpacer.b"].get<float>(), theme.values["tooltipSpacer.a"].get<float>()});
        tooltipCostSpacer.get<PositionComponent>()->setVisibility(false);
        tooltipCostSpacer.get<PositionComponent>()->setZ(6);
        auto tooltipCostSpacerAnchor = tooltipCostSpacer.get<UiAnchor>();

        tooltipCostSpacerAnchor->setWidthConstrain(PosConstrain{tooltipBg.entity.id, AnchorType::Width, PosOpType::Mul, 0.8f});

        tooltipCostSpacerAnchor->setTopAnchor(descTextAnchor->bottom);
        tooltipCostSpacerAnchor->setTopMargin(theme.values["tooltipCostSpacer.topMargin"].get<float>());
        tooltipCostSpacerAnchor->setHorizontalCenter(tooltipBgAnchor->horizontalCenter);

        // Cost Text UI

        auto costTextEnt = makeTTFText(this, 0, 0, 6, theme.values["tooltipTitle.font"].get<std::string>(), "Cost:", theme.values["tooltipTitle.scale"].get<float>());
        auto costTextPos = costTextEnt.get<PositionComponent>();
        auto costText = costTextEnt.get<TTFText>();
        auto costTextAnchor = costTextEnt.get<UiAnchor>();

        costTextPos->setVisibility(false);

        costText->setWrap(true);

        costTextAnchor->setTopAnchor(tooltipCostSpacerAnchor->bottom);
        costTextAnchor->setTopMargin(theme.values["tooltip.topMargin"].get<float>());
        costTextAnchor->setLeftAnchor(tooltipBgAnchor->left);
        costTextAnchor->setLeftMargin(theme.values["tooltip.leftMargin"].get<float>());
        costTextAnchor->setRightAnchor(tooltipBgAnchor->right);
        costTextAnchor->setRightMargin(theme.values["tooltip.rightMargin"].get<float>());

        // Cost Values UI
        auto costValuesEnt = makeTTFText(this, 0, 0, 6, theme.values["tooltipTitle.font"].get<std::string>(), "", theme.values["tooltipTitle.scale"].get<float>());
        auto costValuesPos = costValuesEnt.get<PositionComponent>();
        auto costValuesText = costValuesEnt.get<TTFText>();
        auto costValuesAnchor = costValuesEnt.get<UiAnchor>();

        costValuesPos->setVisibility(false);
        costValuesText->setWrap(true);

        costValuesAnchor->setTopAnchor(costTextAnchor->bottom);
        costValuesAnchor->setTopMargin(theme.values["tooltipCostValues.topMargin"].get<float>());
        costValuesAnchor->setLeftAnchor(tooltipBgAnchor->left);
        costValuesAnchor->setLeftMargin(theme.values["tooltipCostValues.leftMargin"].get<float>());
        costValuesAnchor->setRightAnchor(tooltipBgAnchor->right);
        costValuesAnchor->setRightMargin(theme.values["tooltipCostValues.rightMargin"].get<float>());

        // Register UI elements in map

        tooltipsEntities["background"] = tooltipBg.entity;
        tooltipsEntities["backHighlight"] = tooltipBgHighLight.entity;
        tooltipsEntities["desc"] = descTextEnt.entity;
        tooltipsEntities["costSpacer"] = tooltipCostSpacer.entity;
        tooltipsEntities["costTitle"] = costTextEnt.entity;
        tooltipsEntities["costValues"] = costValuesEnt.entity;

        // -- [End] Tooltip Ui definition

        listenToStandardEvent("add_res_display", [this](const StandardEvent& event) {
            auto res = event.values.at("res").get<std::string>();

            ecsRef->getSystem<NexusSystem>()->addResourceDisplay(res);

            addResourceDisplay(res);
        });

        listenToStandardEvent("add_generator", [this](const StandardEvent& event) {
            if (event.values.find("id") == event.values.end() or event.values.find("res") == event.values.end())
            {
                LOG_ERROR("NexusScene", "Event 'add_generator' received without id and res.");
                return;
            }

            auto id = event.values.at("id").get<std::string>();
            auto res = event.values.at("res").get<std::string>();

            RessourceGenerator gen(id, res);

            if (event.values.find("prod") != event.values.end())
            {
                gen.productionRate = event.values.at("prod").get<float>();
            }

            if (event.values.find("storage") != event.values.end())
            {
                gen.capacity = event.values.at("storage").get<float>();
            }

            if (event.values.find("active") != event.values.end())
            {
                gen.active = event.values.at("active").get<bool>();
            }
            else
            {
                gen.active = true;
            }

            ecsRef->sendEvent(NewGeneratorEvent{gen});
        });

        listenToStandardEvent("one_shot_res", [this](const StandardEvent& event) {
            if (event.values.find("res") == event.values.end() or event.values.find("value") == event.values.end())
            {
                LOG_ERROR("NexusScene", "Event 'one_shot_res' received without res and value.");
                return;
            }

            auto res = event.values.at("res").get<std::string>();

            auto value = event.values.at("value");

            ecsRef->sendEvent(IncreaseFact{res, value});
            ecsRef->sendEvent(IncreaseFact{"total_" + res, value});
        });

        listenToStandardEvent("activate_gen", [this](const StandardEvent&) {
            updateGeneratorViews();
        });

        // Listen for world fact updates to log mana or upgrades.
        listenToEvent<WorldFactsUpdate>([this](const WorldFactsUpdate& event) {
            updateDynamicButtons(*event.factMap);
            updateRessourceView();
            updateGeneratorViews();
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

                    tooltipsEntities["background"]->get<UiAnchor>()->clearBottomAnchor();

                    if (bgEnt->has<UiAnchor>())
                    {
                        tooltipsEntities["background"]->get<UiAnchor>()->setTopAnchor(bgEnt->get<UiAnchor>()->bottom);
                        tooltipsEntities["background"]->get<UiAnchor>()->setTopMargin(-2);
                        tooltipsEntities["background"]->get<UiAnchor>()->setHorizontalCenter(bgEnt->get<UiAnchor>()->horizontalCenter);

                        tooltipsEntities["background"]->get<UiAnchor>()->setBottomAnchor(bgEnt->get<UiAnchor>()->bottom);
                    }

                    if (it->description != "")
                    {
                        tooltipsEntities["desc"]->get<PositionComponent>()->setVisibility(true);
                        tooltipsEntities["desc"]->get<TTFText>()->setText(it->description);

                        tooltipsEntities["background"]->get<UiAnchor>()->setBottomAnchor(tooltipsEntities["desc"]->get<UiAnchor>()->bottom);
                    }

                    if (it->costs.size() > 0)
                    {
                        tooltipsEntities["costSpacer"]->get<PositionComponent>()->setVisibility(true);
                        tooltipsEntities["costTitle"]->get<PositionComponent>()->setVisibility(true);
                        tooltipsEntities["costValues"]->get<PositionComponent>()->setVisibility(true);

                        std::ostringstream costText;

                        for (const auto& cost : it->costs)
                        {
                            auto str = cost.resourceId;
                            str[0] = std::toupper(str[0]);

                            costText << str << ": " << cost.value << "\n";
                        }

                        tooltipsEntities["costValues"]->get<TTFText>()->setText(costText.str());

                        tooltipsEntities["background"]->get<UiAnchor>()->setBottomAnchor(tooltipsEntities["costValues"]->get<UiAnchor>()->bottom);
                    }
                }
                else
                {
                    tooltipsEntities["background"]->get<PositionComponent>()->setVisibility(false);
                    tooltipsEntities["backHighlight"]->get<PositionComponent>()->setVisibility(false);

                    tooltipsEntities["desc"]->get<PositionComponent>()->setVisibility(false);

                    tooltipsEntities["costSpacer"]->get<PositionComponent>()->setVisibility(false);
                    tooltipsEntities["costTitle"]->get<PositionComponent>()->setVisibility(false);

                    tooltipsEntities["costValues"]->get<PositionComponent>()->setVisibility(false);
                }

                if (not it->clickable)
                    return;

                if (bgEnt->has<Simple2DObject>())
                {
                    auto colors = getButtonColors(theme, true, event.state);

                    bgEnt->get<Simple2DObject>()->setColors(colors);
                }
            }
        });

        listenToEvent<HideButton>([this](const HideButton& button) {
            auto it = std::find_if(visibleButtons.begin(), visibleButtons.end(), [button](const DynamicNexusButton& b) {
                return b.id == button.buttonId;
            });

            if (it == visibleButtons.end())
            {
                LOG_ERROR("NexusScene", "Button not found: " << button.buttonId);
                return;
            }

            auto id = it->entityId;

            // Archive the button and remove it from the visible buttons.
            archivedButtons.push_back(*it);
            visibleButtons.erase(it);

            auto layout = nexusLayout.get<HorizontalLayout>();
            layout->removeEntity(id);
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

                if (it->activable)
                {
                    // Todo
                    // it->active = not it->active;
                    it->active = true;

                    ecsRef->sendEvent(NexusButtonStateChange{*it});

                    return;
                }

                // Todo check if all the conditions are met
                for (auto it2 : it->outcome)
                {
                    it2.call(ecsRef);
                }

                if (not it->costs.empty())
                {
                    WorldFacts* wf = ecsRef->getSystem<WorldFacts>();

                    for (auto it3 : it->costs)
                    {
                        if (it3.consumed)
                        {
                            auto cost = IncreaseFact(it3.resourceId, -it3.value);

                            if (it3.valueId != "" and wf->factMap.find(it3.valueId) != wf->factMap.end())
                            {
                                cost.value = -wf->factMap.at(it3.valueId);
                            }

                            ecsRef->sendEvent(cost);
                        }
                    }
                }

                it->nbClick++;

                LOG_ERROR("NexusScene", "Button clicked: " << buttonId << ", clicked: " << it->nbClick << ", it->id: " << it->id);

                if (it->nbClickBeforeArchive != 0 and it->nbClick >= it->nbClickBeforeArchive)
                {
                    it->archived = true;

                    ecsRef->sendEvent(NexusButtonStateChange{*it});

                    auto id = it->entityId;

                    // Archive the button and remove it from the visible buttons.
                    archivedButtons.push_back(*it);
                    visibleButtons.erase(it);

                    auto layout = nexusLayout.get<HorizontalLayout>();
                    layout->removeEntity(id);
                }
                else
                {
                    ecsRef->sendEvent(NexusButtonStateChange{*it});
                }
            }
        });
    }

    void NexusScene::startUp()
    {
        auto sys = ecsRef->getSystem<WorldFacts>();

        for (auto it = maskedButtons.begin(); it != maskedButtons.end();)
        {
            if (it->archived)
            {
                archivedButtons.push_back(*it);
                it = maskedButtons.erase(it);
            }
            else
            {
                ++it;
            }
        }

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

        // Todo lookup the colors from a theme instead of hardcoded
        constant::Vector4D colors = getButtonColors(scene->theme, button->clickable);

        auto background = makeUiSimple2DShape(scene->ecsRef, Shape2D::Square, scene->theme.values["nexusbutton.width"].get<float>(), scene->theme.values["nexusbutton.height"].get<float>(), colors);
        auto backgroundAnchor = background.get<UiAnchor>();

        scene->ecsRef->attach<MouseLeftClickComponent>(background.entity, makeCallable<StandardEvent>("nexus_button_clicked", "id", id));

        scene->ecsRef->attach<MouseEnterComponent>(background.entity, makeCallable<OnBackgroundButtonHover>(OnBackgroundButtonHover{button->id, background.entity.id, true}));
        scene->ecsRef->attach<MouseLeaveComponent>(background.entity, makeCallable<OnBackgroundButtonHover>(OnBackgroundButtonHover{button->id, background.entity.id, false}));

        prefabAnchor->setWidthConstrain(PosConstrain{background.entity.id, AnchorType::Width});
        prefabAnchor->setHeightConstrain(PosConstrain{background.entity.id, AnchorType::Height});
        backgroundAnchor->setTopAnchor(prefabAnchor->top);
        backgroundAnchor->setLeftAnchor(prefabAnchor->left);

        auto ttfText = makeTTFText(scene->ecsRef, 0, 0, 1, scene->theme.values["nexusbutton.font"].get<std::string>(), text, scene->theme.values["nexusbutton.scale"].get<float>());
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
            if (button.neededConditionsForVisibility.empty() and button.costs.empty())
            {
                continue;
            }

            bool clickable = isButtonClickable(factMap, button);

            if (button.clickable != clickable)
            {
                button.clickable = clickable;

                auto background = ecsRef->getEntity(button.backgroundId);
                if (not background or not background->has<PositionComponent>())
                {
                    LOG_ERROR("Nexus scene", "Background: " << button.backgroundId << " is not in a valid state!");
                    continue;
                }

                auto colors = getButtonColors(theme, clickable);

                if (background->has<Simple2DObject>())
                    background->get<Simple2DObject>()->setColors(colors);
            }
        }
    }

    void NexusScene::updateButtonsVisibility(const std::unordered_map<std::string, ElementType>& factMap, std::vector<DynamicNexusButton>& in, std::vector<DynamicNexusButton>& out, bool visiblility)
    {
        auto layout = nexusLayout.get<HorizontalLayout>();

        for (auto it = in.begin(); it != in.end();)
        {
            bool reveal = true;
            bool clickable = false;

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

            if (visiblility == true)
            {
                clickable = isButtonClickable(factMap, *it.base());
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

        // Update value cost of visible buttons
        for (auto& button : visibleButtons)
        {
            for (auto& cost : button.costs)
            {
                if (not cost.valueId.empty() and factMap.find(cost.valueId) != factMap.end()) {
                    cost.value = factMap.at(cost.valueId).get<float>();
                }
            }
        }
    }

    // Helper method to add a new resource display entry.
    void NexusScene::_addResourceDisplay(const std::string& resourceName)
    {
        // Create a new UI text entity using your existing TTFText helper.
        // We start with an empty text; it'll be updated in execute().
        auto textEntity = makeTTFText(this, 0, 0, 1, theme.values["resourcedisplay.font"].get<std::string>(), "", theme.values["resourcedisplay.scale"].get<float>());
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

                auto str = oss.str();
                str[0] = std::toupper(str[0]);
                textComp->setText(str);
            }
        }
    }

    EntityRef NexusScene::createGeneratorView(const RessourceGenerator& gen)
    {
        auto prefabEnt = makeAnchoredPrefab(this);
        auto prefab = prefabEnt.get<Prefab>();
        auto prefabAnchor = prefabEnt.get<UiAnchor>();

        auto genEnt = makeTTFText(ecsRef, 0, 0, 1, theme.values["resourcedisplay.font"].get<std::string>(), gen.id, theme.values["resourcedisplay.scale"].get<float>());
        auto genEntAnchor = genEnt.get<UiAnchor>();

        genEntAnchor->setTopAnchor(prefabAnchor->top);
        genEntAnchor->setLeftAnchor(prefabAnchor->left);
        genEntAnchor->setLeftMargin(2);

        auto genResEnt = makeTTFText(ecsRef, 0, 0, 1, theme.values["resourcedisplay.font"].get<std::string>(), gen.ressource, theme.values["resourcedisplay.scale"].get<float>());
        auto genResEntAnchor = genResEnt.get<UiAnchor>();

        genResEntAnchor->setTopAnchor(genEntAnchor->bottom);
        genResEntAnchor->setTopMargin(5);
        genResEntAnchor->setLeftAnchor(prefabAnchor->left);
        genResEntAnchor->setLeftMargin(15);

        auto genProdEnt = makeTTFText(ecsRef, 0, 0, 1, theme.values["resourcedisplay.font"].get<std::string>(), floatToString(gen.productionRate, 2), theme.values["resourcedisplay.scale"].get<float>());
        auto genProdEntAnchor = genProdEnt.get<UiAnchor>();

        genProdEntAnchor->setBottomAnchor(genResEntAnchor->bottom);
        // genProdEntAnchor->setRightAnchor(prefabAnchor->right);
        genProdEntAnchor->setLeftAnchor(genResEntAnchor->right);
        genProdEntAnchor->setLeftMargin(5);

        auto genCapaEnt = makeTTFText(ecsRef, 0, 0, 1, theme.values["resourcedisplay.font"].get<std::string>(), "/" + std::to_string(int(gen.capacity)), theme.values["resourcedisplay.scale"].get<float>());
        auto genCapaEntAnchor = genCapaEnt.get<UiAnchor>();

        genCapaEntAnchor->setBottomAnchor(genResEntAnchor->bottom);
        genCapaEntAnchor->setRightAnchor(prefabAnchor->right);
        genCapaEntAnchor->setRightMargin(20);

        auto genCurrentEnt = makeTTFText(ecsRef, 0, 0, 1, theme.values["resourcedisplay.font"].get<std::string>(), std::to_string(int(gen.currentMana)), theme.values["resourcedisplay.scale"].get<float>());
        auto genCurrentEntAnchor = genCurrentEnt.get<UiAnchor>();

        genCurrentEntAnchor->setBottomAnchor(genResEntAnchor->bottom);
        genCurrentEntAnchor->setRightAnchor(genCapaEntAnchor->left);
        // genCurrentEntAnchor->setRightMargin(5);

        prefabAnchor->setBottomAnchor(genResEntAnchor->bottom);

        generatorViews[gen.id] = prefabEnt.entity;
        generatorViews[gen.id + "_gen"] = genEnt.entity;
        generatorViews[gen.id + "_res"] = genResEnt.entity;
        generatorViews[gen.id + "_prod"] = genProdEnt.entity;
        generatorViews[gen.id + "_capa"] = genCapaEnt.entity;
        generatorViews[gen.id + "_current"] = genCurrentEnt.entity;

        prefab->addToPrefab(genEnt.entity);
        prefab->addToPrefab(genResEnt.entity);
        prefab->addToPrefab(genProdEnt.entity);
        prefab->addToPrefab(genCapaEnt.entity);
        prefab->addToPrefab(genCurrentEnt.entity);

        return prefabEnt.entity;
    }

    void NexusScene::updateGeneratorViews()
    {
        auto generators = ecsRef->view<RessourceGenerator>();

        for (const auto& gen : generators)
        {
            // Check if a view for this generator already exists.
            if (generatorViews.find(gen->id) == generatorViews.end())
            {
                // Create a new generator view UI element.
                EntityRef viewEnt = createGeneratorView(*gen);
                auto anchor = viewEnt->get<UiAnchor>();
                auto layoutAnchor = resLayout->get<UiAnchor>();

                anchor->setLeftAnchor(layoutAnchor->left);

                anchor->setWidthConstrain(PosConstrain{layoutAnchor.entityId, AnchorType::Width});

                // anchor->setRightAnchor(layoutAnchor->right);

                // Optionally add the view to a ListView layout dedicated to generators.
                resLayout->get<ListView>()->addEntity(viewEnt);
            }

            auto ent1 = generatorViews[gen->id + "_gen"];
            auto ent2 = generatorViews[gen->id + "_res"];
            auto ent3 = generatorViews[gen->id + "_prod"];
            auto ent4 = generatorViews[gen->id + "_capa"];
            auto ent5 = generatorViews[gen->id + "_current"];

            if (ent1 and ent1->has<TTFText>())
            {
                auto textComp = ent1->get<TTFText>();
                textComp->setText(gen->id);
            }

            if (ent2 and ent2->has<TTFText>())
            {
                auto textComp = ent2->get<TTFText>();
                textComp->setText(gen->ressource);
            }

            if (ent3 and ent3->has<TTFText>())
            {
                auto textComp = ent3->get<TTFText>();
                textComp->setText(floatToString(gen->productionRate, 2));
            }

            if (ent4 and ent4->has<TTFText>())
            {
                auto textComp = ent4->get<TTFText>();
                textComp->setText(" / " + std::to_string(int(gen->capacity)));
            }

            if (ent5 and ent5->has<TTFText>())
            {
                auto textComp = ent5->get<TTFText>();
                textComp->setText(std::to_string(int(gen->currentMana)));
            }

            auto prefab = generatorViews[gen->id];

            if (prefab and prefab->has<Prefab>())
            {
                if (gen->active)
                {
                    // Todo a change of visibility of the position component should trigger the set visibility of the prefab component
                    prefab->get<Prefab>()->setVisibility(true);
                    // prefab->get<PositionComponent>()->setVisibility(true);
                }
                else
                {
                    prefab->get<Prefab>()->setVisibility(false);
                    // prefab->get<PositionComponent>()->setVisibility(false);
                }
            }
        }
    }
}
