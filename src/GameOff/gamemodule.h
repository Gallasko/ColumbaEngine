#pragma once

#include "Interpreter/pginterpreter.h"

#include "character.h"
#include "item.h"
#include "locationscene.h"

namespace pg
{
    namespace
    {
        ElementType toElement(ValuablePtr value)
        {
            return value->getValue()->getElement();
        }
    }

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

            // Todo add the rest
            auto list = makeList(this, { {"name", v.toString()}, {"health", 100}, {"ad", 10}, {"speed", 100} } );

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

            // Todo add the rest
            auto list = makeList(this, { {"name", v.toString()}, {"stacksize", -1}, {"nbItems", 1}, {"type", "Key"} } );

            return list;
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
                auto charaObject = std::static_pointer_cast<ClassInstance>(arg);

                // auto sysMethods = sysInstance->getMethods();
                auto sysFields = charaObject->getFields();

                Location location;
                location.name = locationName.toString();

                // Todo toElement Helper

                for (auto& field : sysFields)
                {
                    auto type = field.value->getType();
                    LOG_INFO("Game Module", type);

                    if (type == "ClassInstance")
                    {
                        auto encounterObject = std::static_pointer_cast<ClassInstance>(field.value);

                        location.possibleEnounters.push_back(getEncounter(encounterObject));
                    }
                }

                // encounter.characters = { chara };
                // encounter.dropTable = { { XpStone {}, 1.0f, 2}, { SlimeBall {}, 0.5f } };

                ecsRef->getSystem<LocationSystem>()->locations.push_back(location);
            }

            return nullptr;
        }

        Encounter getEncounter(std::shared_ptr<ClassInstance> arg)
        {
            auto sysFields = arg->getFields();

            Encounter encounter;

            for (auto& field : sysFields)
            {
                if (field == "enemies")
                {
                    auto type = field.value->getType();
                    LOG_INFO("Game Module", type);

                    if (type == "ClassInstance")
                    {
                        auto charaObject = std::static_pointer_cast<ClassInstance>(field.value);

                        encounter.characters = getCharacters(charaObject);
                    }
                }
                else if (field == "drop")
                {
                    LOG_INFO("Game Module", "Drop list");

                    auto type = field.value->getType();
                    LOG_INFO("Game Module", type);

                    if (type == "ClassInstance")
                    {
                        auto dropTableObject = std::static_pointer_cast<ClassInstance>(field.value);

                        encounter.dropTable = getDropTables(dropTableObject);
                    }
                }
            }

            return encounter;
        }

        std::vector<Character> getCharacters(std::shared_ptr<ClassInstance> arg)
        {
            auto sysFields = arg->getFields();

            std::vector<Character> characters;

            for (auto& field : sysFields)
            {
                auto type = field.value->getType();
                LOG_INFO("Game Module", type);

                if (type == "ClassInstance")
                {
                    auto charaObject = std::static_pointer_cast<ClassInstance>(field.value);

                    characters.push_back(getCharacter(charaObject));
                }
            }

            return characters;
        }

        Character getCharacter(std::shared_ptr<ClassInstance> arg)
        {
            auto sysFields = arg->getFields();

            Character chara;

            chara.type = CharacterType::Enemy;

            for (auto& field : sysFields)
            {
                auto value = toElement(field.value);

                LOG_INFO("Game Module", "Chara value: " <<  value.toString());

                if (field == "name")
                {
                    chara.name = value.toString();
                }
                else if (field == "health")
                {
                    chara.stat.health = value.get<int>();
                }
                else if (field == "speed")
                {
                    chara.stat.speed = value.get<int>();
                }
            }

            return chara;
        }

        std::vector<DropChance> getDropTables(std::shared_ptr<ClassInstance> arg)
        {
            std::vector<DropChance> drops;

            auto sysFields = arg->getFields();

            for (auto& field : sysFields)
            {
                auto type = field.value->getType();
                LOG_INFO("Game Module", type);

                if (type == "ClassInstance")
                {
                    auto dropTableObject = std::static_pointer_cast<ClassInstance>(field.value);

                    drops.push_back(getDropTable(dropTableObject));
                }
            }

            return drops;
        }

        DropChance getDropTable(std::shared_ptr<ClassInstance> arg)
        {
            DropChance drop;

            auto sysFields = arg->getFields();

            for (auto& field : sysFields)
            {
                auto type = field.value->getType();
                LOG_INFO("Game Module", type);

                auto value = toElement(field.value);

                LOG_INFO("Game Module", "Drop value: " <<  value.toString());

                if (type == "ClassInstance")
                {
                    auto itemObject = std::static_pointer_cast<ClassInstance>(field.value);

                    drop.item = getItem(itemObject);
                }
                else if (field == "dropChance")
                {
                    drop.dropChance = value.get<float>();
                }
                else if (field == "quantity")
                {
                    drop.quantity = value.get<int>();
                }
            }

            return drop;
        }

        Item getItem(std::shared_ptr<ClassInstance> arg)
        {
            Item item;

            auto sysFields = arg->getFields();

            for (auto& field : sysFields)
            {
                auto value = toElement(field.value);

                LOG_INFO("Game Module", "Chara value: " << value.toString());

                if (field == "name")
                {
                    item.name = value.toString();
                }
                else if (field == "stacksize")
                {
                    item.stacksize = value.get<int>();
                }
                else if (field == "nbItems")
                {
                    item.nbItems = value.get<size_t>();
                }
                else if (field == "type")
                {
                    item.type = stringToItemType.at(value.toString());
                }
            }

            return item;
        }

        EntitySystem *ecsRef;
    };

    struct GameModule : public SysModule
    {
        GameModule(EntitySystem *ecsRef)
        {
            addSystemFunction<CreateCharacter>("createChara");
            addSystemFunction<CreateLocation>("createLocation", ecsRef);
            addSystemFunction<CreateItem>("createItem");
        }
    };
}