#include "ecsserialization.h"

#include "ECS/entitysystem.h"

#include <iostream>

namespace pg
{
    namespace detail
    {
        // Todo maybe even make this whole registrar thing constexpr
        bool registryHasComponent(const std::string& name)
        {
            auto& registry = ComponentSerializerRegistry::instance();
            return not name.empty() and registry.hasSerializer(name);
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
                if (not IS_STRING(args[0]))
                {
                    throw std::runtime_error("attachComp expects first argument to be component name (string)");
                }
                auto componentName = vm->asString(args[0]);

                // Check if there's a registered custom attach handler for this component
                auto& attachRegistry = ComponentAttachRegistry::instance();
                if (attachRegistry.hasHandler(componentName))
                {
                    auto handler = attachRegistry.getHandler(componentName);
                    // Pass all arguments AFTER the component name to the handler
                    bool success = handler(vm, ecsRef, entityPtr, argCount - 1, args + 1);
                    return makeBoolValue(success);
                }

                // Default behavior: Attach the StandardComponent (create empty first)
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

    namespace
    {
        static void* getRawComponentPtr(EntitySystem* ecs, _unique_id entityId, const std::string& typeName)
        {
            auto& reg = ComponentSerializerRegistry::instance();

            if (reg.hasSerializer(typeName))
            {
                auto fn = reg.getRetriever(typeName);
                if (fn)
                    return fn(ecs, entityId);
            }

            auto* owner = ecs->getComponentRegistry()->retrieveStandardComponent(typeName);

            if (owner)
                return owner->getComponent(entityId);

            return nullptr;
        }
    }

    // ============================================================================
    // Registered Component Serializers
    // ============================================================================

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
        // _unique_id entityId = component->entityId;

        // Get the properties table
        auto propVal = table->getField("properties");

        if ((not table->hasField("properties")) or not IS_INSTANCE(propVal))
        {
            LOG_WARNING("ECS Serialization", "No properties table found for StandardComponent, skipping setter generation");
            return;
        }

        ObjInstance* propertiesTable = vm->asInstance(propVal);

        // Flatten properties to the top level of the table
        std::vector<std::string> propertyNames;

        for (const auto& [key, v] : propertiesTable->internedFields)
        {
            auto value = propertiesTable->fieldValues[v];

            if (key != "__className" and not key.empty())
            {
                propertyNames.push_back(key);
                // Copy property to top level
                table->setField(key, vm->retainValue(value));
            }
        }

        LOG_MILE("ECS Serialization", "Flattened " << propertyNames.size() << " properties and generating setters for StandardComponent '" << compTypeName << "'");

        // Generate specific setters (setX, setY, setValue, etc.)
        for (const std::string& propName : propertyNames)
        {
            // Generate method name: "set" + Capitalized(propName)
            std::string methodName = "set";
            if (not propName.empty())
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
                    valueChanged = not (oldValue == newValue);
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

                    propertiesTable->setField(propName, vm->retainValue(args[0]), vm, true);
                }

                return INT_VAL(0);
            };

            // Allocate native function from pool and create Value directly
            auto [nativeFunc, funcIndex] = vm->pools.nativeFuncPool.allocateWithIndex();
            nativeFunc->function = setterFunc;

            Value setterValue = makeNativeFuncValue(static_cast<uint32_t>(funcIndex));

            // Add directly to table without going through globals
            table->setField(methodName, vm->trackNewValue(setterValue));

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

            if (not IS_STRING(args[0]))
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

                propertiesTable->setField(propName, vm->retainValue(args[1]), vm, true);
            }

            return INT_VAL(0);
        };

        // Create native function and add directly to table without going through globals
        table->setField("set", vm->createNativeFunction(genericSetterFunc));

        LOG_MILE("ECS Serialization", "Added generic set() method");
    }

    // // Register component serializers at static initialization time
    // REGISTER_COMPONENT_SERIALIZER(TTFText, serializeTTFTextWithSetters);
    REGISTER_COMPONENT_SERIALIZER(StandardComponent, serializeStandardComponentWithSetters);

    // ============================================================================
    // Public API Implementation
    // ============================================================================

    Value serializeComponentToTable(VM* vm, EntitySystem* ecsRef, const Entity* entity, _unique_id componentId)
    {
        // Get the component type name directly from the registry (no archive needed!)
        std::string componentTypeName = ecsRef->getComponentRegistry()->getComponentTypeName(componentId);

        // For StandardComponent, get the actual runtime type name
        std::string actualTypeName = componentTypeName;
        if (componentTypeName == "StandardComponent")
        {
            // Get the StandardComponent pointer directly to read its typeName field
            // This avoids creating an archive just to extract the type name
            StandardComponent* standardComp = ecsRef->getComponent<StandardComponent>(entity->id);
            if (standardComp)
            {
                actualTypeName = standardComp->typeName;
            }
        }

        // Check if this component has proxy metadata registered
        auto& proxyRegistry = ComponentProxyRegistry::instance();

        // StandardComponent types are stored under their runtime typeName in standardComponentStorageMap,
        // not under "StandardComponent".  Detect them explicitly so they can use the generic
        // "StandardComponent" proxy metadata registered in ComponentProxy::registerWithVM.
        const bool isStandardComponent = ecsRef->getComponentRegistry()->hasStandardComponent(componentTypeName);

        if (proxyRegistry.hasMetadata(componentTypeName) or
            (isStandardComponent and proxyRegistry.hasMetadata("StandardComponent")))
        {
            const std::string proxyMetaKey = proxyRegistry.hasMetadata(componentTypeName)
                ? componentTypeName
                : std::string("StandardComponent");

            LOG_MILE("ECS Serialization", "Using ComponentProxy for " << componentTypeName
                     << " (metadata key: " << proxyMetaKey << ")");

            void* componentPtr = nullptr;
            if (isStandardComponent and proxyMetaKey == "StandardComponent")
            {
                // Retrieve directly from the runtime-named standard component storage
                auto* owner = ecsRef->getComponentRegistry()->retrieveStandardComponent(componentTypeName);
                if (owner)
                    componentPtr = owner->getComponent(entity->id);
            }
            else
            {
                componentPtr = getRawComponentPtr(ecsRef, entity->id, componentTypeName);
            }

            if (componentPtr)
            {
                // Return a proxy instead of a table copy
                return ComponentProxy::createProxy(vm, proxyMetaKey, componentPtr);
            }
            else
            {
                LOG_WARNING("ECS Serialization", "Could not retrieve component pointer for proxy, falling back to table");
            }
        }

        // Fallback: Original table-based serialization (for components without proxy metadata)
        // NOW create the archive (only when needed for table serialization)
        InspectorArchive archive;
        ecsRef->getComponentRegistry()->serializeComponentFromEntity(archive, entity, componentId);

        // Get the Table class
        auto it = vm->globals.find("__Table");
        if (it == vm->globals.end())
        {
            throw std::runtime_error("Table class not found in VM globals");
        }

        Klass* tableClass = vm->asClass(it->second);

        // Create the table instance
        Value tableValue = vm->createInstance(tableClass);
        ObjInstance* table = vm->asInstance(tableValue);

        // Parse the archive and populate the table
        if (archive.mainNode.children.size() > 0)
        {
            auto& compNode = archive.mainNode.children[0];

            // Add the class name (component type)
            if (not compNode.className.empty())
            {
                componentTypeName = compNode.className;
                Value classNameValue = vm->createString(compNode.className);

                table->setField("__className", classNameValue, vm, true);
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
                void* componentPtr = getRawComponentPtr(ecsRef, entity->id, componentTypeName);

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
        entityTable->setField("__entityId", idValue);

        // Add native attachComp function that holds the entity pointer
        // This allows scripts to attach components immediately without entity lookup
        Value attachCompFuncValue = vm->createNativeFunction(detail::createAttachCompFunction(entity, ecsRef));
        entityTable->setField("attachComp", attachCompFuncValue);

        // Add native has() method to check if entity has a specific component
        auto hasFunc = [entity, ecsRef](VM* vm, int argCount, Value* args) -> Value {
            if (argCount != 1)
            {
                throw std::runtime_error("has expects exactly 1 argument (componentName)");
            }

            if (!IS_STRING(args[0]))
            {
                throw std::runtime_error("has expects a string argument (component name)");
            }

            std::string componentName = vm->asString(args[0]);

            // Check if this is a StandardComponent
            auto* standardCompOwner = ecsRef->getComponentRegistry()->retrieveStandardComponent(componentName);
            if (standardCompOwner and standardCompOwner->components.has(entity->id))
            {
                return makeBoolValue(true);
            }

            // Check other registered components by iterating through the entity's component list
            for (const auto& compRef : entity->componentList)
            {
                if (compRef.entityHeldType == Entity::EntityHeld::EntityHeldType::id)
                {
                    _unique_id componentId = compRef.getId();

                    // Use fast getComponentTypeName() instead of creating an archive!
                    std::string compTypeName = ecsRef->getComponentRegistry()->getComponentTypeName(componentId);

                    // For StandardComponent, get the actual type name
                    if (compTypeName == "StandardComponent")
                    {
                        StandardComponent* standardComp = ecsRef->getComponent<StandardComponent>(entity->id);
                        if (standardComp)
                        {
                            compTypeName = standardComp->typeName;
                        }
                    }

                    if (compTypeName == componentName)
                    {
                        return makeBoolValue(true);
                    }
                }
            }

            return makeBoolValue(false);
        };

        Value hasFuncValue = vm->createNativeFunction(hasFunc);
        entityTable->setField("has", hasFuncValue);

        // Serialize each component
        for (const auto& compRef : entity->componentList)
        {
            if (compRef.entityHeldType == Entity::EntityHeld::EntityHeldType::id)
            {
                _unique_id componentId = compRef.getId();

                // Get component type name FIRST (fast lookup, no archive needed)
                std::string componentTypeName = ecsRef->getComponentRegistry()->getComponentTypeName(componentId);

                // For StandardComponent, get the actual runtime type name
                if (componentTypeName == "StandardComponent")
                {
                    StandardComponent* standardComp = ecsRef->getComponent<StandardComponent>(entity->id);
                    if (standardComp)
                    {
                        componentTypeName = standardComp->typeName;
                    }
                }

                // Serialize the component (will return proxy for registered components)
                Value componentValue = serializeComponentToTable(vm, ecsRef, entity, componentId);

                // Add to entity table using the type name we already have
                entityTable->setField(componentTypeName, componentValue);
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
            if (table->hasField("__className"))
            {
                auto val = table->getField("__className");

                if (IS_STRING(val))
                    typeName = vm->asString(val);
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
            for (const auto& [key, v] : currentTable->internedFields)
            {
                auto value = currentTable->fieldValues[v];

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

        if (table->hasField("__entityId"))
        {
            auto val = table->getField("__entityId");

            if (IS_INT(val))
            {
                specifiedId = static_cast<_unique_id>(val);
            }
        }

        // Create or get the entity
        if (createNew or specifiedId == 0)
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
        for (const auto& [key, v] : table->internedFields)
        {
            auto value = table->fieldValues[v];

            // Skip special fields
            if (key == "__entityId" or key == "__className")
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
            entitiesTable->setField(vm->asString(indexKey), vm->retainValue(entityTableValue));
            vm->releaseAndDelete(indexKey);
            vm->releaseAndDelete(entityTableValue);

            index++;
        }

        // Add count field
        Value countKey = vm->createString("count");
        Value countValue = makeIntValue(static_cast<int64_t>(entities.size()));
        entitiesTable->setField(vm->asString(countKey), vm->retainValue(countValue));
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

        for (const auto& [key, v] : table->internedFields)
        {
            // Skip non-numeric keys and special fields
            if (key == "count" or key == "__className")
                continue;

            auto value = table->fieldValues[v];

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

    // ============================================================================
    // Component Proxy Implementation (Zero-Copy Direct Memory Access)
    // ============================================================================

    void ComponentProxy::registerWithVM(VM* vm)
    {
        // Create the ComponentProxy class
        Value klassValue = vm->createClass("ComponentProxy");

        // Add __get metamethod (handles ALL component property reads)
        vm->addNativeMethod(klassValue, "__get", [](VM* vm, int argCount, Value* args) -> Value {
            if (argCount < 2)
            {
                vm->runtimeError("__get requires 2 arguments");
                return INT_VAL(0);
            }

            ObjInstance* self = vm->asInstance(args[0]);
            std::string propName = vm->asString(args[1]);

            if ((not self->hasField("__componentPtr")) or (not self->hasField("__typeName")))
            {
                vm->runtimeError("ComponentProxy missing internal fields");
                return INT_VAL(-1);
            }

            void* componentPtr = vm->asCustomPtr<void>(self->getField("__componentPtr"));
            std::string typeName = vm->asString(self->getField("__typeName"));

            // Get metadata for this component type
            auto& registry = ComponentProxyRegistry::instance();
            if (not registry.hasMetadata(typeName))
            {
                vm->runtimeError("No metadata for component type: " + typeName);
                return INT_VAL(-1);
            }

            const ComponentProxyMetadata& metadata = registry.getMetadata(typeName);
            auto propIt = metadata.properties.find(propName);

            if (propIt == metadata.properties.end())
            {
                // Property not found in static map - try dynamic getter fallback
                if (metadata.dynamicGetter)
                    return metadata.dynamicGetter(componentPtr, propName, vm);
                return INT_VAL(-1);
            }

            const PropertyMetadata& prop = propIt->second;

            if (prop.getter)
            {
                return prop.getter(componentPtr, vm);
            }

            LOG_WARNING("ComponentProxy", "No getter function for property '" << propName << "' !");

            return INT_VAL(-1);
        });

        // Add __set metamethod (handles ALL component property writes)
        vm->addNativeMethod(klassValue, "__set", [](VM* vm, int argCount, Value* args) -> Value {
            if (argCount < 3)
            {
                vm->runtimeError("__set requires 3 arguments");
                return INT_VAL(0);
            }

            ObjInstance* self = vm->asInstance(args[0]);
            std::string propName = vm->asString(args[1]);
            Value newValue = args[2];

            if ((not self->hasField("__componentPtr")) or (not self->hasField("__typeName")))
            {
                vm->runtimeError("ComponentProxy missing internal fields");
                return newValue;
            }

            void* componentPtr = vm->asCustomPtr<void>(self->getField("__componentPtr"));
            std::string typeName = vm->asString(self->getField("__typeName"));

            // Get metadata for this component type
            auto& registry = ComponentProxyRegistry::instance();
            if (not registry.hasMetadata(typeName))
            {
                vm->runtimeError("No metadata for component type: " + typeName);
                return newValue;
            }

            const ComponentProxyMetadata& metadata = registry.getMetadata(typeName);
            auto propIt = metadata.properties.find(propName);

            if (propIt == metadata.properties.end())
            {
                // Property not found in static map - try dynamic setter fallback
                if (metadata.dynamicSetter)
                {
                    metadata.dynamicSetter(componentPtr, propName, vm, newValue);
                    return newValue;
                }
                // Property not found - just return the value
                return newValue;
            }

            const PropertyMetadata& prop = propIt->second;

            if (not prop.writable)
            {
                vm->runtimeError("Property '" + propName + "' is read-only");
                return newValue;
            }

            // If there's a setter function, use it (this calls the component's setter method which fires events!)
            if (prop.setter)
            {
                prop.setter(componentPtr, vm, newValue);
                return newValue;
            }

            LOG_WARNING("ComponentProxy", "No setter function for property '" << propName << "' !");

            return newValue;
        });

        // Store the ComponentProxy class in globals
        vm->globals["ComponentProxy"] = vm->retainValue(klassValue);

        // Register StandardComponent proxy metadata with dynamic property access.
        // This allows any StandardComponent (regardless of its runtime typeName) to be
        // accessed via the proxy system by looking up properties in the properties map.
        ComponentProxyMetadata standardMeta;
        standardMeta.componentTypeName = "StandardComponent";

        standardMeta.dynamicGetter = [](void* componentPtr, const std::string& propName, VM* vm) -> Value {
            auto* comp = static_cast<StandardComponent*>(componentPtr);
            if (comp->has(propName))
                return vm->elementToValue(comp->properties.at(propName));
            return INT_VAL(-1);
        };

        standardMeta.dynamicSetter = [](void* componentPtr, const std::string& propName, VM* vm, Value value) {
            auto* comp = static_cast<StandardComponent*>(componentPtr);
            ElementType newValue = vm->valueToElement(value);
            comp->setWithEvent(propName, newValue);
        };

        ComponentProxyRegistry::instance().registerMetadata(standardMeta);
    }

    Value ComponentProxy::createProxy(VM* vm, const std::string& typeName, void* componentPtr)
    {
        // Get the ComponentProxy class
        auto it = vm->globals.find("ComponentProxy");
        if (it == vm->globals.end())
        {
            throw std::runtime_error("ComponentProxy class not registered with VM");
        }

        Klass* proxyClass = vm->asClass(it->second);

        // Create a new proxy instance
        Value proxyInstance = vm->createInstance(proxyClass);
        ObjInstance* proxy = vm->asInstance(proxyInstance);

        // Store component pointer and type name (both use __ prefix to bypass metamethods!)
        proxy->setField("__componentPtr", vm->createCustomPtr<void>(componentPtr));
        proxy->setField("__typeName", vm->createString(typeName));
        proxy->setField("__className", vm->createString(typeName));

        return proxyInstance;
    }

}