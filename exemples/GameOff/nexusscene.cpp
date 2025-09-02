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

#include "Systems/tween.h"

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

        struct UpdateGenView {};

        constant::Vector4D getButtonColors(ThemeInfo& info, bool clickable, bool activable, bool highlight = false)
        {
            if (clickable)
            {
                if (activable)
                {
                    return {
                        info.values["activeClickableNexusButton.r"].get<float>(),
                        info.values["activeClickableNexusButton.g"].get<float>(),
                        info.values["activeClickableNexusButton.b"].get<float>(),
                        info.values["activeClickableNexusButton.a"].get<float>()};
                }

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

            for (size_t i = 0; i < button.costs.size(); ++i)
            {
                auto it = button.costs[i];

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

                if (button.costIncrease.size() > i and fc.value.isNumber())
                {
                    fc.value = ElementType{fc.value.get<float>() * std::pow(button.costIncrease[i], button.nbClick)};
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

    class ParseRequirement : public Function
    {
        using Function::Function;
    public:
        void setUp() { setArity(1, 1); }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            auto reqStr = args.front()->getElement().toString();
            args.pop();

            FactChecker checker = parseRequirementString(reqStr);
            return serializeToInterpreter(this, checker);
        }

    private:
        FactChecker parseRequirementString(const std::string& req)
        {
            if (req.length() > 0 && req[0] == '!')
            {
                return FactChecker(req.substr(1), false, FactCheckEquality::Equal);
            }
            else if (req.find(">=") != std::string::npos)
            {
                auto pos = req.find(">=");
                auto name = req.substr(0, pos);
                auto value = std::stof(req.substr(pos + 2));
                return FactChecker(name, value, FactCheckEquality::GreaterEqual);
            }
            else if (req.find("<=") != std::string::npos)
            {
                auto pos = req.find("<=");
                auto name = req.substr(0, pos);
                auto value = std::stof(req.substr(pos + 2));
                return FactChecker(name, value, FactCheckEquality::LesserEqual);
            }
            else if (req.find(">") != std::string::npos)
            {
                auto pos = req.find(">");
                auto name = req.substr(0, pos);
                auto value = std::stof(req.substr(pos + 1));
                return FactChecker(name, value, FactCheckEquality::Greater);
            }
            else if (req.find("<") != std::string::npos)
            {
                auto pos = req.find("<");
                auto name = req.substr(0, pos);
                auto value = std::stof(req.substr(pos + 1));
                return FactChecker(name, value, FactCheckEquality::Lesser);
            }
            else if (req.find("==") != std::string::npos)
            {
                auto pos = req.find("==");
                auto name = req.substr(0, pos);
                auto valueStr = req.substr(pos + 2);
                if (valueStr == "true") {
                    return FactChecker(name, true, FactCheckEquality::Equal);
                } else if (valueStr == "false") {
                    return FactChecker(name, false, FactCheckEquality::Equal);
                } else {
                    auto value = std::stof(valueStr);
                    return FactChecker(name, value, FactCheckEquality::Equal);
                }
            }
            else
            {
                return FactChecker(req, true, FactCheckEquality::Equal);
            }
        }
    };

    class CreateGamelog : public Function
    {
        using Function::Function;
    public:
        void setUp() { setArity(1, 1); }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            auto message = args.front()->getElement().toString();
            args.pop();

            auto event = StandardEvent("gamelog", "message", message);
            return serializeToInterpreter(this, AchievementReward(event));
        }
    };

    class CreateFact : public Function
    {
        using Function::Function;
    public:
        void setUp() { setArity(2, 2); }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            auto name = args.front()->getElement().toString();
            args.pop();
            auto value = args.front()->getElement();
            args.pop();

            return serializeToInterpreter(this, AchievementReward(AddFact{name, value}));
        }
    };

    class CreateResource : public Function
    {
        using Function::Function;
    public:
        void setUp() { setArity(2, 2); }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            auto resourceName = args.front()->getElement().toString();
            args.pop();
            auto amount = args.front()->getElement();
            args.pop();

            auto event = StandardEvent("one_shot_res", "res", resourceName, "value", amount);
            return serializeToInterpreter(this, AchievementReward(event));
        }
    };

    class CreateIncrease : public Function
    {
        using Function::Function;
    public:
        void setUp() { setArity(2, 2); }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            auto name = args.front()->getElement().toString();
            args.pop();
            auto value = args.front()->getElement();
            args.pop();

            return serializeToInterpreter(this, AchievementReward(IncreaseFact{name, value}));
        }
    };

    class QuickButton : public Function
    {
        using Function::Function;
    public:
        void setUp(NexusSystem *sys)
        {
            this->sys = sys;
            setArity(5, 8); // id, label, requirements[], outcomes[], description, [category], [clicks], [costs[]]
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            DynamicNexusButton button;

            button.id = args.front()->getElement().toString();
            args.pop();
            button.label = args.front()->getElement().toString();
            args.pop();

            // Parse requirements array
            auto requirementsArg = args.front();
            args.pop();
            if (requirementsArg->getType() == "ClassInstance")
            {
                auto reqList = std::static_pointer_cast<ClassInstance>(requirementsArg);
                for (const auto& field : reqList->getFields())
                {
                    auto checker = deserializeTo<FactChecker>(field.value);
                    button.conditions.push_back(checker);
                }
            }

            // Parse outcomes array
            auto outcomesArg = args.front();
            args.pop();
            if (outcomesArg->getType() == "ClassInstance")
            {
                auto outcomeList = std::static_pointer_cast<ClassInstance>(outcomesArg);
                for (const auto& field : outcomeList->getFields())
                {
                    auto reward = deserializeTo<AchievementReward>(field.value);
                    button.outcome.push_back(reward);
                }
            }

            button.description = args.front()->getElement().toString();
            args.pop();

            // Optional category
            if (!args.empty())
            {
                button.category = args.front()->getElement().toString();
                args.pop();
            }
            else
            {
                button.category = "Main";
            }

            // Optional number of clicks before archive
            if (!args.empty())
            {
                button.nbClickBeforeArchive = args.front()->getElement().get<size_t>();
                args.pop();
            }
            else
            {
                button.nbClickBeforeArchive = 1;
            }

            // Optional costs array
            if (!args.empty())
            {
                auto costsArg = args.front();
                args.pop();
                if (costsArg->getType() == "ClassInstance")
                {
                    auto costList = std::static_pointer_cast<ClassInstance>(costsArg);
                    for (const auto& field : costList->getFields())
                    {
                        auto cost = deserializeTo<NexusButtonCost>(field.value);
                        button.costs.push_back(cost);
                    }
                }
            }

            // Auto-register the button
            sys->savedButtons.push_back(button);

            return nullptr;
        }

        NexusSystem *sys;
    };

    struct NexusModule : public SysModule
    {
        NexusModule(NexusSystem *sys)
        {
            // Existing functions
            addSystemFunction<CreateNexusButton>("NexusButton");
            addSystemFunction<RegisterNexusButton>("registerNexusButton", sys);
            addSystemFunction<TrackNewResource>("addResourceDisplay", sys);
            addSystemFunction<CreateGenerator>("createGenerator", sys);
            addSystemFunction<CreateButtonCost>("ButtonCost");
            addSystemFunction<CreateConverter>("Converter");
            addSystemFunction<RegisterConverter>("registerConverter", sys);

            // New helper functions for simplified button creation
            addSystemFunction<QuickButton>("quickButton", sys);
            addSystemFunction<ParseRequirement>("req");
            addSystemFunction<CreateGamelog>("gamelog");
            addSystemFunction<CreateFact>("fact");
            addSystemFunction<CreateResource>("resource");
            addSystemFunction<CreateIncrease>("increase");

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
    }

    void NexusSystem::execute()
    {
        if (activeButton and deltaTime > 0)
        {
            currentActiveButton->activeTime += deltaTime;

            WorldFacts* wf = ecsRef->getSystem<WorldFacts>();

            if (not isButtonClickable(wf->factMap, *currentActiveButton))
            {
                currentActiveButton->activeTime -= deltaTime;
                currentActiveButton->active = false;
                activeButton = false;

                ecsRef->sendEvent(CurrentActiveButton{std::vector<std::string>{}});

                return;
            }

            if (currentActiveButton->activeTime >= currentActiveButton->activationTime)
            {
                // Todo check if all the conditions are met
                for (auto it2 : currentActiveButton->outcome)
                {
                    it2.call(ecsRef);
                }

                if (not currentActiveButton->costs.empty())
                {
                    for (size_t i = 0; i < currentActiveButton->costs.size(); ++i)
                    {
                        auto it3 = currentActiveButton->costs[i];

                        if (it3.consumed)
                        {
                            auto cost = IncreaseFact(it3.resourceId, -it3.value);

                            if (it3.valueId != "" and wf->factMap.find(it3.valueId) != wf->factMap.end())
                            {
                                cost.value = -wf->factMap.at(it3.valueId);
                            }

                            if (currentActiveButton->costIncrease.size() > i and cost.value.isNumber())
                            {
                                cost.value = ElementType{cost.value.get<float>() * std::pow(currentActiveButton->costIncrease[i], currentActiveButton->nbClick)};
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

                    ecsRef->sendEvent(CurrentActiveButton{std::vector<std::string>{}});

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

        auto listView = makeListView(this, 1, 1, 240, 1);

        auto listViewComp = listView.get<ListView>();
        listViewComp->spacing = 8;

        auto listViewUi = listView.get<UiAnchor>();

        listViewUi->setTopAnchor(windowAnchor->top);
        listViewUi->setTopMargin(120);
        listViewUi->setBottomAnchor(windowAnchor->bottom);
        listViewUi->setLeftAnchor(windowAnchor->left);
        listViewUi->setLeftMargin(10);

        resLayout = listView.entity;

        // auto categoryListView = makeListView(this, 1, 1, 350, 1);
        auto categoryListView = makeVerticalLayout(this, 1, 1, 350, 1, true);
        auto categoryView = categoryListView.get<VerticalLayout>();
        auto categoryListUi = categoryListView.get<UiAnchor>();

        // categoryView->spacing = 8;

        categoryListUi->setTopAnchor(windowAnchor->top);
        categoryListUi->setTopMargin(120);
        categoryListUi->setBottomAnchor(windowAnchor->bottom);
        categoryListUi->setLeftAnchor(listViewUi->right);
        categoryListUi->setLeftMargin(10);

        auto logViewEnt = ecsRef->getEntity("logview");
        auto logViewUi = logViewEnt->get<UiAnchor>();

        categoryListUi->setRightAnchor(logViewUi->left);

        categoryList = categoryListView.entity;

        auto harvestCat = createCategoryUI("Harvest");
        auto mainCat = createCategoryUI("Main");
        auto taskCat = createCategoryUI("Task");
        auto upgradeCat = createCategoryUI("Upgrade");

        categoryView->addEntity(harvestCat);
        categoryView->addEntity(mainCat);
        categoryView->addEntity(taskCat);
        categoryView->addEntity(upgradeCat);

        for (const auto& category : nexusSys->categories)
        {
            if (category == "Main" or category == "Upgrade" or category == "Task" or category == "Harvest")
            {
                continue;
            }

            auto cat = createCategoryUI(category);

            categoryView->addEntity(cat);
        }

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

        tooltipCostSpacerAnchor->setWidthConstrain(PosConstrain{tooltipBg.entity.id, AnchorType::Width, PosOpType::Mul, theme.values["tooltipSpacer.ratio"].get<float>()});

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
        costValuesText->spacing = 2.0f;

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

        // -- [Start] Active Focus

        auto activeFocusTop = makeUiSimple2DShape(this, Shape2D::Square, theme.values["nexusbutton.width"].get<float>() * 0.85, 1, {theme.values["activeFocus.r"].get<float>(), theme.values["activeFocus.g"].get<float>(), theme.values["activeFocus.b"].get<float>(), theme.values["activeFocus.a"].get<float>()});
        auto activeFocusLeft = makeUiSimple2DShape(this, Shape2D::Square, 1, theme.values["nexusbutton.height"].get<float>() * 0.70, {theme.values["activeFocus.r"].get<float>(), theme.values["activeFocus.g"].get<float>(), theme.values["activeFocus.b"].get<float>(), theme.values["activeFocus.a"].get<float>()});
        auto activeFocusRight = makeUiSimple2DShape(this, Shape2D::Square, 1, theme.values["nexusbutton.height"].get<float>() * 0.70, {theme.values["activeFocus.r"].get<float>(), theme.values["activeFocus.g"].get<float>(), theme.values["activeFocus.b"].get<float>(), theme.values["activeFocus.a"].get<float>()});
        auto activeFocusBottom = makeUiSimple2DShape(this, Shape2D::Square, theme.values["nexusbutton.width"].get<float>() * 0.85, 1, {theme.values["activeFocus.r"].get<float>(), theme.values["activeFocus.g"].get<float>(), theme.values["activeFocus.b"].get<float>(), theme.values["activeFocus.a"].get<float>()});

        auto activeFocusTopPos = activeFocusTop.get<PositionComponent>();
        auto activeFocusTopAnchor = activeFocusTop.get<UiAnchor>();
        activeFocusTopPos->setVisibility(false);
        activeFocusTopPos->setZ(3);

        auto activeFocusLeftPos = activeFocusLeft.get<PositionComponent>();
        auto activeFocusLeftAnchor = activeFocusLeft.get<UiAnchor>();
        activeFocusLeftPos->setVisibility(false);
        activeFocusLeftPos->setZ(3);
        activeFocusLeftAnchor->setTopAnchor(activeFocusTopAnchor->top);
        activeFocusLeftAnchor->setLeftAnchor(activeFocusTopAnchor->left);

        auto activeFocusRightPos = activeFocusRight.get<PositionComponent>();
        auto activeFocusRightAnchor = activeFocusRight.get<UiAnchor>();
        activeFocusRightPos->setVisibility(false);
        activeFocusRightPos->setZ(3);
        activeFocusRightAnchor->setTopAnchor(activeFocusTopAnchor->top);
        activeFocusRightAnchor->setRightAnchor(activeFocusTopAnchor->right);

        auto activeFocusBottomPos = activeFocusBottom.get<PositionComponent>();
        auto activeFocusBottomAnchor = activeFocusBottom.get<UiAnchor>();
        activeFocusBottomPos->setVisibility(false);
        activeFocusBottomPos->setZ(3);
        activeFocusBottomAnchor->setBottomAnchor(activeFocusLeftAnchor->bottom);
        activeFocusBottomAnchor->setLeftAnchor(activeFocusTopAnchor->left);

        activeButtonsUi["topHighlight"] = activeFocusTop.entity;
        activeButtonsUi["leftHighlight"] = activeFocusLeft.entity;
        activeButtonsUi["rightHighlight"] = activeFocusRight.entity;
        activeButtonsUi["bottomHighlight"] = activeFocusBottom.entity;

        // -- [End] Active Focus

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
            if (event.values.find("res") == event.values.end() or (event.values.find("value") == event.values.end() and event.values.find("valueId") == event.values.end()))
            {
                LOG_ERROR("NexusScene", "Event 'one_shot_res' received without res and value.");
                return;
            }

            auto res = event.values.at("res").get<std::string>();

            ElementType givenValue;

            if (event.values.find("value") != event.values.end())
            {
                givenValue = event.values.at("value");
            }

            WorldFacts* wf = ecsRef->getSystem<WorldFacts>();
            if (not wf) return;

            if (event.values.find("valueId") != event.values.end())
            {
                auto valueId = event.values.at("valueId").get<std::string>();

                auto it = wf->factMap.find(valueId);
                if (it != wf->factMap.end())
                {
                    givenValue = it->second;
                }
                else
                {
                    LOG_ERROR("NexusScene", "Event 'one_shot_res' received with valueId '" << valueId << "' not found.");
                    return;
                }
            }

            float value = 0.0f;
            float maxValue = 0.0f;
            bool hasMax = false;

            auto it = wf->factMap.find(res);
            if (it != wf->factMap.end())
            {
                value = it->second.get<float>();
            }

            auto itMax = wf->factMap.find(res + "_max_value");
            if (itMax != wf->factMap.end())
            {
                maxValue = itMax->second.get<float>();
                hasMax = true;
            }

            if (hasMax)
            {
                auto availableSpace = maxValue - value;

                if (availableSpace <= 0)
                {
                    LOG_INFO("RessourceGeneratorHarvest", "No space left for ressource '" << res << "'");
                    return;
                }

                availableSpace = std::min(availableSpace, givenValue.get<float>());

                ecsRef->sendEvent(IncreaseFact{res, availableSpace});
                ecsRef->sendEvent(IncreaseFact{"total_" + res, availableSpace});
            }
            else
            {
                ecsRef->sendEvent(IncreaseFact{res, givenValue});
                ecsRef->sendEvent(IncreaseFact{"total_" + res, givenValue});
            }
        });

        listenToEvent<UpdateGenView>([this](const UpdateGenView&) {
            updateGeneratorViews();
        });

        listenToStandardEvent("activate_gen", [this](const StandardEvent&) {
            updateGeneratorViews();
        });

        // Listen for world fact updates to log mana or upgrades.
        listenToEvent<WorldFactsUpdate>([this](const WorldFactsUpdate&) {
            updateGeneratorViews();
            updateUi = true;
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
                tooltipsEntities["background"]->get<PositionComponent>()->setVisibility(false);
                tooltipsEntities["backHighlight"]->get<PositionComponent>()->setVisibility(false);

                tooltipsEntities["desc"]->get<PositionComponent>()->setVisibility(false);

                tooltipsEntities["costSpacer"]->get<PositionComponent>()->setVisibility(false);
                tooltipsEntities["costTitle"]->get<PositionComponent>()->setVisibility(false);

                tooltipsEntities["costValues"]->get<PositionComponent>()->setVisibility(false);

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

                        bool addEscape = false;

                        for (size_t i = 0; i < it->costs.size(); ++i)
                        {
                            if (addEscape)
                            {
                                costText << "\n";
                            }

                            const auto& cost = it->costs[i];

                            auto str = cost.resourceId;
                            str[0] = std::toupper(str[0]);

                            auto value = cost.value;

                            if (it->costIncrease.size() > i)
                            {
                                value = value * std::pow(it->costIncrease[i], it->nbClick);
                            }

                            costText << str << ": " << value;

                            addEscape = true;
                        }

                        tooltipsEntities["costValues"]->get<TTFText>()->setText(costText.str());

                        tooltipsEntities["background"]->get<UiAnchor>()->setBottomAnchor(tooltipsEntities["costValues"]->get<UiAnchor>()->bottom);
                    }
                }

                if (not it->clickable)
                    return;

                if (bgEnt->has<Simple2DObject>())
                {
                    auto colors = getButtonColors(theme, true, it->activable, event.state);

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
            auto category = it->category;

            // Archive the button and remove it from the visible buttons.
            archivedButtons.push_back(*it);
            visibleButtons.erase(it);

            auto layout = categoryMap[category]->get<HorizontalLayout>();

            layout->removeEntity(id);

            auto nbVisible = getNbVisibleElementsInLayout(layout);

            if (nbVisible <= 1)
            {
                categoryMap[category + "_main"]->get<PositionComponent>()->setVisibility(false);
            }
        });

        listenToEvent<CurrentActiveButton>([this](const CurrentActiveButton& event) {
            if (event.ids.empty())
            {
                activeButtonsUi["topHighlight"]->get<PositionComponent>()->setVisibility(false);
                activeButtonsUi["leftHighlight"]->get<PositionComponent>()->setVisibility(false);
                activeButtonsUi["rightHighlight"]->get<PositionComponent>()->setVisibility(false);
                activeButtonsUi["bottomHighlight"]->get<PositionComponent>()->setVisibility(false);
            }
            else
            {
                auto it = std::find_if(visibleButtons.begin(), visibleButtons.end(), [&event](const DynamicNexusButton& button) {
                    return button.id == event.ids[0];
                });

                if (it != visibleButtons.end())
                {
                    activeButtonsUi["topHighlight"]->get<PositionComponent>()->setVisibility(true);
                    activeButtonsUi["leftHighlight"]->get<PositionComponent>()->setVisibility(true);
                    activeButtonsUi["rightHighlight"]->get<PositionComponent>()->setVisibility(true);
                    activeButtonsUi["bottomHighlight"]->get<PositionComponent>()->setVisibility(true);

                    auto ent = ecsRef->getEntity(it->backgroundId);

                    if (ent)
                    {
                        activeButtonsUi["topHighlight"]->get<UiAnchor>()->setTopAnchor(ent->get<UiAnchor>()->top);
                        activeButtonsUi["topHighlight"]->get<UiAnchor>()->setHorizontalCenter(ent->get<UiAnchor>()->horizontalCenter);

                        activeButtonsUi["topHighlight"]->get<UiAnchor>()->setTopMargin(theme.values["nexusbutton.height"].get<float>() * 0.15f);
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

                    for (size_t i = 0; i < it->costs.size(); ++i)
                    {
                        auto it3 = it->costs[i];

                        if (it3.consumed)
                        {
                            auto cost = IncreaseFact(it3.resourceId, -it3.value);

                            if (it3.valueId != "" and wf->factMap.find(it3.valueId) != wf->factMap.end())
                            {
                                cost.value = -wf->factMap.at(it3.valueId);
                            }

                            if (it->costIncrease.size() > i and cost.value.isNumber())
                            {
                                cost.value = ElementType{cost.value.get<float>() * std::pow(it->costIncrease[i], it->nbClick)};
                            }

                            ecsRef->sendEvent(cost);
                        }
                    }

                    std::ostringstream costText;
                    bool addEscape = false;

                    for (size_t i = 0; i < it->costs.size(); ++i)
                    {
                        if (addEscape)
                        {
                            costText << "\n";
                        }

                        const auto& cost = it->costs[i];

                        auto str = cost.resourceId;
                        str[0] = std::toupper(str[0]);

                        auto value = cost.value;

                        if (it->costIncrease.size() > i)
                        {
                            value = value * std::pow(it->costIncrease[i], it->nbClick + 1);
                        }

                        costText << str << ": " << value;

                        addEscape = true;
                    }

                    LOG_INFO("Nexus Scene", "Cost text: " << costText.str());

                    tooltipsEntities["costValues"]->get<TTFText>()->setText(costText.str());
                }

                it->nbClick++;

                LOG_ERROR("NexusScene", "Button clicked: " << buttonId << ", clicked: " << it->nbClick << ", it->id: " << it->id);

                if (it->nbClickBeforeArchive != 0 and it->nbClick >= it->nbClickBeforeArchive)
                {
                    it->archived = true;

                    ecsRef->sendEvent(NexusButtonStateChange{*it});

                    auto id = it->entityId;
                    auto category = it->category;

                    // Archive the button and remove it from the visible buttons.
                    archivedButtons.push_back(*it);
                    visibleButtons.erase(it);

                    auto layout = categoryMap[category]->get<HorizontalLayout>();

                    layout->removeEntity(id);

                    auto nbVisible = getNbVisibleElementsInLayout(layout);

                    if (nbVisible <= 1)
                    {
                        categoryMap[category + "_main"]->get<PositionComponent>()->setVisibility(false);
                    }
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

        ecsRef->sendEvent(UpdateGenView{});

        updateUi = true;
    }

    void NexusScene::execute()
    {
        if (updateUi)
        {
            auto sys = ecsRef->getSystem<WorldFacts>();

            updateDynamicButtons(sys->factMap);
            updateRessourceView();

            updateUi = false;
        }

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
        constant::Vector4D colors = getButtonColors(scene->theme, button->clickable, button->activable);

        colors.w = 0.0f;

        auto background = makeUiSimple2DShape(scene->ecsRef, Shape2D::Square, scene->theme.values["nexusbutton.width"].get<float>(), scene->theme.values["nexusbutton.height"].get<float>(), colors);
        auto backgroundAnchor = background.get<UiAnchor>();

        scene->ecsRef->attach<MouseLeftClickComponent>(background.entity, makeCallable<StandardEvent>("nexus_button_clicked", "id", id));

        scene->ecsRef->attach<MouseEnterComponent>(background.entity, makeCallable<OnBackgroundButtonHover>(OnBackgroundButtonHover{button->id, background.entity.id, true}));
        scene->ecsRef->attach<MouseLeaveComponent>(background.entity, makeCallable<OnBackgroundButtonHover>(OnBackgroundButtonHover{button->id, background.entity.id, false}));

        auto tweenComp = TweenComponent{
            0.0f,
            255.0f,
            350.0f,
            [background](const TweenValue& value) { background.get<Simple2DObject>()->setOpacity(std::get<float>(value)); },
        };

        tweenComp.easing = TweenQuad;

        scene->ecsRef->attach<TweenComponent>(background.entity, tweenComp);

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

    // Helper function to create a category UI panel in the Nexus scene.
    EntityRef NexusScene::createCategoryUI(const std::string &categoryName)
    {
        // Create a vertical layout to hold the category title, separator, and button layout.
        auto categoryLayout = makeVerticalLayout(this, 30, 100, 500, 150); // Adjust x, y, width, height as needed.
        auto verticalLayout = categoryLayout.get<VerticalLayout>();
        categoryLayout.get<PositionComponent>()->setVisibility(false); // Initially hide the category layout.
        verticalLayout->spacing = 5; // Space between elements in the category panel.

        // 1. Category Name: Create a TTFText element for the category title.
        auto titleEnt = makeTTFText(this, 0, 0, 1, theme.values["categoryTitle.font"].get<std::string>(), categoryName, theme.values["categoryTitle.scale"].get<float>());
        // Optionally, you can adjust the anchor of the title here.
        verticalLayout->addEntity(titleEnt.entity);

        // 2. Separator Line: Create a 2D shape rectangle with a height of 1.
        float lineWidth = 500; // Adjust as needed to match category panel width.
        auto lineEnt = makeUiSimple2DShape(this, Shape2D::Square, lineWidth, 1, {200, 200, 200, 255});
        lineEnt.get<UiAnchor>()->setWidthConstrain(PosConstrain{categoryList.id, AnchorType::Width, PosOpType::Mul, 0.9f});
        verticalLayout->addEntity(lineEnt.entity);

        // 3. Horizontal Layout for Buttons: Create an HLayout that will hold the buttons.
        auto buttonLayoutEnt = makeHorizontalLayout(this, 0, 0, lineWidth, 60); // Adjust height as needed.
        buttonLayoutEnt.get<UiAnchor>()->setWidthConstrain(PosConstrain{categoryList.id, AnchorType::Width, PosOpType::Mul, 0.9f});
        auto buttonLayout = buttonLayoutEnt.get<HorizontalLayout>();
        buttonLayout->spacing = 10;
        buttonLayout->fitToAxis = true;
        buttonLayout->spaced = false;

        // buttonLayout->setVisibility(false); // Initially hide the button layout.

        categoryMap[categoryName] = buttonLayoutEnt.entity;

        categoryMap[categoryName + "_main"] = categoryLayout.entity;

        // Add the horizontal button layout to the vertical category layout.
        verticalLayout->addEntity(buttonLayoutEnt.entity);

        // Return the category layout entity for further positioning or manipulation.
        return categoryLayout.entity;
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

                auto colors = getButtonColors(theme, clickable, button.activable);

                if (background->has<Simple2DObject>())
                    background->get<Simple2DObject>()->setColors(colors);
            }
        }
    }

    void NexusScene::updateButtonsVisibility(const std::unordered_map<std::string, ElementType>& factMap, std::vector<DynamicNexusButton>& in, std::vector<DynamicNexusButton>& out, bool visiblility)
    {
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

                        categoryMap[it->category]->get<HorizontalLayout>()->addEntity(buttonEntity);

                        categoryMap[it->category + "_main"]->get<PositionComponent>()->setVisibility(true);
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
                        auto category = it->category;
                        auto layout = categoryMap[category]->get<HorizontalLayout>();

                        layout->removeEntity(it->entityId);

                        it->entityId = 0;

                        auto nbVisible = getNbVisibleElementsInLayout(layout);

                        if (nbVisible <= 1)
                        {
                            categoryMap[category + "_main"]->get<PositionComponent>()->setVisibility(false);
                        }
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

        // Todo remove this
        ecsRef->sendEvent(SkipRenderPass{4});

        // Update value cost of visible buttons
        for (auto& button : visibleButtons)
        {
            for (size_t i = 0; i < button.costs.size(); ++i)
            {
                auto& cost = button.costs[i];

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