#pragma once

#include "entitysystem.h"

#include "Compiler/native_module.h"
#include "Compiler/vm.h"
#include "Compiler/ecsserialization.h"

namespace pg
{
    /**
     * @brief ECS Module for compiled scripts (VM-based)
     *
     * Provides native functions for working with the ECS from scripts:
     *
     * - createEntity(name?): Create a new entity with an optional name, returns entity table
     * - removeEntity(entityId or entityInstance): Remove an entity from the ECS
     * - getEntity(entityId): Get entity data as a table
     * - getEntities(componentName): Get all entities with the specified component as a table of entity tables
     * - sendEvent(eventName, key1, value1, key2, value2, ...): Send an event
     * - attachComponent(entityId or entityInstance, componentName, prop1, value1, prop2, value2, ...):
     *   Attach a StandardComponent to an entity with properties
     * - createSystem(systemName): Create a new StandardSystem builder (script-based system creation)
     *
     * StandardSystem Builder API (returned by createSystem):
     * - ownComponent(componentName): Declare a component owned by this system (chainable)
     * - onInit(scriptPath): Set initialization script (runs once on system startup) (chainable)
     * - onExecute(scriptPath): Set execute script (runs every frame) (chainable)
     * - onDelta(scriptPath): Set delta script (runs every frame with deltaTime parameter) (chainable)
     * - onEvent(eventName, scriptPath): Set event handler script for specific event (chainable)
     * - build(): Finalize and register the system with ECS (returns success boolean)
     *
     * Example usage:
     * @code
     * // ========== Basic ECS Operations ==========
     * import ecs
     *
     * // Get an entity
     * var player = ecs.getEntity(playerId)
     *
     * // Attach a Health component with initial values
     * ecs.attachComponent(player, "Health", "current", 100, "max", 100)
     *
     * // Attach a Position component
     * ecs.attachComponent(playerId, "Position", "x", 10.0, "y", 20.0)
     *
     * // Send an event
     * ecs.sendEvent("PlayerDamaged", "entityId", playerId, "damage", 25)
     *
     * // Remove an entity
     * ecs.removeEntity(playerId)
     *
     * // Get all entities with a specific component
     * var allPlayers = ecs.getEntities("Player")
     * for (var player : allPlayers) {
     *     __dprint("Player entity: " + player.__entityId)
     * }
     *
     * // ========== Creating Systems from Scripts ==========
     *
     * // Create a player management system
     * var playerSystem = ecs.createSystem("PlayerSystem")
     *     .ownComponent("Player")          // This system owns the Player component
     *     .ownComponent("Health")          // And the Health component
     *     .onInit("res/scripts/player_init.pg")     // Initialize system (runs once)
     *     .onEvent("KeyPress", "res/scripts/player_input.pg")  // Handle keyboard input
     *     .onEvent("PlayerDamaged", "res/scripts/player_damage.pg")  // Handle damage
     *     .onDelta("res/scripts/player_update.pg")  // Update every frame
     *     .build()  // Finalize and register
     *
     * // Create a bullet system
     * var bulletSystem = ecs.createSystem("BulletSystem")
     *     .ownComponent("Bullet")
     *     .onInit("res/scripts/bullet_init.pg")
     *     .onEvent("SpawnBullet", "res/scripts/spawn_bullet.pg")
     *     .onDelta("res/scripts/update_bullets.pg")
     *     .build()
     *
     * // ========== Inside System Scripts (e.g., player_update.pg) ==========
     * // System scripts have access to 'sys' module
     * import sys
     *
     * // Get all entities with the Player component
     * var players = sys.getEntities("Player")
     *
     * for (var player : players) {
     *     // Access entity ID
     *     __dprint("Updating player: " + player.__entityId)
     *
     *     // Access components on the entity
     *     var health = player["Health"]
     *     var position = player["Position"]
     *
     *     // Modify component properties
     *     position.x = position.x + 1.0
     *     health.current = health.current - 1
     *
     *     // Send events
     *     if (health.current <= 0) {
     *         ecs.sendEvent("PlayerDied", "entityId", player.__entityId)
     *     }
     * }
     *
     * // ========== Complete Example: Game System Setup Script ==========
     * import ecs
     *
     * // Setup all game systems
     * ecs.createSystem("PlayerSystem")
     *     .ownComponent("Player")
     *     .onInit("res/init_player.pg")
     *     .onEvent("OnSDLScanCode", "res/move_player.pg")
     *     .onEvent("PlayerHit", "res/handle_player_hit.pg")
     *     .onDelta("res/update_player.pg")
     *     .build()
     *
     * ecs.createSystem("EnemySystem")
     *     .ownComponent("Enemy")
     *     .onInit("res/init_enemies.pg")
     *     .onEvent("SpawnEnemy", "res/spawn_enemy.pg")
     *     .onDelta("res/update_enemies.pg")
     *     .build()
     *
     * ecs.createSystem("ScoreSystem")
     *     .onInit("res/init_score.pg")
     *     .onEvent("ScoreUpdate", "res/update_score.pg")
     *     .onEvent("ScoreReset", "res/reset_score.pg")
     *     .build()
     * @endcode
     */
    struct EcsCompiledModule : public NativeModule
    {
        EntitySystem *ecsRef = nullptr;

        virtual void init(VM* vm) const override
        {
            LOG_MILE("EcsCompiledModule", "Initializing ECS module - creating __StandardSysClass");

            auto klass = vm->createClass("__StandardSysClass");

            // Capture ecsRef for use in methods
            auto ecsRefCopy = ecsRef;

            // Add methods to the StandardSysClass for building systems

            // ownComponent(componentName) - adds a component to the system
            vm->addNativeMethod(klass, "ownComponent", [](VM* vm, int argCount, Value* args) -> Value {
                if (argCount < 2)
                {
                    throw std::runtime_error("ownComponent expects at least 1 argument (componentName)");
                }

                ObjInstance* self = vm->asInstance(args[0]);

                if (!IS_STRING(args[1]))
                {
                    throw std::runtime_error("ownComponent expects component name to be a string");
                }

                auto componentName = vm->asString(args[1]);

                // Get or create the components vector
                Value componentsVec = self->getField("__components");
                ObjVector* components = vm->asVector(componentsVec);

                // Add component name to the list
                components->fields.push_back(vm->createString(componentName));

                LOG_MILE("StandardSysClass", "Added component '" << componentName << "' to system");

                // Return self for chaining
                return vm->retainValue(args[0]);
            });

            // onInit(scriptPath) - sets the initialization script path
            vm->addNativeMethod(klass, "onInit", [](VM* vm, int argCount, Value* args) -> Value {
                if (argCount < 2)
                {
                    throw std::runtime_error("onInit expects 1 argument (scriptPath string)");
                }

                ObjInstance* self = vm->asInstance(args[0]);

                if (not IS_STRING(args[1]))
                {
                    throw std::runtime_error("onInit expects a string script path");
                }

                // Store the script path
                self->setField("__initScript", vm->retainValue(args[1]));

                LOG_MILE("StandardSysClass", "Set init script: " << vm->asString(args[1]));

                // Return self for chaining
                return vm->retainValue(args[0]);
            });

            // onExecute(scriptPath) - sets the execute script (runs every frame)
            vm->addNativeMethod(klass, "onExecute", [](VM* vm, int argCount, Value* args) -> Value {
                if (argCount < 2)
                {
                    throw std::runtime_error("onExecute expects 1 argument (scriptPath string)");
                }

                ObjInstance* self = vm->asInstance(args[0]);

                if (not IS_STRING(args[1]))
                {
                    throw std::runtime_error("onExecute expects a string script path");
                }

                // Store the script path
                self->setField("__executeScript", vm->retainValue(args[1]));

                LOG_MILE("StandardSysClass", "Set execute script: " << vm->asString(args[1]));

                // Return self for chaining
                return vm->retainValue(args[0]);
            });

            // onDelta(scriptPath) - sets the delta script (runs every frame with deltaTime)
            vm->addNativeMethod(klass, "onDelta", [](VM* vm, int argCount, Value* args) -> Value {
                if (argCount < 2)
                {
                    throw std::runtime_error("onDelta expects 1 argument (scriptPath string)");
                }

                ObjInstance* self = vm->asInstance(args[0]);

                if (not IS_STRING(args[1]))
                {
                    throw std::runtime_error("onDelta expects a string script path");
                }

                // Store the script path
                self->setField("__deltaScript", vm->retainValue(args[1]));

                LOG_MILE("StandardSysClass", "Set delta script: " << vm->asString(args[1]));

                // Return self for chaining
                return vm->retainValue(args[0]);
            });

            // onEvent(eventName, scriptPath) - sets a script for a specific event
            vm->addNativeMethod(klass, "onEvent", [](VM* vm, int argCount, Value* args) -> Value {
                if (argCount < 3)
                {
                    throw std::runtime_error("onEvent expects 2 arguments (eventName, scriptPath)");
                }

                ObjInstance* self = vm->asInstance(args[0]);

                if (not IS_STRING(args[1]))
                {
                    throw std::runtime_error("onEvent expects event name to be a string");
                }

                if (not IS_STRING(args[2]))
                {
                    throw std::runtime_error("onEvent expects a string script path");
                }

                auto eventName = vm->asString(args[1]);
                auto scriptPath = vm->asString(args[2]);

                // Get or create the event scripts vector
                Value eventScriptsVec = self->getField("__eventScripts");
                ObjVector* eventScripts = vm->asVector(eventScriptsVec);

                // Store as a pair: [eventName, scriptPath]
                Value pair = vm->createVector();
                ObjVector* pairVec = vm->asVector(pair);
                pairVec->fields.push_back(vm->createString(eventName));
                pairVec->fields.push_back(vm->createString(scriptPath));

                eventScripts->fields.push_back(pair);

                LOG_MILE("StandardSysClass", "Set event script for '" << eventName << "': " << scriptPath);

                // Return self for chaining
                return vm->retainValue(args[0]);
            });

            // onProcessEvent(eventName, scriptPath) - sets a script for a specific event
            vm->addNativeMethod(klass, "onProcessEvent", [](VM* vm, int argCount, Value* args) -> Value {
                if (argCount < 3)
                {
                    throw std::runtime_error("onEvent expects 2 arguments (eventName, scriptPath)");
                }

                ObjInstance* self = vm->asInstance(args[0]);

                if (not IS_STRING(args[1]))
                {
                    throw std::runtime_error("onEvent expects event name to be a string");
                }

                if (not IS_STRING(args[2]))
                {
                    throw std::runtime_error("onEvent expects a string script path");
                }

                auto eventName = vm->asString(args[1]);
                auto scriptPath = vm->asString(args[2]);

                // Get or create the event scripts vector
                Value eventScriptsVec = self->getField("__eventDeferedScripts");
                ObjVector* eventScripts = vm->asVector(eventScriptsVec);

                // Store as a pair: [eventName, scriptPath]
                Value pair = vm->createVector();
                ObjVector* pairVec = vm->asVector(pair);
                pairVec->fields.push_back(vm->createString(eventName));
                pairVec->fields.push_back(vm->createString(scriptPath));

                eventScripts->fields.push_back(pair);

                LOG_MILE("StandardSysClass", "Set event script for '" << eventName << "': " << scriptPath);

                // Return self for chaining
                return vm->retainValue(args[0]);
            });

            // build() - finalizes and registers the system with the ECS
            vm->addNativeMethod(klass, "build", [ecsRefCopy](VM* vm, int argCount, Value* args) -> Value {
                if (argCount < 1)
                {
                    throw std::runtime_error("build expects to be called on a system instance");
                }

                ObjInstance* self = vm->asInstance(args[0]);

                // Extract system configuration from the instance fields
                std::string systemName = vm->asString(self->getField("__systemName"));

                ObjVector* componentsVec = vm->asVector(self->getField("__components"));
                // ObjVector* eventsVec = vm->asVector(self->fields["__events"]);

                // Convert component names to vector
                std::vector<std::string> componentNames;
                for (Value compNameVal : componentsVec->fields)
                {
                    componentNames.push_back(vm->asString(compNameVal));
                }

                // Get script paths
                std::string initScript = vm->asString(self->getField("__initScript"));
                std::string executeScript = vm->asString(self->getField("__executeScript"));
                std::string deltaScript = vm->asString(self->getField("__deltaScript"));

                // Build event script map
                _S_EventScriptMap eventScriptMap;
                ObjVector* eventScripts = vm->asVector(self->getField("__eventScripts"));

                for (Value pairVal : eventScripts->fields)
                {
                    ObjVector* pair = vm->asVector(pairVal);
                    std::string eventName = vm->asString(pair->fields[0]);
                    std::string scriptPath = vm->asString(pair->fields[1]);

                    eventScriptMap[eventName] = scriptPath;
                }

                // Build event script map
                _S_EventScriptMap eventDeferedScriptMap;
                ObjVector* eventDeferedScripts = vm->asVector(self->getField("__eventDeferedScripts"));

                for (Value pairVal : eventDeferedScripts->fields)
                {
                    ObjVector* pair = vm->asVector(pairVal);
                    std::string eventName = vm->asString(pair->fields[0]);
                    std::string scriptPath = vm->asString(pair->fields[1]);

                    eventDeferedScriptMap[eventName] = scriptPath;
                }

                // Create the StandardSystemImpl
                auto* systemImpl = new StandardSystemImpl(
                    systemName,
                    componentNames,
                    {}, // defaultComponentValues - empty for now
                    false, // saveLoadEnabled
                    nullptr, // initCallback
                    initScript, // initScriptPath
                    {}, // eventCallbackMap - empty, using scripts instead
                    eventScriptMap, // eventScriptMap
                    eventDeferedScriptMap, // deferredEventScriptMap
                    nullptr, // executeCallback
                    executeScript, // executeScriptPath
                    nullptr, // saveCallback
                    nullptr, // loadCallback
                    nullptr, // firstLoadCallback
                    nullptr, // deltaCallback
                    deltaScript // deltaScriptPath
                );

                // Register the system with the ECS
                ecsRefCopy->registerSystem(systemImpl);

                // Store the system impl pointer in the instance using custom ptr
                Value systemImplPtr = vm->createCustomPtr(systemImpl);
                self->setField("__systemImpl", systemImplPtr);

                LOG_INFO("StandardSysClass", "Built and registered system '" << systemName << "' with "
                         << componentNames.size() << " components");

                return makeBoolValue(true);
            });

            vm->defineGlobal("__StandardSysClass", vm->retainValue(klass));

            LOG_MILE("EcsCompiledModule", "__StandardSysClass registered in VM globals");
        }

        EcsCompiledModule(EntitySystem *ecsRef) : ecsRef(ecsRef)
        {
            LOG_THIS_MEMBER("Ecs Compiled Module");

            // Capture ecsRef by value instead of capturing 'this' to avoid dangling pointer
            auto ecsRefCopy = ecsRef;
            addNativeFunction("removeEntity", [ecsRefCopy](VM* vm, int argCount, Value* args) -> Value {
                if (argCount != 1)
                {
                    throw std::runtime_error("removeEntity expects exactly 1 argument");
                }

                if (IS_INT(args[0]))
                {
                    ecsRefCopy->removeEntity(AS_INT(args[0]));
                    return args[0];
                }
                else if (IS_INSTANCE(args[0]))
                {
                    auto instance = vm->asInstance(args[0]);

                    if (!instance->hasField("__entityId"))
                    {
                        throw std::runtime_error("removeEntity expects an entity with an __entityId field");
                    }

                    auto idValue = instance->getField("__entityId");

                    if (not IS_INT(idValue))
                    {
                        throw std::runtime_error("removeEntity expects an entity with an integer __entityId field");
                    }

                    auto entityId = AS_INT(idValue);

                    LOG_MILE("Ecs Compiled Module", "Removing entity with id " << entityId);

                    ecsRefCopy->removeEntity(entityId);
                    return args[0];
                }

                throw std::runtime_error("removeEntity expects an integer id or an entity instance");
            });

            addNativeFunction("createEntity", [ecsRefCopy](VM* vm, int argCount, Value* args) -> Value {
                if (argCount > 1)
                {
                    throw std::runtime_error("createEntity expects 0 or 1 arguments (optional entity name)");
                }

                EntityRef entityRef;

                if (argCount == 1)
                {
                    if (not IS_STRING(args[0]))
                    {
                        throw std::runtime_error("createEntity expects entity name to be a string");
                    }

                    auto entityName = vm->asString(args[0]);
                    entityRef = ecsRefCopy->createEntity(entityName);

                    LOG_MILE("Ecs Compiled Module", "Created entity with name '" << entityName << "' and id " << entityRef->id);
                }
                else
                {
                    entityRef = ecsRefCopy->createEntity();
                    LOG_MILE("Ecs Compiled Module", "Created entity with id " << entityRef->id);
                }

                // Return the serialized entity table (similar to getEntity)
                return serializeEntityToTable(vm, ecsRefCopy, entityRef.entity);
            });

            addNativeFunction("getEntity", [ecsRefCopy](VM* vm, int argCount, Value* args) -> Value {
                if (argCount != 1)
                {
                    throw std::runtime_error("getEntity expects exactly 1 argument");
                }

                if (!IS_INT(args[0]))
                {
                    throw std::runtime_error("getEntity expects an integer id");
                }

                auto entityId = AS_INT(args[0]);
                auto entity = ecsRefCopy->getEntity(entityId);

                if (not entity)
                {
                    throw std::runtime_error("Entity with id " + std::to_string(entityId) + " not found");
                }

                return serializeEntityToTable(vm, ecsRefCopy, entity);
            });

            addNativeFunction("sendEvent", [ecsRefCopy](VM* vm, int argCount, Value* args) -> Value {
                if (argCount < 1)
                {
                    throw std::runtime_error("sendEvent expects at least 1 argument (event name)");
                }

                if (!IS_STRING(args[0]))
                {
                    throw std::runtime_error("sendEvent expects first argument to be a string (event name)");
                }

                auto eventName = vm->asString(args[0]);
                StandardEvent event(eventName);

                // Process remaining arguments as key-value pairs
                for (int i = 1; i < argCount; i += 2)
                {
                    if (i + 1 >= argCount)
                    {
                        throw std::runtime_error("sendEvent expects key-value pairs after event name");
                    }

                    if (!IS_STRING(args[i]))
                    {
                        throw std::runtime_error("sendEvent expects string keys");
                    }

                    auto key = vm->asString(args[i]);
                    auto value = args[i + 1];

                    // Convert Value to ElementType
                    if (IS_INT(value))
                    {
                        event.values[key] = ElementType{static_cast<int>(AS_INT(value))};
                    }
                    else if (IS_DOUBLE(value))
                    {
                        event.values[key] = ElementType{AS_DOUBLE(value)};
                    }
                    else if (IS_STRING(value))
                    {
                        event.values[key] = ElementType{vm->asString(value)};
                    }
                    else if (IS_BOOL(value))
                    {
                        event.values[key] = ElementType{AS_BOOL(value)};
                    }
                    else
                    {
                        throw std::runtime_error("sendEvent: unsupported value type for key " + key);
                    }
                }

                LOG_MILE("Ecs Compiled Module", "Sending event: " << eventName);
                ecsRefCopy->sendEvent(event);

                return makeIntValue(0);
            });

            addNativeFunction("attachComp", [ecsRefCopy](VM* vm, int argCount, Value* args) -> Value {
                if (argCount < 2)
                {
                    throw std::runtime_error("attachComponent expects at least 2 arguments (entityId, componentName)");
                }

                // Get entity ID (can be either int or entity table)
                _unique_id entityId;

                if (IS_INT(args[0]))
                {
                    entityId = AS_INT(args[0]);
                }
                else if (IS_INSTANCE(args[0]))
                {
                    auto instance = vm->asInstance(args[0]);

                    if (!instance->hasField("__entityId"))
                    {
                        throw std::runtime_error("attachComponent: entity instance must have __entityId field");
                    }

                    auto idValue = instance->getField("__entityId");

                    if (not IS_INT(idValue))
                    {
                        throw std::runtime_error("attachComponent: __entityId must be an integer");
                    }

                    entityId = AS_INT(idValue);
                }
                else
                {
                    throw std::runtime_error("attachComponent expects first argument to be entity ID (int) or entity instance");
                }

                // Get component name
                if (not IS_STRING(args[1]))
                {
                    throw std::runtime_error("attachComponent expects second argument to be component name (string)");
                }

                auto componentName = vm->asString(args[1]);

                // Get the entity
                auto entity = ecsRefCopy->getEntity(entityId);

                if (not entity)
                {
                    throw std::runtime_error("attachComponent: entity with ID " + std::to_string(entityId) + " not found");
                }

                // Build the properties for the StandardComponent from key-value pairs
                std::vector<std::pair<std::string, ElementType>> properties;

                // Process remaining arguments as key-value pairs for component properties
                for (int i = 2; i < argCount; i += 2)
                {
                    if (i + 1 >= argCount)
                    {
                        throw std::runtime_error("attachComponent expects key-value pairs for component properties");
                    }

                    if (not IS_STRING(args[i]))
                    {
                        throw std::runtime_error("attachComponent expects string keys for properties");
                    }

                    auto key = vm->asString(args[i]);
                    auto value = args[i + 1];

                    // Convert Value to ElementType
                    ElementType elementValue;
                    if (IS_INT(value))
                    {
                        elementValue = ElementType{static_cast<int>(AS_INT(value))};
                    }
                    else if (IS_DOUBLE(value))
                    {
                        elementValue = ElementType{AS_DOUBLE(value)};
                    }
                    else if (IS_STRING(value))
                    {
                        elementValue = ElementType{vm->asString(value)};
                    }
                    else if (IS_BOOL(value))
                    {
                        elementValue = ElementType{AS_BOOL(value)};
                    }
                    else
                    {
                        throw std::runtime_error("attachComponent: unsupported value type for property '" + key + "'");
                    }

                    properties.push_back({key, elementValue});
                }

                // Attach the StandardComponent with properties
                try
                {
                    // Create the component with the first property if it exists
                    CompRef<StandardComponent> component;
                    if (properties.empty())
                    {
                        component = ecsRefCopy->attach(entity, componentName);
                    }
                    else
                    {
                        component = ecsRefCopy->attach(entity, componentName,
                            properties[0].first, properties[0].second);

                        // Add remaining properties
                        for (size_t i = 1; i < properties.size(); i++)
                        {
                            component->properties[properties[i].first] = properties[i].second;
                        }
                    }

                    LOG_MILE("Ecs Compiled Module", "Attached StandardComponent '" << componentName
                             << "' to entity " << entityId << " with " << properties.size() << " properties");

                    return makeBoolValue(true);
                }
                catch (const std::exception& e)
                {
                    throw std::runtime_error("attachComponent: failed to attach component '" + componentName + "': " + e.what());
                }
            });

            addNativeFunction("createSystem", [](VM* vm, int argCount, Value* args) -> Value {
                if (argCount < 1)
                {
                    throw std::runtime_error("createSystem expects at least 1 argument (systemName)");
                }

                if (!IS_STRING(args[0]))
                {
                    throw std::runtime_error("createSystem expects first argument to be system name (string)");
                }

                auto systemName = vm->asString(args[0]);

                // Get the StandardSysClass
                VM::GlobalCell* cell = vm->findGlobalCell("__StandardSysClass");
                if (cell == nullptr or not cell->defined)
                {
                    throw std::runtime_error("Standard sys class not found in VM globals");
                }

                Klass* standardSysKlass = vm->asClass(cell->value);

                // Create instance of __StandardSysClass
                auto inst = vm->createInstance(standardSysKlass);
                ObjInstance* systemInstance = vm->asInstance(inst);

                // Store system name
                systemInstance->setField("__systemName", vm->createString(systemName));

                // Create lists to store component names and event names
                Value componentsVector = vm->createVector();
                Value eventsVector = vm->createVector();

                systemInstance->setField("__components", componentsVector);
                systemInstance->setField("__events", eventsVector);

                // Create fields for script paths (stored as strings)
                systemInstance->setField("__initScript", vm->createString(""));
                systemInstance->setField("__executeScript", vm->createString(""));
                systemInstance->setField("__deltaScript", vm->createString(""));
                systemInstance->setField("__eventScripts", vm->createVector()); // Vector of [eventName, scriptPath] pairs
                systemInstance->setField("__eventDeferedScripts", vm->createVector()); // Vector of [eventName, scriptPath] pairs

                LOG_MILE("Ecs Compiled Module", "Created system builder for '" << systemName << "'");

                return inst;
            });

            addNativeFunction("getEntities", [ecsRefCopy](VM* vm, int argCount, Value* args) -> Value {
                if (argCount != 1)
                {
                    throw std::runtime_error("getEntities expects exactly 1 argument (componentName)");
                }

                if (!IS_STRING(args[0]))
                {
                    throw std::runtime_error("getEntities expects a string component name");
                }

                auto componentName = vm->asString(args[0]);

                // Create a vector to hold all entity tables
                Value vectorValue = vm->createVector();
                ObjVector* vector = vm->asVector(vectorValue);

                // Get the component owner from the registry
                auto* owner = ecsRefCopy->getComponentRegistry()->retrieveStandardComponent(componentName);

                if (!owner)
                {
                    throw std::runtime_error("getEntities: Standard Component '" + componentName + "' not found in ECS");
                }

                // Get all components of this type from the owner
                auto componentView = owner->view();

                for (auto* component : componentView)
                {
                    // Get the full entity with ALL components, not just the requested one
                    auto* entity = ecsRefCopy->getEntity(component->entityId);
                    if (entity)
                    {
                        // Serialize the complete entity with all its components
                        Value entityTable = serializeEntityToTable(vm, ecsRefCopy, entity);
                        vector->fields.push_back(vm->retainValue(entityTable));
                    }
                }

                LOG_MILE("Ecs Compiled Module", "getEntities(\"" << componentName << "\") returned "
                         << vector->fields.size() << " entities");

                return vectorValue;
            });

        }
    };
}