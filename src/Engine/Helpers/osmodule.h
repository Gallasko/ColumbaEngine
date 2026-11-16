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
            addNativeFunction("env", nativeEnv);
            addNativeFunction("time", nativeTime);
            addNativeFunction("formatTime", nativeFormatTime);
            addNativeFunction("listDir", nativeListDir);
            addNativeFunction("isDir", nativeIsDir);
            addNativeFunction("makeDir", nativeMakeDir);
            addNativeFunction("exit", nativeExit);
        }

        // call(command) / call(command, {input, cwd})
        static Value nativeCall(VM* vm, int argCount, Value* args);

        // env(name): the variable's value, none when it is not set
        static Value nativeEnv(VM* vm, int argCount, Value* args);

        // time(): milliseconds since 1970-01-01 UTC
        static Value nativeTime(VM* vm, int argCount, Value* args);

        // formatTime(ms, format): `ms` in UTC with strftime's conversions ("%Y-%m-%d")
        static Value nativeFormatTime(VM* vm, int argCount, Value* args);

        // listDir(path): the names in the folder, sorted
        static Value nativeListDir(VM* vm, int argCount, Value* args);

        // isDir(path): whether it is a folder
        static Value nativeIsDir(VM* vm, int argCount, Value* args);

        // makeDir(path): the folder and its missing parents
        static Value nativeMakeDir(VM* vm, int argCount, Value* args);

        // exit(code): ends the program at once
        static Value nativeExit(VM* vm, int argCount, Value* args);
    };
}
