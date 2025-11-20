#pragma once

/**
 * @file ecsserialization.h
 * @brief Helper functions for serializing/deserializing ECS entities and components to/from tables (VM/Compiler)
 * @version 1.0
 * @date 2025-11-07
 *
 * This provides helper functions to convert ECS entities and components into VM table structures
 * (ObjInstance with fields) for use in the PgCompiler VM system.
 */

#include "ECS/entitysystem.h"
#include "object.h"
#include "vm.h"
#include "serialization.h"

namespace pg
{
    /**
     * @brief Serialize a single component to a VM table (ObjInstance)
     *
     * Converts a component into an ObjInstance with fields representing the component's properties.
     * Uses the existing Archive-based serialization system to extract component data.
     *
     * Example result (for a Transform component):
     * {
     *   "__className": "Transform",
     *   "x": 100,
     *   "y": 200,
     *   "rotation": 0.0
     * }
     *
     * @param vm Pointer to the VM (needed for creating strings and values)
     * @param ecsRef Pointer to the entity system
     * @param entity Pointer to the entity owning the component
     * @param componentId The unique ID of the component type
     * @return Value A VM Value containing the table (ObjInstance) representing the component
     */
    inline Value serializeComponentToTable(VM* vm,
                                          EntitySystem* ecsRef,
                                          const Entity* entity,
                                          _unique_id componentId)
    {
        // Get the Table class
        auto it = vm->globals.find("__Table");
        if (it == vm->globals.end())
        {
            throw std::runtime_error("Table class not found in VM globals");
        }

        Klass* tableClass = vm->asClass(it->second);

        // Create an Archive and serialize the component
        InspectorArchive archive;
        ecsRef->getComponentRegistry()->serializeComponentFromEntity(archive, entity, componentId);

        // Create the table instance
        Value tableValue = vm->createInstance(tableClass);
        ObjInstance* table = vm->asInstance(tableValue);

        // Parse the archive and populate the table
        if (archive.mainNode.children.size() > 0)
        {
            auto& compNode = archive.mainNode.children[0];

            // Add the class name (component type)
            if (!compNode.className.empty())
            {
                Value classNameValue = vm->createString(compNode.className);

                if (table->fields.find("__className") != table->fields.end())
                {
                    vm->releaseAndDelete(table->fields["__className"]);
                }

                table->fields["__className"] = classNameValue;
            }

            // Helper to check if a node is an ElementType (has "type" and "data" children)
            auto isElementType = [](const SerializedInfoHolder& node) -> bool {
                if (node.className != "ElementType") return false;
                bool hasType = false;
                bool hasData = false;
                for (const auto& child : node.children)
                {
                    if (child.name == "type") hasType = true;
                    if (child.name == "data") hasData = true;
                }
                return hasType && hasData;
            };

            // Helper to extract the data value from an ElementType node
            auto extractElementTypeValue = [&](const SerializedInfoHolder& node) -> Value {
                for (const auto& child : node.children)
                {
                    if (child.name == "data" && !child.value.empty())
                    {
                        if (child.type == "int")
                            return makeIntValue(std::stoi(child.value));
                        else if (child.type == "bool")
                            return makeBoolValue(child.value == "true");
                        else if (child.type == "float" || child.type == "double")
                            return makeDoubleValue(std::stod(child.value));
                        else if (child.type == "size_t" || child.type == "unsigned int")
                            return makeIntValue(std::stoull(child.value));
                        else if (child.type == "string")
                            return vm->createString(child.value);
                        else
                            return vm->createString(child.value);
                    }
                }
                return makeIntValue(0); // Default
            };

            // Recursively add all children (component properties)
            std::function<void(const SerializedInfoHolder&, ObjInstance*)> processNode;
            processNode = [&](const SerializedInfoHolder& node, ObjInstance* currentTable) {
                // Check if this is an ElementType - flatten it to just the data value
                if (isElementType(node) && !node.name.empty())
                {
                    Value value = extractElementTypeValue(node);
                    currentTable->fields[node.name] = value;
                    return;
                }

                // If this node has a value (it's a leaf property), add it
                if (!node.value.empty() && !node.name.empty())
                {
                    Value value;

                    // Convert the string value to appropriate VM type
                    if (node.type == "int")
                    {
                        value = makeIntValue(std::stoi(node.value));
                    }
                    else if (node.type == "bool")
                    {
                        value = makeBoolValue(node.value == "true");
                    }
                    else if (node.type == "float" or node.type == "double")
                    {
                        value = makeDoubleValue(std::stod(node.value));
                    }
                    else if (node.type == "size_t" or node.type == "unsigned int")
                    {
                        value = makeIntValue(std::stoull(node.value));
                    }
                    else if (node.type == "string")
                    {
                        value = vm->createString(node.value);
                    }
                    else
                    {
                        // Default to string representation
                        value = vm->createString(node.value);
                    }

                    currentTable->fields[node.name] = value;
                }

                // If this node has children, process them
                if (node.children.size() > 0)
                {
                    // Check if this is a Vector (array-like structure)
                    if (node.className == "Vector" && !node.name.empty())
                    {
                        Value nestedTableValue = vm->createInstance(tableClass);
                        ObjInstance* nestedTable = vm->asInstance(nestedTableValue);

                        // Add elements with numeric indices [0], [1], etc.
                        for (size_t i = 0; i < node.children.size(); i++)
                        {
                            const auto& child = node.children[i];

                            // Create a temporary table for this element
                            Value elementValue;
                            if (!child.value.empty())
                            {
                                // Leaf element - convert directly
                                if (child.type == "int")
                                {
                                    elementValue = makeIntValue(std::stoi(child.value));
                                }
                                else if (child.type == "bool")
                                {
                                    elementValue = makeBoolValue(child.value == "true");
                                }
                                else if (child.type == "float" || child.type == "double")
                                {
                                    elementValue = makeDoubleValue(std::stod(child.value));
                                }
                                else if (child.type == "size_t" || child.type == "unsigned int")
                                {
                                    elementValue = makeIntValue(std::stoull(child.value));
                                }
                                else if (child.type == "string")
                                {
                                    elementValue = vm->createString(child.value);
                                }
                                else
                                {
                                    elementValue = vm->createString(child.value);
                                }
                            }
                            else if (child.children.size() > 0)
                            {
                                // Complex element - create nested table
                                Value childTableValue = vm->createInstance(tableClass);
                                ObjInstance* childTable = vm->asInstance(childTableValue);
                                processNode(child, childTable);
                                elementValue = childTableValue;
                            }

                            nestedTable->fields[std::to_string(i)] = elementValue;
                        }

                        if (currentTable->fields.find(node.name) != currentTable->fields.end())
                        {
                            vm->releaseAndDelete(currentTable->fields[node.name]);
                        }

                        currentTable->fields[node.name] = nestedTableValue;
                    }
                    // Check if this is an UnorderedMap
                    else if (node.className == "UnorderedMap" && !node.name.empty())
                    {
                        Value nestedTableValue = vm->createInstance(tableClass);
                        ObjInstance* nestedTable = vm->asInstance(nestedTableValue);

                        // Find nbElements to determine how many key-value pairs
                        size_t nbElements = 0;
                        for (const auto& child : node.children)
                        {
                            if (child.name == "nbElements" && !child.value.empty())
                            {
                                nbElements = std::stoull(child.value);
                                break;
                            }
                        }

                        // Extract key-value pairs and use actual keys as indices
                        for (size_t i = 0; i < nbElements; i++)
                        {
                            std::string keyName = "key" + std::to_string(i);
                            std::string valueName = "value" + std::to_string(i);

                            std::string actualKey;
                            Value actualValue;

                            // Find the key and value in children
                            for (const auto& child : node.children)
                            {
                                if (child.name == keyName && !child.value.empty())
                                {
                                    actualKey = child.value;
                                }
                                else if (child.name == valueName)
                                {
                                    // Check if value is an ElementType - flatten it
                                    if (isElementType(child))
                                    {
                                        actualValue = extractElementTypeValue(child);
                                    }
                                    else if (!child.value.empty())
                                    {
                                        // Leaf value
                                        if (child.type == "int")
                                        {
                                            actualValue = makeIntValue(std::stoi(child.value));
                                        }
                                        else if (child.type == "bool")
                                        {
                                            actualValue = makeBoolValue(child.value == "true");
                                        }
                                        else if (child.type == "float" || child.type == "double")
                                        {
                                            actualValue = makeDoubleValue(std::stod(child.value));
                                        }
                                        else if (child.type == "size_t" || child.type == "unsigned int")
                                        {
                                            actualValue = makeIntValue(std::stoull(child.value));
                                        }
                                        else if (child.type == "string")
                                        {
                                            actualValue = vm->createString(child.value);
                                        }
                                        else
                                        {
                                            actualValue = vm->createString(child.value);
                                        }
                                    }
                                    else if (child.children.size() > 0)
                                    {
                                        // Complex value - create nested table
                                        Value childTableValue = vm->createInstance(tableClass);
                                        ObjInstance* childTable = vm->asInstance(childTableValue);
                                        processNode(child, childTable);
                                        actualValue = childTableValue;
                                    }
                                }
                            }

                            if (!actualKey.empty())
                            {
                                nestedTable->fields[actualKey] = actualValue;
                            }
                        }

                        if (currentTable->fields.find(node.name) != currentTable->fields.end())
                        {
                            vm->releaseAndDelete(currentTable->fields[node.name]);
                        }

                        currentTable->fields[node.name] = nestedTableValue;
                    }
                    // If the node has a name, create a nested table for the children
                    // Otherwise, add children directly to the current table
                    else if (!node.name.empty())
                    {
                        Value nestedTableValue = vm->createInstance(tableClass);
                        ObjInstance* nestedTable = vm->asInstance(nestedTableValue);

                        for (const auto& child : node.children)
                        {
                            processNode(child, nestedTable);
                        }

                        if (currentTable->fields.find(node.name) != currentTable->fields.end())
                        {
                            vm->releaseAndDelete(currentTable->fields[node.name]);
                        }

                        currentTable->fields[node.name] = nestedTableValue;
                    }
                    else
                    {
                        // Node has no name, so add children directly to current table
                        for (const auto& child : node.children)
                        {
                            processNode(child, currentTable);
                        }
                    }
                }
            };

            processNode(compNode, table);
        }

        return tableValue;
    }

    /**
     * @brief Serialize all components of an entity to a VM table
     *
     * Converts an entity and all its components into a table where:
     * - "__entityId" contains the entity ID
     * - Each component type name is a key with a nested table of properties
     *
     * Example result:
     * {
     *   "__entityId": 42,
     *   "Transform": { "__className": "Transform", "x": 100, "y": 200 },
     *   "Velocity": { "__className": "Velocity", "dx": 5.0, "dy": -3.0 }
     * }
     *
     * @param vm Pointer to the VM
     * @param ecsRef Pointer to the entity system
     * @param entity Pointer to the entity to serialize
     * @return Value A VM Value containing the entity table
     */
    inline Value serializeEntityToTable(VM* vm, EntitySystem* ecsRef, Entity* entity)
    {
        // Get the Table class
        auto it = vm->globals.find("__Table");
        if (it == vm->globals.end())
        {
            throw std::runtime_error("Table class not found in VM globals");
        }

        Klass* tableClass = vm->asClass(it->second);

        // Create the entity table
        Value entityTableValue = vm->createInstance(tableClass);
        ObjInstance* entityTable = vm->asInstance(entityTableValue);

        LOG_INFO("ECS Serialization", "Serializing entity ID " << entity->id);

        // Add the entity ID
        Value idValue = makeIntValue(static_cast<int64_t>(entity->id));
        entityTable->fields["__entityId"] = idValue;

        // Serialize each component
        for (const auto& compRef : entity->componentList)
        {
            if (compRef.entityHeldType == Entity::EntityHeld::EntityHeldType::id)
            {
                _unique_id componentId = compRef.getId();

                // Serialize the component
                Value componentTableValue = serializeComponentToTable(vm, ecsRef, entity, componentId);

                // Get the component type name from the serialized table's __className field
                ObjInstance* compTable = vm->asInstance(componentTableValue);
                std::string componentTypeName = "Component";

                auto classNameIt = compTable->fields.find("__className");
                if (classNameIt != compTable->fields.end() && IS_STRING(classNameIt->second))
                {
                    componentTypeName = vm->asString(classNameIt->second)->toString();
                }

                // Add to entity table
                entityTable->fields[componentTypeName] = componentTableValue;
            }
        }

        return entityTableValue;
    }

    /**
     * @brief Deserialize a component from a VM table and attach it to an entity
     *
     * Takes a VM table (ObjInstance) representing a component and attaches it to the specified entity.
     * The table must have a "__className" field to identify the component type.
     *
     * @param vm Pointer to the VM
     * @param ecsRef Pointer to the entity system
     * @param entity Reference to the entity to attach the component to
     * @param componentTable VM Value containing the component table
     * @param componentTypeName Optional explicit component type name (if not in table)
     * @return bool True if successful, false if component type not found
     */
    inline bool deserializeComponentFromTable(VM* vm,
                                             EntitySystem* ecsRef,
                                             EntityRef entity,
                                             Value componentTable,
                                             const std::string& componentTypeName = "")
    {
        if (!IS_INSTANCE(componentTable))
        {
            LOG_ERROR("ECS Serialization", "componentTable is not an instance");
            return false;
        }

        ObjInstance* table = vm->asInstance(componentTable);

        LOG_INFO("ECS Serialization", "=== deserializeComponentFromTable START ===");
        LOG_INFO("ECS Serialization", "Table has " << table->fields.size() << " fields");

        // Determine the component type name
        std::string typeName = componentTypeName;

        if (typeName.empty())
        {
            // Try to find the class name in the table
            auto it = table->fields.find("__className");
            if (it != table->fields.end())
            {
                if (IS_STRING(it->second))
                {
                    typeName = vm->asString(it->second)->toString();
                }
            }

            if (typeName.empty())
            {
                LOG_ERROR("ECS Serialization", "No component type name provided and no __className found in table");
                return false;
            }
        }

        LOG_INFO("ECS Serialization", "Component type name: " << typeName);

        // Convert the table to UnserializedObject
        // Create a dummy serialized string that parseString() can parse correctly
        // Format: "TypeName: TypeName {"
        std::string dummySerializedString = typeName + ": " + typeName + " {";
        UnserializedObject obj(typeName, typeName, dummySerializedString);
        LOG_INFO("ECS Serialization", "Created UnserializedObject - objectName=" << obj.getObjectName() << ", objectType=" << obj.getObjectType());

        // Helper function to convert table fields to UnserializedObject
        std::function<void(ObjInstance*, UnserializedObject&)> processTable;
        processTable = [&](ObjInstance* currentTable, UnserializedObject& currentObj) {
            LOG_INFO("ECS Serialization", "Processing table with " << currentTable->fields.size() << " fields");
            for (const auto& [key, value] : currentTable->fields)
            {
                // Skip special fields
                if (key == "__className")
                    continue;

                if (IS_INSTANCE(value))
                {
                    LOG_INFO("ECS Serialization", "  Field '" << key << "' is a nested table");
                    // Nested table - create child object
                    UnserializedObject child(key, "", "");
                    processTable(vm->asInstance(value), child);
                    currentObj.children.push_back(std::move(child));
                }
                else
                {
                    // Leaf value - create as an attribute (isClass=false)
                    std::string valueStr;
                    std::string typeStr;

                    if (IS_INT(value))
                    {
                        valueStr = std::to_string(AS_INT(value));
                        typeStr = "int";
                    }
                    else if (IS_BOOL(value))
                    {
                        valueStr = AS_BOOL(value) ? "true" : "false";
                        typeStr = "bool";
                    }
                    else if (IS_DOUBLE(value))
                    {
                        valueStr = std::to_string(AS_DOUBLE(value));
                        typeStr = "float";
                    }
                    else if (IS_STRING(value))
                    {
                        valueStr = vm->asString(value)->toString();
                        typeStr = "string";
                    }

                    // Format as: __PGSA type {value}
                    std::string serializedStr = "__PGSA " + typeStr + " {" + valueStr + "}";
                    LOG_INFO("ECS Serialization", "  Field '" << key << "' = " << serializedStr);
                    UnserializedObject attr(serializedStr, key, false);
                    currentObj.children.push_back(std::move(attr));
                }
            }
        };

        processTable(table, obj);
        LOG_INFO("ECS Serialization", "After processTable, obj has " << obj.children.size() << " children");

        // Use the component registry to deserialize and attach
        ecsRef->getComponentRegistry()->deserializeComponentToEntity(obj, entity);
        return true;
    }

    /**
     * @brief Deserialize an entity from a VM table
     *
     * Takes a VM table representing an entity with all its components and creates/populates an entity.
     *
     * Expected table format:
     * {
     *   "__entityId": 42,  // Optional: if provided, tries to use this ID
     *   "Transform": { "__className": "Transform", "x": 100, "y": 200 },
     *   "Velocity": { "__className": "Velocity", "dx": 5.0, "dy": -3.0 }
     * }
     *
     * @param vm Pointer to the VM
     * @param ecsRef Pointer to the entity system
     * @param entityTable VM Value containing the entity table
     * @param createNew If true, always creates a new entity
     * @return EntityRef Reference to the created/populated entity
     */
    inline EntityRef deserializeEntityFromTable(VM* vm,
                                               EntitySystem* ecsRef,
                                               Value entityTable,
                                               bool createNew = false)
    {
        if (!IS_INSTANCE(entityTable))
        {
            throw std::runtime_error("entityTable is not an instance");
        }

        ObjInstance* table = vm->asInstance(entityTable);

        EntityRef entity;

        // Check if there's an entity ID specified
        _unique_id specifiedId = 0;
        auto idIt = table->fields.find("__entityId");
        if (idIt != table->fields.end())
        {
            if (IS_INT(idIt->second))
            {
                specifiedId = static_cast<_unique_id>(AS_INT(idIt->second));
            }
        }

        // Create or get the entity
        if (createNew || specifiedId == 0)
        {
            entity = ecsRef->createEntity();
        }
        else
        {
            // Try to get existing entity
            auto existingEntity = ecsRef->getEntity(specifiedId);
            if (existingEntity)
            {
                entity = EntityRef(existingEntity, ecsRef);
            }
            else
            {
                entity = ecsRef->createEntity();
                LOG_INFO("ECS Serialization", "Entity with ID " << specifiedId << " not found, created new entity with ID " << entity.id);
            }
        }

        // Deserialize each component
        for (const auto& [key, value] : table->fields)
        {
            // Skip special fields
            if (key == "__entityId" || key == "__className")
                continue;

            // Check if the field value is a table (component)
            if (IS_INSTANCE(value))
            {
                std::string componentTypeName = key;
                deserializeComponentFromTable(vm, ecsRef, entity, value, componentTypeName);
            }
        }

        return entity;
    }

    /**
     * @brief Batch serialize multiple entities to a VM table (array of entities)
     *
     * Creates a table with numeric indices containing entity tables.
     *
     * Example result:
     * {
     *   "0": { "__entityId": 1, "Transform": {...}, ... },
     *   "1": { "__entityId": 2, "Velocity": {...}, ... },
     *   "count": 2
     * }
     *
     * @param vm Pointer to the VM
     * @param ecsRef Pointer to the entity system
     * @param entities Vector of entity pointers to serialize
     * @return Value A VM Value containing the entities table
     */
    inline Value serializeEntitiesToTable(VM* vm,
                                         EntitySystem* ecsRef,
                                         const std::vector<Entity*>& entities)
    {
        // Get the Table class
        auto it = vm->globals.find("__Table");
        if (it == vm->globals.end())
        {
            throw std::runtime_error("Table class not found in VM globals");
        }

        Klass* tableClass = vm->asClass(it->second);

        // Create the entities table
        Value entitiesTableValue = vm->createInstance(tableClass);
        ObjInstance* entitiesTable = vm->asInstance(entitiesTableValue);

        size_t index = 0;
        for (auto entity : entities)
        {
            Value entityTableValue = serializeEntityToTable(vm, ecsRef, entity);

            Value indexKey = vm->createString(std::to_string(index));
            entitiesTable->fields[vm->asString(indexKey)->toString()] = vm->retainValue(entityTableValue);
            vm->releaseAndDelete(indexKey);
            vm->releaseAndDelete(entityTableValue);

            index++;
        }

        // Add count field
        Value countKey = vm->createString("count");
        Value countValue = makeIntValue(static_cast<int64_t>(entities.size()));
        entitiesTable->fields[vm->asString(countKey)->toString()] = vm->retainValue(countValue);
        vm->releaseAndDelete(countKey);

        return entitiesTableValue;
    }

    /**
     * @brief Batch deserialize multiple entities from a VM table
     *
     * Expects a table with numeric indices containing entity tables.
     *
     * @param vm Pointer to the VM
     * @param ecsRef Pointer to the entity system
     * @param entitiesTable VM Value containing the entities table
     * @param createNew If true, always creates new entities
     * @return std::vector<EntityRef> Vector of created/populated entity references
     */
    inline std::vector<EntityRef> deserializeEntitiesFromTable(VM* vm,
                                                               EntitySystem* ecsRef,
                                                               Value entitiesTable,
                                                               bool createNew = true)
    {
        if (!IS_INSTANCE(entitiesTable))
        {
            throw std::runtime_error("entitiesTable is not an instance");
        }

        ObjInstance* table = vm->asInstance(entitiesTable);
        std::vector<EntityRef> entities;

        for (const auto& [key, value] : table->fields)
        {
            // Skip non-numeric keys and special fields
            if (key == "count" || key == "__className")
                continue;

            // Try to parse as numeric index
            try
            {
                size_t index = std::stoull(key);
                (void)index; // Suppress unused variable warning

                if (IS_INSTANCE(value))
                {
                    auto entity = deserializeEntityFromTable(vm, ecsRef, value, createNew);
                    entities.push_back(entity);
                }
            }
            catch (...)
            {
                // Not a numeric key, skip it
                continue;
            }
        }

        return entities;
    }

    /**
     * @brief Deserialize a VM table directly to a known component type
     *
     * Similar to the interpreter's deserializeTo() function, this allows you to deserialize
     * a VM table to a specific component type when you know the type at compile time.
     *
     * Example usage:
     * ```cpp
     * Value table = ...; // VM table containing Transform data
     * Transform transform = deserializeTo<Transform>(vm, table);
     * ```
     *
     * @tparam Type The component type to deserialize to
     * @param vm Pointer to the VM
     * @param table VM Value containing the table (ObjInstance) to deserialize
     * @return Type Instance of the deserialized component
     */
    template <typename Type>
    Type deserializeTo(VM* vm, Value table)
    {
        if (!IS_INSTANCE(table))
        {
            LOG_ERROR("ECS Serialization", "Table value is not an instance");
            return Type{};
        }

        ObjInstance* objTable = vm->asInstance(table);

        // Get the class name (component type)
        std::string typeName = Type::getType();

        auto classNameIt = objTable->fields.find("__className");
        if (classNameIt != objTable->fields.end() && IS_STRING(classNameIt->second))
        {
            typeName = vm->asString(classNameIt->second)->toString();
        }

        // Create the root unserialized object
        // Create a dummy serialized string that parseString() can parse correctly
        std::string dummySerializedString = typeName + ": " + typeName + " {";
        UnserializedObject obj(typeName, typeName, dummySerializedString);

        // Helper function to convert table fields to UnserializedObject
        std::function<void(ObjInstance*, UnserializedObject&)> processTable;
        processTable = [&](ObjInstance* currentTable, UnserializedObject& currentObj) {
            for (const auto& [key, value] : currentTable->fields)
            {
                // Skip special fields
                if (key == "__className")
                    continue;

                if (IS_INSTANCE(value))
                {
                    // Nested table - create child object
                    UnserializedObject child(key, "", "");
                    processTable(vm->asInstance(value), child);
                    currentObj.children.push_back(std::move(child));
                }
                else
                {
                    // Leaf value - create as an attribute (isClass=false)
                    std::string valueStr;
                    std::string typeStr;

                    if (IS_INT(value))
                    {
                        valueStr = std::to_string(AS_INT(value));
                        typeStr = "int";
                    }
                    else if (IS_BOOL(value))
                    {
                        valueStr = AS_BOOL(value) ? "true" : "false";
                        typeStr = "bool";
                    }
                    else if (IS_DOUBLE(value))
                    {
                        valueStr = std::to_string(AS_DOUBLE(value));
                        typeStr = "float";
                    }
                    else if (IS_STRING(value))
                    {
                        valueStr = vm->asString(value)->toString();
                        typeStr = "string";
                    }

                    // Format as: __PGSA type {value}
                    std::string serializedStr = "__PGSA " + typeStr + " {" + valueStr + "}";
                    UnserializedObject attr(serializedStr, key, false);
                    currentObj.children.push_back(std::move(attr));
                }
            }
        };

        processTable(objTable, obj);

        // Use the existing deserialize function to convert to the component type
        return deserialize<Type>(obj);
    }

    /**
     * @brief Serialize a component directly to a VM table (templated version)
     *
     * Serialize a component of a known type directly to a VM table.
     * This is useful when you have a component object and want to convert it to a table.
     *
     * Example usage:
     * ```cpp
     * Transform transform;
     * transform.x = 100;
     * transform.y = 200;
     * Value table = serializeToTable<Transform>(vm, transform);
     * ```
     *
     * @tparam Type The component type to serialize
     * @param vm Pointer to the VM
     * @param component The component instance to serialize
     * @return Value VM Value containing the table representation
     */
    template <typename Type>
    Value serializeToTable(VM* vm, const Type& component)
    {
        // Get the Table class
        auto it = vm->globals.find("__Table");
        if (it == vm->globals.end())
        {
            throw std::runtime_error("Table class not found in VM globals");
        }

        Klass* tableClass = vm->asClass(it->second);

        // Create an Archive and serialize the component
        InspectorArchive archive;
        serialize(archive, component);

        // Create the table instance
        Value tableValue = vm->createInstance(tableClass);
        ObjInstance* table = vm->asInstance(tableValue);

        // Helper function to add field to table
        auto addField = [&](const std::string& key, Value value) {
            table->fields[key] = vm->retainValue(value);
        };

        // Parse the archive and populate the table
        if (archive.mainNode.children.size() > 0)
        {
            auto& compNode = archive.mainNode.children[0];

            // Add the class name (component type)
            if (!compNode.className.empty())
            {
                Value classNameKey = vm->createString("__className");
                Value classNameValue = vm->createString(compNode.className);
                addField(vm->asString(classNameKey)->toString(), classNameValue);
                vm->releaseAndDelete(classNameKey);
                vm->releaseAndDelete(classNameValue);  // Release initial reference
            }

            // Helper to check if a node is an ElementType (has "type" and "data" children)
            auto isElementType = [](const SerializedInfoHolder& node) -> bool {
                if (node.className != "ElementType") return false;
                bool hasType = false;
                bool hasData = false;
                for (const auto& child : node.children)
                {
                    if (child.name == "type") hasType = true;
                    if (child.name == "data") hasData = true;
                }
                return hasType && hasData;
            };

            // Helper to extract the data value from an ElementType node
            auto extractElementTypeValue = [&](const SerializedInfoHolder& node) -> Value {
                for (const auto& child : node.children)
                {
                    if (child.name == "data" && !child.value.empty())
                    {
                        if (child.type == "int")
                            return makeIntValue(std::stoi(child.value));
                        else if (child.type == "bool")
                            return makeBoolValue(child.value == "true");
                        else if (child.type == "float" || child.type == "double")
                            return makeDoubleValue(std::stod(child.value));
                        else if (child.type == "size_t" || child.type == "unsigned int")
                            return makeIntValue(std::stoull(child.value));
                        else if (child.type == "string")
                            return vm->createString(child.value);
                        else
                            return vm->createString(child.value);
                    }
                }
                return makeIntValue(0); // Default
            };

            // Recursively add all children (component properties)
            std::function<void(const SerializedInfoHolder&, ObjInstance*)> processNode;
            processNode = [&](const SerializedInfoHolder& node, ObjInstance* currentTable) {
                // Check if this is an ElementType - flatten it to just the data value
                if (isElementType(node) && !node.name.empty())
                {
                    Value key = vm->createString(node.name);
                    Value value = extractElementTypeValue(node);
                    currentTable->fields[vm->asString(key)->toString()] = vm->retainValue(value);
                    vm->releaseAndDelete(key);
                    if (IS_STRING(value))
                    {
                        vm->releaseAndDelete(value);
                    }
                    return;
                }

                // If this node has a value (it's a leaf property), add it
                if (!node.value.empty() && !node.name.empty())
                {
                    Value key = vm->createString(node.name);
                    Value value;

                    // Convert the string value to appropriate VM type
                    if (node.type == "int")
                    {
                        value = makeIntValue(std::stoi(node.value));
                    }
                    else if (node.type == "bool")
                    {
                        value = makeBoolValue(node.value == "true");
                    }
                    else if (node.type == "float" || node.type == "double")
                    {
                        value = makeDoubleValue(std::stod(node.value));
                    }
                    else if (node.type == "size_t" || node.type == "unsigned int")
                    {
                        value = makeIntValue(std::stoull(node.value));
                    }
                    else if (node.type == "string")
                    {
                        value = vm->createString(node.value);
                    }
                    else
                    {
                        // Default to string representation
                        value = vm->createString(node.value);
                    }

                    currentTable->fields[vm->asString(key)->toString()] = vm->retainValue(value);
                    vm->releaseAndDelete(key);
                    // Only release strings (heap objects), not primitives (int, bool, double)
                    if (IS_STRING(value))
                    {
                        vm->releaseAndDelete(value);
                    }
                }

                // If this node has children, process them
                if (node.children.size() > 0)
                {
                    // Check if this is a Vector (array-like structure)
                    if (node.className == "Vector" && !node.name.empty())
                    {
                        Value nestedTableValue = vm->createInstance(tableClass);
                        ObjInstance* nestedTable = vm->asInstance(nestedTableValue);

                        // Add elements with numeric indices [0], [1], etc.
                        for (size_t i = 0; i < node.children.size(); i++)
                        {
                            const auto& child = node.children[i];

                            Value elementValue;
                            if (!child.value.empty())
                            {
                                // Leaf element - convert directly
                                if (child.type == "int")
                                {
                                    elementValue = makeIntValue(std::stoi(child.value));
                                }
                                else if (child.type == "bool")
                                {
                                    elementValue = makeBoolValue(child.value == "true");
                                }
                                else if (child.type == "float" || child.type == "double")
                                {
                                    elementValue = makeDoubleValue(std::stod(child.value));
                                }
                                else if (child.type == "size_t" || child.type == "unsigned int")
                                {
                                    elementValue = makeIntValue(std::stoull(child.value));
                                }
                                else if (child.type == "string")
                                {
                                    elementValue = vm->createString(child.value);
                                }
                                else
                                {
                                    elementValue = vm->createString(child.value);
                                }
                            }
                            else if (child.children.size() > 0)
                            {
                                // Complex element - create nested table
                                Value childTableValue = vm->createInstance(tableClass);
                                ObjInstance* childTable = vm->asInstance(childTableValue);
                                processNode(child, childTable);
                                elementValue = childTableValue;
                            }

                            nestedTable->fields[std::to_string(i)] = vm->retainValue(elementValue);
                            if (IS_STRING(elementValue))
                            {
                                vm->releaseAndDelete(elementValue);
                            }
                        }

                        Value key = vm->createString(node.name);
                        currentTable->fields[vm->asString(key)->toString()] = vm->retainValue(nestedTableValue);
                        vm->releaseAndDelete(key);
                        vm->releaseAndDelete(nestedTableValue);
                    }
                    // Check if this is an UnorderedMap
                    else if (node.className == "UnorderedMap" && !node.name.empty())
                    {
                        Value nestedTableValue = vm->createInstance(tableClass);
                        ObjInstance* nestedTable = vm->asInstance(nestedTableValue);

                        // Find nbElements to determine how many key-value pairs
                        size_t nbElements = 0;
                        for (const auto& child : node.children)
                        {
                            if (child.name == "nbElements" && !child.value.empty())
                            {
                                nbElements = std::stoull(child.value);
                                break;
                            }
                        }

                        // Extract key-value pairs and use actual keys as indices
                        for (size_t i = 0; i < nbElements; i++)
                        {
                            std::string keyName = "key" + std::to_string(i);
                            std::string valueName = "value" + std::to_string(i);

                            std::string actualKey;
                            Value actualValue;

                            // Find the key and value in children
                            for (const auto& child : node.children)
                            {
                                if (child.name == keyName && !child.value.empty())
                                {
                                    actualKey = child.value;
                                }
                                else if (child.name == valueName)
                                {
                                    // Check if value is an ElementType - flatten it
                                    if (isElementType(child))
                                    {
                                        actualValue = extractElementTypeValue(child);
                                    }
                                    else if (!child.value.empty())
                                    {
                                        // Leaf value
                                        if (child.type == "int")
                                        {
                                            actualValue = makeIntValue(std::stoi(child.value));
                                        }
                                        else if (child.type == "bool")
                                        {
                                            actualValue = makeBoolValue(child.value == "true");
                                        }
                                        else if (child.type == "float" || child.type == "double")
                                        {
                                            actualValue = makeDoubleValue(std::stod(child.value));
                                        }
                                        else if (child.type == "size_t" || child.type == "unsigned int")
                                        {
                                            actualValue = makeIntValue(std::stoull(child.value));
                                        }
                                        else if (child.type == "string")
                                        {
                                            actualValue = vm->createString(child.value);
                                        }
                                        else
                                        {
                                            actualValue = vm->createString(child.value);
                                        }
                                    }
                                    else if (child.children.size() > 0)
                                    {
                                        // Complex value - create nested table
                                        Value childTableValue = vm->createInstance(tableClass);
                                        ObjInstance* childTable = vm->asInstance(childTableValue);
                                        processNode(child, childTable);
                                        actualValue = childTableValue;
                                    }
                                }
                            }

                            if (!actualKey.empty())
                            {
                                nestedTable->fields[actualKey] = vm->retainValue(actualValue);
                                if (IS_STRING(actualValue))
                                {
                                    vm->releaseAndDelete(actualValue);
                                }
                            }
                        }

                        Value key = vm->createString(node.name);
                        currentTable->fields[vm->asString(key)->toString()] = vm->retainValue(nestedTableValue);
                        vm->releaseAndDelete(key);
                        vm->releaseAndDelete(nestedTableValue);
                    }
                    // If the node has a name, create a nested table for the children
                    // Otherwise, add children directly to the current table
                    else if (!node.name.empty())
                    {
                        Value nestedTableValue = vm->createInstance(tableClass);
                        ObjInstance* nestedTable = vm->asInstance(nestedTableValue);

                        for (const auto& child : node.children)
                        {
                            processNode(child, nestedTable);
                        }

                        Value key = vm->createString(node.name);
                        currentTable->fields[vm->asString(key)->toString()] = vm->retainValue(nestedTableValue);
                        vm->releaseAndDelete(key);
                        vm->releaseAndDelete(nestedTableValue);
                    }
                    else
                    {
                        // Node has no name, so add children directly to current table
                        for (const auto& child : node.children)
                        {
                            processNode(child, currentTable);
                        }
                    }
                }
            };

            processNode(compNode, table);
        }

        return tableValue;
    }

} // namespace pg
