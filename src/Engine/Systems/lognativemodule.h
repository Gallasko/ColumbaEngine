#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

#include "Compiler/vm.h"
#include "Compiler/native_module.h"

#include "logger.h"

namespace pg
{
    /**
     * Log Module
     * Lets a script print through the engine and configure the filters of a log sink
     */
    class LogNativeModule : public NativeModule
    {
    public:
        LogNativeModule(std::shared_ptr<Logger::LogSink> sink)
        {
            addNativeFunction("log", [](VM* vm, int argCount, Value* args) -> Value {
                std::string output;

                for (int i = 0; i < argCount; ++i)
                {
                    output += vm->valueToElement(args[i]).toString();
                }

                std::cout << "[Script]: " << output << std::endl;

                return makeBoolValue(true);
            });

            addNativeFunction("addFilterScope", [sink](VM* vm, int argCount, Value* args) -> Value {
                if (argCount < 2 or argCount > 3)
                {
                    throw std::runtime_error("addFilterScope expects 2 or 3 arguments (name, scope, bypassErrors)");
                }

                if (not IS_STRING(args[0]) or not IS_STRING(args[1]))
                {
                    throw std::runtime_error("addFilterScope expects the name and the scope to be strings");
                }

                bool bypassErrors = true;

                if (argCount == 3)
                {
                    if (not IS_BOOL(args[2]))
                    {
                        throw std::runtime_error("addFilterScope expects the third argument to be a boolean (bypassErrors)");
                    }

                    bypassErrors = AS_BOOL(args[2]);
                }

                if (not sink)
                {
                    LOG_ERROR("Log Module", "Trying to add a filter to a sinkless log module");

                    return makeBoolValue(false);
                }

                sink->addFilter(vm->asString(args[0]), new Logger::LogSink::FilterScope(vm->asString(args[1]), true, bypassErrors));

                return makeBoolValue(true);
            });

            addNativeFunction("addFilterLevel", [sink](VM* vm, int argCount, Value* args) -> Value {
                const static std::unordered_map<std::string, Logger::InfoLevel> stringToLevel = {
                    {"log",   Logger::InfoLevel::log},
                    {"info",  Logger::InfoLevel::info},
                    {"mile",  Logger::InfoLevel::mile},
                    {"test",  Logger::InfoLevel::test},
                    {"error", Logger::InfoLevel::error}
                };

                if (argCount != 2)
                {
                    throw std::runtime_error("addFilterLevel expects exactly 2 arguments (name, level)");
                }

                if (not IS_STRING(args[0]) or not IS_STRING(args[1]))
                {
                    throw std::runtime_error("addFilterLevel expects the name and the level to be strings");
                }

                const auto level = vm->asString(args[1]);

                const auto it = stringToLevel.find(level);

                if (it == stringToLevel.end())
                {
                    LOG_ERROR("Log Module", "Trying to filter an unknown log level: " << level);

                    return makeBoolValue(false);
                }

                if (not sink)
                {
                    LOG_ERROR("Log Module", "Trying to add a filter to a sinkless log module");

                    return makeBoolValue(false);
                }

                sink->addFilter(vm->asString(args[0]), new Logger::LogSink::FilterLogLevel(it->second));

                return makeBoolValue(true);
            });

            addNativeFunction("setVerboseLogs", [sink](VM*, int argCount, Value* args) -> Value {
                if (argCount != 1 or not IS_BOOL(args[0]))
                {
                    throw std::runtime_error("setVerboseLogs expects exactly 1 boolean argument (enabled)");
                }

                auto terminal = dynamic_cast<TerminalSink*>(sink.get());

                if (not terminal)
                {
                    LOG_ERROR("Log Module", "Cannot set verbose logs, sink is not a TerminalSink");

                    return makeBoolValue(false);
                }

                terminal->setVerboseInfo(AS_BOOL(args[0]));

                return makeBoolValue(true);
            });
        }
    };
}
