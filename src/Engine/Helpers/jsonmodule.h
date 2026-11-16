#pragma once

#include "Compiler/vm.h"
#include "Compiler/native_module.h"

namespace pg
{
    /**
     * Json Module
     * JSON text to PgScript values and back, keys kept in their order:
     *
     *   import "json" as json
     *   var record = json.parse(readFile("record.json"))
     *   writeFile("record.json", json.stringify(record, 2))
     *
     * Objects are tables, arrays lists, null is none. A number written without '.' or an
     * exponent is an int when it fits one, every other number a double.
     */
    class JsonModule : public NativeModule
    {
    public:
        // Deeper nesting than this is refused both ways (it also stops a table containing itself)
        static constexpr int MAX_DEPTH = 512;

        JsonModule()
        {
            addNativeFunction("parse", nativeParse);
            addNativeFunction("stringify", nativeStringify);
        }

        // parse(text)
        static Value nativeParse(VM* vm, int argCount, Value* args);

        // stringify(value) compact, stringify(value, indent) with `indent` spaces a level
        static Value nativeStringify(VM* vm, int argCount, Value* args);
    };
}
