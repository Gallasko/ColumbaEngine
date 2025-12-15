#include "ecsserialization.h"

#include "ECS/entitysystem.h"
#include "2D/position.h"
#include "2D/collisionsystem.h"

#include <iostream>

namespace pg
{
    namespace detail
    {
        // Todo maybe even make this whole registrar thing constexpr
        bool registryHasComponent(const std::string& name)
        {
            auto& registry = ComponentSerializerRegistry::instance();
            return not name.empty() && registry.hasSerializer(name);
        }

        ComponentSerializerFunc getSerializerFuncFromRegistry(const std::string& name)
        {
            auto& registry = ComponentSerializerRegistry::instance();
            return registry.getSerializer(name);
        }

        // Shared implementation for attachComp native function
        NativeFn createAttachCompFunction(Entity* entityPtr, EntitySystem* ecsRef)
        {
            return [entityPtr, ecsRef](VM* vm, int argCount, Value* args) -> Value {
                if (argCount < 1)
                {
                    throw std::runtime_error("attachComp expects at least 1 argument (componentName)");
                }

                // Get component name
                if (!IS_STRING(args[0]))
                {
                    throw std::runtime_error("attachComp expects first argument to be component name (string)");
                }
                auto componentName = vm->asString(args[0]);

                // Special case: if component name is "Collision", attach CollisionComponent instead
                if (componentName == "Collision")
                {
                    // Parse arguments for CollisionComponent
                    // Expected: attachComp("Collision", "layerId", layerId, "scale", scale, ...)
                    size_t layerId = 0;
                    float scale = 1.0f;
                    bool checkSpecificLayer = false;
                    std::vector<size_t> checkLayerId;

                    // Process key-value pairs
                    for (int i = 1; i < argCount; i += 2)
                    {
                        if (i + 1 >= argCount) break;

                        if (!IS_STRING(args[i]))
                        {
                            throw std::runtime_error("attachComp expects string keys for properties");
                        }

                        auto key = vm->asString(args[i]);
                        auto value = args[i + 1];

                        if (key == "layerId" && IS_INT(value))
                        {
                            layerId = static_cast<size_t>(AS_INT(value));
                        }
                        else if (key == "scale")
                        {
                            if (IS_DOUBLE(value))
                                scale = static_cast<float>(AS_DOUBLE(value));
                            else if (IS_INT(value))
                                scale = static_cast<float>(AS_INT(value));
                        }
                        // Could add checkLayerId array parsing here if needed
                    }

                    // Attach CollisionComponent with parsed parameters
                    ecsRef->_attach<CollisionComponent>(entityPtr, layerId, scale);

                    LOG_INFO("ECS Serialization", "Attached native CollisionComponent to entity " << entityPtr->id
                             << " (layerId=" << layerId << ", scale=" << scale << ")");

                    return INT_VAL(0);
                }

                // Attach the StandardComponent (create empty first)
                CompRef<StandardComponent> component = ecsRef->_attach(entityPtr, componentName);

                // Process remaining arguments as key-value pairs and add them directly to the component
                for (int i = 1; i < argCount; i += 2)
                {
                    if (i + 1 >= argCount)
                    {
                        throw std::runtime_error("attachComp expects key-value pairs for component properties");
                    }

                    if (!IS_STRING(args[i]))
                    {
                        throw std::runtime_error("attachComp expects string keys for properties");
                    }

                    auto key = vm->asString(args[i]);
                    auto value = args[i + 1];

                    // Convert Value to ElementType and add directly to component
                    if (IS_INT(value))
                    {
                        component->properties[key] = ElementType{static_cast<int>(AS_INT(value))};
                    }
                    else if (IS_DOUBLE(value))
                    {
                        component->properties[key] = ElementType{AS_DOUBLE(value)};
                    }
                    else if (IS_STRING(value))
                    {
                        component->properties[key] = ElementType{vm->asString(value)};
                    }
                    else if (IS_BOOL(value))
                    {
                        component->properties[key] = ElementType{AS_BOOL(value)};
                    }
                    else
                    {
                        throw std::runtime_error("attachComp: unsupported value type for property '" + key + "'");
                    }
                }

                LOG_INFO("ECS Serialization", "Attached StandardComponent '" << componentName
                         << "' to entity " << entityPtr->id);

                return makeBoolValue(true);
            };
        }
    }

    // ============================================================================
    // Registered Component Serializers
    // ============================================================================

    /**
     * @brief Generate setters for PositionComponent
     *
     * This function adds dynamic setter methods to a PositionComponent table that
     * call the component's C++ setter methods (setX, setY, etc.) which automatically
     * trigger PositionComponentChangedEvent.
     */
    void serializePositionComponentWithSetters(VM* vm, ObjInstance* table, PositionComponent* component)
    {
        // Get component context
        _unique_id entityId = component->id;

        LOG_MILE("ECS Serialization", "Generating setters for PositionComponent on entity " << entityId);

        // Define the properties that have setters in PositionComponent
        struct PropertySetter {
            std::string propName;
            std::string methodName;
        };

        std::vector<PropertySetter> propertiesWithSetters = {
            {"x", "setX"},
            {"y", "setY"},
            {"z", "setZ"},
            {"width", "setWidth"},
            {"height", "setHeight"},
            {"rotation", "setRotation"},
            {"visible", "setVisibility"},
            {"observable", "setObservable"}
        };

        // Generate setter methods for each property
        for (const auto& prop : propertiesWithSetters)
        {
            const std::string& propName = prop.propName;
            const std::string& setterMethodName = prop.methodName;

            // Create native function directly without polluting globals
            // Lambda that implements the setter functionality
            NativeFn setterFunc;

            if (propName == "x")
            {
                setterFunc = [component](VM*, int argCount, Value* args) -> Value {
                    if (argCount != 1) return INT_VAL(0);
                    if (IS_DOUBLE(args[0]))
                        component->setX(static_cast<float>(AS_DOUBLE(args[0])));
                    else if (IS_INT(args[0]))
                        component->setX(static_cast<float>(AS_INT(args[0])));
                    return INT_VAL(0);
                };
            }
            else if (propName == "y")
            {
                setterFunc = [component](VM*, int argCount, Value* args) -> Value {
                    if (argCount != 1) return INT_VAL(0);
                    if (IS_DOUBLE(args[0]))
                        component->setY(static_cast<float>(AS_DOUBLE(args[0])));
                    else if (IS_INT(args[0]))
                        component->setY(static_cast<float>(AS_INT(args[0])));
                    return INT_VAL(0);
                };
            }
            else if (propName == "z")
            {
                setterFunc = [component](VM*, int argCount, Value* args) -> Value {
                    if (argCount != 1) return INT_VAL(0);
                    if (IS_DOUBLE(args[0]))
                        component->setZ(static_cast<float>(AS_DOUBLE(args[0])));
                    else if (IS_INT(args[0]))
                        component->setZ(static_cast<float>(AS_INT(args[0])));
                    return INT_VAL(0);
                };
            }
            else if (propName == "width")
            {
                setterFunc = [component](VM*, int argCount, Value* args) -> Value {
                    if (argCount != 1) return INT_VAL(0);
                    if (IS_DOUBLE(args[0]))
                        component->setWidth(static_cast<float>(AS_DOUBLE(args[0])));
                    else if (IS_INT(args[0]))
                        component->setWidth(static_cast<float>(AS_INT(args[0])));
                    return INT_VAL(0);
                };
            }
            else if (propName == "height")
            {
                setterFunc = [component](VM*, int argCount, Value* args) -> Value {
                    if (argCount != 1) return INT_VAL(0);
                    if (IS_DOUBLE(args[0]))
                        component->setHeight(static_cast<float>(AS_DOUBLE(args[0])));
                    else if (IS_INT(args[0]))
                        component->setHeight(static_cast<float>(AS_INT(args[0])));
                    return INT_VAL(0);
                };
            }
            else if (propName == "rotation")
            {
                setterFunc = [component](VM*, int argCount, Value* args) -> Value {
                    if (argCount != 1) return INT_VAL(0);
                    if (IS_DOUBLE(args[0]))
                        component->setRotation(static_cast<float>(AS_DOUBLE(args[0])));
                    else if (IS_INT(args[0]))
                        component->setRotation(static_cast<float>(AS_INT(args[0])));
                    return INT_VAL(0);
                };
            }
            else if (propName == "visible")
            {
                setterFunc = [component](VM*, int argCount, Value* args) -> Value {
                    if (argCount != 1) return INT_VAL(0);
                    if (IS_BOOL(args[0]))
                        component->setVisibility(AS_BOOL(args[0]));
                    return INT_VAL(0);
                };
            }
            else if (propName == "observable")
            {
                setterFunc = [component](VM*, int argCount, Value* args) -> Value {
                    if (argCount != 1) return INT_VAL(0);
                    if (IS_BOOL(args[0]))
                        component->setObservable(AS_BOOL(args[0]));
                    return INT_VAL(0);
                };
            }

            // Create native function and add directly to table without going through globals
            table->fields[setterMethodName] = vm->createNativeFunction(setterFunc);
        }
    }

    // Register PositionComponent serializer at static initialization time
    REGISTER_COMPONENT_SERIALIZER(PositionComponent, serializePositionComponentWithSetters);

    /**
     * @brief Generate setters for StandardComponent
     *
     * This function adds dynamic setter methods to a StandardComponent table that
     * automatically trigger Changed<ComponentType> events when called.
     */
    void serializeStandardComponentWithSetters(VM* vm, ObjInstance* table, StandardComponent* component)
    {
        // Get component context
        std::string compTypeName = component->typeName;
        _unique_id entityId = component->entityId;

        // Get the properties table
        auto propertiesIt = table->fields.find("properties");
        if (propertiesIt == table->fields.end() || !IS_INSTANCE(propertiesIt->second))
        {
            LOG_WARNING("ECS Serialization", "No properties table found for StandardComponent, skipping setter generation");
            return;
        }

        ObjInstance* propertiesTable = vm->asInstance(propertiesIt->second);

        // Flatten properties to the top level of the table
        std::vector<std::string> propertyNames;
        for (const auto& [key, value] : propertiesTable->fields)
        {
            if (key != "__className" && !key.empty())
            {
                propertyNames.push_back(key);
                // Copy property to top level
                table->fields[key] = vm->retainValue(value);
            }
        }

        LOG_MILE("ECS Serialization", "Flattened " << propertyNames.size() << " properties and generating setters for StandardComponent '" << compTypeName << "'");

        // Generate specific setters (setX, setY, setValue, etc.)
        for (const std::string& propName : propertyNames)
        {
            // Generate method name: "set" + Capitalized(propName)
            std::string methodName = "set";
            if (!propName.empty())
            {
                methodName += static_cast<char>(std::toupper(propName[0]));
                if (propName.size() > 1)
                {
                    methodName += propName.substr(1);
                }
            }

            // Create native function directly without polluting globals
            NativeFn setterFunc = [component, propName, propertiesTable](VM* vm, int argCount, Value* args) -> Value {
                if (argCount != 1)
                {
                    LOG_ERROR("StandardComponent Setter", "Expected 1 argument for setter, got " << argCount);
                    return INT_VAL(0);
                }

                // Convert the VM value to ElementType
                ElementType newValue = vm->valueToElement(args[0]);

                // Check if the value actually changed
                bool valueChanged = false;
                if (component->has(propName))
                {
                    ElementType oldValue = component->properties[propName];
                    valueChanged = !(oldValue == newValue);
                }
                else
                {
                    valueChanged = true;
                }

                // Only update and fire event if value actually changed
                if (valueChanged)
                {
                    // setWithEvent updates the property and fires Changed<ComponentType> event
                    component->setWithEvent(propName, newValue);

                    // Also update the VM table so the script sees the change immediately
                    if (propertiesTable->fields.find(propName) != propertiesTable->fields.end())
                    {
                        vm->releaseAndDelete(propertiesTable->fields[propName]);
                    }
                    propertiesTable->fields[propName] = vm->retainValue(args[0]);
                }

                return INT_VAL(0);
            };

            // Allocate native function from pool and create Value directly
            auto [nativeFunc, funcIndex] = vm->pools.nativeFuncPool.allocateWithIndex();
            nativeFunc->function = setterFunc;

            Value setterValue = makeNativeFuncValue(static_cast<uint32_t>(funcIndex));

            // Add directly to table without going through globals
            table->fields[methodName] = vm->trackNewValue(setterValue);

            LOG_MILE("ECS Serialization", "Added setter method: " << methodName);
        }

        // Add generic set(propertyName, value) method
        // Create native function directly without polluting globals
        NativeFn genericSetterFunc = [component, propertiesTable](VM* vm, int argCount, Value* args) -> Value {
            if (argCount != 2)
            {
                LOG_ERROR("StandardComponent Generic Setter", "Expected 2 arguments (propertyName, value), got " << argCount);
                return INT_VAL(0);
            }

            if (!IS_STRING(args[0]))
            {
                LOG_ERROR("StandardComponent Generic Setter", "First argument must be a string (property name)");
                return INT_VAL(0);
            }

            std::string propName = vm->asString(args[0]);

            // Convert the VM value to ElementType
            ElementType newValue = vm->valueToElement(args[1]);

            // Check if the value actually changed
            bool valueChanged = false;
            if (component->has(propName))
            {
                ElementType oldValue = component->properties[propName];
                valueChanged = !(oldValue == newValue);
            }
            else
            {
                valueChanged = true;
            }

            // Only update and fire event if value actually changed
            if (valueChanged)
            {
                // setWithEvent updates the property and fires Changed<ComponentType> event
                component->setWithEvent(propName, newValue);

                // Also update the VM table so the script sees the change immediately
                if (propertiesTable->fields.find(propName) != propertiesTable->fields.end())
                {
                    vm->releaseAndDelete(propertiesTable->fields[propName]);
                }
                propertiesTable->fields[propName] = vm->retainValue(args[1]);
            }

            return INT_VAL(0);
        };

        // Create native function and add directly to table without going through globals
        table->fields["set"] = vm->createNativeFunction(genericSetterFunc);

        LOG_MILE("ECS Serialization", "Added generic set() method");
    }

    // Register StandardComponent serializer at static initialization time
    REGISTER_COMPONENT_SERIALIZER(StandardComponent, serializeStandardComponentWithSetters);

    // ============================================================================
    // Public API Implementation
    // ============================================================================

    Value serializeComponentToTable(VM* vm, EntitySystem* ecsRef, const Entity* entity, _unique_id componentId)
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
            std::string componentTypeName;
            if (!compNode.className.empty())
            {
                componentTypeName = compNode.className;
                Value classNameValue = vm->createString(compNode.className);

                if (table->fields.find("__className") != table->fields.end())
                {
                    vm->releaseAndDelete(table->fields["__className"]);
                }

                table->fields["__className"] = classNameValue;
            }

            // Process all component properties using the shared helper
            detail::processNodeToTable(vm, tableClass, compNode, table, false);

            // Check if there's a registered serializer for this component type
            // The serializer will add dynamic setters to the table
            auto& registry = ComponentSerializerRegistry::instance();
            if (registry.hasSerializer(componentTypeName))
            {
                LOG_MILE("ECS Serialization", "Using registered serializer for " << componentTypeName);

                // Get the component pointer using the registered retriever function
                void* componentPtr = nullptr;

                if (componentTypeName == "StandardComponent")
                {
                    // StandardComponent requires special handling due to its dynamic nature
                    // Extract the type name from the table
                    std::string compTypeName;
                    auto typeNameIt = table->fields.find("typeName");
                    if (typeNameIt != table->fields.end() && IS_STRING(typeNameIt->second))
                    {
                        compTypeName = vm->asString(typeNameIt->second);
                        auto* owner = ecsRef->getComponentRegistry()->retrieveStandardComponent(compTypeName);

                        if (owner)
                        {
                            componentPtr = owner->getComponent(entity->id);
                        }
                    }
                }
                else
                {
                    // For all other components, use the registered retriever
                    auto retrieverFunc = registry.getRetriever(componentTypeName);
                    if (retrieverFunc)
                    {
                        componentPtr = retrieverFunc(ecsRef, entity->id);
                    }
                }

                if (componentPtr)
                {
                    auto serializerFunc = registry.getSerializer(componentTypeName);
                    serializerFunc(vm, table, componentPtr);
                }
                else
                {
                    LOG_WARNING("ECS Serialization", "Could not retrieve component pointer for " << componentTypeName);
                }
            }
        }

        return tableValue;
    }

    Value serializeEntityToTable(VM* vm, EntitySystem* ecsRef, Entity* entity)
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

        LOG_MILE("ECS Serialization", "Serializing entity ID " << entity->id);

        // Add the entity ID
        Value idValue = makeIntValue(static_cast<int64_t>(entity->id));
        entityTable->fields["__entityId"] = idValue;

        // Add native attachComp function that holds the entity pointer
        // This allows scripts to attach components immediately without entity lookup
        Value attachCompFuncValue = vm->createNativeFunction(detail::createAttachCompFunction(entity, ecsRef));
        entityTable->fields["attachComp"] = attachCompFuncValue;

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
                    componentTypeName = vm->asString(classNameIt->second);

                    // For StandardComponent, use the actual typeName instead of "StandardComponent"
                    if (componentTypeName == "StandardComponent")
                    {
                        auto typeNameIt = compTable->fields.find("typeName");
                        if (typeNameIt != compTable->fields.end() && IS_STRING(typeNameIt->second))
                        {
                            componentTypeName = vm->asString(typeNameIt->second);
                        }
                    }
                }

                // Add to entity table
                entityTable->fields[componentTypeName] = componentTableValue;
            }
        }

        return entityTableValue;
    }

    bool deserializeComponentFromTable(VM* vm, EntitySystem* ecsRef, EntityRef entity,
        Value componentTable, const std::string& componentTypeName)
    {
        if (!IS_INSTANCE(componentTable))
        {
            LOG_ERROR("ECS Serialization", "componentTable is not an instance");
            return false;
        }

        ObjInstance* table = vm->asInstance(componentTable);

        // Determine the component type name
        std::string typeName = componentTypeName;

        if (typeName.empty())
        {
            // Try to find the class name in the table
            auto it = table->fields.find("__className");
            if (it != table->fields.end())
            {
                if (IS_STRING(it->second))
                    typeName = vm->asString(it->second);
            }

            if (typeName.empty())
            {
                LOG_ERROR("ECS Serialization", "No component type name provided and no __className found in table");
                return false;
            }
        }

        LOG_MILE("ECS Serialization", "Component type name: " << typeName);

        // Convert the table to UnserializedObject
        // Create a dummy serialized string that parseString() can parse correctly
        // Format: "TypeName: TypeName {"
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

        processTable(table, obj);

        // Use the component registry to deserialize and attach
        ecsRef->getComponentRegistry()->deserializeComponentToEntity(obj, entity);
        return true;
    }

    EntityRef deserializeEntityFromTable(VM* vm, EntitySystem* ecsRef, Value entityTable, bool createNew)
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
                LOG_MILE("ECS Serialization", "Entity with ID " << specifiedId << " not found, created new entity with ID " << entity.id);
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

    Value serializeEntitiesToTable(VM* vm, EntitySystem* ecsRef, const std::vector<Entity*>& entities)
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
            entitiesTable->fields[vm->asString(indexKey)] = vm->retainValue(entityTableValue);
            vm->releaseAndDelete(indexKey);
            vm->releaseAndDelete(entityTableValue);

            index++;
        }

        // Add count field
        Value countKey = vm->createString("count");
        Value countValue = makeIntValue(static_cast<int64_t>(entities.size()));
        entitiesTable->fields[vm->asString(countKey)] = vm->retainValue(countValue);
        vm->releaseAndDelete(countKey);

        return entitiesTableValue;
    }

    std::vector<EntityRef> deserializeEntitiesFromTable(VM* vm, EntitySystem* ecsRef, Value entitiesTable, bool createNew)
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

    Value serializeToTable(VM* vm, const StandardComponent& component)
    {
        // StandardComponent still needs a specialized overload because it uses
        // serializeToTableBasic instead of the generic serialization
        Value tableValue = serializeToTableBasic(vm, component);
        ObjInstance* table = vm->asInstance(tableValue);

        // Use the registered serializer to add setters
        auto& registry = ComponentSerializerRegistry::instance();
        if (registry.hasSerializer("StandardComponent"))
        {
            auto serializerFunc = registry.getSerializer("StandardComponent");
            serializerFunc(vm, table, (void*)&component);
        }

        return tableValue;
    }
}