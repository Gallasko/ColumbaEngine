#include "nexusscene.h"

#include "managenerator.h"
#include "gamefacts.h"
#include "gamemodule.h"

#include "Interpreter/pginterpreter.h"
#include "Systems/logmodule.h"

namespace pg
{
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

    class CreateReward : public Function
    {
        using Function::Function;
    public:
        void setUp() { setArity(3, -1); } // Infinite arguments like AchievementRewardEvent

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            // Same pattern as CreateAchievementRewardEvent
            std::string eventName = args.front()->getElement().toString();
            args.pop();
            std::string key = args.front()->getElement().toString();
            args.pop();

            const auto& message = args.front()->getElement();
            StandardEvent ev(eventName, key, message);
            args.pop();

            while (!args.empty())
            {
                std::string key = args.front()->getElement().toString();
                args.pop();

                if (args.empty())
                    throw RuntimeException(token, "Invalid number of arguments for function call: '" + token.text + "' expected value after a key .");

                ev.values[key] = args.front()->getElement();
                args.pop();
            }

            AchievementReward reward(ev);
            return serializeToInterpreter(this, reward);
        }
    };

    class CreateCost : public Function
    {
        using Function::Function;
    public:
        void setUp() { setArity(2, 4); } // resourceId, value, [valueId], [consumed]

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            NexusButtonCost cost;

            cost.resourceId = args.front()->getElement().toString();
            args.pop();
            cost.value = args.front()->getElement().get<float>();
            args.pop();

            if (!args.empty())
            {
                cost.valueId = args.front()->getElement().toString();
                args.pop();
            }

            if (!args.empty())
            {
                cost.consumed = args.front()->getElement().get<bool>();
                args.pop();
            }
            else
            {
                cost.consumed = true; // Default to consumed
            }

            return serializeToInterpreter(this, cost);
        }
    };

    class SetDefaultTags : public Function
    {
        using Function::Function;
    public:
        void setUp(NexusSystem *sys)
        {
            this->sys = sys;
            setArity(1, 1); // Array of tags
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            // Clear both tags and stack to prevent bugs
            sys->defaultPrestigeTags.clear();
            while (!sys->prestigeTagStack.empty()) {
                sys->prestigeTagStack.pop();
            }

            auto tagsArg = args.front();
            args.pop();

            if (tagsArg->getType() == "ClassInstance")
            {
                auto tagList = std::static_pointer_cast<ClassInstance>(tagsArg);
                for (const auto& field : tagList->getFields())
                {
                    sys->defaultPrestigeTags.push_back(field.value->getElement().toString());
                }
            }

            return nullptr;
        }

        NexusSystem* sys;
    };

    class PushDefaultTags : public Function
    {
        using Function::Function;
    public:
        void setUp(NexusSystem *sys)
        {
            this->sys = sys;
            setArity(1, 1); // Array of additional tags
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            // Save current tags to stack
            sys->prestigeTagStack.push(sys->defaultPrestigeTags);

            auto tagsArg = args.front();
            args.pop();

            if (tagsArg->getType() == "ClassInstance")
            {
                auto tagList = std::static_pointer_cast<ClassInstance>(tagsArg);
                for (const auto& field : tagList->getFields())
                {
                    sys->defaultPrestigeTags.push_back(field.value->getElement().toString());
                }
            }

            return nullptr;
        }

        NexusSystem* sys;
    };

    class PopDefaultTags : public Function
    {
        using Function::Function;
    public:
        void setUp(NexusSystem *sys)
        {
            this->sys = sys;
            setArity(0, 0);
        }

        virtual ValuablePtr call(ValuableQueue&) override
        {
            if (!sys->prestigeTagStack.empty())
            {
                sys->defaultPrestigeTags = sys->prestigeTagStack.top();
                sys->prestigeTagStack.pop();
            }

            return nullptr;
        }

        NexusSystem* sys;
    };

    class ClearDefaultTags : public Function
    {
        using Function::Function;
    public:
        void setUp(NexusSystem *sys)
        {
            this->sys = sys;
            setArity(0, 0);
        }

        virtual ValuablePtr call(ValuableQueue&) override
        {
            // Clear both tags and stack to prevent bugs
            sys->defaultPrestigeTags.clear();
            while (!sys->prestigeTagStack.empty()) {
                sys->prestigeTagStack.pop();
            }
            return nullptr;
        }

        NexusSystem* sys;
    };

    class QuickButton : public Function
    {
        using Function::Function;
    public:
        void setUp(NexusSystem *sys)
        {
            this->sys = sys;
            setArity(5, 9); // id, label, requirements[], outcomes[], description, [category], [clicks], [costs[]], [tags[]]
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

            // Optional prestige tags array (9th parameter) - if not provided, use defaults
            if (!args.empty())
            {
                auto tagsArg = args.front();
                args.pop();
                if (tagsArg->getType() == "ClassInstance")
                {
                    auto tagList = std::static_pointer_cast<ClassInstance>(tagsArg);
                    for (const auto& field : tagList->getFields())
                    {
                        button.prestigeTags.push_back(field.value->getElement().toString());
                    }
                }
            }
            else
            {
                // Use default tags from NexusSystem
                button.prestigeTags = sys->defaultPrestigeTags;
            }

            // Auto-register the button
            sys->savedButtons.push_back(button);

            return nullptr;
        }

        NexusSystem *sys;
    };

    class CreateAutoClicker : public Function
    {
        using Function::Function;
    public:
        void setUp(AutoClickerSystem *sys)
        {
            this->sys = sys;
            setArity(3, 5); // id, targetButtonId, baseInterval, [costs[]], [unlockConditions[]]
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            AutoClicker clicker;

            clicker.id = args.front()->getElement().toString();
            args.pop();
            clicker.targetButtonId = args.front()->getElement().toString();
            args.pop();
            clicker.baseInterval = args.front()->getElement().get<float>();
            args.pop();

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
                        clicker.costs.push_back(cost);
                    }
                }
            }

            // Optional unlock conditions array
            if (!args.empty())
            {
                auto conditionsArg = args.front();
                args.pop();
                if (conditionsArg->getType() == "ClassInstance")
                {
                    auto conditionList = std::static_pointer_cast<ClassInstance>(conditionsArg);
                    for (const auto& field : conditionList->getFields())
                    {
                        auto condition = deserializeTo<FactChecker>(field.value);
                        clicker.unlockConditions.push_back(condition);
                    }
                }
            }

            // Add to available auto-clickers
            sys->availableAutoClickers.push_back(clicker);

            // Create the auto-clicker facts immediately
            sys->createAutoClickerFacts(clicker);

            return nullptr;
        }

        AutoClickerSystem *sys;
    };

    class PurchaseAutoClickerReward : public Function
    {
        using Function::Function;
    public:
        void setUp() { setArity(1, 1); }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            auto autoClickerId = args.front()->getElement().toString();
            args.pop();

            auto event = StandardEvent("purchase_autoclicker", "id", autoClickerId);
            return serializeToInterpreter(this, AchievementReward(event));
        }
    };

    class ToggleAutoClickerReward : public Function
    {
        using Function::Function;
    public:
        void setUp() { setArity(1, 2); } // autoClickerId, [enable]

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            auto autoClickerId = args.front()->getElement().toString();
            args.pop();

            bool enable = true; // Default to enable
            if (!args.empty())
            {
                enable = args.front()->getElement().get<bool>();
                args.pop();
            }

            auto event = StandardEvent("toggle_autoclicker", "id", autoClickerId, "enable", ElementType(enable));
            return serializeToInterpreter(this, AchievementReward(event));
        }
    };

    struct NexusModule : public SysModule
    {
        NexusModule(NexusSystem *sys, AutoClickerSystem *autoClickerSys)
        {
            // Existing functions
            addSystemFunction<CreateNexusButton>("NexusButton");
            addSystemFunction<RegisterNexusButton>("registerNexusButton", sys);
            addSystemFunction<TrackNewResource>("addResourceDisplay", sys);
            addSystemFunction<CreateGenerator>("createGenerator", sys);
            addSystemFunction<CreateButtonCost>("ButtonCost");
            addSystemFunction<CreateConverter>("Converter");
            addSystemFunction<RegisterConverter>("registerConverter", sys);

            // Tag system functions
            addSystemFunction<SetDefaultTags>("setDefaultTags", sys);
            addSystemFunction<PushDefaultTags>("pushDefaultTags", sys);
            addSystemFunction<PopDefaultTags>("popDefaultTags", sys);
            addSystemFunction<ClearDefaultTags>("clearDefaultTags", sys);

            // New helper functions for simplified button creation
            addSystemFunction<QuickButton>("quickButton", sys);
            addSystemFunction<ParseRequirement>("req");
            addSystemFunction<CreateGamelog>("gamelog");
            addSystemFunction<CreateFact>("fact");
            addSystemFunction<CreateResource>("resource");
            addSystemFunction<CreateIncrease>("increase");

            // Simple helper functions
            addSystemFunction<CreateReward>("reward");
            addSystemFunction<CreateCost>("cost");

            // AutoClicker functions
            addSystemFunction<CreateAutoClicker>("createAutoClicker", autoClickerSys);
            addSystemFunction<PurchaseAutoClickerReward>("purchaseAutoClicker");
            addSystemFunction<ToggleAutoClickerReward>("toggleAutoClicker");

            //Todo add basic generator / converter ids as system vars
            //addSystemVar("SCANCODE_A", SDL_SCANCODE_A);
        }
    };

    void NexusSystem::init()
    {
        PgInterpreter interpreter;

        // Get the AutoClickerSystem reference
        auto* autoClickerSystem = ecsRef->getSystem<AutoClickerSystem>();

        interpreter.addSystemModule("nexus", NexusModule{this, autoClickerSystem});
        interpreter.addSystemModule("log", LogModule{nullptr});
        interpreter.addSystemModule("achievement", AchievementModule{});

        interpreter.interpretFromFile("res/main.pg");
    }
}