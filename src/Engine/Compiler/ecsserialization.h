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

#include "ECS/entitysystem_fwd.h"

#include "object.h"
#include "vm.h"
#include "serialization.h"

#include "ECS/entity.h"

#include <fstream>

namespace pg
{
    // ============================================================================
    // Internal helper functions for ECS serialization
    // ============================================================================

    using ComponentSerializerFunc = std::function<void(VM*, ObjInstance*, void*)>;
    using ComponentRetrieverFunc = std::function<void*(EntitySystem*, _unique_id)>;

    namespace detail
    {
        bool registryHasComponent(const std::string& name);

        ComponentSerializerFunc getSerializerFuncFromRegistry(const std::string& name);

        /**
         * @brief Create a native attachComp function that holds entity pointer
         *
         * This allows scripts to attach components immediately without entity lookup.
         *
         * @param entityPtr Pointer to the entity
         * @param ecsRef Pointer to the entity system
         * @return NativeFn Lambda function that can be registered as a native function
         */
        NativeFn createAttachCompFunction(Entity* entityPtr, EntitySystem* ecsRef);

        /**
         * @brief Check if a SerializedInfoHolder node represents an ElementType
         */
        inline bool isElementType(const SerializedInfoHolder& node)
        {
            if (node.className != "ElementType") return false;
            bool hasType = false;
            bool hasData = false;
            for (const auto& child : node.children)
            {
                if (child.name == "type") hasType = true;
                if (child.name == "data") hasData = true;
            }
            return hasType && hasData;
        }

        /**
         * @brief Extract the data value from an ElementType node and convert to VM Value
         */
        inline Value extractElementTypeValue(VM* vm, const SerializedInfoHolder& node)
        {
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
        }

        /**
         * @brief Convert a primitive value node to VM Value
         */
        inline Value convertPrimitiveToValue(VM* vm, const SerializedInfoHolder& node)
        {
            if (node.type == "int")
                return makeIntValue(std::stoi(node.value));
            else if (node.type == "bool")
                return makeBoolValue(node.value == "true");
            else if (node.type == "float" || node.type == "double")
                return makeDoubleValue(std::stod(node.value));
            else if (node.type == "size_t" || node.type == "unsigned int")
                return makeIntValue(std::stoull(node.value));
            else if (node.type == "string")
                return vm->createString(node.value);
            else
                return vm->createString(node.value);
        }

        /**
         * @brief Process a SerializedInfoHolder node and populate a VM table
         * This is the core recursive function that handles all serialization cases
         */
        inline void processNodeToTable(VM* vm, Klass* tableClass, const SerializedInfoHolder& node,
            ObjInstance* currentTable, bool retainValues = false)
        {
            // Check if this is an ElementType - flatten it to just the data value
            if (detail::isElementType(node) && !node.name.empty())
            {
                Value value = detail::extractElementTypeValue(vm, node);
                if (retainValues)
                {
                    currentTable->fields[node.name] = vm->retainValue(value);
                    if (IS_STRING(value))
                        vm->releaseAndDelete(value);
                }
                else
                {
                    currentTable->fields[node.name] = value;
                }
                return;
            }

            // If this node has a value (it's a leaf property), add it
            if (!node.value.empty() && !node.name.empty())
            {
                Value value = detail::convertPrimitiveToValue(vm, node);

                if (retainValues)
                {
                    currentTable->fields[node.name] = vm->retainValue(value);
                    if (IS_STRING(value))
                        vm->releaseAndDelete(value);
                }
                else
                {
                    currentTable->fields[node.name] = value;
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

                        if (detail::isElementType(child))
                        {
                            elementValue = detail::extractElementTypeValue(vm, child);
                        }
                        else if (!child.value.empty())
                        {
                            elementValue = detail::convertPrimitiveToValue(vm, child);
                        }
                        else if (child.children.size() > 0)
                        {
                            // Complex element - create nested table
                            Value childTableValue = vm->createInstance(tableClass);
                            ObjInstance* childTable = vm->asInstance(childTableValue);
                            detail::processNodeToTable(vm, tableClass, child, childTable, retainValues);
                            elementValue = childTableValue;
                        }

                        if (retainValues)
                        {
                            nestedTable->fields[std::to_string(i)] = vm->retainValue(elementValue);
                            if (IS_STRING(elementValue))
                                vm->releaseAndDelete(elementValue);
                        }
                        else
                        {
                            nestedTable->fields[std::to_string(i)] = elementValue;
                        }
                    }

                    if (retainValues)
                    {
                        currentTable->fields[node.name] = vm->retainValue(nestedTableValue);
                        vm->releaseAndDelete(nestedTableValue);
                    }
                    else
                    {
                        if (currentTable->fields.find(node.name) != currentTable->fields.end())
                            vm->releaseAndDelete(currentTable->fields[node.name]);
                        currentTable->fields[node.name] = nestedTableValue;
                    }
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
                                if (detail::isElementType(child))
                                {
                                    actualValue = detail::extractElementTypeValue(vm, child);
                                }
                                else if (!child.value.empty())
                                {
                                    actualValue = detail::convertPrimitiveToValue(vm, child);
                                }
                                else if (child.children.size() > 0)
                                {
                                    // Complex value - create nested table
                                    Value childTableValue = vm->createInstance(tableClass);
                                    ObjInstance* childTable = vm->asInstance(childTableValue);
                                    detail::processNodeToTable(vm, tableClass, child, childTable, retainValues);
                                    actualValue = childTableValue;
                                }
                            }
                        }

                        if (!actualKey.empty())
                        {
                            if (retainValues)
                            {
                                nestedTable->fields[actualKey] = vm->retainValue(actualValue);
                                if (IS_STRING(actualValue))
                                    vm->releaseAndDelete(actualValue);
                            }
                            else
                            {
                                nestedTable->fields[actualKey] = actualValue;
                            }
                        }
                    }

                    if (retainValues)
                    {
                        currentTable->fields[node.name] = vm->retainValue(nestedTableValue);
                        vm->releaseAndDelete(nestedTableValue);
                    }
                    else
                    {
                        if (currentTable->fields.find(node.name) != currentTable->fields.end())
                            vm->releaseAndDelete(currentTable->fields[node.name]);
                        currentTable->fields[node.name] = nestedTableValue;
                    }
                }
                // If the node has a name, create a nested table for the children
                else if (!node.name.empty())
                {
                    Value nestedTableValue = vm->createInstance(tableClass);
                    ObjInstance* nestedTable = vm->asInstance(nestedTableValue);

                    for (const auto& child : node.children)
                    {
                        detail::processNodeToTable(vm, tableClass, child, nestedTable, retainValues);
                    }

                    if (retainValues)
                    {
                        currentTable->fields[node.name] = vm->retainValue(nestedTableValue);
                        vm->releaseAndDelete(nestedTableValue);
                    }
                    else
                    {
                        if (currentTable->fields.find(node.name) != currentTable->fields.end())
                            vm->releaseAndDelete(currentTable->fields[node.name]);
                        currentTable->fields[node.name] = nestedTableValue;
                    }
                }
                else
                {
                    // Node has no name, so add children directly to current table
                    for (const auto& child : node.children)
                    {
                        detail::processNodeToTable(vm, tableClass, child, currentTable, retainValues);
                    }
                }
            }
        }
    } // namespace detail

    // ============================================================================
    // Public API functions
    // ============================================================================

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
    extern Value serializeComponentToTable(VM* vm, EntitySystem* ecsRef, const Entity* entity, _unique_id componentId);

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
    extern Value serializeEntityToTable(VM* vm, EntitySystem* ecsRef, Entity* entity);

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
    extern bool deserializeComponentFromTable(VM* vm, EntitySystem* ecsRef, EntityRef entity,
        Value componentTable, const std::string& componentTypeName = "");

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
    extern EntityRef deserializeEntityFromTable(VM* vm, EntitySystem* ecsRef, Value entityTable, bool createNew = false);

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
    extern Value serializeEntitiesToTable(VM* vm, EntitySystem* ecsRef, const std::vector<Entity*>& entities);

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
    extern std::vector<EntityRef> deserializeEntitiesFromTable(VM* vm, EntitySystem* ecsRef, Value entitiesTable, bool createNew = true);

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
            typeName = vm->asString(classNameIt->second);
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
                        valueStr = vm->asString(value);
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
     * This function automatically checks the ComponentSerializerRegistry and adds
     * dynamic setter methods if a serializer is registered for this component type.
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
    template <typename Type, typename = std::enable_if_t<!std::is_same_v<Type, StandardComponent>>>
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
        std::string componentTypeName;
        if (archive.mainNode.children.size() > 0)
        {
            auto& compNode = archive.mainNode.children[0];

            // Add the class name (component type)
            if (!compNode.className.empty())
            {
                componentTypeName = compNode.className;
                Value classNameKey = vm->createString("__className");
                Value classNameValue = vm->createString(compNode.className);
                addField(vm->asString(classNameKey), classNameValue);
                vm->releaseAndDelete(classNameKey);
                vm->releaseAndDelete(classNameValue);  // Release initial reference
            }

            // Process all component properties using the shared helper (with retain mode)
            detail::processNodeToTable(vm, tableClass, compNode, table, true);
        }

        // Check if there's a registered serializer to add dynamic setters
        if (detail::registryHasComponent(componentTypeName))
        {
            auto serializerFunc = detail::getSerializerFuncFromRegistry(componentTypeName);
            serializerFunc(vm, table, (void*)&component);
        }

        return tableValue;
    }

    /**
     * @brief Helper to serialize StandardComponent without setters (internal use)
     */
    inline Value serializeToTableBasic(VM* vm, const StandardComponent& component)
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
                addField(vm->asString(classNameKey), classNameValue);
                vm->releaseAndDelete(classNameKey);
                vm->releaseAndDelete(classNameValue);  // Release initial reference
            }

            // Process all component properties using the shared helper (with retain mode)
            detail::processNodeToTable(vm, tableClass, compNode, table, true);
        }

        return tableValue;
    }

    /**
     * @brief Specialized serializeToTable for StandardComponent with setter generation
     *
     * This overload generates dynamic setter methods for StandardComponent properties
     * that automatically trigger change events when called from scripts.
     * Uses the component's own ecsRef and entityId members.
     *
     * @param vm Pointer to the VM
     * @param component The StandardComponent to serialize
     * @return Value VM Value containing the table with setter methods
     */
    extern Value serializeToTable(VM* vm, const StandardComponent& component);

    // ============================================================================
    // Helper functions for CompList serialization
    // ============================================================================

    namespace detail
    {
        /**
         * @brief Helper function to serialize a single component from CompList to entity table
         */
        template <typename Comp, typename... Comps>
        void serializeCompListComponent(VM* vm, ObjInstance* entityTable, const CompList<Comps...>& compList)
        {
            // Get the component from the CompList
            CompRef<Comp> comp = compList.template get<Comp>();

            if (comp)
            {
                // Serialize the component to a table
                Value componentTableValue = serializeToTable(vm, *comp);

                // Get the component type name
                std::string componentTypeName = Comp::getType();

                // Add to entity table
                entityTable->fields[componentTypeName] = componentTableValue;

                LOG_INFO("ECS Serialization", "Serialized component: " << componentTypeName);
            }
        }
    } // namespace detail

    /**
     * @brief Serialize an entity with specific components to a VM table (templated version)
     *
     * Takes a CompList (e.g., CompList<PositionComponent, UiAnchor, Texture2DComponent>)
     * and serializes all the components in the list to a VM table. This is useful when
     * you have a CompList from helper functions like makeUiTexture and want to serialize
     * only those specific components.
     *
     * Example usage:
     * ```cpp
     * auto compList = makeUiTexture(ecs, 100, 100, "texture.png");
     * Value table = serializeEntityToTable<PositionComponent, UiAnchor, Texture2DComponent>(vm, ecs, compList);
     * ```
     *
     * @tparam Comps The component types in the CompList
     * @param vm Pointer to the VM
     * @param ecsRef Pointer to the entity system
     * @param compList The CompList containing the entity and component references
     * @return Value VM Value containing the entity table with specified components
     */
    template <typename... Comps>
    Value serializeEntityToTable(VM* vm, EntitySystem*, const CompList<Comps...>& compList)
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

        LOG_INFO("ECS Serialization", "Serializing entity ID " << compList.id << " with CompList");

        // Add the entity ID
        Value idValue = makeIntValue(static_cast<int64_t>(compList.id));
        entityTable->fields["__entityId"] = idValue;

        // Add native attachComp function that holds the entity pointer
        // This allows scripts to attach components immediately without entity lookup
        Entity* entityPtr = compList.entity.entity;
        EntitySystem* ecsRef = entityPtr->world();
        Value attachCompFuncValue = vm->createNativeFunction(detail::createAttachCompFunction(entityPtr, ecsRef));
        entityTable->fields["attachComp"] = attachCompFuncValue;

        // Serialize each component in the CompList using fold expression
        (detail::serializeCompListComponent<Comps>(vm, entityTable, compList), ...);

        return entityTableValue;
    }

} // namespace pg
