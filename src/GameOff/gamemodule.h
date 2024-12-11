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

        //    Character chara;
            TTFText chara;
                                  
            auto list = serializeToInterpreter(this, chara);

            // Todo add the rest
            // auto list = makeList(this, {
            //     {"name", v.toString()},
            //     {"health", 100},
            //     {"ad", 10},
            //     {"ap", 10},
            //     {"speed", 100}
            //     });

            return list;
        }
    };

    class ReadCharacter : public Function
    {
        using Function::Function;
    public:
        void setUp()
        {
            setArity(1, 1);
        }

        void deserializeToHelper(UnserializedObject& holder, std::vector<ClassInstance::Field> fields, const std::string& className = "")
        {
            auto it = std::find(fields.begin(), fields.end(), "__className");

            if (it != fields.end())
            {
                UnserializedObject klass(className, it->value->getElement().toString(), std::string(""));
                fields.erase(it);

                deserializeToHelper(klass, fields);

                holder.children.push_back(klass);
            }
            else
            {
                for (const auto& field : fields)
                {
                    if (field.value->getType() == "Variable")
                    {
                        const auto& element = field.value->getElement();

                        std::string str;
                        
                        if (strcmp(ARCHIVEVERSION, "1.0.0") == 0)
                            str = ATTRIBUTECONST + " " + element.getTypeString() + " {" + element.toString() + "}";

                        UnserializedObject attribute(str, field.key, false);

                        holder.children.push_back(attribute);

                        LOG_INFO("Module", "Field " << field.key << " is a var of type: " << element.getTypeString());
                    }
                    else if (field.value->getType() == "ClassInstance")
                    {
                        auto nextClass = std::static_pointer_cast<ClassInstance>(field.value);
                        deserializeToHelper(holder, nextClass->getFields(), field.key);
                    }
                    // Todo
                    // else if (field.value->getType() == "Function")
                    else
                    {
                        LOG_ERROR("Game Module", "Field " << field.key << " type is not available for deserialization (" << field.value->getType() << ")");
                    }
                }
            }
        }

        template <typename Type>
        Type deserializeTo(std::shared_ptr<ClassInstance> list)
        {
            UnserializedObject obj;

            deserializeToHelper(obj, list->getFields());

            LOG_INFO("Module", "Nb child: " << obj.getNbChildren());
            LOG_INFO("Module", "Nb child: " << obj.children[0].getNbChildren());

            if (obj.getNbChildren() > 0)
                return deserialize<Type>(obj.children[0]);
            else
            {
                LOG_ERROR("Module", "Error happend when trying to deserialize the list no children found !");
                return Type {};
            }
        }

        virtual ValuablePtr call(ValuableQueue& args) override
        {
            auto arg = args.front();
            args.pop();

            if (arg->getType() == "ClassInstance")
            {
                // Todo create an helper funciton for this cast
                auto charaObject = std::static_pointer_cast<ClassInstance>(arg);

                auto text = deserializeTo<TTFText>(charaObject);

                LOG_INFO("Module", "Deserialized TTFText: " << text.text);
            }

            return nullptr;
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

                    if (type == "ClassInstance")
                    {
                        auto encounterObject = std::static_pointer_cast<ClassInstance>(field.value);

                        location.possibleEnounters.push_back(getEncounter(encounterObject));
                    }
                }

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

            auto list = makeList(this, {
                {"name", v.toString()},
                {"baseDmg", 1},
                {"manaCost", 0},
                {"cooldown", 1},
                {"physicalMultipler", 0.0f},
                {"magicalMultipler", 0.0f},
                {"selfOnly", false},
                {"multiTarget", true},
                {"nbTargets", 1},
                });

            // ApplyOn and RemoveFrom are functions

            return list;
        }
    };

    struct GameModule : public SysModule
    {
        GameModule(EntitySystem *ecsRef)
        {
            addSystemFunction<CreateCharacter>("createChara");
            addSystemFunction<CreateLocation>("createLocation", ecsRef);
            addSystemFunction<CreateItem>("createItem");
            addSystemFunction<CreatePassive>("createPassive");


            addSystemFunction<ReadCharacter>("readChara");
        }
    };
}