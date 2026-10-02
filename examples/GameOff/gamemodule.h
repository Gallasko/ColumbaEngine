#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Compiler/vm.h"
#include "Compiler/native_module.h"
#include "Compiler/tableserialization.h"

#include "character.h"
#include "item.h"
#include "locationscene.h"

#include "nexusscene.h"

namespace pg
{
    /**
     * Objects built by the script helpers (req, fact, cost, ...) stay on the C++ side.
     * A script only holds a handle on them and hands it back to the functions consuming them (quickButton, createAutoClicker, ...)
     */
    struct ScriptHandles
    {
        enum class Kind : uint8_t
        {
            Checker = 1,
            Reward  = 2,
            Cost    = 3
        };

        Value add(const FactChecker& checker)
        {
            checkers.push_back(checker);

            return makeHandle(Kind::Checker, checkers.size() - 1);
        }

        Value add(const AchievementReward& reward)
        {
            rewards.push_back(reward);

            return makeHandle(Kind::Reward, rewards.size() - 1);
        }

        Value add(const NexusButtonCost& cost)
        {
            costs.push_back(cost);

            return makeHandle(Kind::Cost, costs.size() - 1);
        }

        const FactChecker& checker(Value handle) const { return get(checkers, Kind::Checker, handle, "requirement"); }

        const AchievementReward& reward(Value handle) const { return get(rewards, Kind::Reward, handle, "outcome"); }

        const NexusButtonCost& cost(Value handle) const { return get(costs, Kind::Cost, handle, "cost"); }

        std::vector<FactChecker> checkers;

        std::vector<AchievementReward> rewards;

        std::vector<NexusButtonCost> costs;

    private:
        // The kind is kept in the high bits so that a handle given to the wrong function is caught
        static Value makeHandle(Kind kind, size_t index)
        {
            return makeIntValue((static_cast<int64_t>(kind) << 32) | static_cast<int64_t>(index));
        }

        template <typename Type>
        static const Type& get(const std::vector<Type>& store, Kind kind, Value handle, const std::string& what)
        {
            if (not IS_INT(handle) or (AS_INT(handle) >> 32) != static_cast<int64_t>(kind))
            {
                throw std::runtime_error("Expected a " + what + " created by the script helpers");
            }

            const auto index = static_cast<size_t>(AS_INT(handle) & 0xFFFFFFFF);

            if (index >= store.size())
            {
                throw std::runtime_error("Unknown " + what + " handle");
            }

            return store[index];
        }
    };

    inline void checkArgCount(const std::string& function, int argCount, int min, int max)
    {
        if (argCount < min or (max >= 0 and argCount > max))
        {
            throw std::runtime_error(function + " received " + std::to_string(argCount) + " arguments, expected between " + std::to_string(min) + " and " + (max < 0 ? "any number" : std::to_string(max)));
        }
    }

    // Facts and costs are stored as floats while the vm only has doubles
    inline ElementType argElement(VM* vm, Value value)
    {
        if (IS_DOUBLE(value))
            return ElementType(static_cast<float>(AS_DOUBLE(value)));

        return vm->valueToElement(value);
    }

    inline std::string argString(VM* vm, Value value)
    {
        return vm->valueToElement(value).toString();
    }

    // Elements of an array ([a, b]) or values of a table, in insertion order
    inline std::vector<Value> listValues(VM* vm, Value list)
    {
        if (IS_VECTOR(list))
            return vm->asVector(list)->fields;

        if (IS_INSTANCE(list))
            return vm->asInstance(list)->fieldValues;

        return {};
    }

    inline std::vector<std::string> listStrings(VM* vm, Value list)
    {
        std::vector<std::string> strings;

        for (const auto& value : listValues(vm, list))
        {
            strings.push_back(argString(vm, value));
        }

        return strings;
    }

    // Todo arrays nested in a table are not read back by tableToUnserializedObject, only nested tables are
    template <typename Type>
    Type tableTo(VM* vm, Value table, const std::string& typeName)
    {
        if (not IS_INSTANCE(table))
        {
            LOG_ERROR("Game Module", "Can only deserialize a table");

            return Type{};
        }

        auto objTable = vm->asInstance(table);

        std::string name = typeName;

        if (objTable->hasField("__className"))
            name = vm->asString(objTable->getField("__className"));

        return deserialize<Type>(tableToUnserializedObject(vm, objTable, name));
    }

    /**
     * Game Module
     * Lets a script create the characters, items, spells and locations of the game
     */
    class GameNativeModule : public NativeModule
    {
    public:
        GameNativeModule(EntitySystem *ecsRef)
        {
            addNativeFunction("newChara", [](VM* vm, int argCount, Value* args) -> Value {
                checkArgCount("newChara", argCount, 1, 1);

                Character chara;

                chara.type = CharacterType::Enemy;

                chara.name = argString(vm, args[0]);

                return serializeToTable(vm, chara);
            });

            addNativeFunction("newItem", [](VM* vm, int argCount, Value* args) -> Value {
                checkArgCount("newItem", argCount, 1, 1);

                Item item;

                item.name = argString(vm, args[0]);

                return serializeToTable(vm, item);
            });

            addNativeFunction("newSpell", [](VM* vm, int argCount, Value*) -> Value {
                checkArgCount("newSpell", argCount, 1, 1);

                Spell spell;

                return serializeToTable(vm, spell);
            });

            // ApplyOn and RemoveFrom are functions
            addNativeFunction("createPassive", [](VM* vm, int argCount, Value* args) -> Value {
                checkArgCount("createPassive", argCount, 1, 1);

                Value passiveValue = vm->createTable();

                auto passive = vm->asInstance(passiveValue);

                passive->setField("name", vm->createString(argString(vm, args[0])), vm, true);
                passive->setField("nbTurn", makeIntValue(-1), vm, true);
                passive->setField("hidden", makeBoolValue(false), vm, true);
                passive->setField("nbNeededTrigger", makeIntValue(0), vm, true);
                passive->setField("trigger", vm->createString("TurnStart"), vm, true);
                passive->setField("type", vm->createString("SpellEffect"), vm, true);

                return passiveValue;
            });

            addNativeFunction("readCharaList", [](VM* vm, int argCount, Value* args) -> Value {
                checkArgCount("readCharaList", argCount, 1, 1);

                auto charaList = tableTo<Encounter>(vm, args[0], "Encounter");

                for (const auto& chara : charaList.characters)
                {
                    LOG_INFO("Game Module", "Chara in list: " << chara.name);
                }

                return makeBoolValue(true);
            });

            addNativeFunction("createLocation", [ecsRef](VM* vm, int argCount, Value* args) -> Value {
                checkArgCount("createLocation", argCount, 2, 2);

                Location location;

                location.name = argString(vm, args[0]);

                for (const auto& value : listValues(vm, args[1]))
                {
                    auto encounter = tableTo<Encounter>(vm, value, "Encounter");

                    location.possibleEnounters.push_back(encounter);

                    LOG_INFO("Game Module", "nbChara: " << encounter.characters.size());
                }

                ecsRef->getSystem<LocationSystem>()->locations.push_back(location);

                return makeBoolValue(true);
            });
        }
    };

    // Build an event from: eventName, key, value, [key, value]...
    inline StandardEvent eventFromArgs(VM* vm, const std::string& function, int argCount, Value* args)
    {
        checkArgCount(function, argCount, 3, -1);

        if (argCount % 2 == 0)
        {
            throw std::runtime_error(function + " expects a value after each key");
        }

        StandardEvent event(argString(vm, args[0]), argString(vm, args[1]), argElement(vm, args[2]));

        for (int i = 3; i < argCount; i += 2)
        {
            event.values[argString(vm, args[i])] = argElement(vm, args[i + 1]);
        }

        return event;
    }

    /**
     * Achievement Module
     * Lets a script build the fact checkers and the rewards used by the achievements and the nexus buttons
     */
    class AchievementNativeModule : public NativeModule
    {
    public:
        AchievementNativeModule(std::shared_ptr<ScriptHandles> handles)
        {
            // Arguments: name (string), value, equality (string)
            addNativeFunction("FactChecker", [handles](VM* vm, int argCount, Value* args) -> Value {
                checkArgCount("FactChecker", argCount, 3, 3);

                const auto it = stringToEquality.find(argString(vm, args[2]));

                const auto equality = it != stringToEquality.end() ? it->second : FactCheckEquality::None;

                return handles->add(FactChecker(argString(vm, args[0]), argElement(vm, args[1]), equality));
            });

            addNativeFunction("AchievementRewardEvent", [handles](VM* vm, int argCount, Value* args) -> Value {
                return handles->add(AchievementReward(eventFromArgs(vm, "AchievementRewardEvent", argCount, args)));
            });

            // Todo maybe rename it AchievementAddFact
            addNativeFunction("AddFact", [handles](VM* vm, int argCount, Value* args) -> Value {
                checkArgCount("AddFact", argCount, 2, 2);

                return handles->add(AchievementReward(AddFact{argString(vm, args[0]), argElement(vm, args[1])}));
            });

            addNativeFunction("RemoveFact", [handles](VM* vm, int argCount, Value* args) -> Value {
                checkArgCount("RemoveFact", argCount, 1, 1);

                return handles->add(AchievementReward(RemoveFact{argString(vm, args[0])}));
            });

            addNativeFunction("IncreaseFact", [handles](VM* vm, int argCount, Value* args) -> Value {
                checkArgCount("IncreaseFact", argCount, 2, 2);

                return handles->add(AchievementReward(IncreaseFact{argString(vm, args[0]), argElement(vm, args[1])}));
            });
        }
    };
}
