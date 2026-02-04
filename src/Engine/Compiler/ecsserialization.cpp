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
        auto propertiesIt = table->fields.find("properties");
        if (propertiesIt == table->fields.end() or not IS_INSTANCE(propertiesIt->second))
        {
            LOG_WARNING("ECS Serialization", "No properties table found for StandardComponent, skipping setter generation");
            return;
        }

        ObjInstance* propertiesTable = vm->asInstance(propertiesIt->second);

        // Flatten properties to the top level of the table
        std::vector<std::string> propertyNames;
        for (const auto& [key, value] : propertiesTable->fields)
        {
            if (key != "__className" and not key.empty())
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

    // // Register component serializers at static initialization time
    // REGISTER_COMPONENT_SERIALIZER(TTFText, serializeTTFTextWithSetters);
    REGISTER_COMPONENT_SERIALIZER(StandardComponent, serializeStandardComponentWithSetters);

    // ============================================================================
    // Public API Implementation
    // ============================================================================

    Value serializeComponentToTable(VM* vm, EntitySystem* ecsRef, const Entity* entity, _unique_id componentId)
    {
        // Todo add a function that only get the component name without having to do the full serialization

        // Create an Archive and serialize the component to get the component type name
        InspectorArchive archive;
        ecsRef->getComponentRegistry()->serializeComponentFromEntity(archive, entity, componentId);

        std::string componentTypeName;
        if (archive.mainNode.children.size() > 0)
        {
            auto& compNode = archive.mainNode.children[0];
            componentTypeName = compNode.className;
        }

        // Check if this component has proxy metadata registered
        auto& proxyRegistry = ComponentProxyRegistry::instance();

        if (proxyRegistry.hasMetadata(componentTypeName))
        {
            LOG_MILE("ECS Serialization", "Using ComponentProxy for " << componentTypeName);

            // Get the component pointer
            void* componentPtr = nullptr;

            if (componentTypeName == "StandardComponent")
            {
                // StandardComponent requires special handling
                std::string compTypeName;
                auto& compNode = archive.mainNode.children[0];
                for (const auto& child : compNode.children)
                {
                    if (child.name == "typeName" && !child.value.empty())
                    {
                        compTypeName = child.value;
                        break;
                    }
                }

                if (!compTypeName.empty())
                {
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
                auto& registry = ComponentSerializerRegistry::instance();
                if (registry.hasSerializer(componentTypeName))
                {
                    auto retrieverFunc = registry.getRetriever(componentTypeName);
                    if (retrieverFunc)
                    {
                        componentPtr = retrieverFunc(ecsRef, entity->id);
                    }
                }
            }

            if (componentPtr)
            {
                // Return a proxy instead of a table copy
                return ComponentProxy::createProxy(vm, componentTypeName, componentPtr);
            }
            else
            {
                LOG_WARNING("ECS Serialization", "Could not retrieve component pointer for proxy, falling back to table");
            }
        }

        // Fallback: Original table-based serialization (for components without proxy metadata)
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
                    if (typeNameIt != table->fields.end() and IS_STRING(typeNameIt->second))
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

                    // Get the component type name
                    InspectorArchive archive;
                    ecsRef->getComponentRegistry()->serializeComponentFromEntity(archive, entity, componentId);

                    if (archive.mainNode.children.size() > 0)
                    {
                        auto& compNode = archive.mainNode.children[0];
                        std::string compTypeName = compNode.className;

                        // For StandardComponent, check the actual typeName
                        if (compTypeName == "StandardComponent")
                        {
                            for (const auto& child : compNode.children)
                            {
                                if (child.name == "typeName" && !child.children.empty())
                                {
                                    compTypeName = child.children[0].name;
                                    break;
                                }
                            }
                        }

                        if (compTypeName == componentName)
                        {
                            return makeBoolValue(true);
                        }
                    }
                }
            }

            return makeBoolValue(false);
        };

        Value hasFuncValue = vm->createNativeFunction(hasFunc);
        entityTable->fields["has"] = hasFuncValue;

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
                if (classNameIt != compTable->fields.end() and IS_STRING(classNameIt->second))
                {
                    componentTypeName = vm->asString(classNameIt->second);

                    // For StandardComponent, use the actual typeName instead of "StandardComponent"
                    if (componentTypeName == "StandardComponent")
                    {
                        auto typeNameIt = compTable->fields.find("typeName");
                        if (typeNameIt != compTable->fields.end() and IS_STRING(typeNameIt->second))
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
        for (const auto& [key, value] : table->fields)
        {
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
            if (key == "count" or key == "__className")
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

            // Get component pointer and type name from internal fields
            auto ptrIt = self->fields.find("__componentPtr");
            auto typeIt = self->fields.find("__typeName");

            if (ptrIt == self->fields.end() or typeIt == self->fields.end())
            {
                vm->runtimeError("ComponentProxy missing internal fields");
                return INT_VAL(0);
            }

            void* componentPtr = vm->asCustomPtr<void>(ptrIt->second);
            std::string typeName = vm->asString(typeIt->second);

            // Get metadata for this component type
            auto& registry = ComponentProxyRegistry::instance();
            if (not registry.hasMetadata(typeName))
            {
                vm->runtimeError("No metadata for component type: " + typeName);
                return INT_VAL(0);
            }

            const ComponentProxyMetadata& metadata = registry.getMetadata(typeName);
            auto propIt = metadata.properties.find(propName);

            if (propIt == metadata.properties.end())
            {
                // Property not found - return nil or 0
                return INT_VAL(0);
            }

            const PropertyMetadata& prop = propIt->second;

            if (prop.getter)
            {
                return prop.getter(componentPtr, vm);
            }

            LOG_WARNING("ComponentProxy", "No getter function for property '" << propName << "' !");

            return INT_VAL(0);
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

            // Get component pointer and type name from internal fields
            auto ptrIt = self->fields.find("__componentPtr");
            auto typeIt = self->fields.find("__typeName");

            if (ptrIt == self->fields.end() or typeIt == self->fields.end())
            {
                vm->runtimeError("ComponentProxy missing internal fields");
                return newValue;
            }

            void* componentPtr = vm->asCustomPtr<void>(ptrIt->second);
            std::string typeName = vm->asString(typeIt->second);

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

        LOG_INFO("ComponentProxy", "Registered ComponentProxy class with VM");
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
        proxy->fields["__componentPtr"] = vm->createCustomPtr<void>(componentPtr);
        proxy->fields["__typeName"] = vm->createString(typeName);
        proxy->fields["__className"] = vm->createString(typeName);

        return proxyInstance;
    }

}