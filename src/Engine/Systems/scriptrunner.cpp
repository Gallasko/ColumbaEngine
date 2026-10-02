#include "stdafx.h"

#include "scriptrunner.h"

#include "Compiler/vm.h"

#include "ECS/entitysystem.h"

#include "logger.h"

namespace pg
{
    namespace
    {
        static constexpr char const * DOM = "Script Runner System";
    }

    void ScriptRunnerSystem::onEvent(const ExecuteFileScriptEvent& event)
    {
        scriptQueue.push(ScriptCall{true, event.filename, event.functions});
    }

    void ScriptRunnerSystem::onEvent(const ExecuteCodeScriptEvent& event)
    {
        scriptQueue.push(ScriptCall{false, event.data, event.functions});
    }

    void ScriptRunnerSystem::execute()
    {
        if (scriptQueue.empty())
            return;

        // A script can request another one, those run on the next execute
        std::queue<ScriptCall> queue;

        queue.swap(scriptQueue);

        while (not queue.empty())
        {
            const auto& script = queue.front();

            VM vm;

            ecsRef->setupVm(vm);

            for (const auto& function : script.functions)
            {
                vm.registerNative(function.first, function.second);
            }

            auto result = script.fromFile ? vm.interpretFromFile(script.data) : vm.interpretFromText(script.data);

            if (result != InterpretResult::OK)
            {
                LOG_ERROR(DOM, "Script failed: " << (script.fromFile ? script.data : "<inline code>"));
            }

            queue.pop();
        }
    }
}
