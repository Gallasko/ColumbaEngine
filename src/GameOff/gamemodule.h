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

    struct SerializedInfoHolder
    {
        SerializedInfoHolder() {}
        SerializedInfoHolder(const std::string& className) : className(className) {}
        SerializedInfoHolder(const std::string& name, const std::string& type, const std::string& value) : name(name), type(type), value(value) {}
        SerializedInfoHolder(const SerializedInfoHolder& other) = delete;
        SerializedInfoHolder(SerializedInfoHolder&& other) : className(std::move(other.className)), name(std::move(other.name)), type(std::move(other.type)), value(std::move(other.value)), parent(std::move(other.parent)), children(std::move(other.children)) {}

        std::string className;
        std::string name;
        std::string type;
        std::string value;

        SerializedInfoHolder* parent;
        std::vector<SerializedInfoHolder> children;
    };

    struct InspectorArchive : public Archive
    {
        /** Start the serialization process of a class */
        virtual void startSerialization(const std::string& className) override
        {
            auto& node = currentNode->children.emplace_back(className);

            node.name = lastAttributeName;
            lastAttributeName = "";

            node.parent = currentNode;

            currentNode = &node;
        }

        /** Start the serialization process of a class */
        virtual void endSerialization() override
        {
            currentNode = currentNode->parent;
        }

        /** Put an Attribute in the serialization process*/
        virtual void setAttribute(const std::string& value, const std::string& type = "") override
        {
            auto& attributeNode = currentNode->children.emplace_back(lastAttributeName, type, value);

            attributeNode.parent = currentNode;

            lastAttributeName = "";
        }

        virtual void setValueName(const std::string& name) override
        {
            lastAttributeName = name;
        }

        std::string lastAttributeName = "";

        SerializedInfoHolder mainNode;

        SerializedInfoHolder* currentNode = &mainNode;
    };

    class CreateCharacter : public Function
    {
        using Function::Function;
    public:
        void setUp()
        {
            setArity(1, 1);
        }

        void addNewAttribute(const std::string& text, const std::string& type, std::string& value, std::shared_ptr<ClassInstance> currentList)
        {
            LOG_INFO("Game Module", "Adding new attribute: " << text);

            if (type == "int")
            {
                int v = 0;
                std::stringstream sstream(value);
                sstream >> v;

                addToList(currentList, this->token, {text, v});
            }
            else if (type == "bool")
            {
                bool v = false;

                if (value == "true")
                    v = true;
                
                addToList(currentList, this->token, {text, v});
            }
            // Todo this is casted to a size_t (Should not be !)
            else if (type == "unsigned int")
            {
                unsigned int v = 0;
                std::stringstream sstream(value);
                sstream >> v;

                addToList(currentList, this->token, {text, static_cast<size_t>(v)});
            }
            else if (type == "float")
            {
                float v = 0;
                std::stringstream sstream(value);
                sstream >> v;

                addToList(currentList, this->token, {text, v});
            }
            // Todo this is casted to a float (Should not be !)
            else if (type == "double")
            {
                double v = 0;
                std::stringstream sstream(value);
                sstream >> v;

                addToList(currentList, this->token, {text, static_cast<float>(v)});
            }
            else if (type == "size_t")
            {
                size_t v = 0;
                std::stringstream sstream(value);
                sstream >> v;

                addToList(currentList, this->token, {text, v});
            }
            else if (type == "string")
            {
                addToList(currentList, this->token, {text, value});
            }
            else
            {
                LOG_ERROR("Game module", "Unsupported type for interpreter serialization: " << type);
            }
        }

        void addNewText(const std::string& text)
        {

        }

        void printChildren(SerializedInfoHolder& parent, size_t indentLevel, std::shared_ptr<ClassInstance> currentList)
        {            
            // If no class name then we got an attribute
            if (parent.className == "" and indentLevel > 1)
            {
                addNewAttribute(parent.name, parent.type, parent.value, currentList);
            }
            // We got a class name then it is a class ! So no type nor value
            else
            {
                addToList(currentList, this->token, {"__className", parent.className});
            }

            if (parent.children.size() > 0)
            {
                auto childList = makeList(this, {});

                for (auto& child : parent.children)
                {
                    printChildren(child, indentLevel + 1, childList);
                }

                addToList(currentList, this->token, {"__children", childList});
            }

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

            InspectorArchive archive;

            serialize(archive, chara);

            auto list = makeList(this, {});

            printChildren(archive.mainNode, 0, list);

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
        }
    };
}