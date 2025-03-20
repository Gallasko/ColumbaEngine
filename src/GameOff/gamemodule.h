#pragma once

#include "Interpreter/pginterpreter.h"

#include "character.h"
#include "item.h"
#include "locationscene.h"

#include "nexusscene.h"

namespace pg
{
    class CreateCharacter : public Function
    {
        using Function::Function;
    public:
        void setUp()
        {
            setArity(1, 1);
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            auto v = args.front()->getElement();
            args.pop();

            if (not v.isLitteral())
            {
                LOG_ERROR("Character module", "Character name should be a literal");

                return nullptr;
            }

            Character chara;

            chara.type = CharacterType::Enemy;

            chara.name = v.toString();

            auto list = serializeToInterpreter(this, chara);

            return list;
        }
    };

    class CreateItem : public Function
    {
        using Function::Function;
    public:
        void setUp()
        {
            setArity(1, 1);
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            auto v = args.front()->getElement();
            args.pop();

            if (not v.isLitteral())
            {
                LOG_ERROR("Character module", "Character name should be a literal");

                return nullptr;
            }

            Item item;

            item.name = v.toString();

            auto list = serializeToInterpreter(this, item);

            return list;
        }
    };

    class ReadCharaList : public Function
    {
        using Function::Function;
    public:
        void setUp()
        {
            setArity(1, 1);
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            auto arg = args.front();
            args.pop();

            auto charaList = deserializeTo<Encounter>(arg);

            for (const auto& chara : charaList.characters)
            {
                LOG_INFO("Module", "Chara in list: " << chara.name);
            }

            return nullptr;
        }
    };

    class CreateLocation : public Function
    {
        using Function::Function;
    public:
        void setUp(EntitySystem *ecsRef)
        {
            this->ecsRef = ecsRef;

            setArity(2, 2);
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            auto locationName = args.front()->getElement();
            args.pop();

            auto arg = args.front();
            args.pop();

            if (arg->getType() == "ClassInstance")
            {
                // Todo create an helper funciton for this cast
                auto charaObject = std::static_pointer_cast<ClassInstance>(arg);

                // auto sysMethods = sysInstance->getMethods();
                auto sysFields = charaObject->getFields();

                Location location;
                location.name = locationName.toString();

                for (auto& field : sysFields)
                {
                    auto type = field.value->getType();
                    LOG_INFO("Game Module", type);

                    auto encounter = deserializeTo<Encounter>(field.value);

                    location.possibleEnounters.push_back(encounter);

                    LOG_INFO("Game Module", "nbChara: " <<encounter.characters.size());
                }

                ecsRef->getSystem<LocationSystem>()->locations.push_back(location);
            }

            return nullptr;
        }

        EntitySystem *ecsRef;
    };

    class CreatePassive : public Function
    {
        using Function::Function;
    public:
        void setUp()
        {
            setArity(1, 1);
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            auto v = args.front()->getElement();
            args.pop();

            if (not v.isLitteral())
            {
                LOG_ERROR("Character module", "Character name should be a literal");

                return nullptr;
            }

            auto list = makeList(this, {
                {"name", v.toString()},
                {"nbTurn", -1},
                {"hidden", false},
                {"nbNeededTrigger", 0},
                {"trigger", "TurnStart"},
                {"type", "SpellEffect"}
                });

            // ApplyOn and RemoveFrom are functions

            return list;
        }
    };

    class CreateSpell : public Function
    {
        using Function::Function;
    public:
        void setUp()
        {
            setArity(1, 1);
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            auto v = args.front()->getElement();
            args.pop();

            if (not v.isLitteral())
            {
                LOG_ERROR("Character module", "Character name should be a literal");

                return nullptr;
            }

            Spell spell;

            auto list = serializeToInterpreter(this, spell);

            return list;
        }
    };

    struct GameModule : public SysModule
    {
        GameModule(EntitySystem *ecsRef)
        {
            addSystemFunction<CreateCharacter>("newChara");
            addSystemFunction<CreateLocation>("createLocation", ecsRef);
            addSystemFunction<CreateItem>("newItem");
            addSystemFunction<CreateSpell>("newSpell");
            addSystemFunction<CreatePassive>("createPassive");

            addSystemFunction<ReadCharaList>("readCharaList");
        }
    };

    class CreateFactChecker : public Function
    {
        using Function::Function;
    public:
        void setUp() { setArity(3, 3); }

        FactChecker createFactChecker(const std::string& name, const ElementType& value, const std::string& equalityStr)
        {
            auto equalityIt = stringToEquality.find(equalityStr);
            FactCheckEquality equality = (equalityIt != stringToEquality.end()) ? equalityIt->second : FactCheckEquality::None;
            return FactChecker(name, value, equality);
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            // Todo check type of elements gotten here
            // Assume arguments: name (string), value (ElementType), equality (string)
            std::string name = args.front()->getElement().toString();
            args.pop();
            ElementType value = args.front()->getElement();
            args.pop();
            std::string equalityStr = args.front()->getElement().toString();
            args.pop();

            FactChecker fc = createFactChecker(name, value, equalityStr);
            return serializeToInterpreter(this, fc);
        }
    };

    class CreateAchievementRewardEvent : public Function
    {
        using Function::Function;
    public:
        void setUp() { setArity(3, 3); }

        // Helper to create an AchievementReward from a StandardEvent
        AchievementReward createAchievementRewardEvent(const std::string& eventName, const std::string& key, const ElementType& message)
        {
            StandardEvent ev(eventName, key, message);
            return AchievementReward(ev);
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            // Todo check type of elements gotten here
            // Assume arguments: eventName (string), key (string), message (string)
            std::string eventName = args.front()->getElement().toString();
            args.pop();
            std::string key = args.front()->getElement().toString();
            args.pop();

            const auto& message = args.front()->getElement();
            AchievementReward reward = createAchievementRewardEvent(eventName, key, message);
            args.pop();

            return serializeToInterpreter(this, reward);
        }
    };

    struct AchievementModule : public SysModule
    {
        AchievementModule()
        {
            addSystemFunction<CreateFactChecker>("FactChecker");
            addSystemFunction<CreateAchievementRewardEvent>("AchievementRewardEvent");
            // add additional functions as needed
        }
    };
}