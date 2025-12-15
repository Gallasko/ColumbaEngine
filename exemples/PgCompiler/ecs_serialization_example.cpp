/**
 * @file ecs_serialization_example.cpp
 * @brief Example showing how to pass an ECS entity as a global to a PgCompiler script
 *
 * This example demonstrates:
 * 1. Creating an entity with a PositionComponent in C++
 * 2. Serializing it to a VM table
 * 3. Passing it as a global variable to the script (like a system module)
 * 4. Accessing and reading the values in the script
 */

#include "Compiler/vm.h"
#include "Compiler/compiler.h"
#include "ECS/entitysystem.h"
#include "Systems/coresystems.h"
#include "Compiler/ecsserialization.h"
#include "2D/position.h"
#include "logger.h"
#include "Interpreter/lexer.h"

#include "Compiler/pass/long_jump_optimization_pass.h"
#include "Compiler/pass/basic_operator_local_indexing.h"

#include <chrono>

using namespace pg;

// ============================================================================
// Example script that reads entity data from a global
// ============================================================================
const char* exampleScript = R"(
// The 'playerEntity' global is provided by C++
// It's a table containing entity data

logInfo("=== Accessing Entity from Script ===");
logInfo("");

// Check if playerEntity exists
logInfo("Entity ID: " + toString(playerEntity["__entityId"]));

var resId = 0;
for (var i = 0; i < 1000000; i = i + 1) {
    // Just a loop to simulate some processing
    resId = resId + i;
}

logInfo("Computed resId: " + toString(resId));

// Access the PositionComponent
var position = playerEntity["PositionComponent"];

if (position)
{
    logInfo("Position Component found!");
    logInfo("  x: " + toString(position["x"]));
    logInfo("  y: " + toString(position["y"]));
    logInfo("  z: " + toString(position["z"]));
    logInfo("  width: " + toString(position["width"]));
    logInfo("  height: " + toString(position["height"]));
    logInfo("  rotation: " + toString(position["rotation"]));
    logInfo("  visible: " + toString(position["visible"]));

    logInfo("");
    logInfo("Modifying position values...");

    // Modify the values
    position["x"] = position["x"] + 50;
    position["y"] = position["y"] + 25;

    logInfo("New x: " + toString(position["x"]));
    logInfo("New y: " + toString(position["y"]));
}
else
{
    logInfo("Position Component not found!");
}

logInfo("");
logInfo("=== Script Complete ===");
)";

Value setupVm(VM& vm, EntitySystem* ecsRef, Entity* entity)
{
    Value entityTable = serializeEntityToTable(&vm, ecsRef, entity);

    // Pass entity table as a global to the script (like a system module)
    LOG_INFO("Example", "Adding entity as global 'playerEntity' to script...");
    vm.globals["playerEntity"] = entityTable;

    vm.registerNative("logInfo", [](VM* vm, int argCount, Value* args) -> Value {
        if (argCount != 1) return makeBoolValue(false);

        if (IS_STRING(args[0]))
        {
            LOG_INFO("Script", vm->asString(args[0]));
        }
        else if (IS_INT(args[0]))
        {
            LOG_INFO("Script", AS_INT(args[0]));
        }
        else if (IS_DOUBLE(args[0]))
        {
            LOG_INFO("Script", AS_DOUBLE(args[0]));
        }
        else if (IS_BOOL(args[0]))
        {
            LOG_INFO("Script", (AS_BOOL(args[0]) ? "true" : "false"));
        }

        return makeBoolValue(true);
    });

    vm.registerNative("toString", [](VM *vm, int argCount, Value* args) -> Value {
        if (argCount != 1) return makeBoolValue(false);

        std::string str;

        if (IS_STRING(args[0]))
        {
            str = vm->asString(args[0]);
        }
        else if (IS_INT(args[0]))
        {
            str = std::to_string(AS_INT(args[0]));
        }
        else if (IS_DOUBLE(args[0]))
        {
            str = std::to_string(AS_DOUBLE(args[0]));
        }
        else if (IS_BOOL(args[0]))
        {
            str = AS_BOOL(args[0]) ? "true" : "false";
        }
        else
        {
            str = "<unknown>";
        }

        return vm->createString(str);
    });

    vm.registerNative("debugTable", [](VM *vm, int argCount, Value* args) -> Value {
        if (argCount != 1) return makeBoolValue(false);

        if (IS_INSTANCE(args[0]))
        {
            ObjInstance* table = vm->asInstance(args[0]);
            LOG_INFO("Script", "Table contents:");
            for (const auto& [key, value] : table->fields)
            {
                std::string valStr;
                if (IS_STRING(value))
                    valStr = vm->asString(value);
                else if (IS_INT(value))
                    valStr = std::to_string(AS_INT(value));
                else if (IS_DOUBLE(value))
                    valStr = std::to_string(AS_DOUBLE(value));
                else if (IS_BOOL(value))
                    valStr = AS_BOOL(value) ? "true" : "false";
                else
                    valStr = "<complex type>";

                LOG_INFO("Script", "  " << key << " : " << valStr);
            }
        }
        else if (IS_VECTOR(args[0]))
        {
            ObjVector* vector = vm->asVector(args[0]);
            LOG_INFO("Script", "Vector contents:");
            for (size_t i = 0; i < vector->fields.size(); i++)
            {
                Value value = vector->fields[i];
                std::string valStr;
                if (IS_STRING(value))
                    valStr = vm->asString(value);
                else if (IS_INT(value))
                    valStr = std::to_string(AS_INT(value));
                else if (IS_DOUBLE(value))
                    valStr = std::to_string(AS_DOUBLE(value));
                else if (IS_BOOL(value))
                    valStr = AS_BOOL(value) ? "true" : "false";
                else
                    valStr = "<complex type>";

                LOG_INFO("Script", "  [" << i << "] : " << valStr);
            }
        }
        else
        {
            LOG_INFO("Script", "Value is not a table or a vector instance");
        }

        return makeBoolValue(true);
    });

    return entityTable;
}

// ============================================================================
// Main example function
// ============================================================================
int main(int argc, char* argv[])
{
    LOG_INFO("Example", "=== ECS Entity as Script Global Example ===");
    LOG_INFO("Example", "");

    // Setup logging
    auto terminalSink = std::shared_ptr<Logger::LogSink>(Logger::registerSink<TerminalSink>());
    terminalSink->addFilter("log", new Logger::LogSink::FilterLogLevel(Logger::InfoLevel::log));
    // terminalSink->addFilter("info", new Logger::LogSink::FilterLogLevel(Logger::InfoLevel::info));

    // Create ECS
    EntitySystem ecs;

    // Register PositionComponent system
    ecs.createSystem<PositionComponentSystem>();
    ecs.createSystem<EntityNameSystem>();

    // Create an entity with PositionComponent
    LOG_INFO("Example", "Creating entity in C++...");
    EntityRef entity = ecs.createEntity("player");
    auto pos = ecs.attach<PositionComponent>(entity);

    pos->x = 100.0f;
    pos->y = 200.0f;
    pos->z = 0.0f;
    pos->width = 64.0f;
    pos->height = 64.0f;
    pos->rotation = 0.0f;
    pos->visible = true;

    LOG_INFO("Example", "Entity ID: " << entity.id);
    LOG_INFO("Example", "Position: (" << pos->x << ", " << pos->y << ", " << pos->z << ")");
    LOG_INFO("Example", "Size: " << pos->width << " x " << pos->height);
    LOG_INFO("Example", "");

    // Create VM
    VM vm;

    vm.addOptimizationPass(std::make_unique<BasicOperatorLocalIndexingPass>());
    // vm.addOptimizationPass(std::make_unique<LongJumpOptimizationPass>());

    // Register native logInfo function for the script
    setupVm(vm, &ecs, entity.entity);

    // Serialize entity to VM table
    LOG_INFO("Example", "Serializing entity to VM table...");


    LOG_INFO("Example", "");
    LOG_INFO("Example", "Compiling script...");
    LOG_INFO("Example", "");

    // Compile the script once
    vm.interpretFromFile("entity_test.pg", true, "entity_test.pgc");

    // Resetting don't work correctly
    //vm.reset();

    // Compile and run the script
    // Lexer lexer;
    // lexer.readFromText(exampleScript);
    // lexer.readFromFile("entity_test.pg");
    // auto tokens = lexer.getTokens();

    // Run it multiple times to test performance
    InterpretResult result;
    for (int i = 0; i < 10; i++)
    {
        auto start = std::chrono::high_resolution_clock::now();

        VM testVm;

        auto entityTable = setupVm(testVm, &ecs, entity.entity);

        result = testVm.interpretFromBytecodeFile("entity_test.pgc");

        auto end = std::chrono::high_resolution_clock::now();
        auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

        LOG_INFO("Example", "Iteration " << i << " took " << duration_ns << " nanoseconds ("
                 << duration_ns / 1000000.0 << " milliseconds)");

        if (result == InterpretResult::OK)
        {
            LOG_INFO("Example", "Script executed successfully! Results: " << testVm.testOutput);

            LOG_INFO("Example", "=== Reading modified values back to C++ ===");

            // Read the modified values back from the table
            if (IS_INSTANCE(entityTable))
            {
                ObjInstance* table = testVm.asInstance(entityTable);
                auto posIt = table->fields.find("PositionComponent");

                if (posIt != table->fields.end() && IS_INSTANCE(posIt->second))
                {
                    // Deserialize the modified PositionComponent
                    PositionComponent modifiedPos = deserializeTo<PositionComponent>(&testVm, posIt->second);

                    LOG_INFO("Example", "Modified values from script, x: " << modifiedPos.x << " (was " << pos->x << ")" <<
                        "  y: " << modifiedPos.y << " (was " << pos->y << ")");

                    // Optionally apply changes back to the entity
                    pos->x = modifiedPos.x;
                    pos->y = modifiedPos.y;
                }
            }
        }
        else
        {
            LOG_ERROR("Example", "Script compilation/execution failed");
        }
        // result = vm.interpret(tokens);
    }

    auto start = std::chrono::high_resolution_clock::now();

    VM lastVm;
    auto entityTable = setupVm(lastVm, &ecs, entity.entity);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    LOG_INFO("Example", "VM init took: " << duration_ns << " nanoseconds ("
             << duration_ns / 1000000.0 << " milliseconds)");

    result = lastVm.interpretFromBytecodeFile("entity_test.pgc");

    if (result == InterpretResult::OK)
    {
        LOG_INFO("Example", "Script executed successfully! Results: " << lastVm.testOutput);

        LOG_INFO("Example", "");
        LOG_INFO("Example", "=== Reading modified values back to C++ ===");
        LOG_INFO("Example", "Last x: " << pos->x << ", Last y: " << pos->y);

        // Read the modified values back from the table
        if (IS_INSTANCE(entityTable))
        {
            auto desEnt = deserializeEntityFromTable(&lastVm, &ecs, entityTable, false);

            if (desEnt->has<PositionComponent>())
            {
                auto comp = desEnt.get<PositionComponent>();
                LOG_INFO("Example", "ID: " << desEnt.id << " X: " << comp->x << ", Y: " << comp->y);
            }
        }
    }
    else
    {
        LOG_ERROR("Example", "Script compilation/execution failed");
    }

    // Cleanup
    // vm.releaseAndDelete(entityTable);

    LOG_INFO("Example", "");
    LOG_INFO("Example", "=== Example Complete ===");

    return result == InterpretResult::OK ? 0 : 1;
}
