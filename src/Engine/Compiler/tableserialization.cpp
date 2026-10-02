#include "stdafx.h"

#include "tableserialization.h"

#include <cstdio>
#include <cstdlib>

namespace pg
{
    namespace
    {
        // An empty value is only meaningful for a string, everything else serializes to some text
        bool hasValue(const SerializedInfoHolder& node)
        {
            return not node.value.empty() or node.type == "string";
        }

        Value valueFromAttribute(VM* vm, const std::string& type, const std::string& value)
        {
            if (type == "int" or type == "size_t" or type == "unsigned int")
                return makeIntValue(static_cast<int64_t>(std::strtoll(value.c_str(), nullptr, 10)));

            if (type == "bool")
                return makeBoolValue(value == "true");

            if (type == "float" or type == "double")
                return makeDoubleValue(std::strtod(value.c_str(), nullptr));

            // Strings, and any type the VM has no native value for, are kept as text
            return vm->createString(value);
        }

        bool isElementType(const SerializedInfoHolder& node)
        {
            if (node.className != "ElementType")
                return false;

            bool hasType = false;
            bool hasData = false;

            for (const auto& child : node.children)
            {
                if (child.name == "type")
                    hasType = true;

                if (child.name == "data")
                    hasData = true;
            }

            return hasType and hasData;
        }

        Value valueFromElementType(VM* vm, const SerializedInfoHolder& node)
        {
            for (const auto& child : node.children)
            {
                if (child.name == "data" and hasValue(child))
                    return valueFromAttribute(vm, child.type, child.value);
            }

            return makeIntValue(0);
        }

        void addNodeToTable(VM* vm, Klass* tableClass, const SerializedInfoHolder& node, ObjInstance* table);

        // Value of a node that its parent stores itself: a vector element or a map value
        Value valueFromNode(VM* vm, Klass* tableClass, const SerializedInfoHolder& node)
        {
            if (isElementType(node))
                return valueFromElementType(vm, node);

            if (hasValue(node))
                return valueFromAttribute(vm, node.type, node.value);

            if (node.children.empty())
                return makeIntValue(0);

            Value tableValue = vm->createInstance(tableClass);

            addNodeToTable(vm, tableClass, node, vm->asInstance(tableValue));

            return tableValue;
        }

        // Elements are stored under their index: "0", "1", ...
        void addVectorElements(VM* vm, Klass* tableClass, const SerializedInfoHolder& node, ObjInstance* table)
        {
            for (size_t i = 0; i < node.children.size(); ++i)
            {
                table->setField(std::to_string(i), valueFromNode(vm, tableClass, node.children[i]), vm, true);
            }
        }

        // A map is serialized as nbElements followed by key0, value0, key1, value1, ... Each value is stored under its key
        void addMapElements(VM* vm, Klass* tableClass, const SerializedInfoHolder& node, ObjInstance* table)
        {
            std::unordered_map<std::string, const SerializedInfoHolder*> childByName;

            for (const auto& child : node.children)
            {
                childByName[child.name] = &child;
            }

            auto countIt = childByName.find("nbElements");

            if (countIt == childByName.end())
                return;

            const size_t nbElements = std::strtoull(countIt->second->value.c_str(), nullptr, 10);

            for (size_t i = 0; i < nbElements; ++i)
            {
                auto keyIt = childByName.find("key" + std::to_string(i));
                auto valueIt = childByName.find("value" + std::to_string(i));

                if (keyIt == childByName.end() or keyIt->second->value.empty())
                    continue;

                Value value = makeIntValue(0);

                if (valueIt != childByName.end())
                    value = valueFromNode(vm, tableClass, *valueIt->second);

                table->setField(keyIt->second->value, value, vm, true);
            }
        }

        // Add a serialized node, and everything under it, to a table.
        // Every value written here is freshly created: the table takes over its reference
        void addNodeToTable(VM* vm, Klass* tableClass, const SerializedInfoHolder& node, ObjInstance* table)
        {
            if (isElementType(node) and not node.name.empty())
            {
                table->setField(node.name, valueFromElementType(vm, node), vm, true);

                return;
            }

            // Leaf attribute
            if (hasValue(node) and not node.name.empty())
                table->setField(node.name, valueFromAttribute(vm, node.type, node.value), vm, true);

            if (node.children.empty())
                return;

            // An anonymous node adds nothing of its own: its children belong to the current table
            if (node.name.empty())
            {
                for (const auto& child : node.children)
                {
                    addNodeToTable(vm, tableClass, child, table);
                }

                return;
            }

            Value nestedValue = vm->createInstance(tableClass);

            auto nested = vm->asInstance(nestedValue);

            if (node.className == "Vector")
            {
                addVectorElements(vm, tableClass, node, nested);
            }
            else if (node.className == "UnorderedMap")
            {
                addMapElements(vm, tableClass, node, nested);
            }
            else
            {
                for (const auto& child : node.children)
                {
                    addNodeToTable(vm, tableClass, child, nested);
                }
            }

            table->setField(node.name, nestedValue, vm, true);
        }

        void addTableFields(VM* vm, ObjInstance* table, UnserializedObject& object)
        {
            for (const auto& [key, index] : table->internedFields)
            {
                if (key == "__className")
                    continue;

                auto value = table->fieldValues[index];

                if (IS_INSTANCE(value))
                {
                    UnserializedObject child(key, "", "");

                    addTableFields(vm, vm->asInstance(value), child);

                    object.children.push_back(std::move(child));

                    continue;
                }

                std::string type;
                std::string text;

                if (IS_INT(value))
                {
                    type = "int";
                    text = std::to_string(AS_INT(value));
                }
                else if (IS_BOOL(value))
                {
                    type = "bool";
                    text = AS_BOOL(value) ? "true" : "false";
                }
                else if (IS_DOUBLE(value))
                {
                    // Enough digits to read the same double back, std::to_string stops at 6 decimals
                    char buffer[32];

                    std::snprintf(buffer, sizeof(buffer), "%.17g", AS_DOUBLE(value));

                    type = "float";
                    text = buffer;
                }
                else if (IS_STRING(value))
                {
                    type = "string";
                    text = vm->asString(value);
                }
                else
                {
                    // Functions and other script only values have no serialized form
                    continue;
                }

                // An attribute is written as: __PGSA type {value}
                UnserializedObject attribute("__PGSA " + type + " {" + text + "}", key, false);

                object.children.push_back(std::move(attribute));
            }
        }
    }

    Value archiveToTable(VM* vm, const InspectorArchive& archive)
    {
        auto tableClass = vm->findGlobalClass("__Table");

        if (tableClass == nullptr)
            throw std::runtime_error("Table class not found in VM globals");

        Value tableValue = vm->createInstance(tableClass);

        if (archive.mainNode.children.empty())
            return tableValue;

        auto table = vm->asInstance(tableValue);

        const auto& node = archive.mainNode.children[0];

        if (not node.className.empty())
            table->setField("__className", vm->createString(node.className), vm, true);

        addNodeToTable(vm, tableClass, node, table);

        return tableValue;
    }

    UnserializedObject tableToUnserializedObject(VM* vm, ObjInstance* table, const std::string& typeName)
    {
        // The root only needs a header that the object parser accepts: "TypeName: TypeName {"
        std::string header = typeName + ": " + typeName + " {";

        UnserializedObject object(typeName, typeName, header);

        addTableFields(vm, table, object);

        return object;
    }
}
