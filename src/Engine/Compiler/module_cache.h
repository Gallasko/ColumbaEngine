#pragma once

/**
 * @file module_cache.h
 * @brief A module compiled once a process, not once an import.
 *
 * Every script compiles the modules it imports from source, and the modules they import: the
 * Chronicle rules compiled lib.pg fourteen times for eight scripts. A module comes out of its
 * compile the same whenever these are the same, so its bytecode is kept and made again in the
 * next VM that imports it:
 *  - the files it read: its own source and those of the modules it imports, file by file, and
 *    the native modules it imports still no file;
 *  - the VM's passes, AST and bytecode, and whether optimizations are on;
 *  - what the AST passes learnt from the program of the script compiled, for the names the
 *    module calls and those an inlined body brings (AstPass::factsAbout): a module is compiled
 *    below its importer, with what the passes learnt from that whole program.
 *
 * The bytecode kept went through the bytecode passes: a module taken from here, or compiled
 * here, is `precompiled`, as one imported from its .pgc is.
 */

#include "Interpreter/token.h"
#include "value_nanbox.h"

#include <queue>
#include <string>
#include <vector>

namespace pg
{
    struct VM;

    namespace modulecache
    {
        // The module made from the cache for this VM, 0x0 when nothing kept fits. Taken, what it
        // read counts as read by the modules compiled around it
        Value find(VM* vm, const std::string& fullPath, const std::string& moduleName, const std::string& source);

        // A module compiled from source: what it reads is recorded from here...
        void begin(const std::string& fullPath, const std::string& source, const std::queue<Token>& tokens);

        struct Compiled
        {
            Value function = 0x0;   // 0x0: it did not compile
            bool remade = false;    // Made from its bytecode: the compiler's own functions are not used
        };

        // ...to here. `compiled` is its script function (0x0 when it failed), `functions` every
        // function its compile made. The bytecode passes run on them, the module is kept and made
        // again from what is kept. Not remade (it could not be written), the compiled functions are
        // the module, passed already
        Compiled end(VM* vm, Value compiled, const std::vector<Value>& functions, const std::string& moduleName);

        // An import read from bytecode, or a native module: what the modules being compiled depend on
        void readBytecode();
        void foundNoFile(const std::string& fullPath);

        // Everything kept forgotten, and the counts started again
        void clear();

        // The modules compiled from source and those taken from the cache, since the start or clear()
        struct Counts
        {
            size_t compiled = 0;
            size_t taken = 0;
        };

        Counts counts();
    }
}
