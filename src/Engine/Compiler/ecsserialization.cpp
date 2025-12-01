#include "ecsserialization.h"

#include "ECS/entitysystem.h"
#include "2D/position.h"
#include "2D/texture.h"

namespace pg
{
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
    void serializePositionComponentWithSetters(VM* vm, ObjInstance* table, const PositionComponent& component)
    {
        // Get component context
        EntitySystem* ecsRef = component.ecsRef;
        _unique_id entityId = component.id;

        // If component doesn't have ECS context, skip setter generation
        if (!ecsRef || entityId == 0)
        {
            LOG_WARNING("ECS Serialization", "PositionComponent has no ecsRef or entityId, skipping setter generation");
            return;
        }

        LOG_INFO("ECS Serialization", "Generating setters for PositionComponent on entity " << entityId);

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

            // Register a global VM function with a unique name
            std::string globalSetterName = "__positionsetter_" + std::to_string(entityId) + "_" + propName;

            // Lambda that implements the setter functionality
            if (propName == "x")
            {
                vm->registerNative(globalSetterName, [ecsRef, entityId](VM*, int argCount, Value* args) -> Value {
                    if (argCount != 1) return INT_VAL(0);
                    Entity* entity = ecsRef->getEntity(entityId);
                    if (!entity) return INT_VAL(0);
                    auto posComp = entity->get<PositionComponent>();
                    if (!posComp) return INT_VAL(0);
                    if (IS_DOUBLE(args[0]))
                        posComp->setX(static_cast<float>(AS_DOUBLE(args[0])));
                    else if (IS_INT(args[0]))
                        posComp->setX(static_cast<float>(AS_INT(args[0])));
                    return INT_VAL(0);
                });
            }
            else if (propName == "y")
            {
                vm->registerNative(globalSetterName, [ecsRef, entityId](VM*, int argCount, Value* args) -> Value {
                    if (argCount != 1) return INT_VAL(0);
                    Entity* entity = ecsRef->getEntity(entityId);
                    if (!entity) return INT_VAL(0);
                    auto posComp = entity->get<PositionComponent>();
                    if (!posComp) return INT_VAL(0);
                    if (IS_DOUBLE(args[0]))
                        posComp->setY(static_cast<float>(AS_DOUBLE(args[0])));
                    else if (IS_INT(args[0]))
                        posComp->setY(static_cast<float>(AS_INT(args[0])));
                    return INT_VAL(0);
                });
            }
            else if (propName == "z")
            {
                vm->registerNative(globalSetterName, [ecsRef, entityId](VM*, int argCount, Value* args) -> Value {
                    if (argCount != 1) return INT_VAL(0);
                    Entity* entity = ecsRef->getEntity(entityId);
                    if (!entity) return INT_VAL(0);
                    auto posComp = entity->get<PositionComponent>();
                    if (!posComp) return INT_VAL(0);
                    if (IS_DOUBLE(args[0]))
                        posComp->setZ(static_cast<float>(AS_DOUBLE(args[0])));
                    else if (IS_INT(args[0]))
                        posComp->setZ(static_cast<float>(AS_INT(args[0])));
                    return INT_VAL(0);
                });
            }
            else if (propName == "width")
            {
                vm->registerNative(globalSetterName, [ecsRef, entityId](VM*, int argCount, Value* args) -> Value {
                    if (argCount != 1) return INT_VAL(0);
                    Entity* entity = ecsRef->getEntity(entityId);
                    if (!entity) return INT_VAL(0);
                    auto posComp = entity->get<PositionComponent>();
                    if (!posComp) return INT_VAL(0);
                    if (IS_DOUBLE(args[0]))
                        posComp->setWidth(static_cast<float>(AS_DOUBLE(args[0])));
                    else if (IS_INT(args[0]))
                        posComp->setWidth(static_cast<float>(AS_INT(args[0])));
                    return INT_VAL(0);
                });
            }
            else if (propName == "height")
            {
                vm->registerNative(globalSetterName, [ecsRef, entityId](VM*, int argCount, Value* args) -> Value {
                    if (argCount != 1) return INT_VAL(0);
                    Entity* entity = ecsRef->getEntity(entityId);
                    if (!entity) return INT_VAL(0);
                    auto posComp = entity->get<PositionComponent>();
                    if (!posComp) return INT_VAL(0);
                    if (IS_DOUBLE(args[0]))
                        posComp->setHeight(static_cast<float>(AS_DOUBLE(args[0])));
                    else if (IS_INT(args[0]))
                        posComp->setHeight(static_cast<float>(AS_INT(args[0])));
                    return INT_VAL(0);
                });
            }
            else if (propName == "rotation")
            {
                vm->registerNative(globalSetterName, [ecsRef, entityId](VM*, int argCount, Value* args) -> Value {
                    if (argCount != 1) return INT_VAL(0);
                    Entity* entity = ecsRef->getEntity(entityId);
                    if (!entity) return INT_VAL(0);
                    auto posComp = entity->get<PositionComponent>();
                    if (!posComp) return INT_VAL(0);
                    if (IS_DOUBLE(args[0]))
                        posComp->setRotation(static_cast<float>(AS_DOUBLE(args[0])));
                    else if (IS_INT(args[0]))
                        posComp->setRotation(static_cast<float>(AS_INT(args[0])));
                    return INT_VAL(0);
                });
            }
            else if (propName == "visible")
            {
                vm->registerNative(globalSetterName, [ecsRef, entityId](VM*, int argCount, Value* args) -> Value {
                    if (argCount != 1) return INT_VAL(0);
                    Entity* entity = ecsRef->getEntity(entityId);
                    if (!entity) return INT_VAL(0);
                    auto posComp = entity->get<PositionComponent>();
                    if (!posComp) return INT_VAL(0);
                    if (IS_BOOL(args[0]))
                        posComp->setVisibility(AS_BOOL(args[0]));
                    return INT_VAL(0);
                });
            }
            else if (propName == "observable")
            {
                vm->registerNative(globalSetterName, [ecsRef, entityId](VM*, int argCount, Value* args) -> Value {
                    if (argCount != 1) return INT_VAL(0);
                    Entity* entity = ecsRef->getEntity(entityId);
                    if (!entity) return INT_VAL(0);
                    auto posComp = entity->get<PositionComponent>();
                    if (!posComp) return INT_VAL(0);
                    if (IS_BOOL(args[0]))
                        posComp->setObservable(AS_BOOL(args[0]));
                    return INT_VAL(0);
                });
            }

            // Add the setter to the component table
            auto globalIt = vm->globals.find(globalSetterName);
            if (globalIt != vm->globals.end())
            {
                table->fields[setterMethodName] = globalIt->second;
                LOG_INFO("ECS Serialization", "Added PositionComponent setter method: " << setterMethodName);
            }
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
    void serializeStandardComponentWithSetters(VM* vm, ObjInstance* table, const StandardComponent& component)
    {
        // Get component context
        std::string compTypeName = component.typeName;
        EntitySystem* ecsRef = component.ecsRef;
        _unique_id entityId = component.entityId;

        // If component doesn't have ECS context, skip setter generation
        if (!ecsRef)
        {
            LOG_WARNING("ECS Serialization", "StandardComponent has no ecsRef, skipping setter generation");
            return;
        }

        // Get the properties table
        auto propertiesIt = table->fields.find("properties");
        if (propertiesIt == table->fields.end() || !IS_INSTANCE(propertiesIt->second))
        {
            LOG_WARNING("ECS Serialization", "No properties table found for StandardComponent, skipping setter generation");
            return;
        }

        ObjInstance* propertiesTable = vm->asInstance(propertiesIt->second);

        // Collect all property names
        std::vector<std::string> propertyNames;
        for (const auto& [key, value] : propertiesTable->fields)
        {
            if (key != "__className" && !key.empty())
            {
                propertyNames.push_back(key);
            }
        }

        LOG_INFO("ECS Serialization", "Generating " << propertyNames.size() << " setters for StandardComponent '" << compTypeName << "'");

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

            // Register a global VM function with a unique name
            std::string globalSetterName = "__setter_" + std::to_string(entityId) + "_" + compTypeName + "_" + propName;

            vm->registerNative(globalSetterName, [ecsRef, entityId, compTypeName, propName, propertiesTable](VM* vm, int argCount, Value* args) -> Value {
                if (argCount != 1)
                {
                    LOG_ERROR("StandardComponent Setter", "Expected 1 argument for setter, got " << argCount);
                    return INT_VAL(0);
                }

                // Retrieve the entity
                Entity* entity = ecsRef->getEntity(entityId);
                if (!entity)
                {
                    LOG_ERROR("StandardComponent Setter", "Entity not found: " << entityId);
                    return INT_VAL(0);
                }

                // Get the component registry
                auto* registry = ecsRef->getComponentRegistry();
                if (!registry)
                {
                    LOG_ERROR("StandardComponent Setter", "Component registry not found");
                    return INT_VAL(0);
                }

                // Retrieve the StandardComponent owner
                auto* owner = registry->retrieveStandardComponent(compTypeName);
                if (!owner)
                {
                    LOG_ERROR("StandardComponent Setter", "Component type '" << compTypeName << "' not found in registry");
                    return INT_VAL(0);
                }

                // Get the actual component instance
                StandardComponent* comp = owner->getComponent(entityId);
                if (!comp)
                {
                    LOG_ERROR("StandardComponent Setter", "StandardComponent '" << compTypeName << "' not found on entity " << entityId);
                    return INT_VAL(0);
                }

                // Convert the VM value to ElementType
                ElementType newValue = vm->valueToElement(args[0]);

                // Check if the value actually changed
                bool valueChanged = false;
                if (comp->has(propName))
                {
                    ElementType oldValue = comp->properties[propName];
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
                    comp->setWithEvent(propName, newValue);

                    // Also update the VM table so the script sees the change immediately
                    if (propertiesTable->fields.find(propName) != propertiesTable->fields.end())
                    {
                        vm->releaseAndDelete(propertiesTable->fields[propName]);
                    }
                    propertiesTable->fields[propName] = vm->retainValue(args[0]);
                }

                return INT_VAL(0);
            });

            // Add the setter to the component table
            auto globalIt = vm->globals.find(globalSetterName);
            if (globalIt != vm->globals.end())
            {
                table->fields[methodName] = globalIt->second;
                LOG_INFO("ECS Serialization", "Added setter method: " << methodName);
            }
        }

        // Add generic set(propertyName, value) method
        std::string genericSetterName = "__genericSetter_" + std::to_string(entityId) + "_" + compTypeName;

        vm->registerNative(genericSetterName, [ecsRef, entityId, compTypeName, propertiesTable](VM* vm, int argCount, Value* args) -> Value {
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

            std::string propName = vm->asString(args[0])->toString();

            // Retrieve the entity
            Entity* entity = ecsRef->getEntity(entityId);
            if (!entity)
            {
                LOG_ERROR("StandardComponent Generic Setter", "Entity not found: " << entityId);
                return INT_VAL(0);
            }

            // Get the component registry
            auto* registry = ecsRef->getComponentRegistry();
            if (!registry)
            {
                LOG_ERROR("StandardComponent Generic Setter", "Component registry not found");
                return INT_VAL(0);
            }

            // Retrieve the StandardComponent owner
            auto* owner = registry->retrieveStandardComponent(compTypeName);
            if (!owner)
            {
                LOG_ERROR("StandardComponent Generic Setter", "Component type '" << compTypeName << "' not found in registry");
                return INT_VAL(0);
            }

            // Get the actual component instance
            StandardComponent* comp = owner->getComponent(entityId);
            if (!comp)
            {
                LOG_ERROR("StandardComponent Generic Setter", "StandardComponent '" << compTypeName << "' not found on entity " << entityId);
                return INT_VAL(0);
            }

            // Convert the VM value to ElementType
            ElementType newValue = vm->valueToElement(args[1]);

            // Check if the value actually changed
            bool valueChanged = false;
            if (comp->has(propName))
            {
                ElementType oldValue = comp->properties[propName];
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
                comp->setWithEvent(propName, newValue);

                // Also update the VM table so the script sees the change immediately
                if (propertiesTable->fields.find(propName) != propertiesTable->fields.end())
                {
                    vm->releaseAndDelete(propertiesTable->fields[propName]);
                }
                propertiesTable->fields[propName] = vm->retainValue(args[1]);
            }

            return INT_VAL(0);
        });

        // Add the generic setter to the component table
        auto genericIt = vm->globals.find(genericSetterName);
        if (genericIt != vm->globals.end())
        {
            table->fields["set"] = genericIt->second;
            LOG_INFO("ECS Serialization", "Added generic set() method");
        }
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
                LOG_INFO("ECS Serialization", "Using registered serializer for " << componentTypeName);

                // Create a temporary component object from the archive data to pass to the serializer
                // The serializer needs the component's ecsRef and entity ID for generating setters
                // For now, we'll handle the known types explicitly
                // TODO: Make this more generic with a component factory in the registry
                if (componentTypeName == "PositionComponent")
                {
                    PositionComponent* posComp = ecsRef->getComponent<PositionComponent>(entity->id);
                    if (posComp)
                    {
                        auto serializerFunc = registry.getSerializer(componentTypeName);
                        serializerFunc(vm, table, (void*)posComp);
                    }
                }
                else if (componentTypeName == "Texture2DComponent")
                {
                    Texture2DComponent* texComp = ecsRef->getComponent<Texture2DComponent>(entity->id);
                    if (texComp)
                    {
                        auto serializerFunc = registry.getSerializer(componentTypeName);
                        serializerFunc(vm, table, (void*)texComp);
                    }
                }
                else if (componentTypeName == "StandardComponent")
                {
                    // For StandardComponent, we need to get it through the component registry
                    // Extract the type name from the table
                    std::string compTypeName;
                    auto typeNameIt = table->fields.find("typeName");
                    if (typeNameIt != table->fields.end() && IS_STRING(typeNameIt->second))
                    {
                        compTypeName = vm->asString(typeNameIt->second)->toString();
                        auto* owner = ecsRef->getComponentRegistry()->retrieveStandardComponent(compTypeName);
                        if (owner)
                        {
                            StandardComponent* stdComp = owner->getComponent(entity->id);
                            if (stdComp)
                            {
                                auto serializerFunc = registry.getSerializer(componentTypeName);
                                serializerFunc(vm, table, (void*)stdComp);
                            }
                        }
                    }
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

    bool deserializeComponentFromTable(VM* vm, EntitySystem* ecsRef, EntityRef entity,
        Value componentTable, const std::string& componentTypeName)
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
                    typeName = vm->asString(it->second)->toString();
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
        // First, use the basic serialization helper to create the table
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

    Value serializeToTable(VM* vm, const PositionComponent& component)
    {
        // Use the template version to create the basic table
        // This calls the generic template that handles serialization
        Value tableValue = serializeToTable<PositionComponent>(vm, component);
        ObjInstance* table = vm->asInstance(tableValue);

        // Use the registered serializer to add setters
        auto& registry = ComponentSerializerRegistry::instance();
        if (registry.hasSerializer("PositionComponent"))
        {
            auto serializerFunc = registry.getSerializer("PositionComponent");
            serializerFunc(vm, table, (void*)&component);
        }

        return tableValue;
    }
}