#pragma once

#include "Compiler/vm.h"
#include "Compiler/native_module.h"

namespace pg
{
    /**
     * Os Module
     * Commands, the environment, time and folders, for the command-line tool only: never registered
     * by EntitySystem::setupVm, so a game, a mod or the web build cannot run a command.
     *
     *   import "os" as os
     *   var r = os.call(["git", "log", "--oneline", "-5"])   // a list runs directly, no shell
     *   var s = os.call("git log --oneline | wc -l")         // a string runs through /bin/sh -c
     *   var a = os.call(["cat"], {input: "text", cwd: "/tmp"})
     *   // r.code, r.out, r.err
     *
     * A command that fails gives its code; one that cannot start gives 127 and the reason in err;
     * one killed by a signal gives 128 + the signal. Only wrong arguments are script errors.
     */
    class OsModule : public NativeModule
    {
    public:
        OsModule()
        {
            addNativeFunction("call", nativeCall);
        }

        // call(command) / call(command, {input, cwd})
        static Value nativeCall(VM* vm, int argCount, Value* args);
    };
}
