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

#include "vm.h"
#include "compiler.h"
#include "ECS/entitysystem.h"
#include "Systems/coresystems.h"
#include "ecsserialization.h"
#include "2D/position.h"
#include "logger.h"
#include "Interpreter/lexer.h"

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

    // Register native logInfo function for the script
    vm.defineNative("logInfo", [](VM* vm, int argCount, Value* args) -> Value {
        if (argCount != 1) return makeBoolValue(false);

        if (IS_STRING(args[0]))
        {
            LOG_INFO("Script", vm->asString(args[0])->toString());
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

    vm.defineNative("toString", [](VM *vm, int argCount, Value* args) -> Value {
        if (argCount != 1) return makeBoolValue(false);

        std::string str;

        if (IS_STRING(args[0]))
        {
            str = vm->asString(args[0])->toString();
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

    vm.defineNative("debugTable", [](VM *vm, int argCount, Value* args) -> Value {
        if (argCount != 1) return makeBoolValue(false);

        if (IS_INSTANCE(args[0]))
        {
            ObjInstance* table = vm->asInstance(args[0]);
            LOG_INFO("Script", "Table contents:");
            for (const auto& [key, value] : table->fields)
            {
                std::string valStr;
                if (IS_STRING(value))
                    valStr = vm->asString(value)->toString();
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
        else
        {
            LOG_INFO("Script", "Value is not a table instance");
        }

        return makeBoolValue(true);
    });

    // Serialize entity to VM table
    LOG_INFO("Example", "Serializing entity to VM table...");
    Value entityTable = serializeEntityToTable(&vm, &ecs, entity.entity);

    // Pass entity table as a global to the script (like a system module)
    LOG_INFO("Example", "Adding entity as global 'playerEntity' to script...");
    Value globalName = vm.createString("playerEntity");
    vm.globals[vm.asString(globalName)->toString()] = vm.retainValue(entityTable);
    vm.releaseAndDelete(globalName);

    LOG_INFO("Example", "");
    LOG_INFO("Example", "Running script...");
    LOG_INFO("Example", "");

    // Compile and run the script
    Lexer lexer;
    // lexer.readFromText(exampleScript);
    lexer.readFromFile("entity_test.pg");
    auto tokens = lexer.getTokens();

    auto result = vm.interpret(tokens);

    if (result == InterpretResult::OK)
    {
        LOG_INFO("Example", "Script executed successfully! Results: " << vm.testOutput);

        LOG_INFO("Example", "");
        LOG_INFO("Example", "=== Reading modified values back to C++ ===");

        // Read the modified values back from the table
        if (IS_INSTANCE(entityTable))
        {
            ObjInstance* table = vm.asInstance(entityTable);
            auto posIt = table->fields.find("PositionComponent");

            if (posIt != table->fields.end() && IS_INSTANCE(posIt->second))
            {
                // Deserialize the modified PositionComponent
                PositionComponent modifiedPos = deserializeTo<PositionComponent>(&vm, posIt->second);

                LOG_INFO("Example", "Modified values from script:");
                LOG_INFO("Example", "  x: " << modifiedPos.x << " (was " << pos->x << ")");
                LOG_INFO("Example", "  y: " << modifiedPos.y << " (was " << pos->y << ")");
                LOG_INFO("Example", "");

                // Optionally apply changes back to the entity
                pos->x = modifiedPos.x;
                pos->y = modifiedPos.y;

                LOG_INFO("Example", "Changes applied to entity!");
            }
        }
    }
    else
    {
        LOG_ERROR("Example", "Script compilation/execution failed");
    }

    // Cleanup
    vm.releaseAndDelete(entityTable);

    LOG_INFO("Example", "");
    LOG_INFO("Example", "=== Example Complete ===");

    return result == InterpretResult::OK ? 0 : 1;
}
