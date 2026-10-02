#include "nexusscene.h"

#include "managenerator.h"
#include "Systems/gamefacts.h"
#include "gamemodule.h"

#include "Compiler/vm.h"
#include "Systems/lognativemodule.h"

namespace pg
{
    namespace
    {
        // A requirement is written as: name, !name, name>=value, name<=value, name>value, name<value or name==value
        FactChecker parseRequirementString(const std::string& req)
        {
            if (req.length() > 0 and req[0] == '!')
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

                if (valueStr == "true")
                    return FactChecker(name, true, FactCheckEquality::Equal);

                if (valueStr == "false")
                    return FactChecker(name, false, FactCheckEquality::Equal);

                return FactChecker(name, std::stof(valueStr), FactCheckEquality::Equal);
            }

            return FactChecker(req, true, FactCheckEquality::Equal);
        }

        // Arguments: resourceId, [value], [valueId], [consumed]
        NexusButtonCost costFromArgs(VM* vm, int argCount, Value* args)
        {
            NexusButtonCost cost;

            cost.resourceId = argString(vm, args[0]);

            if (argCount > 1)
                cost.value = argElement(vm, args[1]).get<float>();

            if (argCount > 2)
                cost.valueId = argString(vm, args[2]);

            if (argCount > 3)
                cost.consumed = argElement(vm, args[3]).get<bool>();

            return cost;
        }

        Value stringVector(VM* vm, const std::vector<std::string>& strings)
        {
            Value vectorValue = vm->createVector();

            auto vector = vm->asVector(vectorValue);

            for (const auto& string : strings)
            {
                vector->fields.push_back(vm->createString(string));
            }

            return vectorValue;
        }

        std::vector<float> listFloats(VM* vm, Value list)
        {
            std::vector<float> floats;

            for (const auto& value : listValues(vm, list))
            {
                floats.push_back(argElement(vm, value).get<float>());
            }

            return floats;
        }
    }

    /**
     * Nexus Module
     * Lets a script declare the buttons, generators, converters and auto clickers of the nexus
     */
    class NexusNativeModule : public NativeModule
    {
    public:
        NexusNativeModule(NexusSystem *sys, AutoClickerSystem *autoClickerSys, std::shared_ptr<ScriptHandles> handles)
        {
            addNativeFunction("addResourceDisplay", [sys](VM* vm, int argCount, Value* args) -> Value {
                checkArgCount("addResourceDisplay", argCount, 1, 1);

                sys->addResourceDisplay(argString(vm, args[0]));

                return makeBoolValue(true);
            });

            // Arguments: id, ressource, [currentMana], [productionRate], [capacity], [active], [tags[]]
            addNativeFunction("createGenerator", [sys](VM* vm, int argCount, Value* args) -> Value {
                checkArgCount("createGenerator", argCount, 2, 7);

                RessourceGenerator gen;

                gen.id = argString(vm, args[0]);
                gen.ressource = argString(vm, args[1]);

                if (argCount > 2)
                    gen.currentMana = argElement(vm, args[2]).get<float>();

                if (argCount > 3)
                    gen.productionRate = argElement(vm, args[3]).get<float>();

                if (argCount > 4)
                    gen.capacity = argElement(vm, args[4]).get<float>();

                if (argCount > 5)
                    gen.active = argElement(vm, args[5]).get<bool>();

                // Without tags the generator takes the default ones
                gen.prestigeTags = argCount > 6 ? listStrings(vm, args[6]) : sys->defaultPrestigeTags;

                sys->ecsRef->sendEvent(NewGeneratorEvent{gen});

                return vm->createString(gen.id);
            });

            addNativeFunction("ButtonCost", [handles](VM* vm, int argCount, Value* args) -> Value {
                checkArgCount("ButtonCost", argCount, 1, 4);

                return handles->add(costFromArgs(vm, argCount, args));
            });

            // The script fills input, output, cost and yield on the returned table before registering it
            addNativeFunction("Converter", [sys](VM* vm, int argCount, Value* args) -> Value {
                checkArgCount("Converter", argCount, 1, 2);

                Value converterValue = vm->createTable();

                auto converter = vm->asInstance(converterValue);

                converter->setField("id", vm->createString(argString(vm, args[0])), vm, true);
                converter->setField("active", makeBoolValue(false), vm, true);
                converter->setField("prestigeTags", stringVector(vm, argCount > 1 ? listStrings(vm, args[1]) : sys->defaultPrestigeTags), vm, true);

                return converterValue;
            });

            addNativeFunction("registerConverter", [sys](VM* vm, int argCount, Value* args) -> Value {
                checkArgCount("registerConverter", argCount, 1, 1);

                if (not IS_INSTANCE(args[0]))
                {
                    throw std::runtime_error("registerConverter expects the table returned by Converter");
                }

                auto table = vm->asInstance(args[0]);

                ConverterComponent converter;

                if (table->hasField("id"))
                    converter.id = argString(vm, table->getField("id"));

                if (table->hasField("input"))
                    converter.input = listStrings(vm, table->getField("input"));

                if (table->hasField("output"))
                    converter.output = listStrings(vm, table->getField("output"));

                if (table->hasField("cost"))
                    converter.cost = listFloats(vm, table->getField("cost"));

                if (table->hasField("yield"))
                    converter.yield = listFloats(vm, table->getField("yield"));

                if (table->hasField("active"))
                    converter.active = argElement(vm, table->getField("active")).get<bool>();

                if (table->hasField("prestigeTags"))
                    converter.prestigeTags = listStrings(vm, table->getField("prestigeTags"));

                auto converterEntity = sys->ecsRef->createEntity();

                sys->ecsRef->attach<ConverterComponent>(converterEntity, converter);

                return makeIntValue(static_cast<int64_t>(converterEntity.id));
            });

            // Tag system functions
            addNativeFunction("setDefaultTags", [sys](VM* vm, int argCount, Value* args) -> Value {
                checkArgCount("setDefaultTags", argCount, 1, 1);

                // Clear both tags and stack to prevent bugs
                while (not sys->prestigeTagStack.empty())
                {
                    sys->prestigeTagStack.pop();
                }

                sys->defaultPrestigeTags = listStrings(vm, args[0]);

                return makeBoolValue(true);
            });

            addNativeFunction("pushDefaultTags", [sys](VM* vm, int argCount, Value* args) -> Value {
                checkArgCount("pushDefaultTags", argCount, 1, 1);

                // Save current tags to stack
                sys->prestigeTagStack.push(sys->defaultPrestigeTags);

                for (const auto& tag : listStrings(vm, args[0]))
                {
                    sys->defaultPrestigeTags.push_back(tag);
                }

                return makeBoolValue(true);
            });

            addNativeFunction("popDefaultTags", [sys](VM*, int argCount, Value*) -> Value {
                checkArgCount("popDefaultTags", argCount, 0, 0);

                if (not sys->prestigeTagStack.empty())
                {
                    sys->defaultPrestigeTags = sys->prestigeTagStack.top();
                    sys->prestigeTagStack.pop();
                }

                return makeBoolValue(true);
            });

            addNativeFunction("clearDefaultTags", [sys](VM*, int argCount, Value*) -> Value {
                checkArgCount("clearDefaultTags", argCount, 0, 0);

                // Clear both tags and stack to prevent bugs
                sys->defaultPrestigeTags.clear();

                while (not sys->prestigeTagStack.empty())
                {
                    sys->prestigeTagStack.pop();
                }

                return makeBoolValue(true);
            });

            // Arguments: id, label, requirements[], outcomes[], description, [category], [costs[]], [clicks], [costIncrease[]], [activationTime], [tags[]]
            addNativeFunction("quickButton", [sys, handles](VM* vm, int argCount, Value* args) -> Value {
                checkArgCount("quickButton", argCount, 5, 11);

                DynamicNexusButton button;

                button.id = argString(vm, args[0]);
                button.label = argString(vm, args[1]);

                for (const auto& value : listValues(vm, args[2]))
                {
                    button.conditions.push_back(handles->checker(value));
                }

                for (const auto& value : listValues(vm, args[3]))
                {
                    button.outcome.push_back(handles->reward(value));
                }

                button.description = argString(vm, args[4]);

                button.category = argCount > 5 ? argString(vm, args[5]) : "Main";

                if (argCount > 6)
                {
                    for (const auto& value : listValues(vm, args[6]))
                    {
                        button.costs.push_back(handles->cost(value));
                    }
                }

                button.nbClickBeforeArchive = argCount > 7 ? argElement(vm, args[7]).get<size_t>() : 1;

                if (argCount > 8)
                    button.costIncrease = listFloats(vm, args[8]);

                // A button given an activation time becomes activable
                if (argCount > 9)
                {
                    button.activationTime = argElement(vm, args[9]).get<float>();

                    if (button.activationTime > 0.0f)
                        button.activable = true;
                }
                else
                {
                    button.activationTime = 1000.0f;
                }

                // Without tags the button takes the default ones
                button.prestigeTags = argCount > 10 ? listStrings(vm, args[10]) : sys->defaultPrestigeTags;

                FactMetadata buttonMetadata;

                buttonMetadata.meta["buttonId"] = ElementType(button.id);
                buttonMetadata.meta["nbTags"] = ElementType(static_cast<int>(button.prestigeTags.size()));

                for (size_t i = 0; i < button.prestigeTags.size(); ++i)
                {
                    buttonMetadata.meta["tag" + std::to_string(i)] = ElementType(button.prestigeTags[i]);
                }

                // Inject metadata into AddFact and IncreaseFact events in outcomes
                for (auto& outcome : button.outcome)
                {
                    if (outcome.type == AchievementRewardType::Add)
                    {
                        std::get<AddFact>(outcome.reward).metadata = buttonMetadata;
                    }
                    else if (outcome.type == AchievementRewardType::Increase)
                    {
                        std::get<IncreaseFact>(outcome.reward).metadata = buttonMetadata;
                    }
                }

                // Auto-register the button
                sys->savedButtons.push_back(button);

                return makeBoolValue(true);
            });

            // Helper functions building the requirements, outcomes and costs of a button
            addNativeFunction("req", [handles](VM* vm, int argCount, Value* args) -> Value {
                checkArgCount("req", argCount, 1, 1);

                return handles->add(parseRequirementString(argString(vm, args[0])));
            });

            addNativeFunction("gamelog", [handles](VM* vm, int argCount, Value* args) -> Value {
                checkArgCount("gamelog", argCount, 1, 1);

                return handles->add(AchievementReward(StandardEvent("gamelog", "message", argString(vm, args[0]))));
            });

            addNativeFunction("fact", [handles](VM* vm, int argCount, Value* args) -> Value {
                checkArgCount("fact", argCount, 2, 2);

                return handles->add(AchievementReward(AddFact{argString(vm, args[0]), argElement(vm, args[1])}));
            });

            addNativeFunction("resource", [handles](VM* vm, int argCount, Value* args) -> Value {
                checkArgCount("resource", argCount, 2, 2);

                return handles->add(AchievementReward(StandardEvent("one_shot_res", "res", argString(vm, args[0]), "value", argElement(vm, args[1]))));
            });

            addNativeFunction("increase", [handles](VM* vm, int argCount, Value* args) -> Value {
                checkArgCount("increase", argCount, 2, 2);

                return handles->add(AchievementReward(IncreaseFact{argString(vm, args[0]), argElement(vm, args[1])}));
            });

            addNativeFunction("reward", [handles](VM* vm, int argCount, Value* args) -> Value {
                return handles->add(AchievementReward(eventFromArgs(vm, "reward", argCount, args)));
            });

            // Arguments: resourceId, value, [valueId], [consumed]
            addNativeFunction("cost", [handles](VM* vm, int argCount, Value* args) -> Value {
                checkArgCount("cost", argCount, 2, 4);

                return handles->add(costFromArgs(vm, argCount, args));
            });

            addNativeFunction("harvest", [handles](VM* vm, int argCount, Value* args) -> Value {
                checkArgCount("harvest", argCount, 1, 1);

                return handles->add(AchievementReward(StandardEvent("res_harvest", "id", argString(vm, args[0]))));
            });

            // Arguments: resourceName, valueId
            addNativeFunction("resourceFrom", [handles](VM* vm, int argCount, Value* args) -> Value {
                checkArgCount("resourceFrom", argCount, 2, 2);

                return handles->add(AchievementReward(StandardEvent("one_shot_res", "res", argString(vm, args[0]), "valueId", argString(vm, args[1]))));
            });

            // Arguments: id, targetButtonId, baseInterval, [costs[]], [unlockConditions[]]
            addNativeFunction("createAutoClicker", [autoClickerSys, handles](VM* vm, int argCount, Value* args) -> Value {
                checkArgCount("createAutoClicker", argCount, 3, 5);

                AutoClicker clicker;

                clicker.id = argString(vm, args[0]);
                clicker.targetButtonId = argString(vm, args[1]);
                clicker.baseInterval = argElement(vm, args[2]).get<float>();

                if (argCount > 3)
                {
                    for (const auto& value : listValues(vm, args[3]))
                    {
                        clicker.costs.push_back(handles->cost(value));
                    }
                }

                if (argCount > 4)
                {
                    for (const auto& value : listValues(vm, args[4]))
                    {
                        clicker.unlockConditions.push_back(handles->checker(value));
                    }
                }

                autoClickerSys->availableAutoClickers.push_back(clicker);

                // Create the auto-clicker facts immediately
                autoClickerSys->createAutoClickerFacts(clicker);

                return makeBoolValue(true);
            });

            addNativeFunction("purchaseAutoClicker", [handles](VM* vm, int argCount, Value* args) -> Value {
                checkArgCount("purchaseAutoClicker", argCount, 1, 1);

                return handles->add(AchievementReward(StandardEvent("purchase_autoclicker", "id", argString(vm, args[0]))));
            });

            // Arguments: autoClickerId, [enable]
            addNativeFunction("toggleAutoClicker", [handles](VM* vm, int argCount, Value* args) -> Value {
                checkArgCount("toggleAutoClicker", argCount, 1, 2);

                const bool enable = argCount > 1 ? argElement(vm, args[1]).get<bool>() : true;

                return handles->add(AchievementReward(StandardEvent("toggle_autoclicker", "id", argString(vm, args[0]), "enable", ElementType(enable))));
            });

            // Prestige system functions
            addNativeFunction("prestige", [handles](VM* vm, int argCount, Value* args) -> Value {
                checkArgCount("prestige", argCount, 1, 1);

                return handles->add(AchievementReward(StandardEvent("perform_prestige", "level", ElementType(argElement(vm, args[0]).get<int>()))));
            });

            addNativeFunction("setTagTier", [sys](VM* vm, int argCount, Value* args) -> Value {
                checkArgCount("setTagTier", argCount, 2, 2);

                sys->setTagToTierMapping(argString(vm, args[0]), argElement(vm, args[1]).get<int>());

                return makeBoolValue(true);
            });

            addNativeFunction("getTagTier", [sys](VM* vm, int argCount, Value* args) -> Value {
                checkArgCount("getTagTier", argCount, 1, 1);

                return makeIntValue(sys->getTagTier(argString(vm, args[0])));
            });

            // Todo add basic generator / converter ids as system vars
        }
    };

    void NexusSystem::init()
    {
        // Register StandardEvent types we listen to
        addListenerToStandardEvent("perform_prestige");

        auto autoClickerSystem = ecsRef->getSystem<AutoClickerSystem>();

        // The requirements, outcomes and costs built by the script are shared between the modules
        auto handles = std::make_shared<ScriptHandles>();

        VM vm;

        ecsRef->setupVm(vm);

        vm.addNativeModule("nexus", NexusNativeModule{this, autoClickerSystem, handles});
        vm.addNativeModule("log", LogNativeModule{nullptr});
        vm.addNativeModule("achievement", AchievementNativeModule{handles});

        vm.interpretFromFile("res/main.pg");
    }
}
