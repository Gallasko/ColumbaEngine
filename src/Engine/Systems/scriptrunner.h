#pragma once

#include <map>
#include <queue>
#include <string>

#include "Compiler/object.h"

#include "ECS/system.h"

namespace pg
{
    /** Native functions made available to a single script run, by name */
    typedef std::map<std::string, NativeFn> ScriptFunctions;

    struct ExecuteFileScriptEvent
    {
        ExecuteFileScriptEvent(const std::string& filename) : filename(filename) {}
        ExecuteFileScriptEvent(const std::string& filename, const ScriptFunctions& functions) : filename(filename), functions(functions) {}

        std::string filename;

        ScriptFunctions functions;
    };

    struct ExecuteCodeScriptEvent
    {
        ExecuteCodeScriptEvent(const std::string& data) : data(data) {}
        ExecuteCodeScriptEvent(const std::string& data, const ScriptFunctions& functions) : data(data), functions(functions) {}

        std::string data;

        ScriptFunctions functions;
    };

    /**
     * @brief Runs the scripts requested through ExecuteFileScriptEvent and ExecuteCodeScriptEvent
     *
     * Requests are queued when the event is received and run during the next execute.
     * Every script gets its own vm, set up with the modules of the ecs.
     */
    struct ScriptRunnerSystem : public System<Listener<ExecuteFileScriptEvent>, Listener<ExecuteCodeScriptEvent>>
    {
        virtual std::string getSystemName() const override { return "Script Runner System"; }

        virtual void onEvent(const ExecuteFileScriptEvent& event) override;

        virtual void onEvent(const ExecuteCodeScriptEvent& event) override;

        virtual void execute() override;

        struct ScriptCall
        {
            bool fromFile = false;

            std::string data;

            ScriptFunctions functions;
        };

        std::queue<ScriptCall> scriptQueue;
    };
}
